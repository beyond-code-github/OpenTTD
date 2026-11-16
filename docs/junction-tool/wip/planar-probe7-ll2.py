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
TUCK = float(next((a.split('=')[1] for a in sys.argv if a.startswith('--tuck=')), math.sqrt(2)))  # cost per diagonal unit of a bundle turn (tuck in)
AREA_W = float(next((a.split('=')[1] for a in sys.argv if a.startswith('--areaw=')), 10))  # objective weight per tile of area inside the outline
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
        L = M.var(lb=lo, ub=INF if hi is None else hi, c=cost * (1.0 if axis(d) else math.sqrt(2)))  # a diagonal unit is sqrt 2 long
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
    def deck(M, over, xs, n_exact=None, tail_exact=None, n_min=2, tail_min=2):
        n, f = M.var(), M.var()
        if n_exact is None: M.add(n >= n_min)
        else: M.add(n == n_exact)
        if tail_exact is None: M.add(over.L - f >= tail_min)
        else: M.add(over.L - f == tail_exact)
        for a, h in xs: M.add(a - n >= h); M.add(f - a >= h)
        M.decks.append((over, n, f, xs))
        return n, f

    # ---- solve ----
    def solve(M, extra=()):
        n = len(M.lb); lb, ub, obj, rows = list(M.lb), list(M.ub), list(M.obj), list(M.rows) + list(extra)
        for k, v in getattr(M, 'obj_extra', {}).items(): obj[k] += v
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


FCONT = '--fcont' in sys.argv
FDIAG = '--fdiag' in sys.argv
SFORM = 0
EFORM = 0
STEP2 = set()


