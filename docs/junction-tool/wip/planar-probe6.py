"""Probe 6: the component solver (DESIGN-components, D310/D311), T2 first.

Every topology line is one strand: its fray part, its bundle run, its join part. Components have fixed
shapes (S-bend sizes, which lane goes over, doubled decks as the parallelogram, prio rows); bundles are
offset-parallel along one heading sequence; the core crossings are stacks (B over D and F, doubled, D x F
single). An LP (HiGHS, integer half-tile coordinates) places everything; clashes are separated lazily.

Half-tile coordinates (u east, v south); track points on tile-edge midpoints (u + v odd). An axis segment of
length L moves L (even); a diagonal one moves (L, L).
"""
import itertools, json, math, sys, time
import highspy

INF = highspy.kHighsInf
HEAD = 'N NE E SE S SW W NW'.split()
D = {'E': (1, 0), 'W': (-1, 0), 'N': (0, -1), 'S': (0, 1), 'NE': (1, -1), 'NW': (-1, -1), 'SE': (1, 1), 'SW': (-1, 1)}
STUB = 4
TL, CL = 4, 5   # MakeTestJunctionSpec: tl 4; reference speed 160, curve speed 0 -> DeriveCurveSpacing 5
G = min(2 * TL - 1, max(2, CL))
P = int(next((a.split('=')[1] for a in sys.argv if a.startswith('--prio=')), 14))  # prio coverage, tiles (spec.prio_tiles)
FANW = 1.5      # cost per half-tile of a fanned (spread) stretch; the tight stub costs 1
BUDGET = 15.0


def turn(h, k): return HEAD[(HEAD.index(h) + k) % 8]
def rot(a, b):
    d = (HEAD.index(b) - HEAD.index(a)) % 8
    return d - 8 if d > 4 else d
def axis(d): return 0 in D[d]
def right(h): return D[turn(h, 2)]


class Ex:
    __hash__ = None
    def __init__(s, d=None, c=0): s.d = d or {}; s.c = c
    def __add__(s, o):
        o = lift(o); d = dict(s.d)
        for k, v in o.d.items(): d[k] = d.get(k, 0) + v
        return Ex(d, s.c + o.c)
    __radd__ = __add__
    def __neg__(s): return Ex({k: -v for k, v in s.d.items()}, -s.c)
    def __sub__(s, o): return s + (-lift(o))
    def __rsub__(s, o): return lift(o) - s
    def __mul__(s, k): return Ex({a: b * k for a, b in s.d.items()}, s.c * k)
    __rmul__ = __mul__
    def __ge__(s, o): return ('ge', s - o)
    def __le__(s, o): return ('ge', lift(o) - s)
    def __eq__(s, o): return ('eq', s - o)


def lift(o): return o if isinstance(o, Ex) else Ex({}, o)


class Var(Ex):
    def __init__(s, i): super().__init__({i: 1}, 0); s.i = i


def dot(f, p): return p[0] * f[0] + p[1] * f[1]


class Seg:
    def __init__(s, strand, i, d, L, a, b): s.strand, s.i, s.d, s.L, s.a, s.b = strand, i, d, L, a, b


