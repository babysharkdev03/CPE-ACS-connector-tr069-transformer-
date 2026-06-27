#!/usr/bin/env python3
"""Small dependency-free client for the GenieACS NBI API."""

import argparse
import json
import sys
from urllib.error import HTTPError, URLError
from urllib.parse import quote, urlencode
from urllib.request import Request, urlopen


class GenieAcs:
    def __init__(self, base_url: str):
        self.base = base_url.rstrip("/")

    def request(self, method: str, path: str, payload=None):
        body = None if payload is None else json.dumps(payload).encode()
        request = Request(self.base + path, data=body, method=method)
        request.add_header("Accept", "application/json")
        if body is not None:
            request.add_header("Content-Type", "application/json")
        try:
            with urlopen(request, timeout=30) as response:
                raw = response.read().decode()
                return json.loads(raw) if raw else {"status": response.status}
        except HTTPError as exc:
            detail = exc.read().decode(errors="replace")
            raise RuntimeError(f"GenieACS HTTP {exc.code}: {detail}") from exc
        except URLError as exc:
            raise RuntimeError(f"Cannot reach GenieACS NBI: {exc.reason}") from exc

    def task(self, device_id: str, payload: dict, connection_request=True):
        suffix = "?connection_request" if connection_request else ""
        return self.request("POST", f"/devices/{quote(device_id, safe='')}/tasks{suffix}", payload)


def print_json(value):
    print(json.dumps(value, indent=2, ensure_ascii=False, sort_keys=True))


def main():
    parser = argparse.ArgumentParser(description="GenieACS NBI helper for the tr69d demo")
    parser.add_argument("--nbi", default="http://127.0.0.1:7557", help="GenieACS NBI base URL")
    sub = parser.add_subparsers(dest="command", required=True)

    list_parser = sub.add_parser("list-devices")
    list_parser.add_argument("--query", default="{}", help="GenieACS Mongo-style JSON query")

    get_parser = sub.add_parser("get-parameter")
    get_parser.add_argument("device_id")
    get_parser.add_argument("parameter")
    get_parser.add_argument("--no-connection-request", action="store_true")

    set_parser = sub.add_parser("set-parameter")
    set_parser.add_argument("device_id")
    set_parser.add_argument("parameter")
    set_parser.add_argument("value")
    set_parser.add_argument("--type", default="xsd:string")
    set_parser.add_argument("--no-connection-request", action="store_true")

    reboot_parser = sub.add_parser("reboot")
    reboot_parser.add_argument("device_id")
    reboot_parser.add_argument("--no-connection-request", action="store_true")

    download_parser = sub.add_parser("download-file")
    download_parser.add_argument("device_id")
    download_parser.add_argument("file", help="Filename already uploaded to GenieACS")
    download_parser.add_argument("--file-type", default="1 Firmware Upgrade Image")
    download_parser.add_argument("--no-connection-request", action="store_true")

    status_parser = sub.add_parser("task-status")
    status_parser.add_argument("task_id")

    list_tasks_parser = sub.add_parser("list-tasks")
    list_tasks_parser.add_argument("--query", default="{}", help="GenieACS Mongo-style JSON query")

    delete_task_parser = sub.add_parser("delete-task")
    delete_task_parser.add_argument("task_id")

    args = parser.parse_args()
    api = GenieAcs(args.nbi)
    try:
        if args.command == "list-devices":
            query = json.dumps(json.loads(args.query), separators=(",", ":"))
            path = "/devices/?" + urlencode({"query": query, "projection": "_id,_lastInform"})
            result = api.request("GET", path)
        elif args.command == "get-parameter":
            result = api.task(args.device_id,
                {"name": "getParameterValues", "parameterNames": [args.parameter]},
                not args.no_connection_request)
        elif args.command == "set-parameter":
            result = api.task(args.device_id,
                {"name": "setParameterValues", "parameterValues": [[args.parameter, args.value, args.type]]},
                not args.no_connection_request)
        elif args.command == "reboot":
            result = api.task(args.device_id, {"name": "reboot"}, not args.no_connection_request)
        elif args.command == "download-file":
            result = api.task(args.device_id,
                {"name": "download", "file": args.file, "fileType": args.file_type},
                not args.no_connection_request)
        elif args.command == "task-status":
            result = api.request("GET", f"/tasks/{quote(args.task_id, safe='')}")
        elif args.command == "list-tasks":
            query = json.dumps(json.loads(args.query), separators=(",", ":"))
            result = api.request("GET", "/tasks/?" + urlencode({"query": query}))
        else:
            result = api.request("DELETE", f"/tasks/{quote(args.task_id, safe='')}")
        print_json(result)
        return 0
    except (RuntimeError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
