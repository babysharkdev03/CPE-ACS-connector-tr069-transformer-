#!/usr/bin/env python3
"""Small CWMP ACS used to prove that tr69d can complete an Inform session."""

from __future__ import annotations

import html
import json
import os
import tempfile
import threading
import time
import xml.etree.ElementTree as ET
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any


SOAP_NS = "http://schemas.xmlsoap.org/soap/envelope/"
CWMP_NS = "urn:dslforum-org:cwmp-1-2"


def local_name(tag: str) -> str:
    return tag.rsplit("}", 1)[-1].split(":", 1)[-1]


def child(node: ET.Element | None, name: str) -> ET.Element | None:
    if node is None:
        return None
    return next((item for item in node if local_name(item.tag) == name), None)


def descendant(node: ET.Element | None, name: str) -> ET.Element | None:
    if node is None:
        return None
    return next((item for item in node.iter() if local_name(item.tag) == name), None)


def text(node: ET.Element | None) -> str:
    return "" if node is None or node.text is None else node.text.strip()


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


class DeviceStore:
    def __init__(self, path: str):
        self.path = Path(path)
        self.lock = threading.Lock()
        self.devices: dict[str, dict[str, Any]] = {}
        self._load()

    def _load(self) -> None:
        try:
            value = json.loads(self.path.read_text(encoding="utf-8"))
            if isinstance(value, dict):
                self.devices = value
        except (FileNotFoundError, json.JSONDecodeError, OSError):
            self.devices = {}

    def _save(self) -> None:
        self.path.parent.mkdir(parents=True, exist_ok=True)
        fd, temporary = tempfile.mkstemp(prefix="devices-", suffix=".json", dir=self.path.parent)
        try:
            with os.fdopen(fd, "w", encoding="utf-8") as output:
                json.dump(self.devices, output, indent=2, sort_keys=True)
            os.replace(temporary, self.path)
        finally:
            try:
                os.unlink(temporary)
            except FileNotFoundError:
                pass

    def update_from_inform(self, root: ET.Element, source_ip: str) -> dict[str, Any]:
        device_id = descendant(root, "DeviceId")
        identity = {
            "manufacturer": text(child(device_id, "Manufacturer")),
            "oui": text(child(device_id, "OUI")),
            "productClass": text(child(device_id, "ProductClass")),
            "serialNumber": text(child(device_id, "SerialNumber")),
        }
        key = identity["serialNumber"] or "-".join(
            filter(None, (identity["oui"], identity["productClass"], source_ip))
        )
        events = [text(item) for item in root.iter() if local_name(item.tag) == "EventCode"]
        parameters: dict[str, str] = {}
        for item in root.iter():
            if local_name(item.tag) != "ParameterValueStruct":
                continue
            name = text(child(item, "Name"))
            if name:
                parameters[name] = text(child(item, "Value"))

        now_epoch = time.time()
        with self.lock:
            previous = self.devices.get(key, {})
            record = {
                **previous,
                **identity,
                "id": key,
                "sourceIp": source_ip,
                "events": events,
                "parameters": {**previous.get("parameters", {}), **parameters},
                "lastInform": utc_now(),
                "lastSeenEpoch": now_epoch,
                "informCount": int(previous.get("informCount", 0)) + 1,
            }
            self.devices[key] = record
            self._save()
            return dict(record)

    def snapshot(self, online_window: int) -> list[dict[str, Any]]:
        now_epoch = time.time()
        with self.lock:
            records = [dict(value) for value in self.devices.values()]
        for record in records:
            record["online"] = now_epoch - float(record.get("lastSeenEpoch", 0)) <= online_window
        return sorted(records, key=lambda item: item.get("lastSeenEpoch", 0), reverse=True)


def soap_envelope(cwmp_id: str, body: str) -> bytes:
    return (
        '<?xml version="1.0" encoding="UTF-8"?>'
        f'<soap-env:Envelope xmlns:soap-env="{SOAP_NS}" xmlns:cwmp="{CWMP_NS}">'
        '<soap-env:Header>'
        f'<cwmp:ID soap-env:mustUnderstand="1">{html.escape(cwmp_id)}</cwmp:ID>'
        '</soap-env:Header>'
        f'<soap-env:Body>{body}</soap-env:Body>'
        '</soap-env:Envelope>'
    ).encode()