def build_t2(fray_s, jogE, df_over, tri=0, jogB=0):
    stair = False
    """fray_s, join_s: +1 base (the diverging/joining bundle on the right of travel), -1 mirror; df_over: 'D' or 'F'."""
    M = Model()
    X, Y = M.var(c=0.0 if AREA_W else 3.0), M.var(c=0.0 if AREA_W else 3.0)
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
    outs = {'EO1': 'E', 'EO2': 'E', 'WO1': 'W', 'WO2': 'W', 'SO1': 'S', 'SO2': 'S'}

    def lane_line(h, sigma, k):  # normal offset (half-tiles) of line k toward sigma
        return 2 * k * sigma

    bundles = {}  # name -> (lanes left->right, heading sequence)

    def doubled(l, h, sigma, xs_of):
        """A doubled over stretch on lane l (the parallelogram, copy toward sigma, shift back; D145/D314 tight heads, D118 equal bridges and stagger)."""
        M.seg(l, h, lo=2)
        cs = M.end(l); c = l + '"'
        M.strand(c, cs, label=l); M.node(l, c)
        d0 = M.seg(l, h, lo=2)
        M.seg(l, turn(h, sigma), lo=2, hi=2)
        M.seg(c, turn(h, sigma), lo=2, hi=2)
        d1 = M.seg(c, h, lo=2)
        M.tie(M.end(c), M.end(l)); M.copies[c] = (l, d0.i, d0.i + 2)
        M.seg(l, h, lo=2)
        M.seg(l, turn(h, -sigma), lo=2, hi=2)
        n0, f0 = M.deck(d0, xs_of(d0), 4, 0); n1, f1 = M.deck(d1, xs_of(d1), 0, 4)
        M.add(f0 - n0 - (f1 - n1) == 0)
        M.add(dot(D[h], d0.a) + n0 - dot(D[h], d1.a) - n1 == 2)
        return d0, d1

    def block227(h, fA, cA, cB, fB, cA0, cA1, cB0, cB1, c0, c1, L0, L1):
        """The PSG 227 join block (DESIGN-blocks 2): left bundle (fixed fA, choice cA) on lines 1, 2; right bundle (choice cB, fixed fB) on 5, 6."""
        r = right(h); R, Lt = turn(h, +1), turn(h, -1)
        M.add(dot(r, M.end(cB)) - dot(r, M.end(cA)) == 6)   # lines 3 and 4 carry the crossing legs
        M.seg(fA, h, lo=2 * P + 2)                           # R9: the fixed lanes straight through the prio coverage
        M.seg(fB, h, lo=2 * P + 2)
        M.seg(cA, h, lo=2); sA = M.end(cA)
        M.seg(cB, h, lo=2); sB = M.end(cB)
        for a, b in ((cA0, sA), (cA1, sA), (cB0, sB), (cB1, sB)): M.strand(a, b)
        M.node(cA, cA0, cA1); M.node(cB, cB0, cB1)
        M.pred.update({cA0: cA, cA1: cA, cB0: cB, cB1: cB})
        M.seg(cA1, R, lo=2, hi=2)                            # cA1: up to line 3, bridge, down two lines onto line 5
        dA = M.seg(cA1, h, lo=2)
        M.seg(cA1, R, lo=4, hi=4)
        M.seg(cB0, Lt, lo=2, hi=2)                           # cB0: up to line 4, run, down two lines under cA1's span onto line 2
        M.seg(cB0, h, lo=2)
        xB = M.seg(cB0, Lt, lo=4, hi=4)
        M.deck(dA, [M.cross(dA, xB, 4)])
        M.seg(cA0, h, lo=2); M.tie(M.end(cA0), M.end(cB0)); mC0 = M.end(cA0)
        M.seg(cB1, h, lo=2); M.tie(M.end(cB1), M.end(cA1)); mC1 = M.end(cB1)
        for leg in (cA0, cB1):                               # R5: split tile, a whole-train bay (2 TL) to the bay signal, then the merge tile
            M.add(sum((sg.L for sg in M.strands[leg]), Ex()) >= 2 * TL + 4)
        M.strand(c0, mC0); M.node(cA0, cB0, c0); M.pred[c0] = cA0
        M.strand(c1, mC1); M.node(cA1, cB1, c1); M.pred[c1] = cB1
        M.seg(c0, h, lo=2); M.seg(c0, Lt, lo=2, hi=2); M.tie(M.end(c0), M.end(fA)); mL0 = M.end(fA)
        M.seg(c1, h, lo=2); M.seg(c1, R, lo=2, hi=2); M.tie(M.end(c1), M.end(fB)); mL1 = M.end(fB)
        M.strand(L0, mL0); M.node(fA, c0, L0); M.pred[L0] = fA
        M.strand(L1, mL1); M.node(c1, fB, L1); M.pred[L1] = fB
        M.prio_rows.append((fA, mL0, h, -1)); M.prio_rows.append((fB, mL1, h, +1))
        # Out: the inside lane (L1, toward the arm's in port) runs straight to its port; the outside lane closes up beside it straight after its merge.
        M.seg(L1, h, lo=2 + STUB)
        # The outside lane closes up early (D317 amended, engineer's rig edit): a big step straight after its merge to run
        # beside the right bundle's joiner, then the last one-line step into the slot the joiner leaves at its merge.
        M.seg(L0, h, lo=2, cost=FANW)                        # merge signal room; short: the step comes early
        M.seg(L0, R, lo=2)
        if h in STEP2:                                       # per block: one step, or two (the last at the joiner's merge)
            M.seg(L0, h, lo=0)
            M.seg(L0, R, lo=2, hi=2)
        M.seg(L0, h, lo=STUB)
        M.add(dot(r, M.end(L1)) - dot(r, M.end(L0)) == 2)

    def fray(h, sigma, far, near, S_far, S_near, V_far, V_near, prio, cont=False, dform=False, nearfan=True, fan=True, nsplit=False):
        if nsplit:
            # The near lane splits before its fan (engineer's rig edit, 2026-11-16; the fan turns toward the diverging side):
            # its straight leg takes one tile then the fan, its diverging leg stays on the fan's diagonal; the far lane fans late.
            assert turn(h, -1) == turn(h, sigma) and not prio
            Lt = turn(h, -1)
            M.seg(far, h, lo=STUB); ff = M.seg(far, Lt, lo=2); M.seg(far, h, lo=2, cost=FANW)
            M.seg(near, h, lo=STUB)
            sf, sn = M.end(far), M.end(near)
            M.strand(S_far, sf); M.strand(V_far, sf); M.node(far, S_far, V_far); M.pred[S_far] = far; M.pred[V_far] = far
            M.strand(S_near, sn); M.strand(V_near, sn); M.node(near, S_near, V_near); M.pred[S_near] = near; M.pred[V_near] = near
            M.seg(S_near, h, lo=2)
            nf = M.seg(S_near, Lt, lo=2)
            M.add(nf.L - ff.L == 0)
            M.seg(S_far, h, lo=2)
            vf = M.seg(V_far, turn(h, sigma), lo=6, hi=6)
            if not cont: M.seg(V_far, h, lo=2)
            M.seg(V_near, turn(h, sigma), lo=6)
            if not cont: M.seg(V_near, h, lo=2)
            d0 = M.seg(S_near, h, lo=2)
            M.deck(d0, [M.cross(d0, vf, 4)])
            return
        if dform:
            # The diagonal fray (engineer's rig edit, 2026-11-16): both lanes fan away from the diverging side; the near lane
            # splits at the top of its fan (S_near straight on), its diverging leg climbs one line more into the gap beside
            # S_far, and both diverging legs turn toward sigma side by side under one deck on S_near.
            r = right(h); Lt = turn(h, -sigma)
            M.seg(far, h, lo=STUB); ff = M.seg(far, Lt, lo=4 if nearfan else 2)
            if nearfan:
                M.seg(near, h, lo=STUB); nf = M.seg(near, Lt, lo=2)
                M.add(dot(D[h], ff.a) - dot(D[h], nf.a) == 0)
                M.add(ff.L - nf.L == 2)                            # far climbs one line more: two lines between S_far and S_near
            else:                                                  # the inside lane runs straight; only the outside lane steps out
                M.seg(near, h, lo=STUB)
                M.add(ff.L == 2)
            M.seg(far, h, lo=2)                                    # far lane on to its split
            sf, sn = M.end(far), M.end(near)
            M.strand(S_far, sf); M.strand(V_far, sf); M.node(far, S_far, V_far); M.pred[S_far] = far; M.pred[V_far] = far
            M.strand(S_near, sn); M.strand(V_near, sn); M.node(near, S_near, V_near); M.pred[S_near] = near; M.pred[V_near] = near
            M.seg(V_near, Lt, lo=2, hi=2)                          # into the gap line
            M.seg(V_near, h, lo=2)
            M.seg(S_far, h, lo=2)
            dn = M.seg(S_near, h, lo=2)                            # its deck spans both diverging diagonals (added after V's bundle)
            M.seg(S_near, h, lo=2)
            M.pending.append((dn, V_far, V_near))
            return
        fl = []
        for x in (far, near):
            M.seg(x, h, lo=STUB)                         # tight stub at the port
            if not fan: continue                         # no fan: the lanes run straight from the port into the fray
            fl.append(M.seg(x, turn(h, -1), lo=2))       # fan to the left of travel, as late as possible
            M.seg(x, h, lo=2, cost=FANW)
        if fan:
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
        if not cont: M.seg(V_far, h, lo=2)   # cont: the diverging bundle stays on the diagonal (no straight piece after the split)
        M.seg(V_near, turn(h, sigma), lo=4 if prio else 6, hi=4 if prio else 6)
        if not cont: M.seg(V_near, h, lo=2)
        if prio:  # near lane on line 2: the parallelogram steps inward to line 1 (V_far's diagonal meets line 1 first, D118)
            M.seg(S_near, h, lo=4)                      # split tile, one signal tile, then the copy split
            cs = M.end(S_near)
            c = S_near + "'"
            M.strand(c, cs, label=S_near); M.node(S_near, c)
            d0 = M.seg(S_near, h, lo=2)                 # copy 0 deck (line 2)
            M.seg(S_near, turn(h, -sigma), lo=2, hi=2)  # copy 0 bends after its bridge
            M.seg(c, turn(h, -sigma), lo=2, hi=2)       # copy 1 bends before its bridge
            d1 = M.seg(c, h, lo=2)                      # copy 1 deck (line 1), runs to the merge
            M.tie(M.end(c), M.end(S_near)); M.copies[c] = (S_near, d0.i, d0.i + 2)
            M.seg(S_near, h, lo=2)                      # merge signal room; now beside S_far, no shift back
            n0, f0 = M.deck(d0, [M.cross(d0, vf, 4)], 4, 0); n1, f1 = M.deck(d1, [M.cross(d1, vf, 4)], 0, 4)  # D145 tight heads at both ends
            M.add(f0 - n0 - (f1 - n1) == 0)  # D118: equal bridges
            south, north = ((d0, n0), (d1, n1)) if right(h)[1] * sigma > 0 else ((d1, n1), (d0, n0))
            if LEANMODE == 'all': M.add(head_u(*north) - head_u(*south) == LEAN)
            else: M.add(dot(D[h], d0.a) + n0 - dot(D[h], d1.a) - n1 == 2)  # natural D118 stagger
        else:
            d0 = M.seg(S_near, h, lo=2)
            M.deck(d0, [M.cross(d0, vf, 4)], n_min=4)  # split tile, one signal tile, then the head

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
            n0, f0 = M.deck(d0, [M.cross(d0, jf, 4)], 4, 0); n1, f1 = M.deck(d1, [M.cross(d1, jf, 4)], 0, 4)  # D145 tight heads at both ends
            M.add(f0 - n0 - (f1 - n1) == 0)  # D118: equal bridges
            south, north = ((d1, n1), (d0, n0)) if right(h)[1] * sigma > 0 else ((d0, n0), (d1, n1))
            if LEANMODE == 'all': M.add(head_u(*north) - head_u(*south) == LEAN)
            else: M.add(dot(D[h], d0.a) + n0 - dot(D[h], d1.a) - n1 == 2)  # natural D118 stagger
            M.seg(J_near, h, lo=2)
            M.seg(J_near, turn(h, -sigma), lo=4, hi=4)
        else:
            d0 = M.seg(S_near, h, lo=2)
            M.deck(d0, [M.cross(d0, jf, 4)], tail_min=4)  # the far head, one signal tile, then the merge tile
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
                segs[l].append(M.seg(l, h, lo=lo, cost=1.0 if axis(h) else TUCK))  # a turn's diagonal only as long as it must be
            for j, l in enumerate(lanes[1:], 1):
                M.add(dot(r, segs[l][k].a) - dot(r, segs[lanes[0]][k].a) == 2 * j)
                M.add(dot(r, segs[l][k].b) - dot(r, segs[lanes[0]][k].b) == 2 * j)
        bundles[name] = (lanes, heads, segs)
        return segs

    # Frays.
    M.pending = []
    M.tag = 'frayW'; fray('E', +1, 'EF1', 'EF2', 'E1', 'E2', 'F1', 'F2', False, FCONT or FDIAG, FDIAG)
    FRAYMIN = next((int(x.split('=')[1]) for x in sys.argv if x.startswith('--fraymin=')), None)
    if FRAYMIN is not None: M.add(M.start['EF1'][1] - M.start['E1'][1] <= 2 * FRAYMIN)  # experiment: the in-fan climbs at most this many rows
    M.tag = 'frayE'; fray('W', -1, 'AB2', 'AB1', 'B2', 'B1', 'A2', 'A1', False, EFORM == 2, nsplit=EFORM > 0)
    M.tag = 'frayS'
    dS = SFORM in (1, 2)
    if fray_s > 0: fray('N', +1, 'CD1', 'CD2', 'C1', 'C2', 'D1', 'D2', False, dS, dS, SFORM == 1, SFORM != 3)
    else: fray('N', -1, 'CD2', 'CD1', 'D2', 'D1', 'C2', 'C1', False, dS, dS, SFORM == 1, SFORM != 3)

    # Bundles (lanes left -> right of travel).
    M.tag = 'bundles'
    if FDIAG:  # E1 and E2 leave the fray two lines apart and close up in the climb (E2's diagonal 2 longer)
        e = {l: [M.seg(l, 'E', lo=0), M.seg(l, 'NE', lo=2), M.seg(l, 'E', lo=0)] for l in ('E1', 'E2')}
        M.add(e['E2'][1].L - e['E1'][1].L == 2)
        M.add(dot(right('E'), e['E2'][2].a) - dot(right('E'), e['E1'][2].a) == 2)
        M.add(dot(D['E'], e['E2'][1].a) - dot(D['E'], e['E1'][1].a) == 0)
        sE = e
    else:
        sE = bundle('E', ['E1', 'E2'], ['E'] if jogE == 0 else ['E', turn('E', jogE), 'E'])  # optional S-bend (a step across) on the main-line bundle
    sF = bundle('F', ['F1', 'F2'], ['SE', 'S'] if (FCONT or FDIAG) else ['E', 'SE', 'S'])
    hD = ['N', 'NE', 'E'] if tri == 0 else ['N', 'NE', 'N', 'NE', 'E']
    hC = ['N', 'NW', 'W']
    offD = 1 if (SFORM in (1, 2) and fray_s > 0) else 0
    if offD: hD = hD[1:]
    if SFORM in (1, 2) and fray_s < 0: hC = hC[1:]
    if offD and tri == 0: raise ValueError('B crosses D on the N piece the diagonal fray removes')
    sD = bundle('D', ['D1', 'D2'], hD)
    sC = bundle('C', ['C1', 'C2'], hC)
    sA = bundle('A', ['A1', 'A2'], ['SW', 'S'] if EFORM == 2 else ['W', 'SW', 'S'])
    def first_seg(lane): return next(sg[lane][0] for ln, hs, sg in bundles.values() if lane in ln)
    for dn, va, vb in M.pending:
        M.deck(dn, [M.cross(dn, first_seg(v), 4) for v in (va, vb)], n_min=4)
    # B: run; B2 (prio) doubled outward, B1 (choice) single (D299); run.
    M.tag = 'bundleB'
    lanesB = ['B1', 'B2']
    run1 = bundle('B', lanesB, ['W'] if jogB == 0 else ['W', turn('W', jogB), 'W'])
    def core_xs(dk):
        return [M.cross(dk, sF[f][-1], 3) for f in ('F1', 'F2')], [M.cross(dk, sD[d][(0 if tri == 0 else 2) - offD], 3) for d in ('D1', 'D2')]
    def core_decks(dk, first, last):
        xs_F, xs_D = core_xs(dk)
        nF, fF = M.deck(dk, xs_F, None if tri == 1 else first, last if tri == 1 else None)
        nD, fD = M.deck(dk, xs_D, first if tri == 1 else None, None if tri == 1 else last)
        if tri == 0: M.add(nD - fF >= 2)
        else: M.add(nF - fD >= 2)
        return fF - nF, fD - nD, head_u(dk, nF), head_u(dk, nD)
    M.tag = 'coreB'
    b1 = M.seg('B1', 'W', lo=2)
    core_decks(b1, None, None)
    h = 'W'; l = 'B2'; sigma = +1
    M.seg(l, h, lo=2)
    cs = M.end(l); c = l + '"'
    M.strand(c, cs, label=l); M.node(l, c)
    d0 = M.seg(l, h, lo=2)
    M.seg(l, turn(h, sigma), lo=2, hi=2)
    M.seg(c, turn(h, sigma), lo=2, hi=2)
    d1 = M.seg(c, h, lo=2)
    M.tie(M.end(c), M.end(l)); M.copies[c] = (l, d0.i, d0.i + 2)
    M.seg(l, h, lo=2)
    M.seg(l, turn(h, -sigma), lo=2, hi=2)
    sp0 = core_decks(d0, 4, 0); sp1 = core_decks(d1, 0, 4)
    M.add(sp0[0] - sp1[0] == 0); M.add(sp0[1] - sp1[1] == 0)   # D118 equal bridges
    M.add(sp0[2] - sp1[2] == -2); M.add(sp0[3] - sp1[3] == -2)  # copy 1 one tile upstream
    run2 = {}
    for l in lanesB: run2[l] = M.seg(l, 'W', lo=0)
    M.add(dot(right('W'), run2['B2'].a) - dot(right('W'), run2['B1'].a) == 2)

    M.tag = 'coreDF'
    if df_over == 'F':  # F's southbound run over D's NE diagonal; F2 (prio) doubled outward
        xs = lambda dk: [M.cross(dk, sD[d][1 - offD], 4) for d in ('D1', 'D2')]
        f1 = M.seg('F1', 'S', lo=2); M.deck(f1, xs(f1))
        doubled('F2', 'S', +1, xs)
        fr = {f: M.seg(f, 'S', lo=0) for f in ('F1', 'F2')}
        M.add(dot(right('S'), fr['F2'].a) - dot(right('S'), fr['F1'].a) == 2)
    else:               # D's northbound run over F's SE diagonal; D2 (prio) doubled outward (segments before D's bundle)
        raise ValueError('D over F: not built for LL2 yet')

    # Joins: the 227 block at every exit (fixed lanes outside, choice lanes inside).
    M.tag = 'joinE'; block227('E', 'E1', 'E2', 'D1', 'D2', 'E2a', 'E2b', 'D1a', 'D1b', 'EDj1', 'EDj2', 'EO1', 'EO2')
    M.tag = 'joinW'; block227('W', 'C1', 'C2', 'B1', 'B2', 'C2a', 'C2b', 'B1a', 'B1b', 'CBj1', 'CBj2', 'WO1', 'WO2')
    M.tag = 'joinS'; block227('S', 'A1', 'A2', 'F1', 'F2', 'A2a', 'A2b', 'F1a', 'F1b', 'AFj1', 'AFj2', 'SO1', 'SO2')

    # Out ports on the boundary.
    M.tag = 'ports'
    for n, side in outs.items():
        u, v = M.end(n)
        if side == 'W': M.add(u == 0)
        if side == 'E': M.add(u - X == 0)
        if side == 'S': M.add(v - Y == 0)
    # Port order per side (reading order), adjacent within a port.
    def vv(n, start): return (M.start[n] if start else M.end(n))
    order = {'W': [('EF1', 1), ('EF2', 1), ('WO2', 0), ('WO1', 0)], 'E': [('EO1', 0), ('EO2', 0), ('AB2', 1), ('AB1', 1)],
             'S': [('CD1', 1), ('CD2', 1), ('SO2', 0), ('SO1', 0)]}
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
    if '--align' in sys.argv:  # the main line's ports on the same row at both ends
        M.add(M.start['EF1'][1] - M.end('EO1')[1] == 0)
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
    M.routes, M.outline_routes = LL2_ROUTES, LL2_OUTLINE
    M.tag = 'curve'; route_curves(M, LL2_ROUTES)
    M.bundles = bundles
    return M


