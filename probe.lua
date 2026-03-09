--[[
probe.lua - MMUKO OS PROBE Protocol
Pure Lua 5.1 implementation (no FFI required)
Calls probe.exe via io.popen for results
--]]

-- ── Config ───────────────────────────────────────────────────────────────────

local PROBE_EXE = (package.config:sub(1,1) == "\\")
    and ".\\probe.exe"   -- Windows
    or  "./probe"        -- Linux

-- ── State / Question ─────────────────────────────────────────────────────────

local STATE    = { YES = "YES", NO = "NO", MAYBE = "MAYBE" }
local QUESTION = { "WHO", "WHAT", "WHEN", "WHERE", "WHY", "HOW" }

-- ── Run probe binary ──────────────────────────────────────────────────────────

local function run(...)
    local args = {}
    for _, v in ipairs({...}) do
        args[#args+1] = tostring(v)
    end
    local cmd = PROBE_EXE .. " " .. table.concat(args, " ") .. " 2>&1"
    local f = io.popen(cmd)
    if not f then return nil, "failed to run " .. PROBE_EXE end
    local out = f:read("*a")
    f:close()
    return out
end

-- ── Parse result line ─────────────────────────────────────────────────────────

local function parse_line(line)
    local state, detail = line:match("^%[(%u+)%]%s*(.*)")
    if state then
        return { state = state, detail = detail }
    end
    return nil
end

-- ── ProbeResult ───────────────────────────────────────────────────────────────

local Result = {}
Result.__index = Result

function Result.new(state, detail, address, question)
    return setmetatable({
        state    = state or "MAYBE",
        detail   = detail or "",
        address  = address or "",
        question = question or "",
    }, Result)
end

function Result:yes()   return self.state == "YES"   end
function Result:no()    return self.state == "NO"    end
function Result:maybe() return self.state == "MAYBE" end
function Result:__tostring()
    return string.format("[%s] %s", self.state, self.detail)
end

-- ── Probe ─────────────────────────────────────────────────────────────────────

local Probe = {}
Probe.__index = Probe

function Probe.new()
    local self = setmetatable({}, Probe)
    -- Check probe binary exists
    local f = io.open(PROBE_EXE:gsub("^%.[\\/]",""), "r")
        or io.open(PROBE_EXE, "r")
    if f then
        f:close()
        self._ok = true
    else
        print("[PROBE] Warning: " .. PROBE_EXE .. " not found.")
        print("[PROBE] Build first: make (Linux) or follow WSL instructions (Windows)\n")
        self._ok = false
    end
    return self
end

function Probe:ask(question, address)
    if not self._ok then
        return Result.new("MAYBE", question .. ": " .. address .. " [no binary]", address, question)
    end
    local out = run(address, question:upper())
    if not out then
        return Result.new("MAYBE", "run failed", address, question)
    end
    -- Parse first [STATE] line
    for line in out:gmatch("[^\n]+") do
        local r = parse_line(line)
        if r then
            return Result.new(r.state, r.detail, address, question)
        end
    end
    return Result.new("MAYBE", out:gsub("\n",""), address, question)
end

function Probe:network(cidr)
    if not self._ok then return {} end
    local out = run("network", cidr or "192.168.1.0/24")
    if not out then return {} end
    local results = {}
    for line in out:gmatch("[^\n]+") do
        local r = parse_line(line:match("^%s*(.*)") or "")
        if r then
            results[#results+1] = Result.new(r.state, r.detail, "", "WHERE")
        end
    end
    return results
end

-- ? operator style
function Probe:WHO(addr)   return self:ask("WHO",   addr) end
function Probe:WHAT(addr)  return self:ask("WHAT",  addr) end
function Probe:WHEN(addr)  return self:ask("WHEN",  addr) end
function Probe:WHERE(addr) return self:ask("WHERE", addr) end
function Probe:WHY(addr)   return self:ask("WHY",   addr) end
function Probe:HOW(addr)   return self:ask("HOW",   addr) end

-- ── CLI ───────────────────────────────────────────────────────────────────────

if arg and arg[0] then
    local p = Probe.new()

    if not arg[1] then
        print("Usage: lua probe.lua <address> [WHO|WHAT|WHEN|WHERE|WHY|HOW]")
        print("       lua probe.lua network <cidr>")
        os.exit(0)
    end

    if arg[1] == "network" then
        local cidr = arg[2] or "192.168.1.0/24"
        print("Scanning " .. cidr .. "...\n")
        local nodes = p:network(cidr)
        print("Found " .. #nodes .. " nodes:\n")
        for _, n in ipairs(nodes) do print("  " .. tostring(n)) end

    elseif arg[2] then
        local r = p:ask(arg[2], arg[1])
        print(tostring(r))
    else
        print("Probing " .. arg[1] .. "...\n")
        for _, q in ipairs({"WHO","WHAT","WHEN","WHERE"}) do
            local r = p:ask(q, arg[1])
            print("  " .. tostring(r))
        end
    end
end

return Probe
