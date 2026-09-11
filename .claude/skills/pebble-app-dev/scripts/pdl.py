#!/usr/bin/env python3
"""Parallel, resumable HTTP download with curl range requests: pdl.py URL OUT SIZE [N]"""
import os, subprocess, sys, threading
url, out, size = sys.argv[1], sys.argv[2], int(sys.argv[3])
n = int(sys.argv[4]) if len(sys.argv) > 4 else 8
chunk = (size + n - 1) // n
def fetch(i):
    start, end = i * chunk, min(size, (i + 1) * chunk) - 1
    part = f"{out}.part{i}"
    while True:
        have = os.path.getsize(part) if os.path.exists(part) else 0
        if have >= end - start + 1:
            return
        tmp = part + ".tmp"
        r = subprocess.run(["curl", "-sS", "--retry", "3", "-r", f"{start + have}-{end}", "-o", tmp, url])
        if r.returncode == 0 and os.path.exists(tmp):
            with open(part, "ab") as f, open(tmp, "rb") as g:
                f.write(g.read())
        if os.path.exists(tmp):
            os.remove(tmp)
threads = [threading.Thread(target=fetch, args=(i,)) for i in range(n)]
[t.start() for t in threads]; [t.join() for t in threads]
with open(out, "wb") as f:
    for i in range(n):
        with open(f"{out}.part{i}", "rb") as g:
            f.write(g.read())
        os.remove(f"{out}.part{i}")
assert os.path.getsize(out) == size, "size mismatch"
print("OK", out)
