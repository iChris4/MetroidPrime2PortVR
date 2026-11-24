#!/usr/bin/env python3
"""Which of the port's undefined symbols are reachable from the program roots?

The port's link asks for ~523 symbols. A stub for all of them would link, but a
stub is only *safe* for a symbol nothing on the boot path calls. So the question
is not "how many are undefined" but "how many are reachable from the roots", and
that is a graph question.

Roots, and the second is the easy one to miss:

  1. the entry object and everything in `mp_platform`;
  2. **every object that emits a static initialiser** (`_GLOBAL__sub_I*`), because
     those run before `main`. `src/MetroidPrime/ScriptLoader.cpp` is the one that
     matters: its `__sinit_ScriptLoader_cpp` is what drags the whole REL loader
     family in, so every `Load*` symbol is on the boot path whether or not any C++
     code calls it. Leave that root out and the analysis will call the 159 module
     loaders optional, which is the opposite of true.

Granularity is per **object**. `mp_game` is one CMake target holding 200 objects,
so a target-level walk marks all 200 reachable and answers nothing.

Stated weakness: the walk is branch-blind and whole-object, so it
over-approximates. That errs in the safe direction, which is why the first set
below is an upper bound on what a stub would endanger, not a claim that all of it
executes.

Self-checks, because a silently wrong number here would be believed:
  - every object the linker names must be readable by `nm`;
  - the linker's undefined set must be almost entirely contained in what `nm`
    reports as undefined (the linker resolves what nm can pair up), so
    `ld_undef - all_undef` must be near zero. If it is not, the symbol tables
    were not read correctly and the split below is meaningless.
"""
import collections
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
LOG = ROOT / 'build-port-link' / 'build.log'
BUILD = ROOT / 'build-port-link'

text = LOG.read_text(errors='replace')
m = re.search(r'CMakeFiles/mp_port_entry\.dir.*?(?=\s-o\s+metroid_prime2_port)', text, re.S)
if not m:
    sys.exit('could not find the link command in the log')
objs = re.findall(r'(CMakeFiles/(?:mp_\w+)\.dir/\S+?\.o)\b', m.group(0))
paths = [BUILD / o for o in objs]
missing = [p for p in paths if not p.exists()]
paths = [p for p in paths if p.exists()]
print(f'objects on the link line: {len(objs)}, readable: {len(paths)}, missing: {len(missing)}')
if missing:
    sys.exit(f'stopping: {len(missing)} objects unreadable, e.g. {missing[0]}')

DEF_TYPES = {'T', 'W', 't', 'D', 'B', 'R', 'V', 'u', 'G', 'S'}
defined, undef_of = {}, {}
for p in paths:
    d = subprocess.run(['nm', '--defined-only', str(p)], capture_output=True, text=True)
    if d.returncode != 0:
        sys.exit(f'stopping: nm failed on {p}')
    defined[p] = {ln.split()[-1] for ln in d.stdout.split('\n')
                  if ln.strip() and ln.split()[-2:-1] and ln.split()[-2] in DEF_TYPES}
    u = subprocess.run(['nm', '--undefined-only', str(p)], capture_output=True, text=True)
    undef_of[p] = {ln.split()[-1] for ln in u.stdout.split('\n') if ln.strip()}

all_undef = set().union(*undef_of.values()) if undef_of else set()
all_def = set().union(*defined.values()) if defined else set()
# **ld prints demangled names, nm prints mangled ones.** Comparing the two directly
# matches almost nothing and looks like "the linker asks for symbols nm never saw".
# Both sides have to be in the same alphabet. `c++filt` leaves Metroid's
# `Name__F...` spellings alone, which is what we want - they are already unique.
mangled = sorted(all_undef | all_def)
dem = subprocess.run(['c++filt'], input='\n'.join(mangled),
                     capture_output=True, text=True).stdout.split('\n')
to_demangled = dict(zip(mangled, dem))
all_undef = {to_demangled.get(s, s) for s in all_undef}
all_def = {to_demangled.get(s, s) for s in all_def}
undef_of = {p: {to_demangled.get(s, s) for s in v} for p, v in undef_of.items()}
defined = {p: {to_demangled.get(s, s) for s in v} for p, v in defined.items()}

