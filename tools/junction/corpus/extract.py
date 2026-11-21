#!/usr/bin/env python3
"""Hub extractor (X-B1): save -> topology, structure, shape, steps and lengths.  python3 -I extract.py <hub id> --out <dir>

One function per stage, the probes' rules (x1c, x5, x6 in wip/probes) with no hub-specific constants: the box and the lane
list come from hubs.tsv.  Stages: graph (X2 directions by routes), ports (X3), lines (X4 switch nodes, route sets),
crossings (X5), structure (X6), shape (X7), then the writers.  `run()` returns every intermediate for compare.py.
"""
import argparse, bisect, itertools, math, os, sys
from collections import defaultdict, deque
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import load

SIDES = 'NESW'
HEAD_PORT = {'N': 0, 'E': 2, 'S': 4, 'W': 6}            # heading of an out lane on that side (eighths clockwise from north)
HEAD_IN = {'N': 4, 'E': 6, 'S': 0, 'W': 2}              # heading of an in lane on that side
FACE = {'N': 'S', 'E': 'W', 'S': 'N', 'W': 'E'}         # side -> heading letter of an in port there
AXIS = {0: (0, -1), 2: (1, 0), 4: (0, 1), 6: (-1, 0)}
EIGHTH = {(0, -1): 0, (1, -1): 1, (1, 0): 2, (1, 1): 3, (0, 1): 4, (-1, 1): 5, (-1, 0): 6, (-1, -1): 7}
SIX = {'X': ('NE', 'SW'), 'Y': ('NW', 'SE'), 'Upper': ('NE', 'NW'), 'Lower': ('SW', 'SE'), 'Left': ('SW', 'NW'), 'Right': ('NE', 'SE')}


class R: pass            # result bag


def bfs(starts, adj):
    seen = set(starts); q = deque(seen)
    while q:
        a = q.popleft()
        for b in adj.get(a, ()):
            if b not in seen: seen.add(b); q.append(b)
    return seen


# ------------------------------------------------------------------ hubs.tsv
def read_hubs(path=None):
    path = path or os.path.join(HERE, 'hubs.tsv')
    rows = {}
    with open(path) as f:
        head = f.readline().rstrip('\n').split('\t')
        for l in f:
            if not l.strip() or l.startswith('#'): continue
            v = l.rstrip('\n').split('\t'); v += [''] * (len(head) - len(v))
            rows[v[0]] = dict(zip(head, v))
    return rows


def parse_lanes(s):
    """'N84:out N85:out ...' -> {(side, pos): dir}; blank -> None (derive)."""
    s = s.strip()
    if not s: return None
    d = {}
    for tok in s.split():
        a, b = tok.split(':'); d[(a[0], int(a[1:]))] = b
    return d


# ------------------------------------------------------------------ X3 ports (lane selection part comes first: the graph needs the lanes)
def choose_lanes(N, lane_spec):
    """The hub's lanes with directions: the hubs.tsv list (checkpoint, overrides the save's own signal direction) or, when
    blank, every candidate lane the save gives a direction."""
    notes = []
    allk = {(L['side'], L['pos']): L for L in N.lanes_all}
    if lane_spec is not None:
        lanes = []
        for k, d in lane_spec.items():
            if k not in allk: notes.append('lane %s%d in hubs.tsv is not a candidate lane of the box' % k); continue
            L = dict(allk[k]); L['dir'] = d; lanes.append(L)
        diff = [k for k, d in lane_spec.items() if k in allk and allk[k]['dir'] != d]
        if diff: notes.append("lanes whose direction in hubs.tsv differs from the save's own signal direction: " + ' '.join('%s%d' % k for k in sorted(diff)))
        return lanes, notes, 'hubs.tsv'
    lanes = []
    for L in N.lanes_all:
        if L['dir'] in ('in', 'out'): lanes.append(dict(L))
        else: notes.append('candidate lane %s%d dropped: save gives no direction (%s)' % (L['side'], L['pos'], L['dir']))
    notes.append('lanes derived from each lane\'s first signal walking inward, else the component vote: ' + ' '.join('%s%d:%s(%s)' % (L['side'], L['pos'], L['dir'], L['src']) for L in lanes))
    return lanes, notes, 'derived'


def ports(lanes, within=3):
    """Lanes crossing the box edge grouped into ports: same side and direction, gap <= within tiles.  Ports in reading order
    per side (N, E, S, W; N and S west to east, E and W north to south); lane numbers 1-based in reading order."""
    P = []
    for s in SIDES:
        for d in ('in', 'out'):
            ls = sorted([L for L in lanes if L['side'] == s and L['dir'] == d], key=lambda L: L['pos'])
            grp = []
            for L in ls:
                if grp and L['pos'] - grp[-1][-1]['pos'] - 1 <= within: grp[-1].append(L)
                else: grp.append([L])
            for g in grp: P.append(dict(side=s, dir=d, lanes=g))
    # reading order within a side: by first lane position
    P.sort(key=lambda p: (SIDES.index(p['side']), p['lanes'][0]['pos'], p['dir']))
    cnt = defaultdict(int)
    for p in P: cnt[(p['side'], p['dir'])] += 1
    seen = defaultdict(int)
    for p in P:
        seen[(p['side'], p['dir'])] += 1
        p['id'] = '%s-%s' % (p['side'], p['dir']) + (str(seen[(p['side'], p['dir'])]) if cnt[(p['side'], p['dir'])] > 1 else '')
        p['heading'] = FACE[p['side']] if p['dir'] == 'in' else p['side']
        for i, L in enumerate(p['lanes']):
            L['label'] = '%s%d' % (L['side'], L['pos']); L['port'] = p['id']; L['no'] = i + 1
        # lines left to right across travel: reading order for headings N and E, reversed for S and W
        p['across'] = list(p['lanes']) if p['heading'] in 'NE' else list(reversed(p['lanes']))
    return P


# ------------------------------------------------------------------ X2 graph
def graph(N, lanes):
    """Directions by routes (probe x1c): signal pieces keep their direction; every other piece starts passable both ways;
    rounds fix a piece to the one direction in which it is forward-reachable from an in lane and backward-reachable from an
    out lane (none: off-route), until nothing changes."""
    W = N.W
    G = R()
    RA = defaultdict(list); RB = defaultdict(list)
    for a in N.pieces:
        for e in W.pedges(a):
            for (o, e2) in W.across(a, e):
                if not N.inb(o[0]): continue
                oe = [x for x in W.pedges(o) if x != e2]
                if oe: RA[a].append((o, e, oe[0])); RB[o].append((a, e, oe[0]))
    ex = {pc: set(N.exits[pc]) for pc in N.pieces}
    def reach(starts, fwd):
        seen = set(starts); dq = deque(seen)
        while dq:
            a = dq.popleft()
            if fwd:
                for b, e, oe in RA[a]:
                    if b not in seen and e in ex[a] and oe in ex[b]: seen.add(b); dq.append(b)
            else:
                for b, e, oe in RB[a]:
                    if b not in seen and e in ex[b] and oe in ex[a]: seen.add(b); dq.append(b)
        return seen
    ins_pc = [L['pc'] for L in lanes if L['dir'] == 'in']; outs_pc = [L['pc'] for L in lanes if L['dir'] == 'out']
    freep = [p for p in N.pieces if p not in N.sig1]
    offroute = set(); rounds = []
    while True:
        fw0 = reach(ins_pc, True); bw0 = reach(outs_pc, False); ch = 0
        for p in freep:
            if len(ex[p]) != 2: continue
            if p not in fw0 or p not in bw0: ex[p] = set(); offroute.add(p); ch += 1; continue
            old = ex[p]; val = []
            for e in sorted(old):
                ex[p] = {e}
                if p in reach(ins_pc, True) and p in reach(outs_pc, False): val.append(e)
            ex[p] = old
            if len(val) == 1: ex[p] = {val[0]}; ch += 1
            elif len(val) == 0: ex[p] = set(); offroute.add(p); ch += 1
        rounds.append(ch)
        if not ch: break
    G.exits = ex; G.rounds = rounds; G.offroute = offroute
    G.ambiguous = sorted(p for p in freep if len(ex[p]) == 2)
    G.S, G.P = load.edges_of(N, ex)
    fw = bfs(ins_pc, {a: [b for b, _ in v] for a, v in G.S.items()})
    bw = bfs(outs_pc, {a: [b for b, _ in v] for a, v in G.P.items()})
    G.core = fw & bw
    G.CS = {a: sorted(set(b for b, e in G.S[a] if b in G.core)) for a in G.core}
    CP = defaultdict(list)
    for a in G.core:
        for b in G.CS[a]: CP[b].append(a)
    G.CP = CP
    G.n_free = len(freep); G.n_free_fixed = sum(1 for p in freep if len(ex[p]) == 1)
    return G


