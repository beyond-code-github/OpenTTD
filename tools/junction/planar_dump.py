#!/usr/bin/env python3
"""Offline reader of the planar drawing dumps (PlanarDrawOptions::dump_path / PlanarDrawPipelineOptions::dump_dir).

A dump is one JSON file: status, slack, tracks (name, headings, points), faces (walls in walk order),
stretched rows, and every keep-apart pair with its candidates and their margins on the drawing.
Standard library only.
"""

import argparse
import json
import sys


def load(path):
    with open(path) as f:
        return json.load(f)


class Dump:
    def __init__(self, path):
        self.d = load(path)
        self.tracks = self.d["tracks"]
        self.faces = {f["face"]: f["walls"] for f in self.d["faces"]}
        self.pairs = {p["tag"]: p for p in self.d["pairs"]}

    def wall_label(self, track, piece, heading=None):
        """track#piece hH; H is the heading the face walk goes along the wall (the piece's own heading when not given)."""
        if track < 0 or track >= len(self.tracks):
            return "box#%d" % piece
        t = self.tracks[track]
        heads = t["headings"]
        h = heading if heading is not None else (heads[piece] if piece < len(heads) else "?")
        return "%s#%d h%s" % (t["name"], piece, h)

    def wall_points(self, track, piece):
        if track < 0 or track >= len(self.tracks):
            return None
        pts = self.tracks[track]["points"]
        if piece + 1 >= len(pts):
            return None
        return pts[piece], pts[piece + 1]

    def wall_text(self, track, piece, heading=None):
        p = self.wall_points(track, piece)
        label = self.wall_label(track, piece, heading)
        if p is None:
            return label
        return "%s (%d,%d)-(%d,%d)" % (label, p[0][0], p[0][1], p[1][0], p[1][1])

    def wall_len(self, track, piece):
        p = self.wall_points(track, piece)
        if p is None:
            return None
        return max(abs(p[1][0] - p[0][0]), abs(p[1][1] - p[0][1]))

    def decode(self, tag):
        """Return (face, i, j, wall_i, wall_j) of a sep tag, or None."""
        words = tag.split()
        if len(words) != 4 or words[0] not in ("sep", "sepdir", "sephard"):
            return None
        face, i, j = int(words[1]), int(words[2]), int(words[3])
        walls = self.faces.get(face)
        if walls is None or i >= len(walls) or j >= len(walls):
            return None
        return face, i, j, walls[i], walls[j]

    def tag_text(self, tag):
        r = self.decode(tag)
        if r is None:
            return tag
        face, i, j, wi, wj = r
        return "%s | %s  [face %d walls %d,%d]" % (self.wall_label(wi["track"], wi["piece"], wi["heading"]), self.wall_label(wj["track"], wj["piece"], wj["heading"]), face, i, j)

    def walk(self, face, a, b):
        """The walls strictly between wall index a and b going forward."""
        walls = self.faces[face]
        n = len(walls)
        out = []
        k = (a + 1) % n
        while k != b:
            out.append(walls[k])
            k = (k + 1) % n
        return out


def cmd_rows(args):
    d = Dump(args.file)
    rows = d.d["stretched"]
    if args.family:
        rows = [r for r in rows if r["tag"].split()[0] == args.family]
    rows = sorted(rows, key=lambda r: -r["slack"])
    if args.top:
        rows = rows[: args.top]
    print("status %s, total slack %.3f, %d stretched rows shown" % (d.d["status"], d.d["slack"], len(rows)))
    for r in rows:
        print("%9.3f  %s  %s" % (r["slack"], r["tag"], d.tag_text(r["tag"]) if r["tag"].startswith("sep") else ""))


