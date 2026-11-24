#!/usr/bin/env python3
"""Bring src/MetroidPrime/PortLinkStubs.cpp in line with one failed link.

Run it on the log of a link that failed, then relink. The link must have been
run with `-Wl,--no-demangle`: GNU ld otherwise prints C++ names demangled
(`CFoo::Bar(int)`), which cannot be written back as a symbol.

Two edits, both by symbol name, never by line number:

  - **retire** every stub whose symbol the link reports as `multiple definition` - the tree now
    defines it for real. Both stub shapes are recognised: the `reachstub_N() asm("sym")` pair
    with its `// demangled` comment line, and the one-line `extern "C" void sym(void) {...}`
    auto-stub. A duplicate that is not a stub is reported and left alone.
  - **append** a stub for every `undefined reference` not already stubbed. Functions call
    `mpReachStub`, so each logs its own name on entry. Data symbols get zeroed storage (0x400
    bytes, or retail's size if larger), so a read sees 0 instead of code bytes and a write does
    not hit a read-only page. **Data or code is asked, not guessed from the spelling** - the old
    self-heal stubbed `lbl_80418BA8` and `lbl_803A56C0` as functions (docs/HANDOFF.md, "The
    stub-a-data-symbol-as-a-function bug"): an unmangled name takes its `type:` from
    `config/G2ME01/symbols.txt`; a mangled one is data when it demangles without a parameter list
    (vtables, static members); an unmangled name retail does not list is data only if `lbl_`.

A third case has no link line at all: a strong stub for a symbol another object defines *weak*
(nm `V`/`W` - vtables keyed to an inline dtor, COMDAT functions) silently wins. `--objdir DIR`
retires those too, by running nm over DIR's objects (`weak_shadowed`). Data stubs
(`reachdata_N`) are retired by the same rules as function stubs.

Run: `python3 tools/restub_reach.py [--objdir DIR] <link-log> [stub-file]`. Prints `retired N, added M`; exits 0
whether or not it changed anything (the caller decides from the relink).
"""
import datetime
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent


def demangle(syms):
    if not syms:
        return []
    out = subprocess.run(['c++filt'], input='\n'.join(syms), capture_output=True, text=True).stdout
    return out.split('\n')[:len(syms)]


def retail_types():
    """name -> (type, size) from config/G2ME01/symbols.txt; only unmangled names can match."""
    out = {}
    for line in (ROOT / 'config/G2ME01/symbols.txt').read_text(errors='replace').splitlines():
        m = re.match(r'(\S+) = [^;]*; // type:(\w+)(?: size:(0x[0-9A-Fa-f]+))?', line)
        if m:
            out[m.group(1)] = (m.group(2), int(m.group(3), 16) if m.group(3) else 0)
    return out


def weak_shadowed(objdir, stubfile):
    """Symbols some object in `objdir` defines *weak* (nm V/W: inline-keyed vtables, COMDAT
    functions) - a strong stub for one of them wins the link silently, with no `multiple
    definition` line. `_ZTV18CErrorOutputWindow` did exactly that: the zero-filled stub replaced
    the real vtable and frame 1 jumped to address 0 in `win->PreDraw()`."""
    objs = [str(o) for o in pathlib.Path(objdir).rglob('*.o') if stubfile.stem not in o.name]
    weak = set()
    for i in range(0, len(objs), 500):
        out = subprocess.run(['nm', '--defined-only', *objs[i:i + 500]],
                             capture_output=True, text=True).stdout
        weak |= {f[2] for f in (l.split() for l in out.splitlines()) if len(f) == 3 and f[1] in 'VW'}
    return weak


