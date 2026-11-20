"""Corpus loader for the hub extractor (X-B1): whole-map export JSON -> tile world -> piece graph in a box.

Copied from the probes' trace code (c2.World, anatomy.Net's lane part, net.build2) with only what the extractor needs;
the originals are untouched.  The corpus root is an argument (`root`) or the env var JT_CORPUS; DEFAULT_ROOT is only the
fallback.  Under `python3 -I` the caller must put this directory on sys.path itself.
"""
import glob, json, os
from collections import defaultdict

DEFAULT_ROOT = '/mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad/psg'
PAIR = 20          # halo (tiles) around the box for the signal-direction vote and the straight-component pairing
D = {'NE': (-1, 0), 'SE': (0, 1), 'SW': (1, 0), 'NW': (0, -1)}
OPP = {'NE': 'SW', 'SW': 'NE', 'NW': 'SE', 'SE': 'NW'}
TEDGES = {'X': ('NE', 'SW'), 'Y': ('NW', 'SE'), 'Upper': ('NE', 'NW'), 'Lower': ('SE', 'SW'),
          'Left': ('NW', 'SW'), 'Right': ('NE', 'SE')}
PARALLEL = ({'Upper', 'Lower'}, {'Left', 'Right'})
SIDE = {'NE': 'W', 'SW': 'E', 'NW': 'N', 'SE': 'S'}


def corpus_root(arg=None):
    return arg or os.environ.get('JT_CORPUS') or DEFAULT_ROOT


class UF:
    """Union-find with parity (signal direction components)."""
    def __init__(s): s.p = {}; s.par = {}
    def add(s, a):
        if a not in s.p: s.p[a] = a; s.par[a] = 0
    def find(s, a):
        path = []
        p = s.p
        while p[a] != a: path.append(a); a = p[a]
        root = a
        for n in reversed(path):
            pn = p[n]
            if pn != root: s.par[n] ^= s.par[pn]
            p[n] = root
        return root, None
    def get(s, a):
        r, _ = s.find(a)
        return r, (s.par[a] if a != r else 0)
    def union(s, a, b, x):
        ra, pa = s.get(a); rb, pb = s.get(b)
        if ra == rb: return (pa ^ pb) == x
        s.p[ra] = rb; s.par[ra] = pa ^ pb ^ x
        return True


def load_tiles(game, root=None):
    """All tiles of PSG_<game> from the whole-map export (maps/PSG_<game>/*.json, key 'tiles')."""
    d = os.path.join(corpus_root(root), 'maps', 'PSG_%s' % game)
    files = sorted(glob.glob(os.path.join(d, '*.json')))
    if not files: raise SystemExit('no map export under %s' % d)
    tiles = {}
    for f in files:
        for t in json.load(open(f))['tiles']:
            tiles[(t['x'], t['y'])] = t
    return tiles


