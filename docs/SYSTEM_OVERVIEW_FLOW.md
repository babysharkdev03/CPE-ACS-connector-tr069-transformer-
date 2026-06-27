# Phân Tích Flow Sơ Đồ System Overview

Tài liệu này diễn giải chi tiết sơ đồ [system-overview.drawio](system-overview.drawio). Sơ đồ mô tả luồng hoạt động tổng quan của hệ thống Mock TR-069 CPE, từ lúc cấu hình Pi/OpenWrt online lên ACS cho tới các thao tác vận hành như `get`, `set`, `summon`, `refresh`, `reboot` và đồng bộ giá trị giữa ACS và CPE.

## 1. Mục Đích Của Sơ Đồ

Sơ đồ `system-overview.drawio` dùng để trả lời các câu hỏi chính:

- Người vận hành cấu hình CPE online lên ACS như thế nào?
- ACS giao tiếp với CPE qua những endpoint nào?
- Khi ACS set/get/reboot, request đi qua các lớp nào?
- Datamodel TR-181 được map sang UCI và apply runtime ra sao?
- Nếu lỗi xảy ra, có thể khoanh vùng lỗi ở layer nào?
- Khi giá trị được set thành công, hệ thống xác nhận applied value như thế nào?

Sơ đồ được chia thành 5 lane chính:

```text
Operator / Tester
GenieACS / ACS
CPE: tr69d / CWMP
Transformer / UCI
Runtime / Device
```

Mỗi lane đại diện cho một nhóm trách nhiệm khác nhau trong hệ thống.

## 2. Lane Operator / Tester

Lane này đại diện cho người vận hành hoặc tester. Người vận hành có thể thao tác ở hai phía:

- Trên ACS UI để set/get/summon/reboot thiết bị.
- Trên Pi/OpenWrt qua CLI `trs`, `trg`, `uci`, `ubus`, `logread`.

Bước đầu tiên trong sơ đồ là cấu hình ACS URL cho CPE:

```sh
trs Device.ManagementServer.URL http://192.168.10.13:7547/
trs Device.ManagementServer.EnableCWMP true
trs Device.ManagementServer.PeriodicInformEnable true
trs Device.ManagementServer.PeriodicInformInterval 60
```

Ý nghĩa:

- `Device.ManagementServer.URL` là địa chỉ CWMP endpoint của ACS.
- CPE sẽ gửi Inform và nhận RPC từ địa chỉ này.
- Trong lab hiện tại, ACS chạy trong WSL nhưng được expose ra Wi-Fi IP của Windows là `192.168.10.13`, nên Pi cần trỏ tới `http://192.168.10.13:7547/`.

Sau khi cấu hình, người vận hành restart hoặc reload daemon:

```sh
/etc/init.d/tr69 restart
```

Hoặc:

```sh
ubus call tr69 reload '{}'
```

## 3. Lane CPE: tr69d / CWMP

Đây là phần lõi chạy trên OpenWrt. `tr69d` là daemon C++ chịu trách nhiệm:

- Load runtime config từ UCI.
- Mở CWMP session tới ACS.
- Gửi Inform.
- Nhận RPC từ ACS.
- Chạy Connection Request Server.
- Dispatch request sang RPC Handler.
- Apply runtime config sau khi Transformer commit UCI.
- Thực hiện reboot thật khi nhận RPC `Reboot`.

### 3.1 Start / Reload tr69d

Khi daemon start hoặc nhận reload, nó đọc cấu hình từ `/etc/config/tr69`, bao gồm:

```text
tr69.mgmt_srv.url
tr69.mgmt_srv.username
tr69.mgmt_srv.password
tr69.periodic_inform.enable
tr69.periodic_inform.interval
tr69.conn_request.auth
tr69.conn_request.username
tr69.conn_request.password
tr69.settings.port
```

Sau đó daemon cập nhật runtime state:

- ACS URL hiện tại.
- Periodic Inform timer.
- Connection Request listener.
- Credential dùng cho ACS và Connection Request.
- Trạng thái bootstrap/reboot event.

### 3.2 Decision: ACS URL Valid?

Daemon kiểm tra ACS URL có hợp lệ không.

Nếu ACS URL rỗng hoặc không hợp lệ:

