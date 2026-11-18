#!/usr/bin/env python3
"""Local search over LL² variant bits, solving variants on demand (one bit flip per neighbour)."""
import json, os, re, subprocess, sys, time, random
from concurrent.futures import ThreadPoolExecutor

D = os.path.dirname(os.path.abspath(__file__))
EXE = os.path.join(D, "openttd_test")
TEST = "planar components: LL² from its topology text (mapped) at the probe's area"
INFO = re.compile(r"variant (\S+) area (\S+) size")
TAG = re.compile(r"o(\d+)t(\d+)f(\d+)s(\d+)")
J = int(os.environ.get("LS_J", "4"))
cache_file = os.path.join(D, "cache.json")
cache = json.load(open(cache_file)) if os.path.exists(cache_file) else {}


def built_tags():
    r = subprocess.run([EXE, TEST, "-s"], env=dict(os.environ, PLANAR_DRY="1"), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace", timeout=600)
    out = []
    for l in r.stdout.splitlines():
        p = l.split(" | ")
        if l.startswith("DRY ") and p[-1].strip() == "built":
            out.append(p[0][4:].strip())
    return sorted(set(out))


def solve(tag):
    if tag in cache:
        return cache[tag]
    t0 = time.time()
    try:
        r = subprocess.run([EXE, TEST, "-s"], env=dict(os.environ, PLANAR_ONLY=tag), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace", timeout=60)
        a = None
        for l in r.stdout.splitlines():
            m = INFO.search(l)
            if m and m.group(1) == tag:
                a = float(m.group(2))
    except subprocess.TimeoutExpired:
        a = None
    cache[tag] = a
    json.dump(cache, open(cache_file, "w"))
    print(f"  solve {tag} -> {a} ({time.time() - t0:.1f}s)", flush=True)
    return a


def main():
    widths = [int(x) for x in os.environ.get("LS_WIDTHS", "6,3,1,3").split(",")]
    built = [f"o{o}t{t}f{f}s{s}" for o in range(1 << widths[0]) for t in range(1 << widths[1]) for f in range(1 << widths[2]) for s in range(1 << widths[3])]
    bset = set(built)
    print(f"built {len(built)} variants, bit widths o{widths[0]} t{widths[1]} f{widths[2]} s{widths[3]}", flush=True)

    def nbrs(tag):
        v = list(map(int, TAG.match(tag).groups()))
        for i in range(4):
            for b in range(widths[i]):
                w = v[:]
                w[i] ^= 1 << b
                n = f"o{w[0]}t{w[1]}f{w[2]}s{w[3]}"
                if n in bset:
                    yield n

    random.seed(1)
    starts = ["o0t0f0s0", "o0t0f1s0"] + random.sample(built, 2)
    pool = ThreadPoolExecutor(J)
    results = []
    for st in starts:
        cur = st
        a = solve(cur)
        steps = 0
        while True:
            ns = list(nbrs(cur))
            vals = dict(zip(ns, pool.map(solve, ns)))
            good = [n for n in ns if vals[n] is not None]
            if not good:
                break
            m = min(good, key=lambda n: vals[n])
            if a is None or vals[m] < a - 0.01:
                cur, a, steps = m, vals[m], steps + 1
            else:
                break
        print(f"START {st}: local best {cur} {a} after {steps} moves; solved so far {len(cache)}", flush=True)
        results.append((st, cur, a))
    best = min((r for r in results if r[2] is not None), key=lambda r: r[2])
    print(f"BEST {best[1]} {best[2]}; total distinct variants solved {len(cache)} of {len(built)}", flush=True)


if __name__ == "__main__":
    main()
