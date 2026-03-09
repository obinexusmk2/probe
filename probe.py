"""
probe.py - MMUKO OS PROBE Protocol
Python bindings via ctypes - Windows + Linux
"""

import ctypes
import os
import sys
import platform
from enum import IntEnum
from dataclasses import dataclass
from typing import List

# ── Load shared library ──────────────────────────────────────────────────────

def _load_lib():
    is_windows = platform.system() == "Windows"
    base = os.path.dirname(os.path.abspath(__file__))

    if is_windows:
        names = ["probe.dll"]
    else:
        names = ["probe.so", "libprobe.so"]

    for name in names:
        path = os.path.join(base, name)
        if os.path.exists(path):
            return ctypes.CDLL(path)

    raise FileNotFoundError(
        f"probe shared library not found in {base}\n"
        f"  Windows: build probe.dll with mingw in WSL\n"
        f"  Linux:   make shared"
    )

# ── Enums ────────────────────────────────────────────────────────────────────

class ProbeState(IntEnum):
    NO    = 0
    YES   = 1
    MAYBE = 2

class ProbeQuestion(IntEnum):
    WHO   = 0
    WHAT  = 1
    WHEN  = 2
    WHERE = 3
    WHY   = 4
    HOW   = 5

# ── C Structs ────────────────────────────────────────────────────────────────

class _ProbeNode(ctypes.Structure):
    _fields_ = [
        ("id",        ctypes.c_uint),
        ("address",   ctypes.c_char * 64),
        ("state",     ctypes.c_int),
        ("last_seen", ctypes.c_longlong),
        ("data",      ctypes.c_char * 256),
    ]

class _ProbeResult(ctypes.Structure):
    _fields_ = [
        ("question", ctypes.c_int),
        ("state",    ctypes.c_int),
        ("node",     _ProbeNode),
        ("detail",   ctypes.c_char * 256),
    ]

# ── Python dataclass ─────────────────────────────────────────────────────────

@dataclass
class ProbeResult:
    question:  ProbeQuestion
    state:     ProbeState
    address:   str
    detail:    str
    last_seen: int = 0

    def __str__(self):
        return f"[{self.state.name}] {self.detail}"

    @property
    def yes(self):   return self.state == ProbeState.YES
    @property
    def no(self):    return self.state == ProbeState.NO
    @property
    def maybe(self): return self.state == ProbeState.MAYBE

# ── PROBE ────────────────────────────────────────────────────────────────────

class Probe:
    """
    PROBE - MMUKO OS Network Probing Protocol

    p = Probe()
    print(p.WHO("8.8.8.8"))
    print(p.WHERE("192.168.1.1"))
    nodes = p.network("192.168.1.0/24")
    """

    def __init__(self):
        self._lib = None
        try:
            self._lib = _load_lib()
            self._setup()
            self._lib.probe_init()
        except FileNotFoundError as e:
            print(f"[PROBE] {e}")
            print("[PROBE] Running in simulation mode.\n")

    def _setup(self):
        lib = self._lib
        lib.probe_ask.restype  = _ProbeResult
        lib.probe_ask.argtypes = [ctypes.c_int, ctypes.c_char_p]
        lib.probe_network.restype  = ctypes.c_int
        lib.probe_network.argtypes = [ctypes.c_char_p, ctypes.POINTER(_ProbeResult), ctypes.c_int]
        lib.probe_resolve.restype  = ctypes.c_int
        lib.probe_resolve.argtypes = [ctypes.POINTER(_ProbeResult), ctypes.c_int]

    def _wrap(self, r):
        return ProbeResult(
            question  = ProbeQuestion(r.question),
            state     = ProbeState(r.state),
            address   = r.node.address.decode(),
            detail    = r.detail.decode(),
            last_seen = r.node.last_seen,
        )

    def ask(self, question: str, address: str) -> ProbeResult:
        q = ProbeQuestion[question.upper()]
        if self._lib:
            return self._wrap(self._lib.probe_ask(int(q), address.encode()))
        return ProbeResult(q, ProbeState.MAYBE, address, f"{question}: {address} [no library]")

    def resolve(self, result: ProbeResult, retries: int = 3) -> ProbeState:
        if result.maybe and self._lib:
            r = _ProbeResult()
            r.question = int(result.question)
            r.node.address = result.address.encode()
            return ProbeState(self._lib.probe_resolve(ctypes.byref(r), retries))
        return result.state

    def network(self, cidr: str, max_nodes: int = 254) -> List[ProbeResult]:
        if not self._lib:
            return []
        out = (_ProbeResult * max_nodes)()
        count = self._lib.probe_network(cidr.encode(), out, max_nodes)
        return [self._wrap(out[i]) for i in range(count)]

    def WHO(self,   addr): return self.ask("WHO",   addr)
    def WHAT(self,  addr): return self.ask("WHAT",  addr)
    def WHEN(self,  addr): return self.ask("WHEN",  addr)
    def WHERE(self, addr): return self.ask("WHERE", addr)
    def WHY(self,   addr): return self.ask("WHY",   addr)
    def HOW(self,   addr): return self.ask("HOW",   addr)

    def __del__(self):
        if self._lib:
            try: self._lib.probe_destroy()
            except: pass

# ── CLI ──────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    p = Probe()

    if len(sys.argv) < 2:
        print("Usage: python probe.py <address> [WHO|WHAT|WHEN|WHERE|WHY|HOW]")
        print("       python probe.py network <cidr>")
        sys.exit(0)

    if sys.argv[1] == "network":
        cidr = sys.argv[2] if len(sys.argv) > 2 else "192.168.1.0/24"
        print(f"Scanning {cidr}...\n")
        nodes = p.network(cidr)
        print(f"Found {len(nodes)} nodes:\n")
        for n in nodes:
            print(f"  {n}")
    else:
        addr     = sys.argv[1]
        question = sys.argv[2] if len(sys.argv) > 2 else None
        if question:
            r = p.ask(question, addr)
            if r.maybe: p.resolve(r)
            print(r)
        else:
            print(f"Probing {addr}...\n")
            for q in ["WHO", "WHAT", "WHEN", "WHERE"]:
                r = p.ask(q, addr)
                if r.maybe: p.resolve(r, 2)
                print(f"  {r}")