class Model:
    def __init__(M):
        M.lb, M.ub, M.obj, M.rows, M.par = [], [], [], [], {}
        M.strands = {}   # name -> list of Seg
        M.start = {}     # name -> point
        M.related = set()
        M.decks = []     # (seg, n, f, [(crossing a, head)])
        M.crossings = [] # (over seg, under seg, a, b)
        M.prio_rows = [] # (strand, main point of merge, heading, side)
        M.labels = {}
        M.pred = {}      # strand -> predecessor strand (path continues through a node)
        M.tags, M.tag = [], ''
        M.copies = {}    # copy strand -> (strand, first replaced seg, resume seg)

    def var(M, lb=0.0, ub=INF, c=0.0):
        M.lb.append(lb); M.ub.append(ub); M.obj.append(c); return Var(len(M.lb) - 1)
    def add(M, con):
        kind, e = con
        d = {k: v for k, v in e.d.items() if v != 0}
        if not d:
            assert (e.c == 0) if kind == 'eq' else (e.c >= 0), con
            return
        M.rows.append((d, -e.c, -e.c if kind == 'eq' else INF)); M.tags.append(M.tag)
    def parity(M, x, p):
        if isinstance(x, Var): M.par[x.i] = p
    def point(M): return (M.var(lb=-INF), M.var(lb=-INF))
    def tie(M, p, q): M.add(p[0] == q[0]); M.add(p[1] == q[1])

    # ---- strands ----
    def strand(M, name, start=None, label=None):
        M.strands[name] = []
        M.start[name] = start if start is not None else M.point()
        M.labels[name] = label or name
        return name
    def end(M, name):
        s = M.strands[name]
        return s[-1].b if s else M.start[name]
    def seg(M, name, d, lo=0, hi=None, cost=1.0):
        a = M.end(name)
        L = M.var(lb=lo, ub=INF if hi is None else hi, c=cost)
        if axis(d):
            M.parity(a[0], 0 if D[d][1] == 0 else 1); M.parity(a[1], 1 if D[d][1] == 0 else 0); M.parity(L, 0)
        b = M.point()
        M.add(b[0] - a[0] - D[d][0] * L == 0); M.add(b[1] - a[1] - D[d][1] * L == 0)
        sg = Seg(name, len(M.strands[name]), d, L, a, b)
        M.strands[name].append(sg)
        return sg
    def node(M, *names):
        for a, b in itertools.combinations(names, 2): M.related.add(frozenset((a, b)))

    # ---- crossings and decks ----
    def cross(M, over, under, head):
        """under crosses over's axis segment; returns over's param a (half-tiles from over.a)."""
        a, b = M.var(), M.var()
        M.add(b >= 1); M.add(under.L - b >= 1)
        (ex, ey), (fx, fy) = D[over.d], D[under.d]
        M.add(over.a[0] + ex * a - under.a[0] - fx * b == 0); M.add(over.a[1] + ey * a - under.a[1] - fy * b == 0)
        M.crossings.append((over, under, a, b))
        return (a, head)
    def deck(M, over, xs):
        n, f = M.var(), M.var()
        M.add(n >= 2); M.add(over.L - f >= 2)
        for a, h in xs: M.add(a - n >= h); M.add(f - a >= h)
        M.decks.append((over, n, f, xs))
        return n, f

    # ---- solve ----
    def solve(M, extra=()):
        n = len(M.lb); lb, ub, obj, rows = list(M.lb), list(M.ub), list(M.obj), list(M.rows) + list(extra)
        for x, p in M.par.items():
            lb.append(-INF); ub.append(INF); obj.append(0.0)
            rows.append(({x: 1, n: -2}, p, p)); n += 1
        h = highspy.Highs(); h.silent(); h.setOptionValue('time_limit', 5.0)
        lp = highspy.HighsLp()
        lp.num_col_ = n; lp.num_row_ = len(rows)
        lp.col_cost_ = obj; lp.col_lower_ = lb; lp.col_upper_ = ub
        lp.row_lower_ = [r[1] for r in rows]; lp.row_upper_ = [r[2] for r in rows]
        cols = [[] for _ in range(n)]
        for ri, (d, _, _) in enumerate(rows):
            for k, v in d.items(): cols[k].append((ri, v))
        starts, idx, vals = [], [], []
        for k in range(n):
            starts.append(len(idx))
            for ri, v in cols[k]: idx.append(ri); vals.append(v)
        starts.append(len(idx))
        lp.a_matrix_.format_ = highspy.MatrixFormat.kColwise
        lp.a_matrix_.start_ = starts; lp.a_matrix_.index_ = idx; lp.a_matrix_.value_ = vals
        lp.integrality_ = [highspy.HighsVarType.kInteger] * n
        h.passModel(lp); h.run()
        if h.getModelStatus() != highspy.HighsModelStatus.kOptimal: return None
        val = h.getSolution().col_value
        return [round(v) for v in val], h.getInfo().objective_function_value

    def row(M, con):
        kind, e = con
        d = {k: v for k, v in e.d.items() if v != 0}
        return (d, -e.c, -e.c if kind == 'eq' else INF)


def ev(val, e):
    e = lift(e)
    return e.c + sum(val[k] * v for k, v in e.d.items())


