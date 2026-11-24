#!/usr/bin/env python3
"""Dump and diff the save-relevant world data (MLVL + SAVW + HINT) of extracted Echoes discs.

Research tool for save conversion between releases (docs/SAVE_RELEASES.md). Read-only: it only
opens the .pak files under each directory given. Run with `python3 -I`, from outside the
extracted trees:

  python3 -I tools/savw_diff.py [--emit-inc platform/port_save_convert_tables.inc] G2ME01=/tmp/opencode/discs/G2ME01/files \
      G2MP01=/tmp/opencode/discs/G2MP01/files R3ME01=/tmp/opencode/discs/R3ME01/DATA/files/MP2

The first release is the base the others are diffed against. What it reports, and the code each
number mirrors:

- MemoryWorlds: every MLVL in the paks, sorted by asset ID (CMemoryCard::CMemoryCard).
- per SAVW (CWorldSaveGameInfo): areaCount, cinematics, relays, layers, doors, scans, system and
  game variables, unmappable objects; and CalculateHash() over the big-endian (console) and
  little-endian (TARGET_PC host) in-memory images.
- the save signature, a region seed ^ hash of every world (GetSaveSignature in CMemoryCardDriver):
  'USA>' 0x5553413e in G2ME01's DOL, 'EURP' 0x45555250 in G2MP01's. BE is the console value, which
  the port also writes since CalculateHash swaps its words. Older port builds used the USA seed on
  every disc, and the port still accepts what they wrote: BE words ("port before #95", differs only
  on G2MP) and LE words ("legacy port"). The G2ME01 values, 44bfc715 and 37d0412e, equal the two
  `[save] ... signature` lines the port logs; G2MP01's d13a209f is a real PAL console save's (#95).
- per MLVL: the per-area layer counts (CWorldLayers::ReadWorldLayers), which size the
  CWorldLayerState bits.
- the CWorldState record size in bits as CGameState::PutTo writes it, and the global lists
  (scan union sorted by ID, cinematic bits, env-var bits, HINT count).
"""
import os
import struct
import sys
import zlib

ROOT = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ROOT)
import echoes_disc as ed  # noqa: E402

SIGNATURE_SEED = 0x5553413E  # 'USA>': G2ME01, and the port on every disc before #95
SIGNATURE_SEED_PAL = 0x45555250  # 'EURP': G2MP01's GetSaveSignature
# CGameStateEnvVarManager::LoadFields (system scope), added before the SAVW variables.
SYSTEM_BUILTINS = [
    ("FreezeInstructionsFirstPerson", 0, 3), ("FreezeInstructionsMorphBall", 0, 3),
    ("PowerbombPickupMessages", 0, 1), ("PercentScans", 0, 100), ("NormalModeCompleted", 0, 1),
    ("HardModeCompleted", 0, 1), ("AllPickupsFound", 0, 1), ("AutoMapperPaneMode", 0, 2),
    ("LogbookLegendVisible", 0, 1), ("IngAttachedWarningCount", 0, 3), ("SeenIntroText", 0, 1),
]


class Dir:
    """echoes_disc.Iso stand-in over an extracted directory."""

    def __init__(self, root):
        self.root, self.files = root, {}
        for dp, _, fs in os.walk(root):
            for f in fs:
                p = os.path.join(dp, f)
                self.files[os.path.relpath(p, root)] = (p, os.path.getsize(p))

    def read(self, path, off=0, n=None):
        with open(self.files[path][0], "rb") as f:
            f.seek(off)
            return f.read(-1 if n is None else n)


def bitcount(v):
    """CEnvironmentVariable::GetBitCount."""
    return max(v, 0).bit_length()


def cstr(b, p):
    e = b.index(b"\0", p)
    return b[p:e].decode("latin1"), e + 1


def parse_savw(b):
    r = ed.Rd(b)
    s = {"magic": r.u32(), "version": r.u32(), "areaCount": r.u32()}

    def vec(n):
        c = r.u32()
        return [tuple(r.u32() for _ in range(n)) if n > 1 else r.u32() for _ in range(c)]

    s["cinematics"], s["relays"], s["layers"] = vec(1), vec(1), vec(2)
    s["doors"], s["scans"] = vec(1), vec(2)

    def envs():
        out = []
        for _ in range(r.u32()):
            name, r.p = cstr(b, r.p)
            out.append((name, *struct.unpack_from(">iii", b, r.p)))
            r.p += 12
        return out

    s["sysvars"], s["gamevars"] = (envs(), envs()) if s["version"] > 3 else ([], [])
    s["unmappable"] = vec(1) if s["version"] > 4 else []
    s["trailing"] = len(b) - r.p
    s["trailingNonPad"] = any(c not in (0, 0xFF) for c in b[r.p:])
    return s