- Daemon không tạo CWMP session.
- Hệ thống vào trạng thái idle.
- Log cảnh báo kiểu:

```text
CWMP idle: ACS URL is empty
```

Nếu ACS URL hợp lệ:

- Daemon bắt đầu CWMP session.
- Gửi Inform lên ACS.

## 4. Lane GenieACS / ACS

ACS là server quản lý thiết bị. Trong hệ thống này thường dùng GenieACS:

```text
UI:   http://192.168.10.13:3000/
CWMP: http://192.168.10.13:7547/
NBI:  http://192.168.10.13:7557/
```

### 4.1 CPE Gửi Inform

Khi CPE gửi Inform, ACS cập nhật trạng thái thiết bị:

- Last Inform.
- Online now.
- Serial number.
- Product class.
- Datamodel values.
- ConnectionRequestURL.

Các event Inform thường gặp:

```text
0 BOOTSTRAP
1 BOOT
2 PERIODIC
4 VALUE CHANGE
M Reboot
6 CONNECTION REQUEST
```

Sau Inform, ACS có thể trả RPC trong cùng session, ví dụ:

- `GetParameterValues`
- `SetParameterValues`
- `GetParameterNames`
- `Reboot`

### 4.2 ACS Task Type

Trong sơ đồ có decision `ACS task type?`, đại diện cho việc ACS có task nào đang chờ cho thiết bị.

Các nhánh chính:

- `Get / Refresh / Summon`
- `SetParameterValues`
- `Reboot RPC`

## 5. Flow Get / Refresh / Summon

Nhánh này xảy ra khi người vận hành bấm refresh/summon hoặc ACS cần đọc giá trị parameter từ CPE.

### 5.1 ACS Tạo GetParameterValues

ACS gửi RPC:

```text
GetParameterValues
```

Ví dụ parameter:

```text
Device.ManagementServer.PeriodicInformInterval
Device.ManagementServer.ConnectionRequestURL
Device.ManagementServer.CWMPRetryIntervalMultiplier
```

### 5.2 RPC Handler Nhận Request

Trong `tr69d`, `RPC Handler` nhận request và ghi log:

```text
ACS RPC: GetParameterValues
ACS get request: Device.ManagementServer.PeriodicInformInterval
```

Sau đó handler gọi Transformer để đọc giá trị.

### 5.3 Transformer Đọc Datamodel

Transformer tra `Device.map` để biết datamodel tương ứng UCI key nào.

Ví dụ:

```text
Device.ManagementServer.PeriodicInformInterval
  -> tr69.periodic_inform.interval
```

Transformer đọc UCI:

```sh
/sbin/uci -q get tr69.periodic_inform.interval
```

Sau đó trả lại giá trị cho `tr69d`.

### 5.4 CPE Trả Response Cho ACS

`tr69d` build SOAP response và gửi về ACS:

```text
GetParameterValuesResponse
```

ACS cập nhật giá trị hiển thị trên UI.

## 6. Flow SetParameterValues Từ ACS Xuống CPE

Đây là flow quan trọng nhất vì liên quan tới validate, ghi UCI, apply runtime và xác nhận applied value.

### 6.1 ACS Gửi SetParameterValues

Người vận hành set một hoặc nhiều parameter trên ACS, ví dụ:

```text
Device.ManagementServer.PeriodicInformInterval = 137
```

ACS tạo task và gọi Connection Request tới CPE để đánh thức session.

Sau đó trong CWMP session, ACS gửi RPC:

```text
SetParameterValues
```

### 6.2 RPC Handler Log Request

`tr69d` ghi log parameter ACS yêu cầu set:

```text
ACS RPC: SetParameterValues
ACS set request: Device.ManagementServer.PeriodicInformInterval=137
```

Nếu ACS set nhiều parameter cùng lúc, handler log từng parameter được set, không log toàn bộ datamodel không liên quan.

### 6.3 Transformer Validate

Transformer nhận danh sách parameter và validate:

- Parameter có tồn tại trong `Device.map` không?
- Parameter có `readWrite` không?
- Kiểu dữ liệu có đúng không?
- Giá trị có nằm trong range hợp lệ không?
- Enumeration có hợp lệ không?
- Password/secret có cần che log không?

