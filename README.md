# PROBE — MMUKO OS Network Probing Protocol

Part of the **OBINexus / MMUKO Operating System** project.

> *"You are the controller. The system is being controlled."*

PROBE is a three-state consensual network questioning system. Instead of binary ping/no-ping, PROBE asks **who**, **what**, **when**, **where**, **why**, and **how** — and returns **YES**, **NO**, or **MAYBE**.

MAYBE means the answer isn't resolved yet. The system can probe deeper, retry, or pass it forward. This mirrors the PROBE protocol philosophy: questions have three states, not two.

---

## Files

```
probe/
  probe.c          ← core C implementation
  probe.h          ← types, enums, macros
  main.c           ← CLI entry point
  probe.py         ← Python bindings (ctypes)
  probe.lua        ← Lua 5.1 bindings (calls probe.exe)
```

---

## Build

### Windows (MinGW)
```powershell
# Executable
gcc -o probe.exe probe.c main.c -I. -lws2_32

# Shared library (for Python - requires 64-bit MinGW)
gcc -shared -o probe.dll probe.c -I. -lws2_32
```

### Linux / WSL
```bash
# Executable
gcc -o probe probe.c main.c -Iinclude

# Shared library (for Python / Lua FFI)
gcc -shared -fPIC -o probe.so probe.c -Iinclude

# Static library
gcc -c probe.c -Iinclude -o probe.o && ar rcs probe.a probe.o
```

### Cross-compile Windows from WSL
```bash
sudo apt install gcc-mingw-w64-x86-64
x86_64-w64-mingw32-gcc -o probe.exe probe.c main.c -I. -lws2_32
x86_64-w64-mingw32-gcc -shared -o probe.dll probe.c -I. -lws2_32
```

---

## Usage

### probe.exe / probe (CLI)

```powershell
# Probe a single host — all questions
.\probe.exe 8.8.8.8

# Ask a specific question
.\probe.exe 8.8.8.8 WHO
.\probe.exe 8.8.8.8 WHAT
.\probe.exe 8.8.8.8 WHEN
.\probe.exe 8.8.8.8 WHERE

# Scan a network range
.\probe.exe network 192.168.1.0/24
```

### Questions

| Question | What it does |
|----------|-------------|
| `WHO`    | Reverse DNS — who is this host? |
| `WHAT`   | Port scan — what is it running? |
| `WHEN`   | Last seen / reachability timestamp |
| `WHERE`  | Is it reachable on the network? |
| `WHY`    | Reserved — requires deeper probe |
| `HOW`    | Reserved — requires deeper probe |

### States

| State   | Meaning |
|---------|---------|
| `YES`   | Confirmed |
| `NO`    | Not found / unreachable |
| `MAYBE` | Uncertain — retry or probe deeper |

### Example output

```
Probing: 8.8.8.8

  [YES] WHO: 8.8.8.8 -> dns.google
  [YES] WHAT: open ports [443]
  [NO]  WHEN: 8.8.8.8 last_seen=1773099653 state=NO
  [YES] WHERE: 8.8.8.8 reachable=YES
```

---

## Python

Requires `probe.dll` (Windows) or `probe.so` (Linux) in the same folder.

```python
from probe import Probe

p = Probe()

# Ask a question
print(p.WHO("8.8.8.8"))     # [YES] WHO: 8.8.8.8 -> dns.google
print(p.WHERE("1.1.1.1"))   # [YES] WHERE: 1.1.1.1 reachable=YES
print(p.WHAT("1.1.1.1"))    # [YES] WHAT: open ports [80 443 8080]

# Check state
result = p.WHO("8.8.8.8")
if result.yes:
    print("Found:", result.detail)

# Resolve a MAYBE
result = p.WHERE("192.168.1.1")
if result.maybe:
    p.resolve(result, retries=3)

# Scan a network
nodes = p.network("192.168.1.0/24")
for n in nodes:
    print(n)
```

```powershell
# CLI
python probe.py 8.8.8.8
python probe.py 8.8.8.8 WHAT
python probe.py network 192.168.1.0/24
```

---

## Lua

Uses `probe.exe` (Windows) or `./probe` (Linux) via `io.popen`. No FFI required — works with plain Lua 5.1.

```lua
local Probe = require("probe")
local p = Probe.new()

print(p:WHO("8.8.8.8"))
print(p:WHERE("1.1.1.1"))

local nodes = p:network("192.168.1.0/24")
for _, n in ipairs(nodes) do print(n) end
```

```powershell
# CLI
lua probe.lua 8.8.8.8 WHO
lua probe.lua 8.8.8.8 WHERE
lua probe.lua network 192.168.1.0/24
```

> Note: `WHERE` and `WHAT` use TCP connect with timeout. On slow networks, Lua may interrupt. Use `probe.exe` directly for those questions if Lua times out.

---

## C Macro Interface

```c
#include "probe.h"

probe_init();

// ? operator style
ProbeResult r = PROBE(WHO, "8.8.8.8");

if (PROBE_IS(r))     printf("YES: %s\n", r.detail);
if (PROBE_NOT(r))    printf("NO\n");
if (PROBE_UNSURE(r)) printf("MAYBE — probing deeper\n");

// Resolve MAYBE
probe_resolve(&r, 3);

probe_destroy();
```

---

## Python DLL Architecture Note

Your Python must match your `probe.dll` architecture (both 32-bit or both 64-bit).

```powershell
# Check Python arch
python -c "import struct; print(struct.calcsize('P')*8, 'bit')"

# If 64-bit Python, build 64-bit dll via WSL:
x86_64-w64-mingw32-gcc -shared -o probe.dll probe.c -I. -lws2_32
```

---

## Context

PROBE is the questioning layer of **MMUKO OS** — the digital operating system of the OBINexus project.

```
MMUKO OS
  └── NSIGII   — connection layer (physical ↔ digital)
  └── PROBE    — consensual question system (this)
  └── GOAI     — gossip networking language (underneath)
```

PROBE does one thing: it asks questions and gets back YES, NO, or MAYBE.

---

*OBINexus — github.com/obinexus*