def savw_hash(s, endian):
    """CWorldSaveGameInfo::CalculateHash over the in-memory image in the given byte order."""
    words = [s["areaCount"], *s["cinematics"], *s["relays"], *s["doors"]]
    words += [x for pair in s["layers"] for x in pair]
    words += [x for pair in s["scans"] for x in pair]
    words += s["unmappable"]
    data = struct.pack(endian + "%dI" % len(words), *words)
    h = 0
    for name, *_ in s["sysvars"] + s["gamevars"]:
        h = zlib.crc32(name.encode("latin1"), h ^ 0xFFFFFFFF) ^ 0xFFFFFFFF  # seed 0, no xorout
    return zlib.crc32(data, h ^ 0xFFFFFFFF) ^ 0xFFFFFFFF


def parse_mlvl(b):
    """echoes_disc.parse_mlvl plus the SAVW ID, area save IDs and the trailing layer table."""
    r = ed.Rd(b)
    r.u32()
    ver = r.u32()
    m = {"version": ver, "strg": r.u32()}
    if ver >= 22:
        r.u32()
    if ver >= 23:
        r.u32()
    m["savw"] = r.u32()
    r.u32()  # skybox
    assert ver >= 18, ver
    areas = []
    for _ in range(r.u32()):
        r.u32()
        r.skip(48 + 24)
        mrea = r.u32()
        save_id = r.u32()
        r.skip(2 * r.u32())
        r.skip(8 * r.u32())
        r.skip(8 * r.u32())
        r.skip(4 * r.u32())
        for _ in range(r.u32()):
            r.skip(8 * r.u32())
            r.skip(12 * r.u32())
        for _ in range(r.u32()):
            r.cstr()
        r.skip(4 * r.u32())
        r.cstr()
        areas.append((mrea, save_id))
    m["areas"] = areas
    r.u32()  # MAPW
    r.u32()
    r.cstr()  # default audio track
    m["layerCounts"], m["layerDefaults"] = [], []
    for _ in range(r.u32()):
        m["layerCounts"].append(r.u32())
        m["layerDefaults"].append((r.u32() << 32) | r.u32())  # default layer bits, bit i = layer i
    m["layerNames"] = [r.cstr() for _ in range(r.u32())]
    m["layerStart"] = [r.u32() for _ in range(r.u32())]
    return m


def area_layers(m, i):
    start = m["layerStart"][i]
    return m["layerNames"][start:start + m["layerCounts"][i]]


def hint_names(hb):
    """Names of the HINT_Hints entries in file order (CGameHintInfo::CGameHint)."""
    _, ver, count = struct.unpack_from(">III", hb, 0)
    p, names = 12, []
    for _ in range(count):
        name, p = cstr(hb, p)
        p += 12 + (4 if ver > 0 else 0)  # immediate, normal time, string ID, text time
        p += 4 + 16 * struct.unpack_from(">I", hb, p)[0]  # locations: 4 ints each
        names.append(name)
    return names