LL2_ROUTES = [
    ('EF1', 'E1', 'EO1'), ('EF2', 'E2', 'E2a', 'EDj1', 'EO1'), ('EF2', 'E2', 'E2b', 'EDj2', 'EO2'),
    ('CD1', 'D1', 'D1a', 'EDj1', 'EO1'), ('CD1', 'D1', 'D1b', 'EDj2', 'EO2'), ('CD2', 'D2', 'EO2'),
    ('EF1', 'F1', 'F1a', 'AFj1', 'SO1'), ('EF1', 'F1', 'F1b', 'AFj2', 'SO2'), ('EF2', 'F2', 'SO2'),
    ('AB1', 'A1', 'SO1'), ('AB2', 'A2', 'A2a', 'AFj1', 'SO1'), ('AB2', 'A2', 'A2b', 'AFj2', 'SO2'),
    ('AB1', 'B1', 'B1a', 'CBj1', 'WO1'), ('AB1', 'B1', 'B1b', 'CBj2', 'WO2'), ('AB2', 'B2', 'WO2'),
    ('CD1', 'C1', 'WO1'), ('CD2', 'C2', 'C2a', 'CBj1', 'WO1'), ('CD2', 'C2', 'C2b', 'CBj2', 'WO2')]
LL2_OUTLINE = [('EF1', 'E1', 'EO1'), ('AB1', 'A1', 'SO1'), ('CD1', 'C1', 'WO1')]
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


