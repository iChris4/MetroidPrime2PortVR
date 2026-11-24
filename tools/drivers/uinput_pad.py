#!/usr/bin/env python3
"""Xbox 360-style uinput pad. Watches a game log; on a line "[at N]" for N in the plan, holds
the plan's buttons/axes for HOLD seconds. Usage: uinput_pad.py <log> <plan> ; plan = "N:a+lt+rt;M:..."
names: a b x y lb rb back start ls rs lt rt"""
import fcntl, os, struct, sys, time, re

UI_SET_EVBIT, UI_SET_KEYBIT, UI_SET_ABSBIT = 0x40045564, 0x40045565, 0x40045567
UI_DEV_CREATE, UI_DEV_DESTROY = 0x5501, 0x5502
EV_SYN, EV_KEY, EV_ABS = 0, 1, 3
KEYS = dict(a=0x130, b=0x131, x=0x133, y=0x134, lb=0x136, rb=0x137, back=0x13a, start=0x13b,
            guide=0x13c, ls=0x13d, rs=0x13e)
ABS = dict(x=0, y=1, lt=2, rx=3, ry=4, rt=5, hx=0x10, hy=0x11)
RANGE = {0: (-32768, 32767), 1: (-32768, 32767), 3: (-32768, 32767), 4: (-32768, 32767),
         2: (0, 255), 5: (0, 255), 0x10: (-1, 1), 0x11: (-1, 1)}

fd = os.open("/dev/uinput", os.O_WRONLY | os.O_NONBLOCK)
fcntl.ioctl(fd, UI_SET_EVBIT, EV_KEY)
fcntl.ioctl(fd, UI_SET_EVBIT, EV_ABS)
for k in KEYS.values():
    fcntl.ioctl(fd, UI_SET_KEYBIT, k)
for a in RANGE:
    fcntl.ioctl(fd, UI_SET_ABSBIT, a)
absmax = [0] * 64; absmin = [0] * 64
for a, (lo, hi) in RANGE.items():
    absmin[a], absmax[a] = lo, hi
# struct uinput_user_dev: name[80], input_id (4 x u16), ff_effects_max u32, absmax/min/fuzz/flat[64] s32
dev = struct.pack("80sHHHHi", b"Microsoft X-Box 360 pad", 3, 0x045E, 0x028E, 0x114, 0)
dev += struct.pack("64i", *absmax) + struct.pack("64i", *absmin) + bytes(64 * 4 * 2)
os.write(fd, dev)
fcntl.ioctl(fd, UI_DEV_CREATE)


def emit(t, c, v):
    os.write(fd, struct.pack("llHHi", 0, 0, t, c, v))


def syn():
    emit(EV_SYN, 0, 0)


def press(names, down):
    for n in names:
        if n in KEYS:
            emit(EV_KEY, KEYS[n], 1 if down else 0)
        elif n in ("lt", "rt"):
            emit(EV_ABS, ABS[n], 255 if down else 0)
    syn()


log, plan = sys.argv[1], sys.argv[2]
hold = float(os.environ.get("HOLD", "0.6"))
todo = {}
for item in plan.split(";"):
    n, names = item.split(":")
    todo[int(n)] = names.split("+")
print("[uinput_pad] created", flush=True)
deadline = time.time() + float(os.environ.get("SECS", "300"))
pos = 0
try:
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
                    press(names, True)
                    print(f"[uinput_pad] at {n}: hold {names}", flush=True)
                    time.sleep(hold)
                    press(names, False)
        time.sleep(0.02)
    time.sleep(float(os.environ.get("LINGER", "5")))
finally:
    fcntl.ioctl(fd, UI_DEV_DESTROY)
    os.close(fd)