ld_undef = set(re.findall(r'undefined reference to `(.+?)\'', text))
print(f'distinct undefined symbols nm sees across our objects: {len(all_undef)}')
print(f'distinct defined symbols nm sees:                      {len(all_def)}')
print(f'the linker asked for:                                 {len(ld_undef)}')
unseen = ld_undef - all_undef
print(f'  of those, nm never saw them:                         {len(unseen)}'
      f'   <-- must be ~0 or the tables were misread')
if len(unseen) > 0.05 * max(len(ld_undef), 1):
    sys.exit('stopping: the linker asks for symbols nm does not report as undefined, '
             'so the reachable/unreachable split below would be meaningless')

provider = collections.defaultdict(set)
for p, syms in defined.items():
    for s in syms:
        provider[s].add(p)

roots = {p for p, syms in defined.items() if any(s.startswith('_GLOBAL__sub_I') for s in syms)}
roots |= {p for p in defined if 'mp_port_entry.dir' in str(p) or 'mp_platform.dir' in str(p)}
print(f'\nroots: {len(roots)} objects, of which '
      f'{sum(1 for p in roots if any(s.startswith("_GLOBAL__sub_I") for s in defined[p]))}'
      ' carry a static initialiser')

reach, frontier = set(roots), set(roots)
while frontier:
    nxt = set()
    for p in frontier:
        for s in undef_of.get(p, ()):
            for prov in provider.get(s, ()):
                if prov not in reach:
                    reach.add(prov); nxt.add(prov)
    frontier = nxt
print(f'objects reachable: {len(reach)} / {len(defined)}')

on_path = set()
for p in reach:
    on_path |= undef_of.get(p, set())
reach_undef = on_path & ld_undef
safe = ld_undef - on_path
print(f'\nundefined symbols the linker asked for: {len(ld_undef)}')
print(f'  referenced by a REACHABLE object: {len(reach_undef)}  '
      f'<- a blind stub here endangers the boot path')
print(f'  referenced only by UNREACHABLE objects: {len(safe)}  '
      f'<- a stub cannot affect the boot')

# The linker resolves the MANGLED name; ld only *prints* the demangled one. A stub
# has to carry the mangled spelling in its asm label, so save that mapping.
back = {}
for mang, dem in to_demangled.items():
    back.setdefault(dem, []).append(mang)


def spell(dem):
    """The mangled spelling(s) the linker knows this symbol by."""
    return sorted(back.get(dem, [dem]))


out = ROOT / 'docs' / 'research' / 'boot_path_undefined.txt'
out.write_text(
    "# Undefined symbols in the port's link, split by whether a REACHABLE object\n"
    "# references them. Generated by tools/link_reach.py - do not hand-edit.\n"
    "# The first set is an UPPER BOUND: whole-object granularity, branch-blind.\n"
    f"# reachable-referenced: {len(reach_undef)}   unreachable-only: {len(safe)}\n\n"
    "## referenced by a reachable object - a blind stub here endangers the boot path\n\n"
    + '\n'.join(sorted(reach_undef)) +
    "\n\n## referenced only by unreachable objects - a stub cannot affect the boot\n\n"
    + '\n'.join(sorted(safe)) + '\n')
print(f'\nwrote {out.relative_to(ROOT)}')

# The stub generator needs the mangled spellings, and needs to know which objects
# reference each one so a reviewer can check a stub by hand.
refs = collections.defaultdict(set)
for p, syms in undef_of.items():
    for s in syms:
        if s in safe:
            refs[s].add(p.name)
with (ROOT / 'docs' / 'research' / 'boot_path_stubbable.tsv').open('w') as f:
    f.write('# mangled\tdemangled\treferencing objects\n')
    for dem in sorted(safe):
        for mang in spell(dem):
            f.write(f'{mang}\t{dem}\t{",".join(sorted(refs[dem]))}\n')
print(f'wrote docs/research/boot_path_stubbable.tsv')