T2_OUTLINE = [('EF1', 'E1', 'ED1'), ('AB1', 'A1', 'AF1'), ('CD1', 'C1', 'CB1')]


def outline(M):
    """The junction's outline: the outer route between each pair of neighbouring arms, joined along the box edges."""
    pts = []
    for route in M.outline_routes:
        seq = route_segs(M, route)[0]
        pts.append(seq[0].a)
        for sg in seq: pts.append(sg.b)
    return pts


def area_of(val, pts):
    P_ = [(ev(val, p[0]), ev(val, p[1])) for p in pts]
    return 0.5 * sum(P_[i][0] * P_[(i + 1) % len(P_)][1] - P_[(i + 1) % len(P_)][0] * P_[i][1] for i in range(len(P_)))


def linearise_area(M, val):
    """Objective coefficients of the area's gradient at val (tiles; shoelace, sign from the current orientation)."""
    pts = outline(M)
    P_ = [(ev(val, p[0]), ev(val, p[1])) for p in pts]
    sgn = 1 if area_of(val, pts) >= 0 else -1
    n = len(P_); coef = {}
    for i in range(n):
        gx = 0.5 * (P_[(i + 1) % n][1] - P_[i - 1][1]); gy = 0.5 * (P_[i - 1][0] - P_[(i + 1) % n][0])
        for e, g in ((pts[i][0], gx), (pts[i][1], gy)):
            for k, v in lift(e).d.items(): coef[k] = coef.get(k, 0) + sgn * g * v * AREA_W / 4
    M.obj_extra = coef