class World:
    def __init__(self, tiles):
        self.tiles = tiles
        self.nodes = {}; self.heads = {}; self.sw = set(); self.bh = set(); self.stations = []; self.depots = []
        for (x, y), t in tiles.items():
            tt = t['tile_type']
            if tt == 'station': self.stations.append((x, y)); continue
            if tt == 'rail' and t['kind'] in ('track', 'signals'):
                tr = t['tracks']
                self.nodes[(x, y)] = [(n, TEDGES[n]) for n in tr]
                s = set(tr)
                if len(s) >= 2 and not any(s == p for p in PARALLEL): self.sw.add((x, y))
            elif tt == 'rail' and t.get('kind') == 'depot': self.depots.append((x, y))
            elif tt == 'road' and t['kind'] == 'level_crossing':
                n = t['rail_axis']; self.nodes[(x, y)] = [(n, TEDGES[n])]
            elif tt == 'tunnelbridge' and t['transport'] == 'rail':
                d = t['direction']; fe = t['far_end']
                far = (fe['x'], fe['y']) if fe else None
                nm = 'X' if d in ('NE', 'SW') else 'Y'
                self.nodes[(x, y)] = [(nm, TEDGES[nm])]
                self.heads[(x, y)] = (d, far, t['kind'] == 'bridge')
                if t['kind'] == 'bridge': self.bh.add((x, y))
        self.edgeset = {p: set(e for _, es in ns for e in es) for p, ns in self.nodes.items()}
        self.build_dirs()

    def pname(self, pc): return self.nodes[pc[0]][pc[1]][0]
    def pedges(self, pc): return self.nodes[pc[0]][pc[1]][1]

    def across(self, pc, e):
        """Pieces joined to pc through its edge e: [(piece, edge)]. Tunnel/bridge heads jump to the far head."""
        pos = pc[0]; h = self.heads.get(pos)
        if h and h[0] == e:
            far = h[1]
            if far is not None and far in self.heads: return [((far, 0), self.heads[far][0])]
            return []
        q = (pos[0] + D[e][0], pos[1] + D[e][1])
        res = []
        qh = self.heads.get(q)
        for i, (nm, es) in enumerate(self.nodes.get(q, [])):
            if OPP[e] in es and not (qh and qh[0] == OPP[e]): res.append(((q, i), OPP[e]))
        return res

    def build_dirs(self):
        nodes = self.nodes
        uf = UF()
        for p, ns in nodes.items():
            for i in range(len(ns)): uf.add((p, i))
        contra = []
        ex = lambda pc, e: 1 if e == self.pedges(pc)[1] else 0
        side_first = {}
        for p, ns in nodes.items():
            for i in range(len(ns)):
                pc = (p, i)
                for e in ns[i][1]:
                    for (o, e2) in self.across(pc, e):
                        if not uf.union(pc, o, ex(pc, e) ^ ex(o, e2) ^ 1): contra.append((p, o[0]))
                    k = (p, e)
                    if k in side_first:
                        o = side_first[k]
                        if not uf.union(pc, o, ex(pc, e) ^ ex(o, e)): contra.append((p, p))
                    else: side_first[k] = pc
        self.uf = uf; self.contra = contra
        self.votes = defaultdict(set)
        for p, t in self.tiles.items():
            if t['tile_type'] == 'rail' and t.get('signals') and p in nodes:
                names = [n for n, _ in nodes[p]]
                for sg in t['signals']:
                    if sg['track'] in names:
                        self.votes[(p, names.index(sg['track']))].add(sg['exit_edge'])
        self.vlist = []
        for pc, v in self.votes.items():
            if len(v) != 1: continue
            e = next(iter(v)); s_req = 1 if e == self.pedges(pc)[0] else 0
            r, pr = uf.get(pc)
            self.vlist.append((pc[0], pc, s_req ^ pr, r))

    def signs(self, box):
        """Component sign by signal votes inside box."""
        x0, y0, x1, y1 = box
        cv = defaultdict(lambda: [0, 0])
        for p, pc, v, r in self.vlist:
            if x0 <= p[0] <= x1 and y0 <= p[1] <= y1: cv[r][v] += 1
        return {r: (0 if a >= b else 1) for r, (a, b) in cv.items()}