# ------------------------------------------------------------------ X4 routes, nodes, lines
def tl(p): return '%d,%d' % p[0]
def pl(N, p): return '%d,%d%s' % (p[0][0], p[0][1], N.W.pname(p)[:2])


def letters(n):
    s = ''
    while True:
        s = chr(65 + n % 26) + s; n = n // 26 - 1
        if n < 0: return s


def lines(N, G, lanes, P):
    """Route sets, switch nodes and lines (probe x1c).  A route is (in lane, out port); named movement letter + the in lane's
    number on its port.  Movement letters: A, B, ... over the (in port, out port) pairs that some route carries, in the order
    of the in ports then the out ports as listed in the ports section."""
    L = R(); core, CS, CP = G.core, G.CS, G.CP
    in_lanes = [x for x in lanes if x['dir'] == 'in']
    fw = {x['label']: bfs([x['pc']], CS) for x in in_lanes}
    bw = {p['id']: bfs([x['pc'] for x in p['lanes']], CP) for p in P if p['dir'] == 'out'}
    RS = {pc: frozenset((i, a) for i in fw for a in bw if pc in fw[i] and pc in bw[a]) for pc in core}
    inp = [p['id'] for p in P if p['dir'] == 'in']; outp = [p['id'] for p in P if p['dir'] == 'out']
    lab2lane = {x['label']: x for x in lanes}
    reached = {(lab2lane[i]['port'], a) for rs in RS.values() for (i, a) in rs}
    mv = [(i, o) for i in inp for o in outp if (i, o) in reached]
    L.movement = {m: letters(k) for k, m in enumerate(mv)}
    def rname(pair):
        i, a = pair; return L.movement[(lab2lane[i]['port'], a)] + str(lab2lane[i]['no'])
    L.rname = rname; L.RS = RS; L.fw = fw; L.bw = bw
    L.rset = lambda pcs: '+'.join(sorted({rname(x) for x in RS[pcs]}, key=lambda s: (s[:-len(s.lstrip('ABCDEFGHIJKLMNOPQRSTUVWXYZ'))], int(s.lstrip('ABCDEFGHIJKLMNOPQRSTUVWXYZ')))))
    isnode = {p for p in core if len(CS[p]) >= 2 or len(CP[p]) >= 2}
    L.isnode = isnode
    def kind(p):
        s, m = len(CS[p]) >= 2, len(CP[p]) >= 2
        return 'cross' if s and m else 'split' if s else 'merge'
    ninfo = {}
    for p in sorted(isnode):
        kd = kind(p); ar = (len(CP[p]), len(CS[p]))
        succ = frozenset(RS[b] for b in CS[p]); pred = frozenset(RS[a] for a in CP[p])
        if kd == 'split': fl = 'fray' if len(succ) > 1 else 'lane'
        elif kd == 'merge': fl = 'join' if len(pred) > 1 else 'lane'
        else: fl = ('fray' if len(succ) > 1 else 'lane') + '/' + ('join' if len(pred) > 1 else 'lane')
        ninfo[p] = (kd, fl, ar)
    L.ninfo = ninfo
    par = {p: p for p in core}
    def f(x):
        while par[x] != x: par[x] = par[par[x]]; x = par[x]
        return x
    for a in core:
        if a in isnode: continue
        for b in CS[a]:
            if b not in isnode: par[f(a)] = f(b)
    comp = defaultdict(list)
    for p in core:
        if p not in isnode: comp[f(p)].append(p)
    L.lines = sorted(comp.values(), key=lambda ps: min(ps))
    L.LID = {p: k for k, ps in enumerate(L.lines) for p in ps}
    L.bad = [k for k, ps in enumerate(L.lines) if len({RS[p] for p in ps}) > 1]
    # chains in travel order
    L.chains = []
    for ps in L.lines:
        S = set(ps)
        st = [p for p in ps if not any(q in S for q in CP[p])]
        if len(st) != 1: L.chains.append(None); continue
        ch = [st[0]]
        while True:
            nx = [b for b in CS[ch[-1]] if b in S]
            if len(nx) != 1 or nx[0] in ch: break
            ch.append(nx[0])
        L.chains.append(ch if len(ch) == len(ps) else None)
    L.lane_in = {x['pc']: x for x in lanes if x['dir'] == 'in'}
    L.lane_out = {x['pc']: x for x in lanes if x['dir'] == 'out'}
    # a line's boundary tiles (own end tile plus the node piece's tile beyond it), for matching to hand files
    def bnd(k):
        S = set(L.lines[k])
        st = [p for p in L.lines[k] if not any(q in S for q in CP[p])]
        en = [p for p in L.lines[k] if not any(q in S for q in CS[p])]
        a = set(); b = set()
        for p in st:
            a.add(p[0]); a.update(q[0] for q in CP[p])
        for p in en:
            b.add(p[0]); b.update(q[0] for q in CS[p])
        return a, b
    L.bounds = [bnd(k) for k in range(len(L.lines))]
    return L


# ------------------------------------------------------------------ X5 crossings
def mid(t, e):
    x, y = t; return {'NE': (x, y + .5), 'SW': (x + 1, y + .5), 'NW': (x + .5, y), 'SE': (x + .5, y + 1)}[e]