def solve_lazy(M, t0):
    """Area inside the outline by sequential linearisation around the lazy-separated solution."""
    M.obj_extra = {}
    res, why = solve_lazy1(M, t0, [])
    if res is None: return res, why
    if not AREA_W:
        M.area = abs(area_of(res[0][0], outline(M))) / 4
        return res[:4], why
    best = (abs(area_of(res[0][0], outline(M))), res)
    seps = res[4]
    for it in range(6):
        linearise_area(M, best[1][0][0])
        r2, why2 = solve_lazy1(M, t0, seps)
        if r2 is None: break
        a2 = abs(area_of(r2[0][0], outline(M)))
        if a2 >= best[0] - 1: break
        best = (a2, r2); seps = r2[4]
    M.area = best[0] / 4
    return best[1][:4], None


def solve_lazy1(M, t0, seps0):
    seps = list(seps0)
    r = M.solve(seps)
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
        if not bad and not under_bad: return (r, geo, decks, rnd, seps), None
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


def export_routes(M, geo, decks, val, path):
    """Route programs for the C++ checker hook: per route variant (copies by mask) its pieces (x y heading kind line) and copy switches."""
    deck_of = {}
    for k, n, f in decks: deck_of.setdefault(k, []).append((n, f))
    W, H = round(ev(val, M.X)) // 2 + 1, round(ev(val, M.Y)) // 2 + 1
    out = [f'size {W} {H}']
    for route in M.routes:
        name = route[1]
        # variants with region marks: per route line, the copy regions (strand, k, r_, c)
        per_line = []
        for li, st in enumerate(route):
            cps = sorted((k, r_, c) for c, (s_, k, r_) in M.copies.items() if s_ == st)
            opts = []
            for mask in range(1 << len(cps)):
                chosen = {cps[b][0]: cps[b] for b in range(len(cps)) if mask >> b & 1}
                region_at = {k: (k, r_, c) for k, r_, c in cps}
                seq, regions, i = [], [], 0
                segs = M.strands[st]
                while i < len(segs):
                    if i in region_at:
                        k, r_, c = region_at[i]
                        part = M.strands[c] if i in chosen else segs[k:r_]
                        regions.append((len(seq), len(seq) + len(part) - 1))
                        seq += [(s, li) for s in part]; i = r_
                    else:
                        seq.append((segs[i], li)); i += 1
                opts.append((seq, regions))
            per_line.append(opts)
        variants = [([], [])]
        for opts in per_line:
            variants = [(v[0] + o[0], v[1] + [(a + len(v[0]), b + len(v[0])) for a, b in o[1]]) for v in variants for o in opts]
        for copy, (seq, regions) in enumerate(variants):
            ps, seg_first, seg_last = [], [], []
            for s, li in seq:
                u, v, L, d = geo[(s.strand, s.i)]
                dk = deck_of.get((s.strand, s.i))
                seg_first.append(len(ps))
                for tile, kind, _, t in pieces(u, v, L, d):
                    k = 'P'
                    for n_, f_ in dk or []:
                        if n_ <= t and t + 2 <= f_:
                            k = 'H' if (t == n_ or t + 2 == f_) else 'M'
                    ps.append((tile[0], tile[1], HEAD.index(d), k, li))
                seg_last.append(len(ps) - 1)
            sw = []
            for a, b in regions:
                sw.append(f'S {seg_first[a]}'); sw.append(f'M {seg_last[b]}')
            out.append(f'route {name} {copy} {len(ps)} ' + ','.join(route) + ' ' + ' '.join(sw))
            out += [f'{x} {y} {h} {k} {li}' for x, y, h, k, li in ps]
    open(path, 'w').write('\n'.join(out) + '\n')


