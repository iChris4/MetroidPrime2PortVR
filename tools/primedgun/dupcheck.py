import sys, subprocess, collections, os
S, report = sys.argv[1], sys.argv[2]
M = os.path.join(S, "merged", "extern", "aurora")
E = r"C:\Users\Christophe\Documents\PrimedGun-v2.0\MetroidPrime2PortVR"
V = r"C:\Users\Christophe\Documents\PrimedGun-v2.0\MetroidPrimePortVR"
def show(repo, rev, path):
    r = subprocess.run(["git", "-C", repo, "show", f"{rev}:extern/aurora/{path}"], capture_output=True)
    return r.stdout.decode("utf-8", "replace").split("\n") if r.returncode == 0 else []
files = [l.split()[1] for l in open(report, encoding="utf-8") if l.startswith(("merged-clean", "CONFLICT"))]
for f in files:
    merged = open(os.path.join(M, f), "rb").read().decode("utf-8", "replace").split("\n")
    e, v = show(E, "HEAD", f), show(V, "OpenXR", f)
    cm, ce, cv = (collections.Counter(x.strip() for x in L) for L in (merged, e, v))
    bad = [(ln, cm[ln], ce[ln], cv[ln]) for ln in cm
           if len(ln) > 30 and not ln.startswith("//") and cm[ln] > max(ce[ln], cv[ln])]
    if bad:
        print(f"## {f}: {len(bad)} lines more frequent than in either side")
        for ln, a, b, c in bad[:6]:
            print(f"   merged={a} echoes={b} vr={c}: {ln[:110]}")
