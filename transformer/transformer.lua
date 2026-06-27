-- Lua Transformer engine used both by embedded C++ and the trg/trs CLI.
local M = {}

local function shell_quote(value)
  return "'" .. tostring(value):gsub("'", "'\\''") .. "'"
end

local function execute_ok(command)
  local ok, why, code = os.execute(command)
  if type(ok) == "number" then return ok == 0 end
  return ok == true and (why == nil or why == "exit") and (code == nil or code == 0)
end

local uci_bin = execute_ok("[ -x /sbin/uci ]") and "/sbin/uci" or "uci"
local ubus_bin = execute_ok("[ -x /bin/ubus ]") and "/bin/ubus"
  or (execute_ok("[ -x /sbin/ubus ]") and "/sbin/ubus" or "ubus")

local trace_enabled = os.getenv("TR69_TRANSFORMER_TRACE") == "1"

local function trace(layer, message)
  if trace_enabled then
    io.stderr:write("[TRANSFORMER][" .. layer .. "] " .. message .. "\n")
  end
end

local function capture(command)
  local handle = io.popen(command .. " 2>/dev/null")
  if not handle then return nil end
  local value = handle:read("*a")
  local ok = handle:close()
  if not ok then return nil end
  return (value:gsub("[\r\n]+$", ""))
end

local function notify_runtime_reload()
  trace("UBUS", "notify runtime reload command=" .. ubus_bin .. " call tr69 reload")
  local ok = execute_ok(ubus_bin .. " call tr69 reload '{}' >/dev/null 2>&1")
  if not ok then
    trace("UBUS", "notify runtime reload status=failed")
    io.stderr:write("transformer: UCI committed but ubus tr69 reload notification failed\n")
  else
    trace("UBUS", "notify runtime reload status=success")
  end
  return ok
end

local function clone(source)
  local result = {}
  for key, value in pairs(source) do result[key] = value end
  return result
end

local function normalize(spec, raw)
  local value = tostring(raw == nil and "" or raw)
  if spec.required and value == "" then
    return nil, "value is required"
  end
  if spec.max and #value > tonumber(spec.max) then
    return nil, "value exceeds maximum length"
  end
  if spec.type == "boolean" then
    local lower = value:lower()
    if lower == "1" or lower == "true" or lower == "yes" or lower == "on" then
      value = "true"
    elseif lower == "0" or lower == "false" or lower == "no" or lower == "off" then
      value = "false"
    else
      return nil, "expected boolean"
    end
  elseif spec.type == "unsignedInt" then
    if not value:match("^%d+$") then return nil, "expected unsignedInt" end
  elseif spec.type == "int" then
    if not value:match("^-?%d+$") then return nil, "expected int" end
  elseif spec.type == "dateTime" then
    if not value:match("^%d%d%d%d%-%d%d%-%d%dT%d%d:%d%d:%d%d") then
      return nil, "expected ISO-8601 dateTime"
    end
  end
  if spec.range and tonumber(value) then
    local number, valid = tonumber(value), false
    for _, range in ipairs(spec.range) do
      if number >= tonumber(range.min) and number <= tonumber(range.max) then
        valid = true
        break
      end
    end
    if not valid then return nil, "value outside allowed range" end
  end
  if spec.enumeration then
    local valid = false
    for _, item in ipairs(spec.enumeration) do
      if value == item then valid = true break end
    end
    if not valid then return nil, "value not in enumeration" end
  end
  return value
end

local function validate_declared_type(spec, declared)
  if not declared or declared == "" then return true end
  local actual = tostring(declared):match("([^:]+)$") or tostring(declared)
  local expected = spec.type or "string"
  if actual ~= expected then
    return nil, "type mismatch: expected " .. expected .. ", received " .. actual
  end
  return true
end

local function uci_key(spec)
  return table.concat({spec.uci.config, spec.uci.section, spec.uci.option}, ".")
end

local function parameter_uci_key(spec)
  if not spec or not spec.uci then return "<none>" end
  return uci_key(spec)
end