def main() -> int:
    args = sys.argv[1:]
    objdir = None
    if '--objdir' in args:
        k = args.index('--objdir')
        objdir = args[k + 1]
        del args[k:k + 2]
    log = pathlib.Path(args[0]).read_text(errors='replace')
    path = pathlib.Path(args[1]) if len(args) > 1 else ROOT / 'src/MetroidPrime/PortLinkStubs.cpp'
    lines = path.read_text().split('\n')

    dups = set(re.findall(r"multiple definition of `([^']+)'", log))
    reported = set(dups)
    if objdir:
        dups |= weak_shadowed(objdir, path)
    kill = set()
    retired = []
    for i, line in enumerate(lines):
        m = re.match(r'extern "C" __attribute__\(\(aligned\(32\)\)\) char (reachdata_\d+)\[[^]]+\] asm\("([^"]+)"\);$', line)
        if m and m.group(2) in dups:
            kill.add(i)
            if i > 0 and lines[i - 1].startswith('// '):
                kill.add(i - 1)
            if i + 1 < len(lines) and re.match(rf'__attribute__\(\(aligned\(32\)\)\) char {m.group(1)}\[', lines[i + 1]):
                kill.add(i + 1)
                if i + 2 < len(lines) and lines[i + 2] == '':
                    kill.add(i + 2)
            retired.append(m.group(2))
            continue
        m = re.match(r'extern "C" void (reachstub_\d+)\(\) asm\("([^"]+)"\);$', line)
        if m and m.group(2) in dups:
            kill.add(i)
            if i > 0 and lines[i - 1].startswith('// '):
                kill.add(i - 1)
            if i + 1 < len(lines) and lines[i + 1].startswith(f'extern "C" void {m.group(1)}() {{'):
                kill.add(i + 1)
                if i + 2 < len(lines) and lines[i + 2] == '':
                    kill.add(i + 2)
            retired.append(m.group(2))
            continue
        m = re.match(r'extern "C" void ([A-Za-z_$][A-Za-z0-9_$]*)\(void\) \{', line)
        if m and m.group(1) in dups:
            kill.add(i)
            retired.append(m.group(1))
    lines = [l for i, l in enumerate(lines) if i not in kill]
    for sym in sorted(reported - set(retired)):
        print(f'restub_reach: duplicate `{sym}` is not a stub - left for a human', file=sys.stderr)

    text = '\n'.join(lines)
    have = set(re.findall(r'asm\("([^"]+)"\)', text))
    have |= set(re.findall(r'^extern "C" void ([A-Za-z_$][A-Za-z0-9_$]*)\(void\) \{', text, re.M))
    undef = sorted(set(re.findall(r"undefined reference to `([^']+)'", log)) - have)
    bad = [s for s in undef if not re.fullmatch(r'[A-Za-z_$.][A-Za-z0-9_$.]*', s)]
    for s in bad:
        print(f'restub_reach: `{s}` is not a symbol name (was the link run with '
              f'-Wl,--no-demangle?) - skipped', file=sys.stderr)
    undef = [s for s in undef if s not in bad]

    n = max([int(x) for x in re.findall(r'reach(?:stub|data)_(\d+)', text)] or [0]) + 1
    types = retail_types()
    out = []
    for sym, dem in zip(undef, demangle(undef)):
        kind, size = types.get(sym, (None, 0))
        if kind is not None:
            data = kind == 'object'
        else:
            data = sym.startswith('lbl_') or (sym.startswith('_Z') and '(' not in dem)
        out.append(f'// {dem}')
        if data:
            size = f'{max(0x400, (size + 31) & ~31):#x}'
            out.append(f'extern "C" __attribute__((aligned(32))) char reachdata_{n}[{size}] asm("{sym}");')
            out.append(f'__attribute__((aligned(32))) char reachdata_{n}[{size}] = {{}};')
        else:
            out.append(f'extern "C" void reachstub_{n}() asm("{sym}");')
            out.append(f'extern "C" void reachstub_{n}() {{ mpReachStub("{sym}", "{dem}"); }}')
        out.append('')
        n += 1

    body = text.rstrip('\n') + '\n'
    if retired:
        print(f'restub_reach: retired {len(retired)} stub(s) the tree now defines: '
              + ', '.join(retired[:8]) + (' ...' if len(retired) > 8 else ''))
    if out:
        stamp = datetime.datetime.now().isoformat(timespec='seconds')
        body += '\n'.join(['', f'// --- appended by tools/restub_reach.py on {stamp} ---',
                           '// Unresolved symbols one link asked for; see the file header.'] + out)
    if retired or out:
        path.write_text(body)
    print(f'retired {len(retired)}, added {len(undef)}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
