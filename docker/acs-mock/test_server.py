#!/usr/bin/env python3

import http.client
import json
import tempfile
import threading
import unittest
from pathlib import Path

from server import create_server


INFORM = b'''<?xml version="1.0"?>
<soap-env:Envelope xmlns:soap-env="http://schemas.xmlsoap.org/soap/envelope/"
 xmlns:cwmp="urn:dslforum-org:cwmp-1-2">
 <soap-env:Header><cwmp:ID>test-1</cwmp:ID></soap-env:Header>
 <soap-env:Body><cwmp:Inform>
  <DeviceId><Manufacturer>dev</Manufacturer><OUI>000000</OUI>
   <ProductClass>OpenWrt-RPi5</ProductClass><SerialNumber>RPI5-TEST</SerialNumber></DeviceId>
  <Event><EventStruct><EventCode>1 BOOT</EventCode><CommandKey/></EventStruct></Event>
  <ParameterList><ParameterValueStruct><Name>Device.ManagementServer.URL</Name>
   <Value>http://acs:3000/</Value></ParameterValueStruct></ParameterList>
 </cwmp:Inform></soap-env:Body>
</soap-env:Envelope>'''


class AcsMockTest(unittest.TestCase):
    def test_inform_and_session_close(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            server = create_server("127.0.0.1", 0, str(Path(directory) / "devices.json"), 180)
            thread = threading.Thread(target=server.serve_forever, daemon=True)
            thread.start()
            port = server.server_address[1]
            try:
                connection = http.client.HTTPConnection("127.0.0.1", port)
                connection.request("POST", "/", INFORM, {"Content-Type": "text/xml"})
                response = connection.getresponse()
                self.assertEqual(response.status, 200)
                self.assertIn(b"InformResponse", response.read())

                connection = http.client.HTTPConnection("127.0.0.1", port)
                connection.request("POST", "/", b"")
                self.assertEqual(connection.getresponse().status, 204)

                connection = http.client.HTTPConnection("127.0.0.1", port)
                connection.request("GET", "/api/devices")
                response = connection.getresponse()
                devices = json.loads(response.read())
                self.assertEqual(devices[0]["serialNumber"], "RPI5-TEST")
                self.assertTrue(devices[0]["online"])
            finally:
                server.shutdown()
                server.server_close()
                thread.join(timeout=2)


if __name__ == "__main__":
    unittest.main()
