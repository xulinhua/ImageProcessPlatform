# -*- coding: utf-8 -*-
"""Restore GBK Chinese in corrupted UTF-8 files using git HEAD as reference."""
from __future__ import print_function

import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPO = os.path.dirname(ROOT)


def read_head(rel):
    data = subprocess.check_output(["git", "show", "HEAD:%s" % rel], cwd=REPO)
    return data.decode("gbk")


def read_work(rel):
    path = os.path.join(REPO, rel)
    data = open(path, "rb").read()
    if data.startswith(b"\xef\xbb\xbf"):
        data = data[3:]
    repl = data.count(b"\xef\xbf\xbd")
    text = data.decode("utf-8", errors="replace")
    return text, repl, data


def skeleton(line):
    return re.sub(r"[^\x00-\x7f]+", "", line)


def is_broken_line(line):
    if u"\ufffd" in line:
        return True
    # Latin-1 mojibake from GBK read as cp1252
    if re.search(r"[\u00c0-\u00ff]{2,}", line):
        return True
    if re.search(r"[\u0080-\u00ff]", line) and re.search(r"[\u4e00-\u9fff]", line) is None:
        if re.search(r"(brief|tr\(|T\(|///<|/\*|//)", line):
            return True
    return False


def fix_file(rel, dry_run=False):
    head_lines = read_head(rel).splitlines()
    work_text, repl, _raw = read_work(rel)
    if repl == 0 and not any(is_broken_line(l) for l in work_text.splitlines()):
        return False, "already ok"

    work_lines = work_text.splitlines()
    head_map = {}
    for hl in head_lines:
        sk = skeleton(hl)
        if sk.strip():
            head_map[sk] = hl

    fixed = []
    changed = 0
    unmatched = 0
    for wl in work_lines:
        if is_broken_line(wl):
            sk = skeleton(wl)
            if sk in head_map:
                fixed.append(head_map[sk])
                changed += 1
                continue
            unmatched += 1
        fixed.append(wl)

    out = "\n".join(fixed)
    if work_text.endswith("\n"):
        out += "\n"

    if dry_run:
        return True, "would fix %d lines, unmatched %d, repl %d" % (changed, unmatched, repl)

    out_bytes = out.encode("gbk", errors="strict")
    target = os.path.join(REPO, rel)
    tmp = target + ".fixtmp"
    with open(tmp, "wb") as f:
        f.write(out_bytes)
    os.replace(tmp, target)
    return True, "fixed %d lines, unmatched %d, repl was %d" % (changed, unmatched, repl)


def iter_targets():
    out = subprocess.check_output(["git", "diff", "--name-only", "HEAD"], cwd=REPO)
    for rel in out.decode("utf-8", errors="replace").splitlines():
        path = os.path.join(REPO, rel)
        if not os.path.isfile(path):
            continue
        data = open(path, "rb").read()
        if data.count(b"\xef\xbf\xbd") == 0:
            text = data.decode("utf-8", errors="replace")
            if not any(is_broken_line(l) for l in text.splitlines()):
                continue
        yield rel


def main():
    dry = "--dry-run" in sys.argv
    ok = 0
    fail = 0
    for rel in iter_targets():
        try:
            did, msg = fix_file(rel, dry_run=dry)
            print("%s: %s" % (rel, msg))
            if did:
                ok += 1
        except Exception as exc:
            print("%s: ERROR %s" % (rel, exc))
            fail += 1
    print("done: %d files touched, %d errors" % (ok, fail))
    return 1 if fail else 0


if __name__ == "__main__":
    sys.exit(main())
