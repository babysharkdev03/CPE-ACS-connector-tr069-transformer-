# dev ACS Mock

ACS HTTP/CWMP tối giản để kiểm tra `tr69d` có gửi Inform thành công hay không.

```sh
docker compose up -d --build
docker compose logs -f
```

- CWMP URL: `http://<IP-may-chay-Docker>:3000/`
- Web UI: `http://localhost:3000/`
- JSON devices: `http://localhost:3000/api/devices`
- Health: `http://localhost:3000/healthz`

Trên Pi:

```sh
trs Device.ManagementServer.EnableCWMP true acs-enable
trs Device.ManagementServer.URL 'http://<IP-may-chay-Docker>:3000/' acs-url
trs Device.ManagementServer.Username '' acs-user
trs Device.ManagementServer.Password '' acs-password
trs Device.ManagementServer.PeriodicInformEnable true acs-periodic
trs Device.ManagementServer.PeriodicInformInterval 60 acs-interval
ubus call tr69 inform_now '{}'
logread -f -e tr69d
```

Đây là mock ACS cho Inform/session test, không thay thế đầy đủ GenieACS.