local function display_value(spec, value)
  if spec and spec.secret then return "<redacted>" end
  value = tostring(value == nil and "" or value)
  if value == "" then return "<empty>" end
  return value
end

local function display_uci_value(key, value)
  if tostring(key):match("password") or tostring(key):match("secret") then
    return "<redacted>"
  end
  if value == nil then return "<missing>" end
  value = tostring(value)
  if value == "" then return "<empty>" end
  return value
end

local function load_mock(path)
  local values = {}
  if not path then return values end
  local file = io.open(path, "r")
  if not file then return values end
  for line in file:lines() do
    local key, value = line:match("^([^=]+)=(.*)$")
    if key then values[key] = value end
  end
  file:close()
  return values
end

local function save_mock(path, values)
  local temporary = path .. ".tmp"
  local file, err = io.open(temporary, "w")
  if not file then return nil, err end
  local keys = {}
  for key in pairs(values) do keys[#keys + 1] = key end
  table.sort(keys)
  for _, key in ipairs(keys) do file:write(key, "=", values[key], "\n") end
  file:close()
  if not os.rename(temporary, path) then return nil, "cannot replace mock database" end
  return true
end

function M.new(map_path)
  local ok, model = pcall(dofile, map_path)
  if not ok or type(model) ~= "table" or type(model.parameters) ~= "table" then
    return nil, "cannot load Device.map: " .. tostring(model)
  end
  trace("INIT", "loaded map=" .. tostring(map_path))

  local engine = {}
  local mock_path = os.getenv("TRANSFORMER_MOCK_DB")
  local mock_values = load_mock(mock_path)

  local function raw_get(key)
    local value
    if mock_path then
      value = mock_values[key]
    else
      value = capture(uci_bin .. " -q get " .. shell_quote(key))
    end
    trace("UCI", "get key=" .. tostring(key) .. " value=" ..
      display_uci_value(key, value))
    return value
  end

  local function shell_safe_name(value)
    return type(value) == "string" and value:match("^[%w%._:%-]+$") ~= nil
  end

  local function usable_ipv4(value)
    value = tostring(value or ""):match("^%s*(.-)%s*$")
    local a, b, c, d = value:match("^(%d+)%.(%d+)%.(%d+)%.(%d+)$")
    a, b, c, d = tonumber(a), tonumber(b), tonumber(c), tonumber(d)
    if not a or not b or not c or not d then return nil end
    if a > 255 or b > 255 or c > 255 or d > 255 then return nil end
    -- Never advertise loopback/unspecified/multicast as ConnectionRequestURL.
    -- If ACS receives 127.0.0.1 here it will call itself, usually producing HTTP 405.
    if a == 0 or a == 127 or a >= 224 then return nil end
    return string.format("%d.%d.%d.%d", a, b, c, d)
  end

  local function pattern_escape(value)
    return (tostring(value):gsub("([%^%$%(%)%%%.%[%]%*%+%-%?])", "%%%1"))
  end

  local function json_string_field(json, field)
    if not json then return nil end
    return json:match('"' .. pattern_escape(field) .. '"%s*:%s*"([^"]*)"')
  end

  local function ip_from_ubus_interface(interface)
    if not shell_safe_name(interface) then return nil end
    trace("UBUS", "get network interface status interface=" .. interface)
    local status = capture(ubus_bin .. " call " .. shell_quote("network.interface." .. interface) ..
      " status")
    if not status or status == "" then return nil end

    local ipv4_block = status:match('"' .. pattern_escape("ipv4-address") .. '"%s*:%s*(%b[])')
    local ip = usable_ipv4(ipv4_block and ipv4_block:match('"address"%s*:%s*"([^"]+)"'))
    if ip then return ip end

    local device = json_string_field(status, "l3_device") or json_string_field(status, "device")
    if device and shell_safe_name(device) then
      ip = usable_ipv4(capture("ip -4 -o addr show dev " .. shell_quote(device) ..
        " scope global | awk '{split($4,a,\"/\"); print a[1]; exit}'"))
      if ip then return ip end
    end
    return nil
  end

  local function acs_url_host()
    local url = raw_get("tr69.mgmt_srv.url") or ""
    local host = url:match("^%a[%w%+%.%-]*://([^/:]+)")
    if not host then return nil end
    host = host:gsub("^%[", ""):gsub("%]$", "")
    if not host:match("^[%w%.%-]+$") then return nil end
    return host
  end

  local function ip_from_route_to_acs()
    local host = acs_url_host()
    if not host then return nil end
    return usable_ipv4(capture("ip -4 route get " .. shell_quote(host) ..
      " | awk '{for(i=1;i<=NF;i++) if($i==\"src\") {print $(i+1); exit}}'"))
  end

  local function configured_connection_request_url()
    local url = raw_get("tr69.conn_request.url")
    if not url or url == "" then return nil end
    if not url:match("^https?://") then return nil end
    local host, port = url:match("^https?://([^/:]+):?(%d*)")
    if not host or host == "localhost" then return nil end
    if host:match("^%d") and not usable_ipv4(host) then return nil end
    local acs = raw_get("tr69.mgmt_srv.url") or ""
    local acs_host, acs_port = acs:match("^%a[%w%+%.%-]*://([^/:]+):?(%d*)")
    if acs_host == host and (port == "" or acs_port == "" or port == acs_port) then
      trace("CONNECTION_REQUEST", "ignore configured URL because it points at ACS url=" .. url)
      return nil
    end
    if host == acs_host and (port == "3000" or port == "7547") then
      trace("CONNECTION_REQUEST", "ignore configured URL because it uses ACS host/port url=" .. url)
      return nil
    end
    return url
  end

  local getters = {}
  getters.serial_number = function(spec)
    local value = capture("[ -r /proc/device-tree/serial-number ] && tr -d '\\000' < /proc/device-tree/serial-number")
    return value and value ~= "" and value or spec.default
  end
  getters.software_version = function(spec)
    local value = capture(". /etc/openwrt_release 2>/dev/null; printf %s \"$DISTRIB_DESCRIPTION\"")
    return value and value ~= "" and value or spec.default
  end
  getters.connection_request_url = function()
    local configured = configured_connection_request_url()
    if configured then return configured end
    local interface = raw_get("tr69.settings.interface") or "wan"
    if not interface:match("^[%w_%-]+$") then interface = "wan" end
    local ip = ip_from_ubus_interface(interface)
      or ip_from_route_to_acs()
      or usable_ipv4(raw_get("network." .. interface .. ".ipaddr"))
      or usable_ipv4(raw_get("network.lan.ipaddr"))
    if not ip then
      trace("CONNECTION_REQUEST", "no usable local IPv4 for ConnectionRequestURL")
      return ""
    end
    local port = raw_get("tr69.settings.port") or "7547"
    if not tostring(port):match("^%d+$") then port = "7547" end
    return "http://" .. ip .. ":" .. port .. "/connection-request"
  end

  local function read(path, reveal_secrets)
    local spec = model.parameters[path]
    if not spec then return nil end
    if spec.secret and not reveal_secrets then return "" end
    if spec.getter and getters[spec.getter] then
      return getters[spec.getter](spec) or spec.default or ""
    end
    if not spec.uci then return spec.default or "" end
    local value = raw_get(uci_key(spec))
    if value == nil or value == "" then return spec.default or "" end
    return value
  end

  function engine.getParameterValues(paths, reveal_secrets)
    local values, invalid = {}, {}
    trace("GET", "request count=" .. tostring(#(paths or {})) ..
      " reveal_secrets=" .. tostring(reveal_secrets and true or false))
    for _, path in ipairs(paths or {}) do
      local spec = model.parameters[path]
      if not spec then
        trace("GET", "invalid parameter=" .. tostring(path))
        invalid[#invalid + 1] = path
      else
        trace("GET", "read parameter=" .. path .. " uci=" .. parameter_uci_key(spec))
        local value = tostring(read(path, reveal_secrets) or "")
        trace("GET", "result parameter=" .. path .. " value=" ..
          display_value(spec, value) .. " type=xsd:" .. (spec.type or "string"))
        values[#values + 1] = {
          name = path,
          value = value,
          type = "xsd:" .. (spec.type or "string")
        }
      end
    end
    return values, invalid
  end

  function engine.getParameterNames(path)
    path = path or "Device."
    trace("NAMES", "request path=" .. tostring(path))
    local names = {}
    for name, spec in pairs(model.parameters) do
      if name == path or name:sub(1, #path) == path then
        names[#names + 1] = { name = name, writable = spec.access == "readWrite" }
      end
    end
    table.sort(names, function(a, b) return a.name < b.name end)
    if #names == 0 then
      trace("NAMES", "result path=" .. tostring(path) .. " error=unknown parameter or object")
      return nil, "unknown parameter or object: " .. path
    end
    trace("NAMES", "result path=" .. tostring(path) .. " count=" .. tostring(#names))
    return names, nil
  end

  function engine.setParameterValues(values, parameter_key)
    local staged = {}
    trace("SET", "transaction start count=" .. tostring(#(values or {})) ..
      " parameter_key=" .. (parameter_key == "" and "<empty>" or tostring(parameter_key)))
    for _, item in ipairs(values or {}) do
      local spec = model.parameters[item.name]
      if not spec then
        trace("SET", "reject parameter=" .. tostring(item.name) .. " error=unknown parameter")
        return false, { path = item.name, message = "unknown parameter" }
      end
      if spec.access ~= "readWrite" or not spec.uci then
        trace("SET", "reject parameter=" .. tostring(item.name) .. " uci=" ..
          parameter_uci_key(spec) .. " error=read-only")
        return false, { path = item.name, message = "parameter is read-only" }
      end
      local type_ok, type_error = validate_declared_type(spec, item.type)
      if not type_ok then
        trace("SET", "reject parameter=" .. tostring(item.name) .. " uci=" ..
          parameter_uci_key(spec) .. " error=" .. tostring(type_error))
        return false, { path = item.name, message = type_error }
      end
      local value, err = normalize(spec, item.value)
      if not value then
        trace("SET", "reject parameter=" .. tostring(item.name) .. " uci=" ..
          parameter_uci_key(spec) .. " value=" .. display_value(spec, item.value) ..
          " error=" .. tostring(err))
        return false, { path = item.name, message = err }
      end
      trace("SET", "stage parameter=" .. item.name .. " uci=" .. uci_key(spec) ..
        " value=" .. display_value(spec, value) .. " type=" .. tostring(item.type or ""))
      staged[#staged + 1] = { path = item.name, spec = spec, value = value }
    end

    local parameter_key_spec = model.parameters["Device.ManagementServer.ParameterKey"]
    if not parameter_key_spec or not parameter_key_spec.uci then
      trace("SET", "reject parameter=Device.ManagementServer.ParameterKey error=mapping missing")
      return false, { path = "Device.ManagementServer.ParameterKey", message = "mapping missing" }
    end
    local normalized_key, key_error = normalize(parameter_key_spec, parameter_key or "")
    if not normalized_key then
      trace("SET", "reject parameter=Device.ManagementServer.ParameterKey error=" ..
        tostring(key_error))
      return false, { path = "Device.ManagementServer.ParameterKey", message = key_error }
    end

    if mock_path then
      local candidate = clone(mock_values)
      for _, item in ipairs(staged) do candidate[uci_key(item.spec)] = item.value end
      candidate[uci_key(parameter_key_spec)] = normalized_key
      local saved, err = save_mock(mock_path, candidate)
      if not saved then
        trace("SET", "mock transaction failed error=" .. tostring(err))
        return false, { path = "", message = err }
      end
      mock_values = candidate
      trace("SET", "mock transaction committed count=" .. tostring(#staged))
      return true, nil
    end

    local originals = {}
    for _, item in ipairs(staged) do
      local key = uci_key(item.spec)
      if originals[key] == nil then originals[key] = { value = raw_get(key) } end
      trace("UCI", "original key=" .. key .. " value=" ..
        (originals[key].value == nil and "<missing>" or display_value(item.spec, originals[key].value)))
    end
    local parameter_key_name = uci_key(parameter_key_spec)
    if originals[parameter_key_name] == nil then
      originals[parameter_key_name] = { value = raw_get(parameter_key_name) }
    end
    trace("UCI", "parameter_key uci=" .. parameter_key_name .. " value=" ..
      display_value(parameter_key_spec, normalized_key))

    local function restore_originals()
      trace("UCI", "rollback start count=" .. tostring(#staged + 1))
      for key, original in pairs(originals) do
        if original.value == nil then
          trace("UCI", "rollback delete key=" .. key)
          execute_ok(uci_bin .. " -q delete " .. shell_quote(key))
        else
          trace("UCI", "rollback restore key=" .. key)
          execute_ok(uci_bin .. " -q set " .. shell_quote(key .. "=" .. original.value))
        end
      end
      local ok = execute_ok(uci_bin .. " -q commit tr69")
      trace("UCI", "rollback commit package=tr69 status=" .. (ok and "success" or "failed"))
    end

    -- Do not revert the whole package here. The first CPE configuration can leave
    -- the ACS URL staged but not committed; package-wide revert would erase it and
    -- make the daemon idle after the ACS' first SetParameterValues.
    for _, item in ipairs(staged) do
      local assignment = uci_key(item.spec) .. "=" .. item.value
      trace("UCI", "set key=" .. uci_key(item.spec) .. " parameter=" .. item.path ..
        " value=" .. display_value(item.spec, item.value))
      if not execute_ok(uci_bin .. " -q set " .. shell_quote(assignment)) then
        trace("UCI", "set key=" .. uci_key(item.spec) .. " status=failed")
        restore_originals()
        return false, { path = item.path, message = "UCI set failed" }
      end
      trace("UCI", "set key=" .. uci_key(item.spec) .. " status=success")
    end
    local key_assignment = uci_key(parameter_key_spec) .. "=" .. normalized_key
    trace("UCI", "set key=" .. parameter_key_name ..
      " parameter=Device.ManagementServer.ParameterKey value=" ..
      display_value(parameter_key_spec, normalized_key))
    if not execute_ok(uci_bin .. " -q set " .. shell_quote(key_assignment)) then
      trace("UCI", "set key=" .. parameter_key_name .. " status=failed")
      restore_originals()
      return false, { path = "Device.ManagementServer.ParameterKey", message = "UCI set failed" }
    end
    trace("UCI", "set key=" .. parameter_key_name .. " status=success")
    trace("UCI", "commit package=tr69")
    if not execute_ok(uci_bin .. " -q commit tr69") then
      trace("UCI", "commit package=tr69 status=failed")
      restore_originals()
      return false, { path = "", message = "UCI commit failed" }
    end
    trace("UCI", "commit package=tr69 status=success")
    for _, item in ipairs(staged) do
      local applied = raw_get(uci_key(item.spec))
      trace("UCI", "readback key=" .. uci_key(item.spec) .. " expected=" ..
        display_value(item.spec, item.value) .. " actual=" ..
        display_value(item.spec, applied))
      if applied ~= item.value then
        restore_originals()
        return false, { path = item.path, message = "UCI read-back verification failed" }
      end
    end
    local applied_key = raw_get(parameter_key_name)
    trace("UCI", "readback key=" .. parameter_key_name .. " expected=" ..
      display_value(parameter_key_spec, normalized_key) .. " actual=" ..
      display_value(parameter_key_spec, applied_key))
    if applied_key ~= normalized_key then
      restore_originals()
      return false, {
        path = "Device.ManagementServer.ParameterKey",
        message = "UCI read-back verification failed"
      }
    end
    -- UCI is already durable at this point. Notify the long-running daemon; never
    -- restart it and never roll back a valid commit solely because IPC is unavailable.
    notify_runtime_reload()
    trace("SET", "transaction committed count=" .. tostring(#staged))
    return true, nil
  end

  return engine, nil
end

return M