def load_release(label, root):
    d = ed.Iso(root) if root.lower().endswith(".iso") else Dir(root)
    rel = {"label": label, "mlvl": {}, "savw": {}, "hint": None}
    for p in sorted(d.files):
        if not p.lower().endswith(".pak"):
            continue
        pak = ed.Pak(d, p)
        for rid, (t, *_) in pak.res.items():
            if t == "MLVL" and rid not in rel["mlvl"]:
                rel["mlvl"][rid] = (parse_mlvl(pak.get(rid)), p)
            elif t == "SAVW" and rid not in rel["savw"]:
                rel["savw"][rid] = parse_savw(pak.get(rid))
        if rel["hint"] is None and "HINT_Hints" in pak.names:
            hb = pak.get(pak.names["HINT_Hints"][1])
            magic, ver, count = struct.unpack_from(">III", hb, 0)
            rel["hint"] = {"id": pak.names["HINT_Hints"][1], "version": ver, "count": count,
                           "crc": zlib.crc32(hb), "pak": p, "names": hint_names(hb)}
    rel["worlds"] = sorted(rel["mlvl"])  # MemoryWorlds order
    sysvars = {n: (lo, hi) for n, lo, hi in SYSTEM_BUILTINS}
    gamevars, scans, cines = {}, set(), 0
    sig_be = sig_le = SIGNATURE_SEED
    for w in rel["worlds"]:
        s = rel["savw"][rel["mlvl"][w][0]["savw"]]
        s["hashBE"], s["hashLE"] = savw_hash(s, ">"), savw_hash(s, "<")
        sig_be ^= s["hashBE"]
        sig_le ^= s["hashLE"]
        scans.update(s["scans"])
        cines += len(s["cinematics"])
        for n, lo, hi, _ in s["sysvars"]:
            sysvars.setdefault(n, (lo, hi))
        for n, lo, hi, _ in s["gamevars"]:
            gamevars.setdefault(n, (lo, hi))
    seed = SIGNATURE_SEED_PAL if label.startswith("G2MP") else SIGNATURE_SEED
    rel["sigBE"], rel["sigPort"], rel["sigLE"] = sig_be ^ SIGNATURE_SEED ^ seed, sig_be, sig_le
    rel["scanUnion"] = sorted(scans)
    rel["cinematicBits"] = cines
    rel["sysvars"], rel["gamevars"] = sysvars, gamevars
    return rel


def world_bits(m, s):
    """CWorldState::PutTo body size in bits (after the u32 MLVL + u16 bitCount prefix)."""
    layers = sum(c - 1 for c in m["layerCounts"])
    return (64 + len(s["relays"]) + 2 * s["areaCount"] + len(s["doors"]) +
            len(s["unmappable"]) + 1 + 10 + layers)


def envbits(v):
    return sum(bitcount(hi - lo) for lo, hi in v.values())


def h(v):
    return "%08x" % v


def dump(rel):
    print("== %s" % rel["label"])
    print("  MemoryWorlds (%d, sorted by MLVL ID):" % len(rel["worlds"]))
    print("    %-8s %-8s %-15s %5s %4s %4s %4s %4s %4s %4s %4s %6s %8s %8s" % (
        "MLVL", "SAVW", "pak", "areas", "ver", "cine", "rely", "lyrs", "door", "scan", "unmp",
        "wbits", "hashBE", "hashLE"))
    for w in rel["worlds"]:
        m, pak = rel["mlvl"][w]
        s = rel["savw"][m["savw"]]
        print("    %s %s %-15s %5d %4d %4d %4d %4d %4d %4d %4d %6d %s %s" % (
            h(w), h(m["savw"]), pak, s["areaCount"], s["version"], len(s["cinematics"]),
            len(s["relays"]), len(s["layers"]), len(s["doors"]), len(s["scans"]),
            len(s["unmappable"]), world_bits(m, s), h(s["hashBE"]), h(s["hashLE"])))
        if len(m["areas"]) != s["areaCount"] or len(m["layerCounts"]) != s["areaCount"]:
            print("      !! MLVL areas %d, layer table %d" % (len(m["areas"]), len(m["layerCounts"])))
        if s["trailingNonPad"]:
            print("      !! %d trailing SAVW bytes that are not 00/FF padding" % s["trailing"])
    print("  signature console (BE words; port writes) %s   port before #95 (USA seed) %s   "
          "legacy port (LE words) %s" % (h(rel["sigBE"]), h(rel["sigPort"]), h(rel["sigLE"])))
    print("  scan union %d entries (+4 fixed) -> POWR..SCAN scan bits %d" % (
        len(rel["scanUnion"]), 2 * (len(rel["scanUnion"]) + 4)))
    print("  cinematic bits %d; system vars %d (%d bits); game vars %d (%d bits)" % (
        rel["cinematicBits"], len(rel["sysvars"]), envbits(rel["sysvars"]),
        len(rel["gamevars"]), envbits(rel["gamevars"])))
    hi = rel["hint"]
    print("  HINT_Hints %s" % ("not found" if hi is None else "%s v%d count %d crc %s (%s)" % (
        h(hi["id"]), hi["version"], hi["count"], h(hi["crc"]), hi["pak"])))


def diff_list(name, a, b, out):
    if a == b:
        return
    sa, sb = set(a), set(b)
    if sa == sb:
        out.append("    %s: same %d entries, different order" % (name, len(a)))
        return
    out.append("    %s: %d -> %d; only base %d, only other %d%s" % (
        name, len(a), len(b), len(sa - sb), len(sb - sa),
        "; common entries keep relative order" if [x for x in a if x in sb] ==
        [x for x in b if x in sa] else "; common entries reordered"))