Nếu validate fail, Transformer trả lỗi về RPC Handler. CPE trả CWMP Fault về ACS.

### 6.4 Map Datamodel Sang UCI

Nếu validate OK, Transformer map datamodel sang UCI.

Ví dụ:

```text
Device.ManagementServer.PeriodicInformInterval
  -> tr69.periodic_inform.interval
```

Sau đó chạy:

```sh
/sbin/uci -q set tr69.periodic_inform.interval='137'
```

Điểm quan trọng: daemon và transformer dùng `/sbin/uci` nếu có để tránh lỗi môi trường `procd` không có PATH giống shell interactive.

### 6.5 Commit UCI

Sau khi stage tất cả parameter hợp lệ, Transformer commit:

```sh
/sbin/uci -q commit tr69
```

Nếu commit fail:

- Transformer restore/rollback các giá trị cũ nếu cần.
- RPC Handler trả CWMP Fault.
- ACS hiển thị task fault.
- Log giúp xác định lỗi nằm ở UCI layer.

### 6.6 Read-Back Verification

Sau commit, Transformer đọc lại UCI để đảm bảo giá trị thực sự đã ghi đúng.

Ví dụ:

```sh
/sbin/uci -q get tr69.periodic_inform.interval
```

Nếu expected và actual không khớp:

```text
expected=137
actual=300
```

Thì hệ thống coi là lỗi applied/read-back và trả fault. Đây là bước quan trọng để tránh tình trạng ACS báo set xong nhưng CPE runtime vẫn giữ giá trị cũ.

### 6.7 ubus Reload Runtime

Nếu commit và read-back OK, Transformer gọi:

```sh
ubus call tr69 reload '{}'
```

Mục tiêu:

- Không restart process `tr69d`.
- Không mất session không cần thiết.
- Daemon tự đọc lại config mới.
- Timer, ACS URL, credential, Connection Request server được update trong runtime.

### 6.8 Runtime Apply

`tr69d` so sánh config cũ và config mới:

- Nếu `PeriodicInformInterval` đổi, update timer.
- Nếu ACS URL đổi, session sau sẽ dùng URL mới.
- Nếu Connection Request auth/port đổi, restart listener.
- Nếu reboot flag/event đổi, xử lý event tương ứng.

Log mong đợi:

```text
SetParameterValues committed 1 parameter(s)
ACS set applied: Device.ManagementServer.PeriodicInformInterval=137
Runtime configuration updated: CWMP=enabled, URL=http://192.168.10.13:7547/, PeriodicInform=enabled/137s
```

### 6.9 Response Về ACS

Sau khi apply thành công, CPE trả:

```text
SetParameterValuesResponse
```

ACS clear task và cập nhật giá trị applied.

## 7. Flow Reboot

Nhánh `Reboot RPC` xảy ra khi người vận hành bấm reboot trên ACS.

### 7.1 ACS Gửi Reboot RPC

ACS gửi:

```text
Reboot
```

`tr69d` log:

```text
ACS RPC: Reboot
ACS reboot request: CommandKey=<empty>
```

### 7.2 CPE Trả RebootResponse Trước

Theo TR-069, CPE nên trả response trước rồi mới reboot. Vì vậy daemon:

1. Trả `RebootResponse`.
2. Schedule reboot sau một khoảng ngắn.

### 7.3 Runtime Schedule Real Reboot

Trên OpenWrt thật, daemon gọi:

```sh
/sbin/reboot
```

Ở môi trường mock test, có thể dùng mock reboot để không reboot máy host.

Sau khi thiết bị boot lại, CPE gửi Inform event:

```text
1 BOOT
M Reboot
```

ACS thấy thiết bị online trở lại.

## 8. Flow CPE Local Set Lên ACS

Flow này không nằm thành một nhánh riêng lớn trong draw.io nhưng là một luồng vận hành quan trọng.

Người vận hành chạy trên Pi:

```sh
trs Device.ManagementServer.PeriodicInformInterval 137
```

Luồng xử lý:

