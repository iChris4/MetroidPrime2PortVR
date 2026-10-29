#!/usr/bin/env python3
"""Three-way merge of extern/aurora: base = MP1 upstream 174975c2c, echoes = MetroidPrime2PortVR HEAD,
vr = MetroidPrimePortVR OpenXR. Writes the merged tree (LF, diff3 markers in conflicted files) to
<out>/extern/aurora and a report to <out>/report.txt.

    python -I aurora_merge.py <out-dir> [--echoes-rev HEAD] [--vr-rev OpenXR] [--base 174975c2c]
"""
import argparse
import os
import subprocess
import sys
import tempfile

ROOT = r"C:\Users\Christophe\Documents\PrimedGun-v2.0"
ECHOES = os.path.join(ROOT, "MetroidPrime2PortVR")
VR = os.path.join(ROOT, "MetroidPrimePortVR")
PREFIX = "extern/aurora/"


def tree(repo, rev):
    out = subprocess.run(["git", "-C", repo, "ls-tree", "-r", "-z", rev, PREFIX],
                         check=True, capture_output=True).stdout
    entries = {}
    for rec in out.split(b"\0"):
        if not rec:
            continue
        meta, path = rec.split(b"\t", 1)
        mode, kind, blob = meta.decode().split()
        entries[path.decode("utf-8")[len(PREFIX):]] = (mode, blob)
    return entries


def blob(repo, sha):
    return subprocess.run(["git", "-C", repo, "cat-file", "blob", sha], check=True,
                          capture_output=True).stdout


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--echoes-rev", default="HEAD")
    ap.add_argument("--vr-rev", default="OpenXR")
    ap.add_argument("--base", default="174975c2c")
    a = ap.parse_args()

    base = tree(VR, a.base)
    ech = tree(ECHOES, a.echoes_rev)
    vr = tree(VR, a.vr_rev)
    outroot = os.path.join(a.out, "extern", "aurora")
    report = []
    counts = {}

    def note(kind, path, extra=""):
        counts[kind] = counts.get(kind, 0) + 1
        report.append(f"{kind:14s} {path}{(' ' + extra) if extra else ''}")

    def write(path, data):
        dst = os.path.join(outroot, path)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        with open(dst, "wb") as f:
            f.write(data)

    for path in sorted(set(base) | set(ech) | set(vr)):
        b, e, v = base.get(path), ech.get(path), vr.get(path)
        bb = b[1] if b else None
        eb = e[1] if e else None
        vb = v[1] if v else None
        if eb == vb:
            if eb is None:
                note("both-deleted", path)
            else:
                write(path, blob(ECHOES, eb))
                note("same", path)
            continue
        if eb == bb:  # only vr changed (incl. add / delete)
            if vb is None:
                note("vr-deleted", path)
            else:
                write(path, blob(VR, vb))
                note("take-vr" if bb else "vr-added", path)
            continue
        if vb == bb:  # only echoes changed
            if eb is None:
                note("echoes-deleted", path)
            else:
                write(path, blob(ECHOES, eb))
                note("take-echoes" if bb else "echoes-added", path)
            continue
        # both changed, differently
        if eb is None or vb is None:
            keep = blob(ECHOES, eb) if eb else blob(VR, vb)
            write(path, keep)
            note("DELETE-VS-EDIT", path, "kept the edited side")
            continue
        with tempfile.TemporaryDirectory() as td:
            pe, pb, pv = (os.path.join(td, n) for n in ("echoes", "base", "vr"))
            open(pe, "wb").write(blob(ECHOES, eb))
            open(pb, "wb").write(blob(VR, bb) if bb else b"")
            open(pv, "wb").write(blob(VR, vb))
            r = subprocess.run(["git", "merge-file", "-p", "--diff3", "-L", "echoes", "-L", "base",
                                "-L", "vr", pe, pb, pv], capture_output=True)
            write(path, r.stdout)
            if r.returncode == 0:
                note("merged-clean", path)
            elif r.returncode > 0:
                note("CONFLICT", path, f"{r.returncode} hunks" + ("" if bb else " (added on both sides)"))
            else:
                note("MERGE-ERROR", path, r.stderr.decode(errors="replace").strip())

    os.makedirs(a.out, exist_ok=True)
    with open(os.path.join(a.out, "report.txt"), "w", encoding="utf-8") as f:
        f.write(f"base={a.base} echoes={a.echoes_rev} vr={a.vr_rev}\n")
        f.write("\n".join(report) + "\n")
    for k in sorted(counts):
        print(f"{k:14s} {counts[k]}")
    print("\n".join(l for l in report if l.startswith(("CONFLICT", "DELETE", "MERGE-ERROR"))))


if __name__ == "__main__":
    sys.exit(main())