def pieces(u, v, L, d):
    dx, dy = D[d]
    out = []
    if axis(d):
        for t in range(0, L, 2):
            a = (u + dx * t, v + dy * t); b = (u + dx * (t + 2), v + dy * (t + 2))
            m = ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2)
            out.append(((int(m[0] // 2), int(m[1] // 2)), 'X' if dy == 0 else 'Y', (a, b), t))
    else:
        for t in range(L):
            a = (u + dx * t, v + dy * t); b = (u + dx * (t + 1), v + dy * (t + 1))
            m = ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2)
            tile = (int(m[0] // 2), int(m[1] // 2))
            corner = ('N' if m[1] - 2 * tile[1] < 1 else 'S') + ('W' if m[0] - 2 * tile[0] < 1 else 'E')
            out.append((tile, corner, (a, b), t))
    return out


# ================= T2 composition =================

LEANMODE = next((a.split('=')[1] for a in sys.argv if a.startswith('--lean=')), 'auto')  # auto (pairs one way, stack form by cost) | all | none
LEAN = 2  # every doubled pair leans one way: the more northerly deck's heads one tile (2) further east


def head_u(seg, n): return seg.a[0] + D[seg.d][0] * n  # u of a deck's near-head end


def build_t2(fray_s, join_s, df_over, tri=0, stair=None):
    if stair is None: stair = LEANMODE == 'all'
    """fray_s, join_s: +1 base (the diverging/joining bundle on the right of travel), -1 mirror; df_over: 'D' or 'F'."""
    M = Model()
    X, Y = M.var(c=3.0), M.var(c=3.0)
    M.X, M.Y = X, Y
    M.fan_in = []

    def port_in(name, side):
        M.strand(name)
        u, v = M.start[name]
        M.fan_in.append(name)
        if side == 'W': M.add(u == 0)
        if side == 'E': M.add(u - X == 0)
        if side == 'S': M.add(v - Y == 0)
        return name

    # Arms: in-lines start at the boundary.
    for n in ('EF1', 'EF2'): port_in(n, 'W')
    for n in ('AB1', 'AB2'): port_in(n, 'E')
    for n in ('CD1', 'CD2'): port_in(n, 'S')
    outs = {'ED1': 'E', 'ED2': 'E', 'CB1': 'W', 'CB2': 'W', 'AF1': 'S', 'AF2': 'S'}

    def lane_line(h, sigma, k):  # normal offset (half-tiles) of line k toward sigma
        return 2 * k * sigma

    bundles = {}  # name -> (lanes left->right, heading sequence)

    def fray(h, sigma, far, near, S_far, S_near, V_far, V_near, prio):
        fl = []
        for x in (far, near):
            M.seg(x, h, lo=STUB)                         # tight stub at the port
            fl.append(M.seg(x, turn(h, -1), lo=2))       # fan to the left of travel, as late as possible
            M.seg(x, h, lo=2, cost=FANW)
        M.add(dot(D[h], fl[0].a) - dot(D[h], fl[1].a) == 0)
        if not prio: M.add(fl[0].L - fl[1].L == 0)
        r = right(h)
        # far lane on line 0; near on line sigma*1, or sigma*2 when its parallelogram steps inward (the fan makes the room)
        M.add(dot(r, M.end(near)) - dot(r, M.end(far)) == (4 if prio else 2) * sigma)
        sf, sn = M.end(far), M.end(near)
        M.strand(S_far, sf); M.strand(V_far, sf); M.node(far, S_far, V_far); M.pred[S_far] = far; M.pred[V_far] = far
        M.strand(S_near, sn); M.strand(V_near, sn); M.node(near, S_near, V_near); M.pred[S_near] = near; M.pred[V_near] = near
        M.seg(S_far, h, lo=2)
        vf = M.seg(V_far, turn(h, sigma), lo=6, hi=6)
        M.seg(V_far, h, lo=2)
        M.seg(V_near, turn(h, sigma), lo=4 if prio else 6, hi=4 if prio else 6)
        M.seg(V_near, h, lo=2)
        if prio:  # near lane on line 2: the parallelogram steps inward to line 1 (V_far's diagonal meets line 1 first, D118)
            M.seg(S_near, h, lo=2)                      # split to copy split: one signal tile
            cs = M.end(S_near)
            c = S_near + "'"
            M.strand(c, cs, label=S_near); M.node(S_near, c)
            d0 = M.seg(S_near, h, lo=2)                 # copy 0 deck (line 2)
            M.seg(S_near, turn(h, -sigma), lo=2, hi=2)  # copy 0 bends after its bridge
            M.seg(c, turn(h, -sigma), lo=2, hi=2)       # copy 1 bends before its bridge
            d1 = M.seg(c, h, lo=2)                      # copy 1 deck (line 1), runs to the merge
            M.tie(M.end(c), M.end(S_near)); M.copies[c] = (S_near, d0.i, d0.i + 2)
            M.seg(S_near, h, lo=2)                      # merge signal room; now beside S_far, no shift back
            n0, f0 = M.deck(d0, [M.cross(d0, vf, 4)]); n1, f1 = M.deck(d1, [M.cross(d1, vf, 4)])
            M.add(f0 - n0 - (f1 - n1) == 0)  # D118: equal bridges
            south, north = ((d0, n0), (d1, n1)) if right(h)[1] * sigma > 0 else ((d1, n1), (d0, n0))
            if LEANMODE != 'none': M.add(head_u(*north) - head_u(*south) == LEAN)
            else: M.add(dot(D[h], d0.a) + n0 - dot(D[h], d1.a) - n1 == 2)  # natural D118 stagger
        else:
            d0 = M.seg(S_near, h, lo=2)
            M.deck(d0, [M.cross(d0, vf, 4)])

    def join(h, sigma, S_far, S_near, J_far, J_near, O_far, O_near, prio):
        """Straight bundle S, joining bundle J from side sigma. Ends the in-strands; starts the out-strands."""
        r = right(h)
        # far fixed lane runs to its merge; J_far's diagonal lands on it
        M.seg(S_far, h, lo=2 * P + 2 if prio else 2)
        M.seg(J_far, h, lo=2)
        jf = M.seg(J_far, turn(h, -sigma), lo=6, hi=6)
        mf = M.end(S_far); M.tie(M.end(J_far), mf)
        if prio:
            M.seg(S_near, h, lo=2)
            cs = M.end(S_near)
            c = S_near + "''"
            M.strand(c, cs, label=S_near); M.node(S_near, c)
            d0 = M.seg(S_near, h, lo=2)
            M.seg(S_near, turn(h, sigma), lo=2, hi=2)
            M.seg(c, turn(h, sigma), lo=2, hi=2)
            d1 = M.seg(c, h, lo=2)
            M.tie(M.end(c), M.end(S_near)); M.copies[c] = (S_near, d0.i, d0.i + 2)
            M.seg(S_near, h, lo=2 * P + 2)               # R9: no bend in the prio coverage
            n0, f0 = M.deck(d0, [M.cross(d0, jf, 4)]); n1, f1 = M.deck(d1, [M.cross(d1, jf, 4)])
            M.add(f0 - n0 - (f1 - n1) == 0)  # D118: equal bridges
            south, north = ((d1, n1), (d0, n0)) if right(h)[1] * sigma > 0 else ((d0, n0), (d1, n1))
            if LEANMODE != 'none': M.add(head_u(*north) - head_u(*south) == LEAN)
            else: M.add(dot(D[h], d0.a) + n0 - dot(D[h], d1.a) - n1 == 2)  # natural D118 stagger
            M.seg(J_near, h, lo=2)
            M.seg(J_near, turn(h, -sigma), lo=4, hi=4)
        else:
            d0 = M.seg(S_near, h, lo=2)
            M.deck(d0, [M.cross(d0, jf, 4)])
            M.seg(J_near, h, lo=2)
            M.seg(J_near, turn(h, -sigma), lo=6, hi=6)
        mn = M.end(S_near); M.tie(M.end(J_near), mn)
        M.strand(O_far, mf); M.node(S_far, J_far, O_far); M.pred[O_far] = S_far
        M.strand(O_near, mn); M.node(S_near, J_near, O_near); M.pred[O_near] = S_near
        if prio:
            M.prio_rows.append((S_far, mf, h, -sigma))
            M.prio_rows.append((S_near, mn, h, -sigma))
            M.seg(O_near, h, lo=2)
            M.seg(O_near, turn(h, -sigma), lo=2, hi=2)
        fo = []
        for o in (O_far, O_near):
            M.seg(o, h, lo=2, cost=FANW)
            fo.append(M.seg(o, turn(h, +1), lo=2))   # fan back toward the arm's other direction
            M.seg(o, h, lo=STUB)
        M.add(fo[0].L - fo[1].L == 0); M.add(dot(D[h], fo[0].a) - dot(D[h], fo[1].a) == 0)  # the bundle fans as one
        M.add(dot(r, M.end(O_near)) - dot(r, M.end(O_far)) == 2 * sigma)

    def bundle(name, lanes, heads):
        """lanes left->right; each lane continues with one segment per heading; offset-parallel."""
        segs = {l: [] for l in lanes}
        for k, h in enumerate(heads):
            r = right(h)
            for l in lanes:
                lo = 0
                segs[l].append(M.seg(l, h, lo=lo))
            for j, l in enumerate(lanes[1:], 1):
                M.add(dot(r, segs[l][k].a) - dot(r, segs[lanes[0]][k].a) == 2 * j)
                M.add(dot(r, segs[l][k].b) - dot(r, segs[lanes[0]][k].b) == 2 * j)
        bundles[name] = (lanes, heads, segs)
        return segs

    # Frays.
    M.tag = 'frayW'; fray('E', +1, 'EF1', 'EF2', 'E1', 'E2', 'F1', 'F2', True)
    M.tag = 'frayE'; fray('W', -1, 'AB2', 'AB1', 'B2', 'B1', 'A2', 'A1', True)
    M.tag = 'frayS'
    if fray_s > 0: fray('N', +1, 'CD1', 'CD2', 'C1', 'C2', 'D1', 'D2', False)
    else: fray('N', -1, 'CD2', 'CD1', 'D2', 'D1', 'C2', 'C1', False)

    # Bundles (lanes left -> right of travel).
    M.tag = 'bundles'
    sE = bundle('E', ['E1', 'E2'], ['E'])
    sF = bundle('F', ['F1', 'F2'], ['E', 'SE', 'S'])
    sD = bundle('D', ['D1', 'D2'], ['N', 'NE', 'E'] if tri == 0 else ['N', 'NE', 'N', 'NE', 'E'])
    sC = bundle('C', ['C1', 'C2'], ['N', 'NW', 'W'])
    sA = bundle('A', ['A1', 'A2'], ['W', 'SW', 'S'])
    # B: run, doubled stretch per track (copy outward, the parallelogram), run.
    M.tag = 'bundleB'
    lanesB = ['B1', 'B2']
    run1 = bundle('B', lanesB, ['W'])
    decksB = {}
    for l, sigma in (('B1', -1), ('B2', +1)):
        h = 'W'
        M.seg(l, h, lo=2)
        cs = M.end(l)
        c = l + '"'
        M.strand(c, cs, label=l); M.node(l, c)
        d0 = M.seg(l, h, lo=2)
        M.seg(l, turn(h, sigma), lo=2, hi=2)
        M.seg(c, turn(h, sigma), lo=2, hi=2)
        d1 = M.seg(c, h, lo=2)
        M.tie(M.end(c), M.end(l)); M.copies[c] = (l, d0.i, d0.i + 2)
        M.seg(l, h, lo=2)
        M.seg(l, turn(h, -sigma), lo=2, hi=2)
        decksB[l] = (d0, d1)
    run2 = {}
    for l in lanesB: run2[l] = M.seg(l, 'W', lo=0)
    M.add(dot(right('W'), run2['B2'].a) - dot(right('W'), run2['B1'].a) == 2)

    # Core crossings: B over D (D's N segment) and F (F's S segment), every deck; D x F single.
    M.tag = 'coreB'
    stack = {}
    for l in lanesB:
        spans = []
        for dk in decksB[l]:
            xs_F = [M.cross(dk, sF[f][2], 3) for f in ('F1', 'F2')]
            xs_D = [M.cross(dk, sD[d][0 if tri == 0 else 2], 3) for d in ('D1', 'D2')]
            nF, fF = M.deck(dk, xs_F)
            nD, fD = M.deck(dk, xs_D)
            spans.append((fF - nF, fD - nD, head_u(dk, nF), head_u(dk, nD)))
            if tri == 0: M.add(nD - fF >= 2)  # heading W: F's deck first, one plain tile before D's
            else: M.add(nF - fD >= 2)         # flipped triangle: D's deck first
        M.add(spans[0][0] - spans[1][0] == 0); M.add(spans[0][1] - spans[1][1] == 0)  # D118: equal bridges per pair
        if not stair:  # per pair: copy 1 one tile upstream (the chevron)
            st = 2 if l == 'B2' else -2
            M.add(spans[0][2] - spans[1][2] == st); M.add(spans[0][3] - spans[1][3] == st)
        stack[l] = spans
    # One staircase across the four decks, north to south: B2's copy, B2, B1, B1's copy; each one tile further west.
    seq = [stack['B2'][1], stack['B2'][0], stack['B1'][0], stack['B1'][1]]
    for a_, b_ in zip(seq, seq[1:]) if stair else []:
        M.add(a_[2] - b_[2] == LEAN); M.add(a_[3] - b_[3] == LEAN)
        M.add(a_[0] - b_[0] == 0); M.add(a_[1] - b_[1] == 0)  # equal bridges across the stack
    M.tag = 'coreDF'
    if df_over == 'F':  # F's southbound run over D's NE diagonal (S-bend form)
        for f in ('F1', 'F2'): M.deck(sF[f][2], [M.cross(sF[f][2], sD[d][1], 4) for d in ('D1', 'D2')])
    else:               # D's northbound run over F's SE diagonal
        for d in ('D1', 'D2'): M.deck(sD[d][0], [M.cross(sD[d][0], sF[f][1], 4) for f in ('F1', 'F2')])

    # Joins.
    M.tag = 'joinE'; join('E', +1, 'E1', 'E2', 'D1', 'D2', 'ED1', 'ED2', True)
    M.tag = 'joinW'; join('W', -1, 'B2', 'B1', 'C2', 'C1', 'CB2', 'CB1', True)
    M.tag = 'joinS'
    if join_s > 0: join('S', +1, 'A1', 'A2', 'F1', 'F2', 'AF1', 'AF2', False)
    else: join('S', -1, 'F2', 'F1', 'A2', 'A1', 'AF2', 'AF1', False)

    # Out ports on the boundary.
    M.tag = 'ports'
    for n, side in outs.items():
        u, v = M.end(n)
        if side == 'W': M.add(u == 0)
        if side == 'E': M.add(u - X == 0)
        if side == 'S': M.add(v - Y == 0)
    # Port order per side (reading order), adjacent within a port.
    def vv(n, start): return (M.start[n] if start else M.end(n))
    order = {'W': [('EF1', 1), ('EF2', 1), ('CB2', 0), ('CB1', 0)], 'E': [('ED1', 0), ('ED2', 0), ('AB2', 1), ('AB1', 1)],
             'S': [('CD1', 1), ('CD2', 1), ('AF2', 0), ('AF1', 0)]}
    GAPW = float(next((x.split('=')[1] for x in sys.argv if x.startswith('--gapw=')), 0))
    GAPMAX = next((int(x.split('=')[1]) for x in sys.argv if x.startswith('--gapmax=')), None)
    M.gaps = []
    for side, seq in order.items():
        k = 0 if side == 'S' else 1
        for idx, ((a, sa), (b, sb)) in enumerate(zip(seq, seq[1:])):
            M.add(vv(b, sb)[k] - vv(a, sa)[k] == (4 if idx == 1 else 2))  # LL_RR: lanes adjacent, one tile between the directions
            if idx == 1:  # between the arm's two directions
                g = M.var(c=GAPW); M.add(g - (vv(b, sb)[k] - vv(a, sa)[k]) == 0); M.gaps.append(g)
                if GAPMAX is not None: M.add(g <= 2 * GAPMAX)
    # Box.
    M.tag = 'box'
    for name, segs in M.strands.items():
        for p in [M.start[name]] + [s.b for s in segs]:
            M.add(p[0] >= 0); M.add(X - p[0] >= 0); M.add(p[1] >= 1); M.add(Y - p[1] >= 0)
    M.tag = 'prio'
    # Prio rows (parallel unused track on side, P tiles before the merge): inside the box, unrelated to all.
    for main, m, h, side in M.prio_rows:
        r = right(h); dx, dy = D[h]
        start = (m[0] - dx * 2 * P + side * r[0] * 2, m[1] - dy * 2 * P + side * r[1] * 2)
        nm = 'prio:' + main
        M.strand(nm, M.point(), label=nm)
        M.tie(M.start[nm], start)
        M.seg(nm, h, lo=2 * P, hi=2 * P, cost=0)
        for p in (M.start[nm], M.end(nm)):
            M.add(p[0] >= 0); M.add(X - p[0] >= 0); M.add(p[1] >= 1); M.add(Y - p[1] >= 0)
    M.tag = 'curve'; route_curves(M, T2_ROUTES)
    M.bundles = bundles
    return M


T2_ROUTES = [(a + k, b + k, c + k) for k in '12' for a, b, c in
             (('EF', 'E', 'ED'), ('EF', 'F', 'AF'), ('AB', 'B', 'CB'), ('AB', 'A', 'AF'), ('CD', 'C', 'CB'), ('CD', 'D', 'ED'))]


def route_segs(M, route):
    """Every segment sequence of a route: its strands in order, each doubled stretch by either copy."""
    variants = [[]]
    for st in route:
        cps = sorted((k, r_, c) for c, (s_, k, r_) in M.copies.items() if s_ == st)
        opts = []
        for mask in range(1 << len(cps)):
            seq, i = [], 0
            segs = M.strands[st]
            chosen = {cps[b][0]: cps[b] for b in range(len(cps)) if mask >> b & 1}
            while i < len(segs):
                if i in chosen:
                    k, r_, c = chosen[i]
                    seq += M.strands[c]; i = r_
                else:
                    seq.append(segs[i]); i += 1
            opts.append(seq)
        variants = [v + o for v in variants for o in opts]
    return variants


def route_curves(M, routes):
    """JunctionCurveGaps on every route: curves k apart at least min(2TL-1, k*CL), consecutive opposite hands 2."""
    seen = set()
    for route in routes:
        for seq in route_segs(M, route):
            pos, curves, acc = [], [], Ex()
            for a, b in zip(seq, seq[1:]):
                acc = acc + a.L
                if a.d != b.d:
                    assert abs(rot(a.d, b.d)) == 1, (route, a.d, b.d)
                    curves.append((acc, rot(a.d, b.d) > 0))
            for i in range(len(curves)):
                for j in range(i + 1, min(len(curves), i + 4)):
                    if j == i + 1: gap = 2 if curves[i][1] != curves[j][1] else G
                    else: gap = min(2 * TL - 1, (j - i) * CL)
                    e = curves[j][0] - curves[i][0]
                    key = (tuple(sorted(e.d.items())), e.c, gap)
                    if key in seen: continue
                    seen.add(key); M.add(e >= gap)


def curve_gaps(M):
    """Consecutive bends on a path: runs between them >= 2 (opposite hands) or G (same hand). Paths continue through nodes."""
    for name, segs in M.strands.items():
        if name.startswith('prio:'): continue
        pre = M.pred.get(name)
        dirs = ([M.strands[pre][-1].d] if pre and M.strands[pre] else []) + [s.d for s in segs]
        off = 1 if pre and M.strands[pre] else 0
        # runs of equal directions (by segment index into segs)
        runs = []
        for i, s in enumerate(segs):
            if runs and runs[-1][0] == s.d: runs[-1][1].append(s)
            else: runs.append((s.d, [s]))
        prev = dirs[0] if off else None
        for k in range(len(runs)):
            d, ss = runs[k]
            before = runs[k - 1][0] if k > 0 else prev
            after = runs[k + 1][0] if k + 1 < len(runs) else None
            if before is None or after is None or before == d or after == d: continue
            same = (rot(before, d) > 0) == (rot(d, after) > 0)
            M.add(sum((s.L for s in ss), Ex()) >= (G if same else 2))


# ================= raster, clashes, separation =================

def geometry(M, val):
    geo = {}
    for name, segs in M.strands.items():
        for s in segs:
            geo[(name, s.i)] = (round(ev(val, s.a[0])), round(ev(val, s.a[1])), round(ev(val, s.L)), s.d)
    decks = []
    for over, n, f, xs in M.decks:
        decks.append(((over.strand, over.i), round(ev(val, n)), round(ev(val, f))))
    return geo, decks


def occupancy(M, geo, decks, heads_as_track=True):
    dk = {}
    for k, n, f in decks: dk.setdefault(k, []).append((n, f))
    occ = {}
    for (name, i), (u, v, L, d) in geo.items():
        for tile, kind, (a, b), t in pieces(u, v, L, d):
            inside = any(n <= t and t + 2 <= f for n, f in dk.get((name, i), []))
            if inside:
                is_head = any(t == n or t + 2 == f for n, f in dk.get((name, i), []))
                if not (heads_as_track and is_head): continue
            occ.setdefault(tile, []).append((name, i, kind, a, b))
    return occ


def clashes(M, geo, decks):
    occ = occupancy(M, geo, decks)
    ok_pair = {frozenset(('NW', 'SE')), frozenset(('NE', 'SW'))}
    bad = set()
    def unrelated(p, q):
        if p == q: return False
        bp, bq = M.labels[p], M.labels[q]
        if frozenset((p, q)) in M.related: return False
        return True
    pts = {}
    for tile, lst in occ.items():
        for (p, i, kp, a1, b1), (q, j, kq, a2, b2) in itertools.combinations(lst, 2):
            if not unrelated(p, q): continue
            if frozenset((kp, kq)) in ok_pair: continue
            bad.add(tuple(sorted(((p, i), (q, j)))))
        for (p, i, kp, a, b) in lst:
            for x in (a, b): pts.setdefault(x, set()).add((p, i))
    for x, s in pts.items():
        for (p, i), (q, j) in itertools.combinations(sorted(s), 2):
            if unrelated(p, q): bad.add(((p, i), (q, j)))
    return sorted(bad)


DIRS8 = [(1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (-1, -1), (1, -1), (-1, 1)]


def sep_options(M, geo, pr):
    (p, i), (q, j) = pr
    sp, sq = M.strands[p][i], M.strands[q][j]
    def ends(key):
        u, v, L, d = geo[key]
        return [(u, v), (u + D[d][0] * L, v + D[d][1] * L)]
    ep, eq = ends((p, i)), ends((q, j))
    opts = []
    for f in DIRS8:
        cur = min(dot(f, b) for b in eq) - max(dot(f, a) for a in ep)
        opts.append((-cur, f))
    opts.sort()
    out = []
    for _, f in opts:
        rows = []
        for a in (sp.a, sp.b):
            for b in (sq.a, sq.b):
                rows.append(M.row(dot(f, b) - dot(f, a) >= 2))
        out.append((f, rows))
    return out


def solve_lazy(M, t0):
    seps = []
    r = M.solve()
    if r is None: return None, 'base infeasible'
    for rnd in range(40):
        geo, decks = geometry(M, r[0])
        bad = clashes(M, geo, decks)
        # An under track is at ground where it is crossed: not inside a deck (heads included) of its own segment.
        under_bad = []
        for over, under, a, bb in M.crossings:
            bv = ev(r[0], bb)
            for dseg, n, f, xs in M.decks:
                if dseg is under and ev(r[0], n) - 3 < bv < ev(r[0], f) + 3:
                    under_bad.append([M.row(bb <= n - 3), M.row(bb >= f + 3)])
        if not bad and not under_bad: return (r, geo, decks, rnd), None
        if under_bad:
            for rows in under_bad[0]:
                if M.solve(seps + [rows]) is not None: seps.append(rows); break
            else: return None, 'under-deck conflict not separable'
            r = M.solve(seps)
            continue
        if time.perf_counter() - t0 > BUDGET: return None, f'budget at round {rnd}, {len(bad)} clashes'
        added = False
        for pr in bad:
            for f, rows in sep_options(M, geo, pr)[:4]:
                if M.solve(seps + rows) is not None: seps += rows; added = True; break
            else:
                return None, f'clash {pr} not separable'
            break  # one clash per round, then re-solve (deterministic)
        r = M.solve(seps)
        if r is None: return None, 'infeasible after separation'
    return None, 'rounds exhausted'


def render(M, geo, decks, val):
    occ = occupancy(M, geo, decks, heads_as_track=False)
    W, H = round(ev(val, M.X)) // 2 + 1, round(ev(val, M.Y)) // 2 + 1
    g = [['.'] * W for _ in range(H)]
    for (tx, ty), lst in occ.items():
        if 0 <= ty < H and 0 <= tx < W:
            chs = {('p' if l[0].startswith('prio') else M.labels[l[0]][0]) for l in lst}
            g[ty][tx] = chs.pop() if len(chs) == 1 else '#'
    dk = {}
    for (name, i), n, f in decks:
        u, v, L, d = geo[(name, i)]
        for tile, kind, _, t in pieces(u, v, L, d):
            if n <= t and t + 2 <= f:
                tx, ty = tile
                g[ty][tx] = 'H' if (t == n or t + 2 == f) else '='
    return '\n'.join(''.join(r) for r in g)


def export(M, geo, decks, val, path):
    BITS = {'X': 'X', 'Y': 'Y', 'NW': 'Upper', 'SE': 'Lower', 'NE': 'Left', 'SW': 'Right'}
    occ = occupancy(M, geo, decks, heads_as_track=False)
    tracks = {}
    for tile, lst in occ.items():
        for l in lst: tracks.setdefault(tile, set()).add(BITS[l[2]])
    # prio joins: a diagonal pair from the main track onto the prio row at its start, and back at its end
    for main, m, h, side in M.prio_rows:
        nm = 'prio:' + main
        u, v, L, d = geo[(nm, 0)]
        r = right(h); dx, dy = D[h]
        # first join (trailing): from the prio row's start, diagonal forward onto the main track
        for tile, kind, _, _ in pieces(u, v, 2, turn(h, -side)): tracks.setdefault(tile, set()).add(BITS[kind])
        # exit join: prio point 2 before its end, diagonal forward-to-main, length 2 (lands on the merge point)
        eu, ev_ = u + dx * (L - 2), v + dy * (L - 2)
        for tile, kind, _, _ in pieces(eu, ev_, 2, turn(h, -side)): tracks.setdefault(tile, set()).add(BITS[kind])
    bridges = []
    for (name, i), n, f in decks:
        u, v, L, d = geo[(name, i)]
        ps = [p for p in pieces(u, v, L, d) if n <= p[3] and p[3] + 2 <= f]
        bridges.append({'start': list(ps[0][0]), 'end': list(ps[-1][0])})
    W, H = round(ev(val, M.X)) // 2 + 1, round(ev(val, M.Y)) // 2 + 1
    out = {'size': [W, H], 'tracks': [{'x': t[0], 'y': t[1], 'bits': sorted(b)} for t, b in sorted(tracks.items())],
           'bridges': bridges, 'signals': []}
    json.dump(out, open(path, 'w'))
    return out


if __name__ == '__main__':
    t0 = time.perf_counter()
    results = []
    stairs = (True, False) if LEANMODE == 'auto' else ((True,) if LEANMODE == 'all' else (False,))
    combos = [(fs, js, o, 0, st) for fs in (1, -1) for js in (1, -1) for o in ('F', 'D') for st in stairs] + [(fs, js, 'F', 1, st) for fs in (1, -1) for js in (1, -1) for st in stairs]
    only = next((a.split('=')[1] for a in sys.argv if a.startswith('--only=')), None)
    if only: combos = [c for c in combos if f'{c[0]:+d}{c[1]:+d}{c[2]}{c[3]}{"s" if c[4] else "c"}' == only]
    for fs, js, o, tri, st in combos:
        ts = time.perf_counter()
        M = build_t2(fs, js, o, tri, st)
        res, why = solve_lazy(M, ts)
        tag = f'{fs:+d}{js:+d}{o}{tri}{"s" if st else "c"}'
        if res is None:
            print(f'{tag}: NO ({why}) {time.perf_counter() - ts:.2f}s'); continue
        (val, obj), geo, decks, rnd = res
        W, H = round(ev(val, M.X)) // 2 + 1, round(ev(val, M.Y)) // 2 + 1
        print(f'{tag}: {W}x{H} obj {obj:.0f} sep rounds {rnd} {time.perf_counter() - ts:.2f}s gaps(tiles) {[round(ev(val, g)) // 2 for g in M.gaps]}')
        results.append((obj, tag, M, val, geo, decks))
    print(f'total {time.perf_counter() - t0:.2f}s')
    if results:
        results.sort(key=lambda r: r[0])
        obj, tag, M, val, geo, decks = results[0]
        print('best', tag)
        print(render(M, geo, decks, val))
        exp = next((a.split('=', 1)[1] for a in sys.argv if a.startswith('--export=')), None)
        if exp:
            out = export(M, geo, decks, val, exp)
            print(f'exported {exp}: {len(out["tracks"])} tiles, {len(out["bridges"])} bridges')


def glyphs(M, geo, decks, val):
    occ = occupancy(M, geo, decks, heads_as_track=False)
    W, H = round(ev(val, M.X)) // 2 + 1, round(ev(val, M.Y)) // 2 + 1
    g = [[' '] * W for _ in range(H)]
    for (tx, ty), lst in occ.items():
        ks = {l[2] for l in lst}
        prio = all(l[0].startswith('prio') for l in lst)
        if ks <= {'X'}: c = ':' if prio else '-'
        elif ks <= {'Y'}: c = '|'
        elif ks <= {'NE', 'SW'}: c = '\\'
        elif ks <= {'NW', 'SE'}: c = '/'
        else: c = '+'
        if 0 <= ty < H and 0 <= tx < W: g[ty][tx] = c
    for (name, i), n, f in decks:
        u, v, L, d = geo[(name, i)]
        for tile, kind, _, t in pieces(u, v, L, d):
            if n <= t and t + 2 <= f:
                tx, ty = tile
                g[ty][tx] = 'H' if (t == n or t + 2 == f) else ('=' if kind == 'X' else '"')
    return '\n'.join(''.join(r).rstrip() for r in g)
