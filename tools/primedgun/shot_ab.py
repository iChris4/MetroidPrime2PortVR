"""Pair the screenshots of two runs (same order), write PNGs and a side-by-side, print diff stats.

    python -I shot_ab.py <dirA> <dirB> <outdir>
"""
import os
import sys

import numpy as np
from PIL import Image

a_dir, b_dir, out = sys.argv[1], sys.argv[2], sys.argv[3]
os.makedirs(out, exist_ok=True)
a_files = sorted(f for f in os.listdir(a_dir) if f.lower().endswith((".bmp", ".png")))
b_files = sorted(f for f in os.listdir(b_dir) if f.lower().endswith((".bmp", ".png")))
for i, (fa, fb) in enumerate(zip(a_files, b_files)):
    ia = Image.open(os.path.join(a_dir, fa)).convert("RGB")
    ib = Image.open(os.path.join(b_dir, fb)).convert("RGB")
    if ia.size != ib.size:
        print(f"shot {i}: sizes differ {ia.size} vs {ib.size}")
        ib = ib.resize(ia.size)
    na, nb = np.asarray(ia, dtype=np.int16), np.asarray(ib, dtype=np.int16)
    d = np.abs(na - nb).max(axis=2)
    print(f"shot {i}: {ia.size[0]}x{ia.size[1]} mean abs diff {d.mean():.2f}, "
          f"pixels >16: {(d > 16).mean() * 100:.2f}%, >64: {(d > 64).mean() * 100:.2f}%")
    w, h = ia.size
    side = Image.new("RGB", (w * 2, h))
    side.paste(ia, (0, 0))
    side.paste(ib, (w, 0))
    side.thumbnail((1600, 800))
    side.save(os.path.join(out, f"ab_{i}.png"))