1. `trs` gọi Lua Transformer CLI.
2. Transformer validate parameter.
3. Transformer map datamodel sang UCI.
4. Transformer `uci set`, `uci commit`, read-back.
5. Transformer gọi `ubus call tr69 reload`.
6. `tr69d` reload runtime config.
7. `tr69d` gửi Inform event `4 VALUE CHANGE`.
8. ACS nhận Inform và cập nhật value.

Log mong đợi:

```text
Runtime configuration updated: CWMP=enabled, URL=http://192.168.10.13:7547/, PeriodicInform=enabled/137s
CWMP Inform: events=4 VALUE CHANGE
CWMP state: CONNECTING
CWMP state: INFORM_SENT
CWMP state: SESSION_CLOSE
```

## 9. Vai Trò Của Connection Request URL

Connection Request URL là địa chỉ để ACS gọi ngược về CPE. Đây là điểm rất dễ gây lỗi `No contact from CPE` hoặc `405`.

Đúng:

```text
Device.ManagementServer.ConnectionRequestURL = http://<ip-cua-pi>:7547/connection-request
```

Sai:

```text
http://192.168.10.13:7547/connection-request
```

Lý do: `192.168.10.13` là ACS/Windows host, không phải Pi. Nếu ACS gọi vào chính nó hoặc gọi nhầm port, GenieACS có thể trả `405`, còn CPE không nhận được request nào.

Transformer hiện có logic tránh advertise URL xấu:

- Không dùng `0.0.0.0`.
- Không dùng `127.0.0.1`.
- Không để Connection Request URL trỏ về ACS host/port.
- Ưu tiên IP thực của interface hoặc route tới ACS.

## 10. Phân Tích Lỗi Theo Layer

Sơ đồ giúp khoanh vùng lỗi theo layer:

### Operator / Network

Dấu hiệu:

- Pi không ping được ACS.
- Browser không mở được `http://192.168.10.13:3000`.
- ACS URL cấu hình sai.

Kiểm tra:

```sh
trg Device.ManagementServer.URL
curl -I http://192.168.10.13:3000/
curl -I http://192.168.10.13:7547/
```

### ACS Layer

Dấu hiệu:

- Task bị stale/fault.
- ACS báo `No contact from CPE`.
- ACS báo `Connection request error`.

Kiểm tra:

- CPE có online không.
- ConnectionRequestURL trên ACS có phải IP Pi không.
- ACS có gọi được URL đó không.

### CPE / CWMP Layer

Dấu hiệu:

- CPE không gửi Inform.
- Session đóng bất thường.
- Không nhận RPC từ ACS.

Kiểm tra:

```sh
logread -f | grep tr69d
ubus call tr69 status '{}'
```

### Transformer / UCI Layer

Dấu hiệu:

- ACS set request đúng nhưng applied sai.
- `trs` báo OK nhưng runtime không đổi.
- Log có `UCI set failed`, `UCI commit failed`, read-back mismatch.

Kiểm tra:

```sh
uci show tr69
uci commit tr69
trg Device.ManagementServer.PeriodicInformInterval
```

### Runtime / Device Layer

Dấu hiệu:

- UCI đã đúng nhưng daemon không apply.
- PeriodicInformInterval không đổi trong runtime.
- Reboot RPC trả OK nhưng thiết bị không reboot.

Kiểm tra:

```sh
ubus call tr69 reload '{}'
ubus call tr69 status '{}'
logread -f | grep tr69d
```

## 11. Kết Luận Flow

Flow chuẩn của hệ thống có thể tóm tắt như sau:

```text
Operator cấu hình ACS URL
  -> tr69d load runtime config
  -> CPE gửi Inform
  -> ACS thấy thiết bị online
  -> ACS tạo task get/set/reboot
  -> CPE nhận RPC trong CWMP session
  -> RPC Handler dispatch sang Transformer hoặc Runtime
  -> Transformer validate/map/uci commit/read-back
  -> ubus reload runtime
  -> tr69d apply config
  -> CPE trả response
  -> ACS cập nhật applied value
```

Điểm bảo vệ quan trọng nhất trong thiết kế hiện tại là `read-back verification`: sau mỗi lần set, hệ thống không chỉ tin rằng `uci commit` thành công mà còn đọc lại giá trị thực tế để đảm bảo value CPE đã ghi trùng với value ACS yêu cầu.