def diff(base, other):
    print("== diff %s -> %s" % (base["label"], other["label"]))
    wa, wb = base["worlds"], other["worlds"]
    print("  worlds only in %s: %s" % (base["label"], [h(w) for w in wa if w not in wb] or "-"))
    print("  worlds only in %s: %s" % (other["label"], [h(w) for w in wb if w not in wa] or "-"))
    common = [w for w in wa if w in wb]
    if [w for w in wb if w in wa] != common:
        print("  !! MemoryWorlds order of common worlds differs")
    for w in common:
        ma, mb = base["mlvl"][w][0], other["mlvl"][w][0]
        sa, sb = base["savw"][ma["savw"]], other["savw"][mb["savw"]]
        out = []
        if ma["savw"] != mb["savw"]:
            out.append("    SAVW id %s -> %s" % (h(ma["savw"]), h(mb["savw"])))
        for k in ("version", "areaCount"):
            if sa[k] != sb[k]:
                out.append("    %s %d -> %d" % (k, sa[k], sb[k]))
        if ma["areas"] != mb["areas"]:
            out.append("    MLVL (MREA, area save ID) list differs")
        if ma["layerCounts"] != mb["layerCounts"]:
            d = [(i, x, y) for i, (x, y) in enumerate(zip(ma["layerCounts"], mb["layerCounts"]))
                 if x != y]
            out.append("    layer counts differ in %d areas: %s" % (len(d), d[:8]))
            for i, _, _ in d[:8]:
                out.append("      area %d base   %s" % (i, area_layers(ma, i)))
                out.append("      area %d other  %s" % (i, area_layers(mb, i)))
        for k in ("cinematics", "relays", "layers", "doors", "scans", "sysvars", "gamevars",
                  "unmappable"):
            diff_list(k, sa[k], sb[k], out)
        if world_bits(ma, sa) != world_bits(mb, sb):
            out.append("    CWorldState bits %d -> %d" % (world_bits(ma, sa), world_bits(mb, sb)))
        if sa["hashBE"] != sb["hashBE"]:
            out.append("    hash %s -> %s" % (h(sa["hashBE"]), h(sb["hashBE"])))
        print("  world %s: %s" % (h(w), "identical" if not out else ""))
        for line in out:
            print(line)
    print("  signature console %s -> %s" % (h(base["sigBE"]), h(other["sigBE"])))
    out = []
    diff_list("scan union", base["scanUnion"], other["scanUnion"], out)
    for line in out or ["    scan union identical (%d)" % len(base["scanUnion"])]:
        print(line)
    for k in ("sysvars", "gamevars"):
        if base[k] != other[k]:
            print("    %s differ: only base %s, only other %s, range changed %s" % (
                k, sorted(set(base[k]) - set(other[k])), sorted(set(other[k]) - set(base[k])),
                sorted(n for n in base[k] if n in other[k] and base[k][n] != other[k][n])))
        else:
            print("    %s identical (%d)" % (k, len(base[k])))
    if base["cinematicBits"] != other["cinematicBits"]:
        print("    cinematic bits %d -> %d" % (base["cinematicBits"], other["cinematicBits"]))
    hb, ho = base["hint"], other["hint"]
    if hb and ho:
        print("    HINT count %d -> %d, bytes %s" % (
            hb["count"], ho["count"], "identical" if hb["crc"] == ho["crc"] else "differ"))


def maps_by_key(dst, src):
    """dst index -> src index of the same key, or -1; None when the lists are identical."""
    if dst == src:
        return None
    assert len(set(dst)) == len(dst) and len(set(src)) == len(src), "duplicate keys"
    pos = {k: i for i, k in enumerate(src)}
    return [pos.get(k, -1) for k in dst]


def release_tables(rel, name):
    """The per-release rows the converter needs to parse a slot written by that release."""
    out = []
    lines = []
    for w in rel["worlds"]:
        m = rel["mlvl"][w][0]
        s = rel["savw"][m["savw"]]
        n = "k%s_%08x" % (name, w)
        lines.append("static const uint8_t %s_layers[] = {%s};" % (
            n, ", ".join(str(c) for c in m["layerCounts"])))
        lines.append("static const uint64_t %s_defaults[] = {%s};" % (
            n, ", ".join("0x%xull" % d for d in m["layerDefaults"])))
        out.append("    {0x%08x, %d, %d, %d, %d, %s_layers, %s_defaults}," % (
            w, s["areaCount"], len(s["relays"]), len(s["doors"]), len(s["unmappable"]), n, n))
    return lines, out