if __name__ == '__main__':
    t0 = time.perf_counter()
    results = []
    stairs = (True, False) if LEANMODE == 'auto' else ((True,) if LEANMODE == 'all' else (False,))
    combos = [(fs, je, 'F', tri, jb) for fs in (1, -1) for tri in (0, 1) for je in (0, -1, 1) for jb in (0, -1, 1)]
    only = next((a.split('=')[1] for a in sys.argv if a.startswith('--only=')), None)
    if only: combos = [c for c in combos if f'{c[0]:+d}{c[1]:+d}{c[2]}{c[3]}{c[4]:+d}' == only]
    combos = [c + (sf, ef) for c in combos for sf in (0, 3) for ef in (0, 1, 2)]
    for fs, js, o, tri, st, sf, ef in combos:
        ts = time.perf_counter()
        SFORM, EFORM = sf, ef
        try: M = build_t2(fs, js, o, tri, st)
        except ValueError as e: print('skip', e); continue
        res, why = solve_lazy(M, ts)
        tag = f'{fs:+d}{js:+d}{o}{tri}{st:+d}s{sf}e{ef}'
        if res is None:
            print(f'{tag}: NO ({why}) {time.perf_counter() - ts:.2f}s'); continue
        (val, obj), geo, decks, rnd = res
        W, H = round(ev(val, M.X)) // 2 + 1, round(ev(val, M.Y)) // 2 + 1
        print(f'{tag}: area {getattr(M, "area", 0):.0f} {W}x{H} obj {obj:.0f} sep rounds {rnd} {time.perf_counter() - ts:.2f}s gaps(tiles) {[round(ev(val, g)) // 2 for g in M.gaps]}')
        results.append((M.area, tag, M, val, geo, decks))
    print(f'total {time.perf_counter() - t0:.2f}s')
    if results:
        results.sort(key=lambda r: r[0])
        best_tag = results[0][1]
        c = next(c for c in combos if f'{c[0]:+d}{c[1]:+d}{c[2]}{c[3]}{c[4]:+d}s{c[5]}e{c[6]}' == best_tag)
        for k in range(1, 8):
            STEP2 = {h for i, h in enumerate('ESW') if k >> i & 1}
            SFORM, EFORM = c[5], c[6]
            ts = time.perf_counter()
            M = build_t2(c[0], c[1], c[2], c[3], c[4])
            res, why = solve_lazy(M, ts)
            if res is None: print(best_tag, sorted(STEP2), 'NO'); continue
            (val, obj), geo, decks, rnd = res
            print(f'{best_tag} two-step {"".join(sorted(STEP2))}: area {M.area:.0f}')
            results.append((M.area, best_tag + '/2' + ''.join(sorted(STEP2)), M, val, geo, decks))
        results.sort(key=lambda r: r[0])
        obj, tag, M, val, geo, decks = results[0]
        print('best', tag)
        print(render(M, geo, decks, val))
        exp = next((a.split('=', 1)[1] for a in sys.argv if a.startswith('--export=')), None)
        if exp:
            out = export(M, geo, decks, val, exp)
            print(f'exported {exp}: {len(out["tracks"])} tiles, {len(out["bridges"])} bridges')
        rexp = next((a.split('=', 1)[1] for a in sys.argv if a.startswith('--routes=')), None)
        if rexp:
            export_routes(M, geo, decks, val, rexp); print(f'routes {rexp}')


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