class AcsHandler(BaseHTTPRequestHandler):
    server_version = "dev-ACS-Mock/1.0"

    @property
    def acs(self) -> "AcsServer":
        return self.server  # type: ignore[return-value]

    def _send(self, status: int, payload: bytes = b"", content_type: str = "text/plain") -> None:
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(payload)))
        self.send_header("Connection", "close")
        self.end_headers()
        if payload:
            self.wfile.write(payload)

    def do_POST(self) -> None:  # noqa: N802
        try:
            length = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            length = 0
        payload = self.rfile.read(length) if length else b""
        if not payload.strip():
            self._send(204)
            return
        try:
            root = ET.fromstring(payload)
        except ET.ParseError as error:
            self._send(400, f"Malformed SOAP XML: {error}\n".encode())
            return

        body = descendant(root, "Body")
        method = next((item for item in list(body or []) if isinstance(item.tag, str)), None)
        method_name = local_name(method.tag) if method is not None else ""
        cwmp_id = text(descendant(descendant(root, "Header"), "ID")) or "0"

        if method_name == "Inform":
            record = self.acs.store.update_from_inform(root, self.client_address[0])
            print(
                f"INFORM id={record['id']} source={record['sourceIp']} "
                f"events={','.join(record['events'])}",
                flush=True,
            )
            response = soap_envelope(
                cwmp_id, "<cwmp:InformResponse><MaxEnvelopes>1</MaxEnvelopes></cwmp:InformResponse>"
            )
            self._send(200, response, "text/xml; charset=utf-8")
        elif method_name == "TransferComplete":
            response = soap_envelope(cwmp_id, "<cwmp:TransferCompleteResponse/>")
            self._send(200, response, "text/xml; charset=utf-8")
        else:
            self._send(204)

    def do_GET(self) -> None:  # noqa: N802
        devices = self.acs.store.snapshot(self.acs.online_window)
        if self.path == "/healthz":
            self._send(200, b"ok\n")
            return
        if self.path == "/api/devices":
            self._send(200, json.dumps(devices, indent=2).encode(), "application/json")
            return
        if self.path not in ("/", "/index.html"):
            self._send(404, b"not found\n")
            return

        rows = []
        for device in devices:
            status = "ONLINE" if device["online"] else "OFFLINE"
            rows.append(
                "<tr>"
                f"<td class='{status.lower()}'>{status}</td>"
                f"<td>{html.escape(device.get('serialNumber', ''))}</td>"
                f"<td>{html.escape(device.get('manufacturer', ''))}</td>"
                f"<td>{html.escape(device.get('productClass', ''))}</td>"
                f"<td>{html.escape(device.get('sourceIp', ''))}</td>"
                f"<td>{html.escape(', '.join(device.get('events', [])))}</td>"
                f"<td>{html.escape(device.get('lastInform', ''))}</td>"
                f"<td>{device.get('informCount', 0)}</td>"
                "</tr>"
            )
        page = f"""<!doctype html>
<html><head><meta charset="utf-8"><meta http-equiv="refresh" content="10">
<title>dev ACS Mock</title><style>
body{{font-family:system-ui;margin:2rem;background:#111827;color:#e5e7eb}}
table{{border-collapse:collapse;width:100%;background:#1f2937}}
th,td{{padding:.7rem;border:1px solid #374151;text-align:left}}
.online{{color:#4ade80;font-weight:bold}} .offline{{color:#f87171;font-weight:bold}}
code{{background:#374151;padding:.2rem .4rem}}</style></head>
<body><h1>dev ACS Mock</h1>
<p>CWMP URL: <code>http://HOST_IP:3000/</code> · JSON: <a href="/api/devices">/api/devices</a></p>
<table><thead><tr><th>Status</th><th>Serial</th><th>Manufacturer</th><th>Product</th>
<th>Source IP</th><th>Events</th><th>Last Inform</th><th>Count</th></tr></thead>
<tbody>{''.join(rows) or '<tr><td colspan="8">Chưa nhận Inform</td></tr>'}</tbody></table>
</body></html>"""
        self._send(200, page.encode(), "text/html; charset=utf-8")

    def log_message(self, format_string: str, *args: Any) -> None:
        print(f"{self.client_address[0]} {format_string % args}", flush=True)


class AcsServer(ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self, address: tuple[str, int], store: DeviceStore, online_window: int):
        super().__init__(address, AcsHandler)
        self.store = store
        self.online_window = online_window


def create_server(host: str, port: int, data_file: str, online_window: int) -> AcsServer:
    return AcsServer((host, port), DeviceStore(data_file), online_window)


def main() -> None:
    host = os.getenv("ACS_HOST", "0.0.0.0")
    port = int(os.getenv("ACS_PORT", "3000"))
    data_file = os.getenv("ACS_DATA_FILE", "/data/devices.json")
    online_window = int(os.getenv("ACS_ONLINE_WINDOW", "180"))
    server = create_server(host, port, data_file, online_window)
    print(f"dev ACS Mock listening on http://{host}:{port}/", flush=True)
    server.serve_forever()


if __name__ == "__main__":
    main()
