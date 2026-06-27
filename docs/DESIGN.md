# System design

## Component diagram

```mermaid
flowchart LR
    ACS["GenieACS on Linux PC"] <-->|"SOAP over HTTP(S)"| HTTP["HttpClient / libcurl"]
    ACS -->|"Digest Connection Request"| CRS["ConnectionRequestServer"]
    CRS --> D["Tr069Daemon"]
    D --> S["CwmpSession"]
    S --> SM["CwmpStateMachine"]
    S --> HTTP
    S --> SB["SoapBuilder"]
    S --> SP["SoapParser / libxml2"]
    S --> RPC["RpcHandler"]
    RPC --> DM["DataModel"]
    DM --> UCI["UciAdapter"]
    UCI <--> CFG["/etc/config/tr69 + tr181"]
    RPC --> DL["DownloadManager"]
    RPC --> RB["RebootHandler"]
```

## CWMP Inform session

```mermaid
sequenceDiagram
    participant CPE as tr69d CPE
    participant ACS as GenieACS
    CPE->>ACS: HTTP POST cwmp:Inform
    ACS-->>CPE: cwmp:InformResponse
    CPE->>ACS: Empty HTTP POST
    alt ACS has a task
        ACS-->>CPE: Get/Set/Reboot/Download
        CPE->>ACS: Matching RPC Response
        ACS-->>CPE: Next RPC or empty response
    else no task
        ACS-->>CPE: HTTP 204 or empty body
    end
    CPE->>CPE: Close session
```

## State machine

```mermaid
stateDiagram-v2
    [*] --> IDLE
    IDLE --> CONNECTING: boot / periodic / connection request
    CONNECTING --> INFORM_SENT: InformResponse
    INFORM_SENT --> RPC_PROCESSING: ACS RPC available
    RPC_PROCESSING --> RPC_PROCESSING: next ACS RPC
    INFORM_SENT --> SESSION_CLOSE: no ACS RPC
    RPC_PROCESSING --> SESSION_CLOSE: empty ACS response
    SESSION_CLOSE --> IDLE
```

## TR-181 to UCI mapping

| TR-181 path | UCI key | Type | Access |
|---|---|---|---|
| `Device.DeviceInfo.Manufacturer` | `tr181.device.manufacturer` | string | read-only |
| `Device.DeviceInfo.ManufacturerOUI` | `tr181.device.manufacturer_oui` | string | read-only |
| `Device.DeviceInfo.ProductClass` | `tr181.device.product_class` | string | read-only |
| `Device.DeviceInfo.ModelName` | `tr181.device.model_name` | string | read-only |
| `Device.DeviceInfo.SerialNumber` | `tr181.device.serial_number` | string | read-only |
| `Device.DeviceInfo.SoftwareVersion` | `tr181.device.software_version` | string | read-only |
| `Device.ManagementServer.URL` | `tr69.mgmt_srv.url` | string | read/write |
| `Device.ManagementServer.Username` | `tr69.mgmt_srv.username` | string | read/write |
| `Device.ManagementServer.Password` | `tr69.mgmt_srv.password` | string | write; reads empty |
| `Device.ManagementServer.PeriodicInformEnable` | `tr69.periodic_inform.enable` | boolean | read/write |
| `Device.ManagementServer.PeriodicInformInterval` | `tr69.periodic_inform.interval` | unsignedInt | read/write |
| `Device.ManagementServer.ConnectionRequestURL` | computed from runtime IPv4 + `tr69.settings.port` | string | read-only |
| `Device.ManagementServer.ConnectionRequestUsername` | `tr69.conn_request.username` | string | read/write |
| `Device.ManagementServer.ConnectionRequestPassword` | `tr69.conn_request.password` | string | write; reads empty |

`ManufacturerOUI` and `ProductClass` extend the requested subset because CWMP Inform's `DeviceId` requires OUI and ProductClass. UCI values are accessed using `uci get`, `uci set`, and `uci commit`. Parameter names are mapped through a fixed allowlist before any command runs.

ACS URL/credentials and the periodic interval are re-read after `ubus call tr69 reload`.
Connection Request listener bind/port/credentials/auth are hot-reloaded internally; the
`tr69d` process must not be restarted after `SetParameterValues`.

## GenieACS task flow

```mermaid
sequenceDiagram
    participant CLI as PC CLI / GenieACS UI
    participant NBI as GenieACS NBI
    participant ACS as GenieACS CWMP
    participant CPE as tr69d
    CLI->>NBI: POST device task
    NBI->>ACS: Queue task
    opt connection_request query enabled
        ACS->>CPE: HTTP Digest Connection Request
        CPE->>ACS: Inform event 6
    end
    ACS->>CPE: RPC in CWMP session
    CPE->>ACS: RPC response
    ACS->>NBI: Complete/fault task
    CLI->>NBI: GET /tasks/:id
    NBI-->>CLI: Task status JSON
```

## Download and reboot behavior

For Download, the CPE immediately acknowledges with status `1`, ends the ACS task exchange, downloads to `/tmp/tr069-downloads`, and starts a TransferComplete session using `7 TRANSFER COMPLETE` plus `M Download`. This keeps the transfer outside the RPC response round trip. Firmware application is intentionally out of scope.

Reboot is acknowledged before `RebootHandler` runs. With `mock_mode=1`, only a warning is logged. With `mock_mode=0`, `/sbin/reboot` is invoked; the restarted daemon emits `1 BOOT`.