def ctr(t): return (t[0] + .5, t[1] + .5)
def dist(a, b): return ((a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2) ** .5
def unit(a, b):
    d = dist(a, b); return ((b[0] - a[0]) / d, (b[1] - a[1]) / d)


def crossings(N, G, Ln):
    """Geometry of every line as a polyline of piece segments (a tunnel/bridge is a jump head centre to head centre), each
    displaced sideways by 1e-6*(line+1); a crossing is a proper interior intersection of segments of two lines; one per line
    pair (first along the lower-numbered line); order by arc length; marks by marks.py's rule (D389); D390 (1): a node piece on
    a tunnel's middle tile is attached to its trunk line only."""
    W = N.W; CS, CP = G.CS, G.CP; isnode = Ln.isnode; lines_ = Ln.lines; chains = Ln.chains; LID = Ln.LID; ninfo = Ln.ninfo
    X5 = R(); X5.checks = []
    def ends(pc): return [mid(pc[0], e) for e in SIX[W.pname(pc)]]
    def shared(a, b):
        s = [p for p in ends(a) if p in ends(b)]; return s[0] if len(s) == 1 else None
    def isjump(a, b): return abs(a[0][0] - b[0][0]) + abs(a[0][1] - b[0][1]) > 1
    def segs_of(k):
        ch = chains[k]; n = len(ch); res = []
        pre = list(CP[ch[0]]); nxt = list(CS[ch[-1]])
        for i, pc in enumerate(ch):
            E = ends(pc); c = ctr(pc[0])
            injump = i > 0 and isjump(ch[i - 1], pc)
            outjump = i + 1 < n and isjump(pc, ch[i + 1])
            if outjump: ex = None
            elif i + 1 < n: ex = shared(pc, ch[i + 1])
            elif nxt:
                sh = {shared(pc, b) for b in nxt}; ex = sh.pop() if len(sh) == 1 and None not in sh else None
            else: ex = None
            if injump: en = None
            elif i > 0: en = shared(ch[i - 1], pc)
            elif pre:
                sh = {shared(a, pc) for a in pre}; en = sh.pop() if len(sh) == 1 and None not in sh else None
            else: en = None
            if ex is None and not outjump: ex = [p for p in E if p != en][0] if en else E[1]
            if en is None and not injump: en = [p for p in E if p != ex][0]
            if en is not None and ex is not None and en == ex: raise ValueError('entry equals exit at %s' % tl(pc))
            if injump and outjump: raise ValueError('tunnel head both ways at %s' % tl(pc))
            if injump:
                if ex: res.append((c, ex, 'g', pc))
            elif outjump: res.append((en, c, 'g', pc))
            else: res.append((en, ex, 'g', pc))
            if outjump: res.append((c, ctr(ch[i + 1][0]), 'j', (pc, ch[i + 1])))
        return res
    def displace(pts, d):
        segn = []
        for a, b in zip(pts, pts[1:]):
            Lg = dist(a, b); dx, dy = (b[0] - a[0]) / Lg, (b[1] - a[1]) / Lg; segn.append((-dy, dx))
        o = []
        for i, p in enumerate(pts):
            if i == 0: n = segn[0]; o.append((p[0] + d * n[0], p[1] + d * n[1])); continue
            if i == len(pts) - 1: n = segn[-1]; o.append((p[0] + d * n[0], p[1] + d * n[1])); continue
            n1, n2 = segn[i - 1], segn[i]; kk = d / (1 + n1[0] * n2[0] + n1[1] * n2[1])
            o.append((p[0] + kk * (n1[0] + n2[0]), p[1] + kk * (n1[1] + n2[1])))
        return o
    X5.ends, X5.shared, X5.ctr, X5.segs_of = ends, shared, ctr, segs_of
    EXT = {}
    for p in isnode:
        kd = ninfo[p][0]
        if kd == 'split': ls = {LID.get(a) for a in CP[p]}; pos = 'end'
        elif kd == 'merge': ls = {LID.get(b) for b in CS[p]}; pos = 'start'
        else: continue
        if len(ls) == 1 and None not in ls: EXT[ls.pop()] = (pos, p)
    SEG = {}; ARC = {}; RAWPTS = {}; skipped = []
    for k, ch in enumerate(chains):
        if not ch: skipped.append((k, 'no simple chain')); continue
        try:
            s = segs_of(k)
            if k in EXT:
                pos, nd = EXT[k]
                if pos == 'end':
                    sh = shared(ch[-1], nd)
                    if sh is not None: s.append((sh, [p for p in ends(nd) if p != sh][0], 'g', nd))
                else:
                    sh = shared(nd, ch[0])
                    if sh is not None: s.insert(0, ([p for p in ends(nd) if p != sh][0], sh, 'g', nd))
            pts = [s[0][0]] + [x[1] for x in s]
            for i in range(len(s) - 1):
                if dist(s[i][1], s[i + 1][0]) > 1e-9: raise ValueError('discontinuous polyline at %s' % tl(s[i][3] if s[i][2] == 'g' else s[i][3][0]))
        except (ValueError, IndexError) as e:
            skipped.append((k, str(e))); continue
        RAWPTS[k] = {(round(p[0], 6), round(p[1], 6)) for p in pts}
        dp = displace(pts, 1e-6 * (k + 1))
        s = [(dp[i], dp[i + 1], s[i][2], s[i][3]) for i in range(len(s))]
        SEG[k] = s
        a = [0.0]
        for x, y, kd, pc in s: a.append(a[-1] + dist(x, y))
        ARC[k] = a
    X5.skipped = skipped; X5.SEG, X5.ARC, X5.EXT = SEG, ARC, EXT
    def proper(p1, p2, q1, q2):
        r = (p2[0] - p1[0], p2[1] - p1[1]); q = (q2[0] - q1[0], q2[1] - q1[1])
        den = r[0] * q[1] - r[1] * q[0]
        if abs(den) < 1e-12: return None
        wv = (q1[0] - p1[0], q1[1] - p1[1])
        t = (wv[0] * q[1] - wv[1] * q[0]) / den; u = (wv[0] * r[1] - wv[1] * r[0]) / den
        e = 1e-9
        if e < t < 1 - e and e < u < 1 - e: return t, u
        return None
    def sat(k, i, t): return ARC[k][i] + t * (ARC[k][i + 1] - ARC[k][i])
    hits = defaultdict(list); cnt = defaultdict(int)
    items = [(k, i) for k in SEG for i in range(len(SEG[k]))]
    for (k1, i1), (k2, i2) in itertools.combinations(items, 2):
        if k1 == k2: continue
        a = SEG[k1][i1]; b = SEG[k2][i2]
        node_a = a[2] == 'g' and a[3] in isnode; node_b = b[2] == 'g' and b[3] in isnode
        if a[2] == 'g' and b[2] == 'g': ty = 'a'
        elif a[2] == 'j' and b[2] == 'j': ty = 'c'
        else: ty = 'b'
        if min(a[0][0], a[1][0]) > max(b[0][0], b[1][0]) or min(b[0][0], b[1][0]) > max(a[0][0], a[1][0]): continue
        r = proper(a[0], a[1], b[0], b[1])
        if not r: continue
        t, u = r
        pt = (a[0][0] + t * (a[1][0] - a[0][0]), a[0][1] + t * (a[1][1] - a[0][1]))
        shp = RAWPTS[k1] & RAWPTS[k2]
        if any(dist(pt, q) < 1e-3 for q in shp): cnt['spur'] += 1; continue
        if ty == 'b' and (node_a or node_b): ty = 'n'
        if ty == 'a' and (node_a or node_b): ty = 'na'
        hits[frozenset((k1, k2))].append((k1, sat(k1, i1, t), k2, sat(k2, i2, u), pt, unit(a[0], a[1]), unit(b[0], b[1]), ty))
        cnt[ty] += 1
    # bridges: a jump over a bridge span is counted as a jump (same geometry); kinds are split in the report
    X = {}; multi = []
    for ks, hl in hits.items():
        k1, k2 = sorted(ks)
        hl2 = sorted([h if h[0] == k1 else (h[2], h[3], h[0], h[1], h[4], h[6], h[5], h[7]) for h in hl], key=lambda h: h[1])
        if len(hl2) > 1: multi.append((k1, k2, [h[4] for h in hl2]))
        X[ks] = hl2[0]
    def mk(dL, dM):
        dot = dM[0] * dL[1] + dM[1] * (-dL[0]); return '>' if dot < 0 else '<'
    EX = defaultdict(list)
    for ks, h in X.items():
        k1, s1, k2, s2, pt, d1, d2, ty = h
        EX[k1].append((s1, k2, mk(d1, d2), ty)); EX[k2].append((s2, k1, mk(d2, d1), ty))
    for k in EX: EX[k].sort()
    X5.X, X5.EX, X5.multi, X5.cnt = X, EX, multi, cnt
    node_seg = {p: (ends(p)[0], ends(p)[1]) for p in isnode}
    lvl = []
    for p, (n0, n1) in node_seg.items():
        for k in SEG:
            for sg in SEG[k]:
                if sg[2] == 'g' and proper(n0, n1, sg[0], sg[1]): lvl.append((tl(p), 'line %d' % (k + 1)))
    for p, q in itertools.combinations(node_seg, 2):
        if proper(*node_seg[p], *node_seg[q]): lvl.append((tl(p), tl(q)))
    X5.level_nodes = lvl
    X5.level_lines = [(k1, k2, h[4]) for ks, hl in hits.items() for h in hl for k1, k2 in [tuple(sorted(ks))] if h[7] in ('a', 'na')]
    X5.same_set = [(a, b, X[frozenset((a, b))][4]) for a, b in (tuple(sorted(ks)) for ks in X)
                   if Ln.RS[lines_[a][0]] == Ln.RS[lines_[b][0]]]
    return X5


# ------------------------------------------------------------------ X6 structure
def head(a, b):
    dx, dy = b[0] - a[0], b[1] - a[1]
    sx = 0 if abs(dx) < 1e-4 else (1 if dx > 0 else -1); sy = 0 if abs(dy) < 1e-4 else (1 if dy > 0 else -1)
    if sx and sy and abs(abs(dx) - abs(dy)) > 1e-4: raise ValueError('segment is not an eighth heading: %s -> %s' % (a, b))
    return EIGHTH[(sx, sy)]


def structure(N, G, Ln, X5, lanes):
    """Tunnels and bridges (a jump of a line; separator = that line, crossers = lines crossing it, in arc order), groups
    (same heading and length, entrance tiles side by side, closed transitively) and the straight leg at every node piece."""
    S6 = R(); S6.head = {}; W = N.W; SEG, ARC, X, CS, CP = X5.SEG, X5.ARC, X5.X, G.CS, G.CP; isnode, ninfo, LID = Ln.isnode, Ln.ninfo, Ln.LID
    jumps = {}
    for k in SEG:
        for i, sg in enumerate(SEG[k]):
            if sg[2] == 'j': jumps[(k, i)] = (sg[3][0][0], sg[3][1][0])
    def segidx(k, s):
        i = bisect.bisect_right(ARC[k], s) - 1; return min(max(i, 0), len(SEG[k]) - 1)
    S6.segidx = segidx
    mem = defaultdict(list)
    for ks, h in X.items():
        k1, s1, k2, s2 = h[0], h[1], h[2], h[3]
        i1, i2 = segidx(k1, s1), segidx(k2, s2)
        if (k1, i1) in jumps: mem[(k1, i1)].append((s1, k2))
        if (k2, i2) in jumps: mem[(k2, i2)].append((s2, k1))
    ties = 0; T = {}
    for key, hd in jumps.items():
        lst = sorted(mem.get(key, []))
        for a, b in zip(lst, lst[1:]):
            if abs(a[0] - b[0]) < 1e-9: ties += 1
        if not W.heads[hd[0]][2]:       # a tunnel: a crossing on one of its portal tiles is another, deeper tunnel passing under the portal
            portals = {tuple(hd[0]), tuple(hd[1])}
            lst = [(s, c) for s, c in lst if (math.floor(X[frozenset((key[0], c))][4][0]), math.floor(X[frozenset((key[0], c))][4][1])) not in portals]
        T[hd[0]] = (key[0], [c for _, c in lst], hd[1], W.heads[hd[0]][2])      # entrance -> (separator, crossers, exit, is_bridge)
    S6.ties = ties; S6.T = T
    S6.order = sorted(T, key=lambda t: (t[1], t[0]))          # numbering: entrance tile by (y, x), as the 197 table
    def tinfo(hd):
        a, b = hd, T[hd][2]; d = (b[0] - a[0], b[1] - a[1])
        return (d[0] > 0) - (d[0] < 0), (d[1] > 0) - (d[1] < 0), abs(d[0]) + abs(d[1])
    def adj(a, b):
        ia, ib = tinfo(a), tinfo(b)
        if ia != ib or T[a][3] != T[b][3]: return False
        dx, dy, _ = ia
        return (dx == 0 and a[1] == b[1] and abs(a[0] - b[0]) == 1) or (dy == 0 and a[0] == b[0] and abs(a[1] - b[1]) == 1)
    par = {h: h for h in T}
    def f(x):
        while par[x] != x: par[x] = par[par[x]]; x = par[x]
        return x
    withc = [h for h, v in T.items() if v[1]]
    for a in withc:
        for b in withc:
            if a < b and adj(a, b): par[f(a)] = f(b)
    gm = defaultdict(set)
    for h in withc: gm[f(h)].add(h)
    S6.groups = [frozenset(v) for v in gm.values() if len(v) >= 2]
    # straight legs
    ports_in = {x['pc']: x['side'] for x in lanes if x['dir'] == 'in'}; ports_out = {x['pc']: x['side'] for x in lanes if x['dir'] == 'out'}
    def offnode(k): return [sg for sg in SEG[k] if not (sg[2] == 'g' and sg[3] in isnode)]
    def hd0(sg): return head(sg[0], sg[1])
    nodes = []
    for p in sorted(isnode, key=lambda p: p[0]):
        kd = ninfo[p][0]
        if kd == 'cross': nodes.append((p, kd, None, (), ('skip', 'cross node'))); continue
        tr_p, lg_p = (CP[p], CS[p]) if kd == 'split' else (CS[p], CP[p])
        trs = {LID.get(a) for a in tr_p}; lgs = tuple(sorted(LID.get(b, -1) for b in lg_p))
        if len(trs) > 1 or len(tr_p) > 1 and None in trs:
            nodes.append((p, kd, None, lgs, ('und', 'several trunk pieces'))); continue
        tk = trs.pop() if trs else None
        ends_, shared_ = X5.ends, X5.shared
        def nodehead(q, toward_n):
            """Heading of node piece q in its travel direction, next to node piece p (q precedes p when toward_n)."""
            sh = shared_(q, p)
            if sh is None: raise ValueError('node piece %s has no single shared end with %s' % (tl(q), tl(p)))
            other = [e for e in ends_(q) if e != sh][0]
            return head(other, sh) if toward_n else head(sh, other)
        try:
            if tk is not None:
                if tk not in SEG: nodes.append((p, kd, tk, lgs, ('und', 'trunk line has no geometry'))); continue
                sg = offnode(tk)
                if not sg: nodes.append((p, kd, tk, lgs, ('und', 'trunk has no off-node piece'))); continue
                h = hd0(sg[-1] if kd == 'split' else sg[0])
            elif tr_p:
                tk = -1; h = nodehead(tr_p[0], kd == 'split')
            else:
                if kd == 'split' and p in ports_in: h = HEAD_IN[ports_in[p]]
                elif kd == 'merge' and p in ports_out: h = HEAD_PORT[ports_out[p]]
                else: nodes.append((p, kd, None, lgs, ('und', 'no trunk'))); continue
            S6.head[p] = h
            ok = []; bad = None
            for b in lg_p:
                if b in isnode:
                    if nodehead(b, kd == 'merge') == h: ok.append(('e', p, b) if kd == 'split' else ('e', b, p))
                    continue
                lk = LID[b]
                if lk not in SEG: bad = 'leg line has no geometry'; break
                sg = offnode(lk)
                if not sg: bad = 'leg has no off-node piece'; break
                if hd0(sg[0] if kd == 'split' else sg[-1]) == h: ok.append(lk)
        except ValueError as e:
            nodes.append((p, kd, tk, lgs, ('und', str(e)))); continue
        if bad: nodes.append((p, kd, tk, lgs, ('und', bad)))
        else: nodes.append((p, kd, tk, lgs, ('straight', ok[0]) if len(ok) == 1 else ('none', None) if not ok else ('und', 'several legs keep heading')))
    S6.nodes = nodes
    return S6


# ------------------------------------------------------------------ X7 shape (+ lengths)
def lift(hs):
    o = []
    for h in hs:
        if not o: o.append(h); continue
        t = (h - o[-1]) % 8
        if t > 4: t -= 8
        o.append(o[-1] + t)
    return o


def seg_len(a, b):
    """Half tiles: an axis tile 2 (head to centre 1, a jump 2 per tile centre to centre), a diagonal piece 1."""
    return 1 if head(a, b) % 2 else int(round(2 * dist(a, b)))


def shape(N, G, Ln, X5, S6, lanes):
    """Per line: its raw lifted headings (a run of equal heading = one piece), the piece each crossing sits on, the lengths
    of the raw pieces, then the projection + steps split (split_shape.py, D396).  Lines without a chain/geometry are listed."""
    Sh = R(); CS, CP, isnode, LID = G.CS, G.CP, Ln.isnode, Ln.LID
    SEG, EX, segidx = X5.SEG, X5.EX, S6.segidx
    ends, shared, segs_of = X5.ends, X5.shared, X5.segs_of
    ports_out = {x['pc']: x['side'] for x in lanes if x['dir'] == 'out'}
    def shape_segs(k):
        s = segs_of(k); ch = Ln.chains[k]; extra = 0
        pre = [a for a in CP[ch[0]] if a in isnode]; nxt = [b for b in CS[ch[-1]] if b in isnode]
        if len(pre) == 1 and len(CP[ch[0]]) == 1 and Ln.ninfo[pre[0]][0] != 'split':   # a split's node piece is its trunk's last piece, not a leg's first
            nd = pre[0]; sh = shared(nd, ch[0])
            if sh is not None: s.insert(0, ([q for q in ends(nd) if q != sh][0], sh, 'g', nd))
        if len(nxt) == 1 and len(CS[ch[-1]]) == 1 and Ln.ninfo[nxt[0]][0] != 'merge':   # a merge's node piece is its out line's first piece, not a leg's last
            nd = nxt[0]; sh = shared(ch[-1], nd)
            if sh is not None: s.append((sh, [q for q in ends(nd) if q != sh][0], 'g', nd))
        return s
    key_of = lambda sg: (sg[2], sg[3])
    ES = {}; notes = []; Sh.tilelen = {}
    for k in sorted(SEG):
        try:
            ss = shape_segs(k)
            runs = []; rlen = []; segrun = []
            for a, b, kd, pc in ss:
                h = head(a, b); ln = seg_len(a, b)
                if runs and runs[-1] == h: rlen[-1] += ln
                else: runs.append(h); rlen.append(ln)
                segrun.append(len(runs) - 1)
        except ValueError as e:
            notes.append((k, str(e))); continue
        raw = lift(runs); lens = list(rlen)
        # independent tile-path length: pieces by name (axis 2, diagonal 1), tunnel middle tiles 2 each
        tp = 0; ch = Ln.chains[k]; pcs = list(ch)
        for i, pc in enumerate(ch):
            tp += 2 if N.W.pname(pc) in ('X', 'Y') else 1
            if i + 1 < len(ch):
                d = abs(pc[0][0] - ch[i + 1][0][0]) + abs(pc[0][1] - ch[i + 1][0][1])
                if d > 1: tp += 2 * (d - 1)
        keyidx = {key_of(sg): i for i, sg in enumerate(ss)}
        chs = set(ch)       # an end node piece counts once per use (a loop back to the same node piece uses it twice)
        for sg in ss:
            if sg[2] == 'g' and sg[3] in isnode and sg[3] not in chs: tp += 2 if N.W.pname(sg[3]) in ('X', 'Y') else 1
        at = []
        for s_, o, m, ty in EX[k]:
            i = segidx(k, s_); sg = SEG[k][i]
            at.append((o, segrun[keyidx[key_of(sg)]], m))
        if ch[-1] in ports_out and not CS[ch[-1]]:
            P_ = HEAD_PORT[ports_out[ch[-1]]]; hh = raw[-1]
            if (hh - P_) % 8:
                for d in (-1, 1):
                    if (hh + d - P_) % 8 == 0: raw.append(hh + d); lens.append(0); break
                else: notes.append((k, 'port end needs more than one eighth'))
        ends_ = [p for _, p, _ in at] + [len(raw) - 1]
        proj, newp, steps = [raw[0]], [], {}
        start = 0
        for kk, e in enumerate(ends_):
            seg = raw[start:e + 1]; to = raw[e]
            wig = sum(abs(seg[i + 1] - seg[i]) for i in range(len(seg) - 1))
            if wig > abs(to - raw[start]): steps[kk] = seg
            while proj[-1] != to: proj.append(proj[-1] + (1 if to > proj[-1] else -1))
            if kk + 1 < len(ends_): newp.append(len(proj) - 1)
            start = e
        ES[k] = (raw, at, proj, newp, steps)
        Sh.tilelen[k] = (sum(lens), tp)
        if not hasattr(Sh, 'lens'): Sh.lens = {}
        Sh.lens[k] = lens
    Sh.ES = ES; Sh.notes = notes
    return Sh


# ------------------------------------------------------------------ text writers
def line_names(Ln):
    """L1.. for the real lines (index + 1); empty lines (no pieces) are named later in creation order."""
    return {('r', k): 'L%d' % (k + 1) for k in range(len(Ln.lines))}


def find_cycle(stm, names):
    """The line graph (each node statement: every in line -> every out line) must be acyclic.  Returns the names of one cycle
    as a list that starts and ends with the same line, or None."""
    nxt = defaultdict(list)
    for kd, ins, outs, p in stm:
        for a in ins:
            for b in outs: nxt[a].append(b)
    state = {}; path = []
    def dfs(a):
        state[a] = 1; path.append(a)
        for b in nxt[a]:
            if state.get(b) == 1: return [names[x] for x in path[path.index(b):]] + [names[b]]
            if b not in state:
                c = dfs(b)
                if c: return c
        state[a] = 2; path.pop(); return None
    for a in list(nxt):
        if a not in state:
            c = dfs(a)
            if c: return c
    return None


def build_text_model(N, G, Ln, P, lanes, S6, X5):
    """Lines (real and empty), node statements (a 3-way or cross piece becomes a cascade) and straight statements.
    Empty lines carry no track: an in/out lane that is itself a node piece, an edge between two node pieces, a cascade's
    middle line."""
    T = R(); CS, CP, isnode, LID, RS = G.CS, G.CP, Ln.isnode, Ln.LID, Ln.RS
    names = line_names(Ln); T.names = names; T.empty = []; T.heading = {}
    routes = {}                                   # key -> frozenset of route names
    for k, ps in enumerate(Ln.lines): routes[('r', k)] = frozenset(Ln.rname(x) for x in RS[ps[0]])
    nextid = [len(Ln.lines)]
    def newline(key, rs):
        nextid[0] += 1; names[key] = 'L%d' % nextid[0]; routes[key] = frozenset(rs); T.empty.append(key); return key
    W = N.W
    def edge_line(a, b):
        key = ('e', a, b)
        if key not in names:
            newline(key, {Ln.rname(x) for x in RS[a] & RS[b]})
            sh = shared(a, b)
            if sh is None: raise ValueError('empty line between %s and %s: no single shared end' % (tl(a), tl(b)))
            T.heading[key] = head([q for q in ends(a) if q != sh][0], sh)
        return key
    ends, shared = X5.ends, X5.shared
    def dirvec(a, b):
        d = dist(a, b); return ((b[0] - a[0]) / d, (b[1] - a[1]) / d)
    def split_order(n, legs):
        """legs (pieces) left to right across travel."""
        tr = CP[n][0] if CP[n] else None
        E = ends(n)
        if tr is not None:
            sh = shared(tr, n)
            dt = dirvec(sh, [q for q in E if q != sh][0]) if sh is not None else None
        else:
            lane = next((x for x in lanes if x['pc'] == n and x['dir'] == 'in'), None)
            dt = AXIS[HEAD_IN[lane['side']]] if lane else None
        sc = []
        for b in legs:
            exit_pt = [q for q in E if q in ends(b)]
            if len(exit_pt) != 1 or dt is None: sc.append(0.0); continue
            far = [q for q in ends(b) if q != exit_pt[0]][0]
            d = dirvec(exit_pt[0], far); left = (dt[1], -dt[0])
            sc.append(d[0] * left[0] + d[1] * left[1])
        order = sorted(range(len(legs)), key=lambda i: -sc[i])
        return [legs[i] for i in order], len(set(round(s, 6) for s in sc)) < len(sc)
    def merge_order(n, srcs):
        out = CS[n][0] if CS[n] else None
        E = ends(n)
        if out is None: return list(srcs), True
        sh = shared(n, out)
        if sh is None: return list(srcs), True
        do = dirvec([q for q in E if q != sh][0] if False else sh, [q for q in ends(out) if q != sh][0]); left = (do[1], -do[0])
        sc = []
        for a in srcs:
            far = [q for q in ends(a) if q not in E]
            far = far[0] if far else ends(a)[0]
            ref = [q for q in ends(a) if q in E]
            ref = ref[0] if ref else sh
            sc.append((far[0] - ref[0]) * left[0] + (far[1] - ref[1]) * left[1])
        order = sorted(range(len(srcs)), key=lambda i: -sc[i])
        return [srcs[i] for i in order], len(set(round(s, 6) for s in sc)) < len(sc)
    def line_of_in(a, n):            # the line carrying the edge a -> n (a precedes node n)
        return ('r', LID[a]) if a not in isnode else edge_line(a, n)
    def line_of_out(n, b):
        return ('r', LID[b]) if b not in isnode else edge_line(n, b)
    def cascade_head(n):
        if n not in S6.head: raise ValueError('cascade middle line at %s: the node piece has no trunk heading (%s)' % (tl(n), sres[n][4][1]))
        return S6.head[n]
    port_in_line = {}; port_out_line = {}
    for x in lanes:
        pc = x['pc']
        if x['dir'] == 'in' and pc in G.core:
            if pc in isnode:
                key = ('pi', pc); newline(key, {Ln.rname(r) for r in RS[pc]}); port_in_line[x['label']] = key; T.heading[key] = HEAD_IN[x['side']]
            else: port_in_line[x['label']] = ('r', LID[pc])
        if x['dir'] == 'out' and pc in G.core:
            if pc in isnode:
                key = ('po', pc); newline(key, {Ln.rname(r) for r in RS[pc]}); port_out_line[x['label']] = key; T.heading[key] = HEAD_PORT[x['side']]
            else: port_out_line[x['label']] = ('r', LID[pc])
    stm = []; T.ties = []; T.cascades = []      # statements: ('split'|'merge', in lines, out lines, piece)
    straight = {}                               # statement index -> leg key | None | ('und', why)
    sres = {e[0]: e for e in S6.nodes}
    def un(rs_keys): return set().union(*[routes[k] for k in rs_keys])
    def add_stm(kd, ins, outs, p):
        stm.append((kd, ins, outs, p)); return len(stm) - 1
    T.straight = []                              # (trunk key, leg key | None | ('und', why))
    for n in sorted(isnode, key=lambda p: p[0]):
        kd = Ln.ninfo[n][0]
        res = sres[n][4]
        def legkey(k): return k if isinstance(k, tuple) else ('r', k)
        if kd == 'split':
            trunk = line_of_in(CP[n][0], n) if CP[n] else port_in_line.get(next((x['label'] for x in lanes if x['pc'] == n and x['dir'] == 'in'), None))
            legs_p, tie = split_order(n, list(CS[n]))
            legs = [line_of_out(n, b) for b in legs_p]
            if tie: T.ties.append(('split', tl(n), 'legs have equal sideways sense'))
            if len(legs) > 2: T.cascades.append(('3-way split' if len(legs) == 3 else '%d-way split' % len(legs), tl(n)))
            sleg = None
            if res[0] == 'straight': sleg = legkey(res[1])
            elif res[0] == 'none': sleg = None
            else: sleg = ('und', res[1])
            rem = list(legs); cur = trunk; cur_s = sleg
            while len(rem) > 2:
                r = rem.pop()
                m = newline(('c', n, len(rem)), un(rem)); T.heading[m] = cascade_head(n)
                add_stm('split', [cur], [m, r], n)
                if cur_s is None or (isinstance(cur_s, tuple) and cur_s[0] == 'und'): T.straight.append((cur, cur_s))
                elif cur_s in rem: T.straight.append((cur, m))
                else: T.straight.append((cur, r)); cur_s = None
                cur = m
            add_stm('split', [cur], rem, n)
            T.straight.append((cur, cur_s if (cur_s is None or (isinstance(cur_s, tuple) and cur_s[0] == 'und') or cur_s in rem) else None))
        elif kd == 'merge':
            srcs_p, tie = merge_order(n, list(CP[n]))
            ins = [line_of_in(a, n) for a in srcs_p]
            if tie: T.ties.append(('merge', tl(n), 'sources have equal sideways sense'))
            if CS[n]: out = line_of_out(n, CS[n][0])
            else: out = port_out_line.get(next((x['label'] for x in lanes if x['pc'] == n and x['dir'] == 'out'), None))
            if len(ins) > 2: T.cascades.append(('%d-way merge' % len(ins), tl(n)))
            sleg = None
            if res[0] == 'straight': sleg = legkey(res[1])
            elif res[0] == 'none': sleg = None
            else: sleg = ('und', res[1])
            rem = list(ins); cur_s = sleg
            # peel pairs from the left into a middle line, the last statement keeps the node's out line
            while len(rem) > 2:
                a = rem.pop(0); b = rem.pop(0)
                m = newline(('c', n, len(rem)), un([a, b])); T.heading[m] = cascade_head(n)
                add_stm('merge', [a, b], [m], n)
                if cur_s in (a, b): T.straight.append((m, cur_s)); cur_s = m
                else: T.straight.append((m, None))
                rem.insert(0, m)
            add_stm('merge', rem, [out], n)
            T.straight.append((out, cur_s))
        else:
            ins_p, t1 = merge_order(n, list(CP[n]))
            ins = [line_of_in(a, n) for a in ins_p]
            legs_p, t2 = split_order(n, list(CS[n]))
            legs = [line_of_out(n, b) for b in legs_p]
            m = newline(('x', n), un(ins)); raise ValueError('cross middle line at %s: no rule for its heading' % tl(n))
            add_stm('merge', ins, [m], n); T.straight.append((m, ('und', 'cross node')))
            add_stm('split', [m], legs, n); T.straight.append((m, ('und', 'cross node')))
            T.cascades.append(('cross node written as a merge then a split', tl(n)))
    T.cycle = find_cycle(stm, names)
    T.routes = routes; T.stm = stm; T.port_in_line = port_in_line; T.port_out_line = port_out_line
    return T


def write_topology(path, hub, N, G, Ln, P, lanes, T, X5):
    names, routes = T.names, T.routes
    out = []; w = out.append
    w('topology %s' % hub['name'].replace(' ', ''))
    w('# Extracted by tools/junction/corpus/extract.py (X-B1) from PSG %s, box %s; lanes from %s.' % (hub['game'], hub['box'], hub.get('_lanesrc', '?')))
    w('# Ports: per side in reading order (N and S west to east, E and W north to south); each port lists its lines left to right across travel.')
    w('# Lane numbers are 1-based in reading order on each port: %s.' % '; '.join('%s %s' % (p['id'], ' '.join('%s=%d' % (x['label'], x['no']) for x in p['lanes'])) for p in P))
    w('# Routes: a movement letter per (in port, out port) pair, A, B, ... in the order of the in ports then the out ports as listed in the ports section, plus the in lane\'s number on its port.')
    w('# Movements: %s.' % '; '.join('%s: %s -> %s' % (l, i, o) for (i, o), l in Ln.movement.items()))
    w('# Lines L1.. are named by index: lines with track first (by their first piece), then empty lines (no track: a lane that is itself a node piece, an edge between two node pieces, a cascade or cross middle line). Leg order at a node is left to right across travel in the x east, y south frame.')
    seen = set()
    def nm(key):
        n = names[key]
        if key in seen: return n
        seen.add(key); return '%s=%s' % (n, '+'.join(sorted(routes[key], key=rkey)))
    def rkey(s):
        i = 0
        while s[i].isalpha(): i += 1
        return (s[:i], int(s[i:]))
    def port_line(x):
        return (T.port_in_line if x['dir'] == 'in' else T.port_out_line).get(x['label'])
    for p in P:
        keys = [port_line(x) for x in p['across']]
        w('port %s %s %s: %s' % (p['side'], p['dir'], p['heading'], ', '.join(nm(k) if k else '?' for k in keys)))
    for kd, ins, outs, piece in T.stm:
        w('node %s -> %s' % (', '.join(nm(k) for k in ins), ', '.join(nm(k) for k in outs)))
    w('# Crossings (D389): each line\'s crossings in travel order, mark > = the partner arrives from the line\'s left (marks.py).')
    for k in sorted(X5.EX):
        if not X5.EX[k]: continue
        w('cross %s: %s' % (names[('r', k)], ', '.join('%s%s' % (names[('r', o)], m) for s_, o, m, t in X5.EX[k])))
    open(path, 'w').write('\n'.join(out) + '\n')


def write_structure(path, hub, G, Ln, S6, T):
    names = T.names; out = []; w = out.append
    w('# %s: structure decisions (D389) extracted by tools/junction/corpus/extract.py; tunnel numbers by entrance tile (y, x).' % hub['name'])
    tun = [t for t in S6.order if not S6.T[t][3]]
    num = {t: i + 1 for i, t in enumerate(tun)}
    for t in tun: w('# T%d %d,%d -> %d,%d' % (num[t], t[0], t[1], S6.T[t][2][0], S6.T[t][2][1]))
    for t in tun:
        sep, cr, ex, br = S6.T[t]
        w('tunnel T%d %s: %s' % (num[t], names[('r', sep)], ', '.join(names[('r', c)] for c in cr)))
    gs = sorted([sorted(g, key=lambda t: (t[1], t[0])) for g in S6.groups if all(t in num for t in g)], key=lambda g: (g[0][1], g[0][0]))
    for i, g in enumerate(gs): w('group G%02d: %s' % (i + 1, ', '.join('T%d' % num[t] for t in g)))
    nb = len(S6.order) - len(tun)
    if nb: w('# %d bridge spans (and their groups) are not written: this notation has no bridge statement yet (gap, report.txt)' % nb)
    for trunk, leg in T.straight:
        if leg is None: w('straight %s: none' % names[trunk])
        elif isinstance(leg, tuple) and leg[0] == 'und': w('# straight %s: undetermined (%s)' % (names[trunk], leg[1]))
        else: w('straight %s: %s' % (names[trunk], names[leg]))
    open(path, 'w').write('\n'.join(out) + '\n')


def write_shape(path_shape, path_steps, path_len, hub, Ln, Sh, T, P):
    names = T.names
    so = ['# %s shape at Shape\'s level (D396): each stretch monotone from its start to its end heading; sideways steps are in steps.txt; extracted by tools/junction/corpus/extract.py.' % hub['name']]
    st = ['# %s: sideways steps per stretch (D396), the raw headings of each stretch that is not monotone; with shape.txt it gives the raw shape.' % hub['name']]
    ln = ['# %s: per line the length of each piece of its RAW heading list (before projection), in half tiles: an axis tile 2, a diagonal piece 1, a tunnel or bridge 2 per tile; a port-end heading appended to meet the port has length 0.' % hub['name']]
    inport = {x['pc'] for x in Ln.lane_in.values()}
    tally = defaultdict(int)
    for k in sorted(Sh.ES):
        raw, at, proj, newp, steps = Sh.ES[k]; n = names[('r', k)]
        so.append('shape %s: %s' % (n, ' '.join(map(str, proj))))
        if at: so.append('at %s: %s' % (n, ', '.join('%s@%d' % (names[('r', o)], newp[i]) for i, (o, p, m) in enumerate(at))))
        ends_ = [p for _, p, _ in at] + [len(raw) - 1]
        ch = Ln.chains[k]
        for kk, seg in sorted(steps.items()):
            fr = ('port' if ch[0] in inport else 'node') if kk == 0 else 'crossing'
            tt = 'node' if kk == len(ends_) - 1 else 'crossing'
            st.append('step %s %d: %s  # %s-%s' % (n, kk, ' '.join(map(str, seg)), fr, tt)); tally[fr + '-' + tt] += 1
        ln.append('lengths %s: %s' % (n, ' '.join(map(str, Sh.lens[k]))))
    for key in T.empty:
        so.append('shape %s: %d' % (names[key], T.heading[key])); ln.append('lengths %s: 0' % names[key])
    open(path_shape, 'w').write('\n'.join(so) + '\n'); open(path_steps, 'w').write('\n'.join(st) + '\n'); open(path_len, 'w').write('\n'.join(ln) + '\n')


def half(p):
    return (int(round(2 * p[0])), int(round(2 * p[1])))


def pins(box, G, Ln, lanes, X5, T):
    """Answer-key points in half tiles, map frame: per line its first and last point, per crossing its point.  Returns
    (lines, crossings, nopoint): lines = [(name, start, end)], crossings = [(nameA, nameB, point)]."""
    CS, CP, isnode, names = G.CS, G.CP, Ln.isnode, T.names
    ends, shared = X5.ends, X5.shared
    x0, y0, x1, y1 = box
    def onbox(side, pt):
        return {'W': pt[0] == 2 * x0, 'E': pt[0] == 2 * x1 + 2, 'N': pt[1] == 2 * y0, 'S': pt[1] == 2 * y1 + 2}[side]
    def port_end(pc, side):
        c = [half(q) for q in ends(pc) if onbox(side, half(q))]
        return c[0] if len(c) == 1 else None
    def src_pt(n):          # a merge piece's source-side end (the node point)
        out = [shared(n, b) for b in CS[n]]
        if out and out[0] is not None: return half([q for q in ends(n) if q != out[0]][0])
        lane = Ln.lane_out.get(n)
        if lane:
            pe = port_end(n, lane['side'])
            return half([q for q in ends(n) if half(q) != pe][0]) if pe else None
        return None
    def leg_pt(n):          # a split piece's leg-side end (the node point)
        sh = [shared(n, b) for b in CS[n]]
        return half(sh[0]) if sh and sh[0] is not None else None
    def node_pt(n): return leg_pt(n) if len(CS[n]) >= 2 else src_pt(n)
    P = {}; nop = []
    for k, ch in enumerate(Ln.chains):
        nm = names[('r', k)]
        if not ch: continue
        li = Ln.lane_in.get(ch[0]); lo = Ln.lane_out.get(ch[-1])
        pre = CP[ch[0]]; nxt = CS[ch[-1]]
        if li: st = port_end(ch[0], li['side'])
        elif pre and len(CP[pre[0]]) >= 2 and len(CS[pre[0]]) < 2: st = src_pt(pre[0])       # first piece of the out line is the merge piece
        elif pre:
            sh = shared(pre[0], ch[0]); st = half(sh) if sh is not None else None
        else: st = None
        if lo: en = port_end(ch[-1], lo['side'])
        elif nxt and len(CS[nxt[0]]) >= 2: en = leg_pt(nxt[0])                                  # last piece of the trunk is the split piece
        elif nxt:
            sh = shared(ch[-1], nxt[0]); en = half(sh) if sh is not None else None
        else: en = None
        P[nm] = (st, en)
    for k, ch in enumerate(Ln.chains):
        if not ch: nop.append('line %s: no simple chain' % names[('r', k)])
    for key in T.empty:
        nm = names[key]
        try:
            if key[0] == 'pi':
                n = key[1]; lane = Ln.lane_in[n]; P[nm] = (port_end(n, lane['side']), node_pt(n))
            elif key[0] == 'po':
                n = key[1]; lane = Ln.lane_out[n]; P[nm] = (node_pt(n), port_end(n, lane['side']))
            elif key[0] == 'e':
                a, b = key[1], key[2]; sh = half(shared(a, b))
                st = src_pt(a) if len(CP[a]) >= 2 else sh
                en = leg_pt(b) if len(CS[b]) >= 2 else sh
                P[nm] = (st, en)
            elif key[0] == 'c':
                p = node_pt(key[1]); P[nm] = (p, p)
            else: nop.append('line %s: empty line kind %s' % (nm, key[0]))
        except (KeyError, TypeError, IndexError) as e: nop.append('line %s: empty line %s: %r' % (nm, key[0], e))
    lines_out = []
    for nm in sorted(P, key=lambda s: int(s[1:])):
        st, en = P[nm]
        if st is None or en is None: nop.append('line %s: end point not found' % nm); continue
        lines_out.append((nm, st, en))
    cr = []
    for ks, h in sorted(X5.X.items(), key=lambda kv: tuple(sorted(kv[0]))):
        a, b = sorted(ks); pt = h[4]
        if abs(2 * pt[0] - round(2 * pt[0])) > 1e-2 or abs(2 * pt[1] - round(2 * pt[1])) > 1e-2:
            nop.append('crossing %s x %s: %.4f,%.4f is not a half-tile point' % (names[('r', a)], names[('r', b)], pt[0], pt[1])); continue
        cr.append((names[('r', a)], names[('r', b)], half(pt)))
    return lines_out, cr, nop


def write_pins(path, box, G, Ln, lanes, X5, T):
    ls, cr, nop = pins(box, G, Ln, lanes, X5, T)
    out = ['# Answer-key points (extracted by tools/junction/corpus/extract.py).',
           '# Half tiles, map frame (x east, y south): tile (x, y) spans [2x, 2x+2] x [2y, 2y+2].']
    for n, a, b in ls:
        out.append('end %s start %d %d' % (n, a[0], a[1])); out.append('end %s end %d %d' % (n, b[0], b[1]))
    for a, b, p in cr: out.append('cross %s %s %d %d' % (a, b, p[0], p[1]))
    for w in nop: out.append('# no point: ' + w)
    open(path, 'w').write('\n'.join(out) + '\n')


def write_report(path, hub, N, G, Ln, P, lanes, lane_notes, X5, S6, Sh, T):
    out = []; w = out.append; names = T.names
    kinds = defaultdict(int)
    for p, (kd, fl, ar) in Ln.ninfo.items(): kinds['%s %s %d->%d' % (kd, fl, ar[0], ar[1])] += 1
    ckind = defaultdict(int)
    for h in X5.X.values(): ckind[h[7]] += 1
    nj = sum(1 for v in S6.T.values())
    nbr = sum(1 for v in S6.T.values() if v[3])
    und = [e for e in S6.nodes if e[4][0] == 'und']
    w('# Extraction report: %s (PSG %s, box %s)' % (hub['name'], hub['game'], hub['box']))
    w('## Counts')
    w('ports %d (%s); lanes %d (%s)' % (len(P), ' '.join(p['id'] for p in P), len(lanes), hub.get('_lanesrc')))
    w('movements %d; routes (in lane x out port reached) %d' % (len(Ln.movement), len({r for rs in Ln.RS.values() for r in rs})))
    w('graph: %d box pieces, %d core pieces, direction rounds %s changes, %d off-route, %d free pieces fixed by routes, %d ambiguous' % (len(N.pieces), len(G.core), G.rounds, len(G.offroute), G.n_free_fixed, len(G.ambiguous)))
    w('lines with track %d (simple chain %d), empty lines %d' % (len(Ln.lines), sum(1 for c in Ln.chains if c), len(T.empty)))
    w('node pieces %d: %s' % (len(Ln.isnode), dict(sorted(kinds.items()))))
    w('crossing pairs %d: %s (a level, b ground over span, c span under span, n node piece over span, na node piece level)' % (len(X5.X), dict(sorted(ckind.items()))))
    w('spans %d: tunnels %d, bridges %d; groups of tunnels+bridges %d' % (nj, nj - nbr, nbr, len(S6.groups)))
    w('straight legs undetermined %d of %d node pieces' % (len(und), len(S6.nodes)))
    w('shape: %d lines shaped, %d raw pieces, %d steps, %d crossings' % (len(Sh.ES), sum(len(v[0]) for v in Sh.ES.values()), sum(len(v[4]) for v in Sh.ES.values()), sum(len(v[1]) for v in Sh.ES.values())))
    w('## Checkpoints (named, never silent)')
    for n in lane_notes: w('lanes: ' + n)
    if N.stations_in_box: w('stations inside the box (ignored by the graph): %s' % ' '.join('%d,%d' % s for s in N.stations_in_box[:20]))
    w('ambiguous pieces (both directions valid): %s' % (' '.join(pl(N, p) for p in G.ambiguous) or 'none'))
    w('off-route pieces: %d' % len(G.offroute))
    w('lines whose pieces carry different route sets: %s' % (' '.join(names[('r', k)] for k in Ln.bad) or 'none'))
    w('lines without a simple chain (no geometry): %s' % (' '.join(names[('r', k)] for k, c in enumerate(Ln.chains) if not c) or 'none'))
    w('lines skipped by crossings: %s' % ('; '.join('%s: %s' % (names[('r', k)], why) for k, why in X5.skipped) or 'none'))
    w('lines skipped by shape: %s' % ('; '.join('%s: %s' % (names[('r', k)], why) for k, why in Sh.notes) or 'none'))
    w('undetermined straight legs: %s' % ('; '.join('%s %s (%s)' % (tl(e[0]), e[1], e[4][1]) for e in und) or 'none'))
    w('cycle: %s' % (' -> '.join(T.cycle) + ' (topology.txt not written)' if T.cycle else 'none'))
    w('level crossings among lines (a): %d %s' % (len(X5.level_lines), ' '.join('%s x %s at %.1f,%.1f' % (names[('r', a)], names[('r', b)], p[0], p[1]) for a, b, p in X5.level_lines[:12])))
    w('node pieces crossing a ground piece (level): %d %s' % (len(X5.level_nodes), ' '.join('%s/%s' % t for t in X5.level_nodes[:12])))
    w('lattice-like: line pairs with several hits (first hit kept): %d %s' % (len(X5.multi), ' '.join('%s x %s at %s' % (names[('r', a)], names[('r', b)], ','.join('%.1f,%.1f' % q for q in pts)) for a, b, pts in X5.multi[:12])))
    w('same-channel crossings (two lines with equal route sets): %d %s' % (len(X5.same_set), ' '.join('%s x %s at %.1f,%.1f' % (names[('r', a)], names[('r', b)], p[0], p[1]) for a, b, p in X5.same_set[:12])))
    w('tied leg order at nodes (written in encounter order): %s' % ('; '.join(' '.join(t) for t in T.ties) or 'none'))
    w('## Gaps (D393)')
    w('cascades written for pieces with more than 2 legs or a cross: %s' % ('; '.join('%s at %s' % c for c in T.cascades) or 'none'))
    w('empty lines (no track between two events): %d; they have no crossings, their shape is one heading of length 0' % len(T.empty))
    w('bridge spans not written to structure.txt (no bridge statement in the notation yet): %d' % nbr)
    w('node pieces adjacent to node pieces: %d' % sum(1 for p in Ln.isnode for b in G.CS[p] if b in Ln.isnode))
    open(path, 'w').write('\n'.join(out) + '\n')


# ------------------------------------------------------------------ driver
def run(hub_id, root=None, hubs_path=None):
    hubs = read_hubs(hubs_path)
    if hub_id not in hubs: raise SystemExit('unknown hub %r (hubs.tsv: %s)' % (hub_id, ' '.join(hubs)))
    hub = hubs[hub_id]
    if not hub['box'].strip(): raise SystemExit('hub %s has no box in hubs.tsv' % hub_id)
    box = tuple(int(v) for v in hub['box'].split(','))
    tiles = load.load_tiles(hub['game'], root)
    Rr = R(); Rr.hub = hub; Rr.box = box
    Rr.N = N = load.build_net(tiles, box)
    Rr.lanes, Rr.lane_notes, hub['_lanesrc'] = choose_lanes(N, parse_lanes(hub['lanes']))
    Rr.P = P = ports(Rr.lanes)
    Rr.G = G = graph(N, Rr.lanes)
    Rr.Ln = Ln = lines(N, G, Rr.lanes, P)
    Rr.X5 = X5 = crossings(N, G, Ln)
    Rr.S6 = S6 = structure(N, G, Ln, X5, Rr.lanes)
    Rr.Sh = Sh = shape(N, G, Ln, X5, S6, Rr.lanes)
    Rr.T = build_text_model(N, G, Ln, P, Rr.lanes, S6, X5)
    return Rr


def write_all(Rr, out):
    os.makedirs(out, exist_ok=True)
    hub = Rr.hub
    tp = os.path.join(out, 'topology.txt')
    if Rr.T.cycle:
        if os.path.exists(tp): os.remove(tp)      # a stale topology from an earlier run must not outlive the refusal
    else: write_topology(tp, hub, Rr.N, Rr.G, Rr.Ln, Rr.P, Rr.lanes, Rr.T, Rr.X5)
    write_structure(os.path.join(out, 'structure.txt'), hub, Rr.G, Rr.Ln, Rr.S6, Rr.T)
    write_shape(os.path.join(out, 'shape.txt'), os.path.join(out, 'steps.txt'), os.path.join(out, 'lengths.txt'), hub, Rr.Ln, Rr.Sh, Rr.T, Rr.P)
    write_pins(os.path.join(out, 'pins.txt'), Rr.box, Rr.G, Rr.Ln, Rr.lanes, Rr.X5, Rr.T)
    write_report(os.path.join(out, 'report.txt'), hub, Rr.N, Rr.G, Rr.Ln, Rr.P, Rr.lanes, Rr.lane_notes, Rr.X5, Rr.S6, Rr.Sh, Rr.T)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('hub'); ap.add_argument('--out', required=True); ap.add_argument('--root', default=None, help='corpus root (env JT_CORPUS)')
    a = ap.parse_args()
    Rr = run(a.hub, a.root)
    write_all(Rr, a.out)
    print(open(os.path.join(a.out, 'report.txt')).read())


if __name__ == '__main__':
    main()
