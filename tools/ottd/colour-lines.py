#!/usr/bin/env python3
"""colour-lines.py LAYOUT.json LINES.txt OUT.json [--types ID,ID,...]
Give each line of a layout a rail type: sets "rail_type" on tracks and bridges entries (see README, Layout JSON)."""
import argparse, json, re, sys


def base(line):
    if line.startswith("prio:"):
        line = line[5:]
    return re.sub(r"(\"\d*|')$", "", line)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("layout")
    ap.add_argument("lines")
    ap.add_argument("out")
    ap.add_argument("--types", default=None)
    a = ap.parse_args()
    with open(a.layout) as f:
        lay = json.load(f)
    types = [int(x) for x in a.types.split(",")] if a.types else None
    order, tile_lines = [], {}
    with open(a.lines) as f:
        for raw in f:
            p = raw.split()
            if len(p) < 5:
                continue
            x, y, line = int(p[0]), int(p[1]), p[4]
            b = base(line)
            if b not in order:
                order.append(b)
            tile_lines.setdefault((x, y), []).append(b)
    if types is not None and len(types) < len(order):
        sys.exit("FAIL: %d lines but only %d types" % (len(order), len(types)))
    tid = {b: (types[i] if types else i) for i, b in enumerate(order)}
    mixed = sum(1 for ls in tile_lines.values() if len({tid[b] for b in ls}) > 1)
    for t in lay.get("tracks", []):
        ls = tile_lines.get((t["x"], t["y"]))
        if ls:
            t["rail_type"] = tid[ls[0]]
    for b in lay.get("bridges", []):
        ls = tile_lines.get(tuple(b["start"]))
        if ls:
            b["rail_type"] = tid[ls[0]]
    with open(a.out, "w") as f:
        json.dump(lay, f, separators=(",", ":"))
    for b in order:
        print("%s %d" % (b, tid[b]))
    print("mixed-type tiles: %d" % mixed)


main()
