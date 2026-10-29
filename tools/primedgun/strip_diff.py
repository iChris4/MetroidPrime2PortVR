#!/usr/bin/env python3
"""The TARGET_PC strip-diff (docs/DECOMP_WORKFLOW.md "Done means verified"): a decomp file's text as
the matching build sees it (TARGET_PC undefined) must be what it was at the base revision.

    python -I tools/primedgun/strip_diff.py <base-rev> [file...]   # default: changed src/ include/ files

Prints CLEAN or the differing lines per file; exits 1 when any file differs.
"""
import re
import subprocess
import sys

COND = re.compile(r"^\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)$")


def non_pc_text(text):
    """Evaluate only TARGET_PC conditionals, as undefined; every other conditional line is kept."""
    out = []
    # Stack of (kind, active, taken): kind "pc" for a TARGET_PC conditional, "other" otherwise.
    stack = []

    def emitting():
        return all(entry[1] for entry in stack)

    for line in text.split("\n"):
        m = COND.match(line)
        if m:
            directive, rest = m.group(1), m.group(2).strip()
            if directive in ("if", "ifdef", "ifndef"):
                mentions_pc = re.fullmatch(r"(defined\s*\(\s*TARGET_PC\s*\)|TARGET_PC)", rest) is not None
                if directive in ("ifdef", "ifndef") and rest == "TARGET_PC" or (directive == "if" and mentions_pc):
                    active = directive == "ifndef"
                    stack.append(["pc", active, active])
                    continue
                if directive == "if" and re.fullmatch(r"!\s*defined\s*\(\s*TARGET_PC\s*\)", rest):
                    stack.append(["pc", True, True])
                    continue
                stack.append(["other", True, True])
                if emitting():
                    out.append(line)
                continue
            if not stack:
                out.append(line)
                continue
            top = stack[-1]
            if top[0] == "pc":
                if directive == "else":
                    top[1] = not top[2]
                    top[2] = True
                elif directive == "elif":
                    top[1] = False
                elif directive == "endif":
                    stack.pop()
                continue
            if directive == "endif":
                if emitting():
                    out.append(line)
                stack.pop()
                continue
            if emitting():
                out.append(line)
            continue
        if emitting():
            out.append(line)
    return [l.rstrip() for l in out]


def show(rev, path):
    r = subprocess.run(["git", "show", f"{rev}:{path}"], capture_output=True)
    if r.returncode != 0:
        return None
    return r.stdout.decode("utf-8", "replace").replace("\r\n", "\n")


def main():
    base = sys.argv[1]
    files = sys.argv[2:]
    if not files:
        r = subprocess.run(["git", "diff", "--name-only", base, "--", "src", "include"], capture_output=True,
                           text=True, check=True)
        files = [f for f in r.stdout.split() if f.endswith((".cpp", ".hpp", ".h", ".c"))]
    bad = 0
    for path in files:
        old = show(base, path)
        new = open(path, "rb").read().decode("utf-8", "replace").replace("\r\n", "\n")
        if old is None:
            print(f"NEW   {path} (no base text: it must be port-only and in files.cmake by gen_files_cmake)")
            continue
        a, b = non_pc_text(old), non_pc_text(new)
        if a == b:
            print(f"CLEAN {path}")
            continue
        bad += 1
        print(f"DIFF  {path}")
        import difflib
        for line in list(difflib.unified_diff(a, b, "base", "now", n=1, lineterm=""))[:40]:
            print("      " + line)
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