def cmd_pair(args):
    d = Dump(args.file)
    tag = args.tag
    r = d.decode(tag)
    p = d.pairs.get(tag)
    if r is None or p is None:
        sys.exit("no pair %r in %s" % (tag, args.file))
    face, i, j, wi, wj = r
    print("%s" % tag)
    print("wall i: %s  len %s" % (d.wall_text(wi["track"], wi["piece"], wi["heading"]), d.wall_len(wi["track"], wi["piece"])))
    print("wall j: %s  len %s" % (d.wall_text(wj["track"], wj["piece"], wj["heading"]), d.wall_len(wj["track"], wj["piece"])))
    print("candidates (chosen marked *):")
    for c in p["candidates"]:
        print(" %s c%d s(%d,%d) spacing %d from %s margin %d" % ("*" if c["c"] == p["chosen"] else " ", c["c"], c["s"][0], c["s"][1], c["spacing"], c["from"], c["margin"]))
    if p["chosen"] < 0:
        print("  (no chosen candidate readable)")
    for name, a, b in (("i to j", i, j), ("j to i", j, i)):
        print("walk %s:" % name)
        between = d.walk(face, a, b)
        if not between:
            print("  (adjacent)")
        for w in between:
            ln = d.wall_len(w["track"], w["piece"]) if w["track"] >= 0 else None
            print("  %s len %s" % (d.wall_label(w["track"], w["piece"], w["heading"]), ln if ln is not None else "-"))
    for s in d.d["stretched"]:
        if s["tag"] == tag:
            print("stretched: %.3f" % s["slack"])


def cmd_face(args):
    d = Dump(args.file)
    walls = d.faces.get(args.id)
    if walls is None:
        sys.exit("no face %d" % args.id)
    print("face %d, %d walls" % (args.id, len(walls)))
    for k, w in enumerate(walls):
        ln = d.wall_len(w["track"], w["piece"]) if w["track"] >= 0 else None
        print("  %2d  %s  len %s" % (k, d.wall_text(w["track"], w["piece"], w["heading"]), ln if ln is not None else "-"))
    print("stretched rows of the face:")
    for s in sorted(d.d["stretched"], key=lambda s: -s["slack"]):
        r = d.decode(s["tag"])
        if r is not None and r[0] == args.id:
            print("  %9.3f  %s  %s" % (s["slack"], s["tag"], d.tag_text(s["tag"])))
        elif s["tag"].split()[:2] == ["clear", str(args.id)]:
            print("  %9.3f  %s" % (s["slack"], s["tag"]))


def cmd_diff(args):
    a, b = Dump(args.a), Dump(args.b)
    n = 0
    for tag, pa in a.pairs.items():
        pb = b.pairs.get(tag)
        if pb is None or pa["chosen"] == pb["chosen"]:
            continue
        n += 1
        print("%s  %s" % (tag, a.tag_text(tag)))
        print("   A chosen c%d: %s" % (pa["chosen"], " ; ".join("c%d %d" % (c["c"], c["margin"]) for c in pa["candidates"])))
        print("   B chosen c%d: %s" % (pb["chosen"], " ; ".join("c%d %d" % (c["c"], c["margin"]) for c in pb["candidates"])))
    print("%d pairs differ (of %d in both)" % (n, sum(1 for t in a.pairs if t in b.pairs)))