def emit_array(name, ctype, values, fmt="%d"):
    body = ", ".join(fmt % v for v in values)
    return "static const %s %s[] = {%s};" % (ctype, name, body)


def emit_inc(src, dst, path):
    """Write the ME<->MP conversion tables (platform/port_save_convert_tables.inc)."""
    rels = {"ME": src, "MP": dst}
    lines = [
        "// Generated by tools/savw_diff.py --emit-inc; do not edit. Source: the extracted G2ME01 and",
        "// G2MP01 paks (MLVL, SAVW and HINT_Hints). Included once by port_save_convert.cpp.",
        "//",
        "// Per release: world rows (MemoryWorlds order), the scan union, the hint names and the",
        "// signature. Per direction: the maps from each target index to the source index that holds",
        "// the same ID, -1 where the target has an entry the source lacks.",
        "",
        "struct SConvWorld {",
        "  uint32_t mlvl;",
        "  uint16_t areas, relays, doors, unmappable;",
        "  const uint8_t* layerCounts;    // per area",
        "  const uint64_t* layerDefaults;  // per area, bit i = layer i",
        "};",
        "",
        "struct SConvRelease {",
        "  const char* code;",
        "  uint32_t signature;        // console order, region seed: what the console and the port write",
        "  uint32_t portSignature;    // console order, USA seed: what port builds before #95 wrote",
        "  uint32_t legacySignature;  // host-order words, USA seed: what older port builds wrote",
        "  int worldCount;",
        "  const SConvWorld* worlds;",
        "  int scanCount;             // the scan union, without the 4 fixed entries",
        "  const uint32_t* scans;     // sorted by ID",
        "  int hintCount;",
        "  const char* const* hints;",
        "  int gameVarBits;",
        "  int systemBits;            // CPersistentOptions env-var bits (informational)",
        "};",
        "",
        "struct SConvLayerMap {",
        "  uint16_t world, area;",
        "  uint8_t count;",
        "  const int8_t* map;  // target layer -> source layer, -1 if new",
        "};",
        "",
        "struct SConvWorldMap {",
        "  const int16_t* relays;      // null: identical lists",
        "  const int16_t* doors;",
        "  const int16_t* unmappable;",
        "};",
        "",
        "struct SConvDirection {",
        "  const SConvRelease* from;",
        "  const SConvRelease* to;",
        "  const uint32_t* worldIds;      // target MLVL for each source world index",
        "  const SConvWorldMap* worlds;   // per world index",
        "  const int16_t* scans;          // per target scan index -> source scan index or -1",
        "  const int16_t* hints;          // per target hint -> source hint or -1",
        "  int layerMapCount;",
        "  const SConvLayerMap* layerMaps;",
        "};",
        "",
    ]
    for name, rel in rels.items():
        a, rows = release_tables(rel, name)
        lines += a
        lines.append("static const SConvWorld k%sWorlds[] = {" % name)
        lines += rows
        lines.append("};")
        lines.append(emit_array("k%sScans" % name, "uint32_t",
                                [sc[0] for sc in rel["scanUnion"]], "0x%08x"))
        hn = rel["hint"]["names"]
        lines.append("static const char* const k%sHints[] = {%s};" % (
            name, ", ".join('"%s"' % x for x in hn)))
        lines.append("")
    for name, rel in rels.items():
        code = "G2ME" if name == "ME" else "G2MP"
        lines.append("static const SConvRelease k%sRelease = {\"%s\", 0x%08x, 0x%08x, 0x%08x, %d, "
                     "k%sWorlds, %d, k%sScans, %d, k%sHints, %d, %d};" % (
                         name, code, rel["sigBE"], rel["sigPort"], rel["sigLE"], len(rel["worlds"]), name,
                         len(rel["scanUnion"]), name, len(rel["hint"]["names"]), name,
                         envbits(rel["gamevars"]), envbits(rel["sysvars"])))
    lines.append("")
    for a, b, tag in ((src, dst, "MEtoMP"), (dst, src, "MPtoME")):
        sn, dn = ("ME", "MP") if tag == "MEtoMP" else ("MP", "ME")
        wa, wb = a["worlds"], b["worlds"]
        assert len(wa) == len(wb)
        # World order must survive the ID mapping, or the world records would need re-sorting.
        for i, (x, y) in enumerate(zip(wa, wb)):
            if x != y:
                print("  world %d: %08x -> %08x" % (i, x, y), file=sys.stderr)
        # Cinematic bits are copied verbatim, so the lists must agree world for world.
        for x, y in zip(wa, wb):
            sx, sy = (a["savw"][a["mlvl"][x][0]["savw"]], b["savw"][b["mlvl"][y][0]["savw"]])
            assert sx["cinematics"] == sy["cinematics"], "cinematics differ in %08x" % x
            assert sx["areaCount"] == sy["areaCount"]
        assert a["sysvars"] == b["sysvars"] and a["gamevars"] == b["gamevars"]
        wm_rows, layer_rows = [], []
        for wi, (x, y) in enumerate(zip(wa, wb)):
            ma, mb = a["mlvl"][x][0], b["mlvl"][y][0]
            sa, sb = a["savw"][ma["savw"]], b["savw"][mb["savw"]]
            names = []
            for key in ("relays", "doors", "unmappable"):
                mp = maps_by_key(sb[key], sa[key])
                if mp is None:
                    names.append("nullptr")
                else:
                    n = "k%s_%s_%d" % (tag, key, wi)
                    lines.append(emit_array(n, "int16_t", mp))
                    names.append(n)
            wm_rows.append("    {%s}," % ", ".join(names))
            for ai in range(sa["areaCount"]):
                la, lb = area_layers(ma, ai), area_layers(mb, ai)
                if la != lb:
                    assert len(set(la)) == len(la) and len(set(lb)) == len(lb)
                    n = "k%s_layers_%d_%d" % (tag, wi, ai)
                    lines.append(emit_array(n, "int8_t",
                                            [la.index(k) if k in la else -1 for k in lb]))
                    layer_rows.append("    {%d, %d, %d, %s}," % (wi, ai, len(lb), n))
        sa_ids = [sc[0] for sc in a["scanUnion"]]
        sb_ids = [sc[0] for sc in b["scanUnion"]]
        assert len(set(sa_ids)) == len(sa_ids) and len(set(sb_ids)) == len(sb_ids)
        sm = maps_by_key(sb_ids, sa_ids)
        ha, hb = a["hint"]["names"], b["hint"]["names"]
        assert len(set(ha)) == len(ha) and len(set(hb)) == len(hb), "duplicate hint names"
        hm = maps_by_key(hb, ha)
        lines.append(emit_array("k%s_scans" % tag, "int16_t", sm or list(range(len(sb_ids)))))
        lines.append(emit_array("k%s_hints" % tag, "int16_t", hm or list(range(len(hb)))))
        lines.append(emit_array("k%s_worldIds" % tag, "uint32_t", wb, "0x%08x"))
        lines.append("static const SConvWorldMap k%s_worlds[] = {" % tag)
        lines += wm_rows
        lines.append("};")
        if layer_rows:
            lines.append("static const SConvLayerMap k%s_layerMaps[] = {" % tag)
            lines += layer_rows
            lines.append("};")
        lines.append("static const SConvDirection k%s = {&k%sRelease, &k%sRelease, k%s_worldIds, "
                     "k%s_worlds, k%s_scans, k%s_hints, %d, %s};" % (
                         tag, sn, dn, tag, tag, tag, tag, len(layer_rows),
                         "k%s_layerMaps" % tag if layer_rows else "nullptr"))
        lines.append("")
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")


def main(argv):
    inc = None
    if argv and argv[0] == "--emit-inc":
        inc, argv = argv[1], argv[2:]
    if not argv:
        argv = ["G2ME01=/tmp/opencode/discs/G2ME01/files",
                "G2MP01=/tmp/opencode/discs/G2MP01/files",
                "R3ME01=/tmp/opencode/discs/R3ME01/DATA/files/MP2"]
    rels = [load_release(*a.split("=", 1)) for a in argv]
    if inc:
        emit_inc(rels[0], rels[1], inc)
        return
    for r in rels:
        dump(r)
    for r in rels[1:]:
        diff(rels[0], r)
    if len(rels) > 2:
        diff(rels[1], rels[2])


if __name__ == "__main__":
    main(sys.argv[1:])
