#!/usr/bin/env python3
"""Show or resolve diff3 conflict hunks.

  cfl.py show <file> [hunk#] [--ctx N]   print hunks (echoes | base | vr) with line numbers
  cfl.py take <file> <hunk#> echoes|vr|base|both-ev|both-ve   resolve one hunk with a side
  cfl.py count <file>
"""
import re
import sys


def load(path):
    with open(path, "rb") as f:
        return f.read().decode("utf-8").split("\n")


def hunks(lines):
    out = []
    i = 0
    while i < len(lines):
        if lines[i].startswith("<<<<<<< "):
            start = i
            j = i + 1
            while not lines[j].startswith("||||||| "):
                j += 1
            k = j + 1
            while not lines[k].startswith("======="):
                k += 1
            m = k + 1
            while not lines[m].startswith(">>>>>>> "):
                m += 1
            out.append((start, j, k, m))
            i = m + 1
        else:
            i += 1
    return out


def main():
    cmd, path = sys.argv[1], sys.argv[2]
    lines = load(path)
    hs = hunks(lines)
    if cmd == "count":
        print(len(hs))
        return
    if cmd == "show":
        ctx = 3
        args = sys.argv[3:]
        if "--ctx" in args:
            i = args.index("--ctx")
            ctx = int(args[i + 1])
            del args[i:i + 2]
        sel = int(args[0]) if args else None
        for n, (s, j, k, m) in enumerate(hs):
            if sel is not None and n != sel:
                continue
            print(f"===== hunk {n}: lines {s + 1}-{m + 1} =====")
            for x in range(max(0, s - ctx), s):
                print(f"{x + 1:6d}  {lines[x]}")
            for x in range(s, m + 1):
                print(f"{x + 1:6d}| {lines[x]}")
            for x in range(m + 1, min(len(lines), m + 1 + ctx)):
                print(f"{x + 1:6d}  {lines[x]}")
        return
    if cmd == "take":
        n, side = int(sys.argv[3]), sys.argv[4]
        s, j, k, m = hs[n]
        e, b, v = lines[s + 1:j], lines[j + 1:k], lines[k + 1:m]
        rep = {"echoes": e, "vr": v, "base": b, "both-ev": e + v, "both-ve": v + e}[side]
        lines[s:m + 1] = rep
        with open(path, "wb") as f:
            f.write("\n".join(lines).encode("utf-8"))
        print(f"hunk {n} -> {side}; {len(hunks(lines))} left")
        return


if __name__ == "__main__":
    main()