def cmd_svg(args):
    d = Dump(args.file)
    ref = Dump(args.ref) if args.ref else None
    segs = []  # (track, piece, dump)
    if args.face:
        for fid in args.face:
            for w in d.faces.get(fid, []):
                if w["track"] >= 0 and d.wall_points(w["track"], w["piece"]):
                    segs.append((w["track"], w["piece"]))
    else:
        names = set(args.tracks or [])
        for t, tr in enumerate(d.tracks):
            if tr["name"] in names:
                for k in range(len(tr["points"]) - 1):
                    segs.append((t, k))
    segs = list(dict.fromkeys(segs))
    ref_segs = []
    if ref is not None:
        ref_tracks = {tr["name"]: tr for tr in ref.d["tracks"]}
        for t, k in segs:
            name = d.tracks[t]["name"]
            rt = ref_tracks.get(name)
            if rt and k + 1 < len(rt["points"]):
                ref_segs.append((name, k, rt["points"][k], rt["points"][k + 1]))
    pts = []
    for t, k in segs:
        pts.extend(d.wall_points(t, k))
    for r in ref_segs:
        pts.extend([r[2], r[3]])
    if not pts:
        sys.exit("nothing to draw")
    x0, x1 = min(p[0] for p in pts), max(p[0] for p in pts)
    y0, y1 = min(p[1] for p in pts), max(p[1] for p in pts)
    pad = 3
    w, h = x1 - x0 + 2 * pad, y1 - y0 + 2 * pad
    scale = 800.0 / max(w, h)  # stroke widths in user units that read at 800 px
    sw = 1.5 / scale
    fs = 11 / scale
    out = ['<svg xmlns="http://www.w3.org/2000/svg" viewBox="%g %g %g %g" width="%d" height="%d">' % (x0 - pad, y0 - pad, w, h, round(w * scale), round(h * scale))]
    out.append('<rect x="%g" y="%g" width="%g" height="%g" fill="white"/>' % (x0 - pad, y0 - pad, w, h))
    for name, k, p, q in ref_segs:
        out.append('<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="#bbb" stroke-width="%g"/>' % (p[0], p[1], q[0], q[1], sw * 1.5))
    mid = {}
    for t, k in segs:
        p, q = d.wall_points(t, k)
        out.append('<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="#1f4e9c" stroke-width="%g"/>' % (p[0], p[1], q[0], q[1], sw))
        mx, my = (p[0] + q[0]) / 2.0, (p[1] + q[1]) / 2.0
        mid[(t, k)] = (mx, my)
        out.append('<text x="%g" y="%g" font-size="%g" fill="#222" font-family="monospace">%s</text>' % (mx, my, fs, "%s#%d" % (d.tracks[t]["name"], k)))
    for s in d.d["stretched"]:
        r = d.decode(s["tag"])
        if r is None or not s["tag"].startswith("sep "):
            continue
        face, i, j, wi, wj = r
        a, b = mid.get((wi["track"], wi["piece"])), mid.get((wj["track"], wj["piece"]))
        if a and b:
            out.append('<line x1="%g" y1="%g" x2="%g" y2="%g" stroke="red" stroke-width="%g" stroke-dasharray="%g %g"/>' % (a[0], a[1], b[0], b[1], sw, 4 * sw, 3 * sw))
    out.append("</svg>")
    with open(args.output, "w") as f:
        f.write("\n".join(out) + "\n")
    print("wrote %s (%d walls, %d reference walls)" % (args.output, len(segs), len(ref_segs)))


def main():
    ap = argparse.ArgumentParser(description="Read planar drawing dumps offline.")
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("rows", help="stretched rows, largest first, sep tags decoded")
    p.add_argument("file")
    p.add_argument("--family", help="only this row family (sep, curve, ...)")
    p.add_argument("--top", type=int, help="only the N largest")
    p.set_defaults(fn=cmd_rows)
    p = sub.add_parser("pair", help="one sep pair: walls, candidates with margins, the face walk both ways")
    p.add_argument("file")
    p.add_argument("tag", help='e.g. "sep 13 3 12"')
    p.set_defaults(fn=cmd_pair)
    p = sub.add_parser("face", help="one face's walls in order and its stretched rows")
    p.add_argument("file")
    p.add_argument("id", type=int)
    p.set_defaults(fn=cmd_face)
    p = sub.add_parser("diff", help="pairs in both dumps whose chosen candidate differs")
    p.add_argument("a")
    p.add_argument("b")
    p.set_defaults(fn=cmd_diff)
    p = sub.add_parser("svg", help="draw faces' walls or tracks as SVG")
    p.add_argument("file")
    g = p.add_mutually_exclusive_group(required=True)
    g.add_argument("--face", type=int, nargs="+", help="face ids")
    g.add_argument("--tracks", nargs="+", help="track names")
    p.add_argument("-o", "--output", required=True)
    p.add_argument("--ref", help="a second dump whose same tracks are drawn in grey underneath")
    p.set_defaults(fn=cmd_svg)
    args = ap.parse_args()
    args.fn(args)


if __name__ == "__main__":
    main()
