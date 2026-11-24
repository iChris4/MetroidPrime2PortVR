#!/usr/bin/env python3
"""XTEST keys on an X display (the run's Xvfb). Watches a game log; on "[at N]" for N in the plan,
holds the plan's keysyms for HOLD seconds. Usage: xtest_keys.py <display> <log> "N:q+e+z;M:..." """
import ctypes, os, re, sys, time

x11 = ctypes.CDLL("libX11.so.6")
xt = ctypes.CDLL("libXtst.so.6")
x11.XOpenDisplay.restype = ctypes.c_void_p
x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
x11.XStringToKeysym.restype = ctypes.c_ulong
x11.XStringToKeysym.argtypes = [ctypes.c_char_p]
x11.XKeysymToKeycode.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
x11.XFlush.argtypes = [ctypes.c_void_p]
xt.XTestFakeKeyEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong]
xt.XTestFakeMotionEvent.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_ulong]

disp, log, plan = sys.argv[1], sys.argv[2], sys.argv[3]
d = None
while d is None or d == 0:
    d = x11.XOpenDisplay(disp.encode())
    if not d:
        time.sleep(0.5)
hold = float(os.environ.get("HOLD", "0.6"))
todo = {int(n): names.split("+") for n, names in (i.split(":") for i in plan.split(";"))}


def keys(names, down):
    xt.XTestFakeMotionEvent(d, -1, 640, 360, 0)  # focus follows the pointer without a WM
    for n in names:
        code = x11.XKeysymToKeycode(d, x11.XStringToKeysym(n.encode()))
        xt.XTestFakeKeyEvent(d, code, 1 if down else 0, 0)
    x11.XFlush(d)


pos = 0
deadline = time.time() + float(os.environ.get("SECS", "300"))
while todo and time.time() < deadline:
    if os.path.exists(log):
        with open(log, errors="replace") as f:
            f.seek(pos)
            data = f.read()
            pos = f.tell()
        for m in re.finditer(r"\[at (\d+)\]", data):
            n = int(m.group(1))
            if n in todo:
                names = todo.pop(n)
                keys(names, True)
                print(f"[xtest_keys] at {n}: hold {names}", flush=True)
                time.sleep(hold)
                keys(names, False)
    time.sleep(0.02)
