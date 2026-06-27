#!/usr/bin/lua

local script = os.getenv("TR69_TRANSFORMER_SCRIPT") or "/usr/lib/tr69d/transformer.lua"
local map = os.getenv("TR69_DEVICE_MAP") or "/etc/transformer/maps/Device.map"
local module = assert(dofile(script))
local engine, error_message = module.new(map)
if not engine then io.stderr:write(error_message, "\n") os.exit(1) end

local command, path, value, parameter_key = arg[1], arg[2], arg[3], arg[4]
if command == "get" then
  if not path then io.stderr:write("Usage: trg <parameter-or-object>\n") os.exit(2) end
  local names, name_error = engine.getParameterNames(path)
  if not names then io.stderr:write(name_error, "\n") os.exit(1) end
  local paths = {}
  for _, item in ipairs(names) do paths[#paths + 1] = item.name end
  local values, invalid = engine.getParameterValues(paths, false)
  if #invalid > 0 then io.stderr:write("Invalid parameter\n") os.exit(1) end
  if #values == 1 and values[1].name == path then
    print(values[1].value)
  else
    for _, item in ipairs(values) do print(item.name .. "=" .. item.value) end
  end
elseif command == "set" then
  if not path or value == nil then
    io.stderr:write("Usage: trs <parameter> <value> [parameter-key]\n") os.exit(2)
  end
  local ok, err = engine.setParameterValues({{ name = path, value = value }}, parameter_key or "")
  if not ok then io.stderr:write((err.path or ""), ": ", err.message or "set failed", "\n") os.exit(1) end
  print(path .. "=OK")
else
  io.stderr:write("Usage: transformer-cli.lua <get|set> ...\n")
  os.exit(2)
end