def box_lanes(W, box):
    """Candidate lanes: every piece edge crossing the box edge, in components with a switch (or a straight component that
    pairs with one within PAIR), with the save's own signal direction ('in'/'out'/'bidir'/None).
    Lane dict: pc, e, side, pos (x on N/S, y on E/W), dir, trk, id."""
    x0, y0, x1, y1 = box
    inb = lambda q: x0 <= q[0] <= x1 and y0 <= q[1] <= y1
    nodes = W.nodes
    pcs = [(p, i) for p in nodes if inb(p) for i in range(len(nodes[p]))]
    seen = {}; comps = []
    for pc in pcs:
        if pc in seen: continue
        cid = len(comps); st = [pc]; seen[pc] = cid; members = []
        while st:
            a = st.pop(); members.append(a)
            for e in W.pedges(a):
                for (o, e2) in W.across(a, e):
                    if inb(o[0]) and o not in seen: seen[o] = cid; st.append(o)
            for i in range(len(nodes[a[0]])):
                o = (a[0], i)
                if o != a and o not in seen and set(W.pedges(o)) & set(W.pedges(a)): seen[o] = cid; st.append(o)
        comps.append(members)
    hasswitch = [any(m[0] in W.sw or m[0] in W.bh for m in c) for c in comps]

    def crossings_of(members):
        res = []
        for pc in members:
            pos = pc[0]; h = W.heads.get(pos)
            for e in W.pedges(pc):
                if h and h[0] == e:
                    far = h[1]
                    if far is None: continue
                    if not inb(far): res.append((pc, e))
                    continue
                q = (pos[0] + D[e][0], pos[1] + D[e][1])
                if inb(q): continue
                qh = W.heads.get(q)
                if q in nodes and OPP[e] in W.edgeset[q] and not (qh and qh[0] == OPP[e]): res.append((pc, e))
        return res
    sign = W.signs((x0 - PAIR, y0 - PAIR, x1 + PAIR, y1 + PAIR))
    def exit_dir(pc):
        r, p = W.uf.get(pc)
        if r not in sign: return None
        return W.pedges(pc)[0] if (sign[r] ^ p) == 1 else W.pedges(pc)[1]
    bidir = set(pc for pc, v in W.votes.items() if len(v) > 1 and inb(pc[0]))
    lanekeys = set()
    def mklanes(members, start):
        ls = []
        for (pc, e) in crossings_of(members):
            if (pc[0], e) in lanekeys: continue
            lanekeys.add((pc[0], e))
            ed = exit_dir(pc)
            pos = pc[0][1] if e in ('NE', 'SW') else pc[0][0]
            d = None if ed is None else ('out' if ed == e else 'in')
            if pc in bidir: d = 'bidir'
            ls.append(dict(pc=pc, e=e, side=SIDE[e], pos=pos, dir=d, trk=W.pname(pc), id=start + len(ls)))
        return ls
    lanes = []
    for ci, c in enumerate(comps):
        if hasswitch[ci]: lanes += mklanes(c, len(lanes))
    nswc = len(lanes)
    for ci, c in enumerate(comps):
        if hasswitch[ci]: continue
        ls = mklanes(c, len(lanes))
        if not ls: continue
        ok = False
        for L in ls:
            if L['dir'] not in ('in', 'out'): continue
            for K in lanes[:nswc]:
                if K['side'] == L['side'] and K['dir'] in ('in', 'out') and K['dir'] != L['dir'] and abs(K['pos'] - L['pos']) - 1 <= PAIR: ok = True
        if ok: lanes += ls
    return lanes


class Net:
    """The piece graph inside a box: signal-directed pieces, free pieces both ways (the extractor orients them by routes)."""
    pass


def build_net(tiles, box):
    """Pieces, signals, signal-fixed exits and the directed piece graph S/P of the box (net.build2 without the anatomy base)."""
    W = World(tiles)
    x0, y0, x1, y1 = box
    N = Net(); N.W = W; N.box = box
    N.inb = lambda p: x0 <= p[0] <= x1 and y0 <= p[1] <= y1
    N.stations_in_box = [s for s in W.stations if N.inb(s)]
    N.lanes_all = box_lanes(W, box)
    N.sig = {}
    for p, t in W.tiles.items():
        if t['tile_type'] == 'rail' and t.get('signals') and p in W.nodes and N.inb(p):
            names = [n for n, _ in W.nodes[p]]
            for sg in t['signals']:
                if sg['track'] in names:
                    N.sig.setdefault((p, names.index(sg['track'])), []).append(sg)
    N.pieces = [(p, i) for p, ns in W.nodes.items() if N.inb(p) for i in range(len(ns))]
    contra_tiles = set(a for a, b in W.contra if N.inb(a))
    twoway = set(pc for pc, v in N.sig.items() if len(set(s['exit_edge'] for s in v)) > 1)
    # OpenTTD: every signal type except a plain path signal is one-way
    pathfree = set(pc for pc, v in N.sig.items() if all(s['type'] == 'path' for s in v))
    N.sig1 = {pc: v[0]['exit_edge'] for pc, v in N.sig.items() if pc not in twoway and pc not in pathfree}
    N.exits = {pc: ({N.sig1[pc]} if pc in N.sig1 else set(W.pedges(pc))) for pc in N.pieces}
    return N


def edges_of(N, exits):
    """Directed graph S[a] = [(b, exit edge of a)], P[b] = [(a, exit edge of a)] for the given exit sets."""
    W = N.W
    S = defaultdict(list); P = defaultdict(list)
    for a in N.pieces:
        for e in exits[a]:
            for (o, e2) in W.across(a, e):
                if not N.inb(o[0]): continue
                oe = [x for x in W.pedges(o) if x != e2]
                if oe and oe[0] in exits[o]:
                    S[a].append((o, e)); P[o].append((a, e))
    return S, P
