"""Probe v2: choices first, then positions (HiGHS, integer coords, no binaries), on the T at 1 and 2 lanes.

Geometry: half-tile coordinates (u east, v south). Track points sit on tile-edge midpoints (u + v odd).
An axis segment of length L half-tiles moves L along its axis (L even); a diagonal one moves (L, L), one
half piece per unit (a diagonal half piece counts half a tile, as CL does).
Topology mirrors PlanarTText(lanes) (main W-E, branch S, left-hand running): A E->S, B E->W, C S->W,
D S->E, E W->E, F W->S; crossings as the C++ derivation (lanes 1: B×D B×F D×F; lanes 2: 12 core + 6 fray).
Rules modelled: port order and lane spacing, split/merge legs (straight leg + 45-degree leg), fixed segment
directions per line with spare S-bends present (>= curve gap) or absent (= 0) as a choice, curve gaps
between consecutive bends (incl. the node bend), crossing vertex shared with head floors (3 perpendicular /
4 diagonal half-tiles each side on the axis-aligned over line, 1 on the under line), unrelated tracks never
share an incompatible tile or touch (shared edge midpoint), enforced lazily by one of 8 separation sectors
chosen by push (least violated now), backtracking on infeasibility.
NOT modelled: signals/prio floors, bridge-over-bridge / consecutive-crossing rules, bundle same-form rule.
"""
import itertools, math, sys, time
import highspy

INF = highspy.kHighsInf
D = {'E': (1, 0), 'W': (-1, 0), 'N': (0, -1), 'S': (0, 1),
     'NE': (1, -1), 'NW': (-1, -1), 'SE': (1, 1), 'SW': (-1, 1)}
G = 4          # curve gap, half-tiles (2 tiles): min(2TL-1, max(2, CL)) with CL 2
STUB = 4       # port stub, half-tiles
LANE = 2       # adjacent rows/columns
SEP = 2        # least separation along a sector's axis (adjacent rows, parallel diagonals sharing a tile)
TIME_BUDGET = float(sys.argv[2]) if len(sys.argv) > 2 else 30.0


def axis(d): return 0 in D[d]
def parallel(a, b): return D[a][0] * D[b][1] - D[a][1] * D[b][0] == 0


class Ex:
    """Linear expression over builder variable indices."""
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


DIVE = '--dive' in sys.argv
best_x = [None]
DOUBLE = '--double' in sys.argv
MERGED = '--merged' in sys.argv
COST = '--cost' in sys.argv  # D114: footprint tiles first (W per tile), then money at game prices; box only as a tie-break
W_TILE, RAIL = 1000.0, 100.0
AREA = None  # (X0, Y0) half-tiles: box area linearised at the root solution (--area)


def seg_cost(d):
    if not COST: return 0.01
    tiles_per_unit = 0.5 if 0 in D[d] else 1.0  # axis: 2 half-tiles per tile piece; diagonal: one half piece (one tile corner) per unit
    return (W_TILE + RAIL) * tiles_per_unit


def bridge_len_factor(n):
    if n < 2: return n
    n -= 2; s = 2; d = 1
    while True:
        for _ in range(d):
            if n == 0: return s
            s += d; n -= 1
        d += 1


def bridge_extra(head):
    # Middle tiles: head 3 -> 1, head 4 -> 2 (heads at +-head half-tiles); price above the rail it replaces (tubular, price 240, base 275).
    n = 1 if head == 3 else 2
    return (bridge_len_factor(n + 2) * 275 * 240) / 256
BATCH = '--batch' in sys.argv
EMBED = '--embed' in sys.argv
TRACE = '--trace' in sys.argv
JOGS = '--jogs' in sys.argv
SPANCLEAR = '--spanclear' in sys.argv
FREEFAN = '--channels' in sys.argv
GROUPS = '--groups' in sys.argv
FREESTUB = '--freestub' in sys.argv  # bundled approach (port stub runs) is main line: no junction cost
trace = []
DPEN = float(next((a.split('=')[1] for a in sys.argv if a.startswith('--dpen=')), 0))
from topo5 import turn as topo_turn, rot as rot8
SCORE_ONLY = False
dive_n = [0]


class Builder:
    def __init__(s):
        s.lb, s.ub, s.obj, s.rows, s.par = [], [], [], [], {}
    def var(s, lb=0.0, ub=INF, c=0.0):
        s.lb.append(lb); s.ub.append(ub); s.obj.append(c); return Var(len(s.lb) - 1)
    def add(s, con):
        kind, e = con
        d = {k: v for k, v in e.d.items() if v != 0}
        s.rows.append((d, -e.c, -e.c if kind == 'eq' else INF))
    def parity(s, x, p): s.par[x.i] = p
    def dive_order(s):
        # X, Y first (columns 0, 1), then the rest in creation order (nodes, then segments).
        return list(range(len(s.lb)))

    def _run(s, n, lb, ub, obj, rows, integer, dive=None):
        h = highspy.Highs(); h.silent(); h.setOptionValue('time_limit', 5.0)
        lp = highspy.HighsLp()
        lp.num_col_ = n; lp.num_row_ = len(rows)
        lp.col_cost_ = obj; lp.col_lower_ = lb; lp.col_upper_ = ub
        lp.row_lower_ = [r[1] for r in rows]; lp.row_upper_ = [r[2] for r in rows]
        starts, idx, vals = [], [], []
        cols = [[] for _ in range(n)]
        for ri, (d, _, _) in enumerate(rows):
            for k, v in d.items(): cols[k].append((ri, v))
        for k in range(n):
            starts.append(len(idx))
            for ri, v in cols[k]: idx.append(ri); vals.append(v)
        starts.append(len(idx))
        lp.a_matrix_.format_ = highspy.MatrixFormat.kColwise
        lp.a_matrix_.start_ = starts; lp.a_matrix_.index_ = idx; lp.a_matrix_.value_ = vals
        if integer: lp.integrality_ = [highspy.HighsVarType.kInteger] * n
        h.passModel(lp)
        h.run()
        if h.getModelStatus() != highspy.HighsModelStatus.kOptimal: return None
        val = list(h.getSolution().col_value)
        if dive is None: return val
        # Dive: round one half-integral column up (then down), warm-started, until integral.
        order = dive
        while True:
            fr = [k for k in order if abs(val[k] - round(val[k])) > 1e-6]
            if not fr: return val
            k = fr[0]; dive_n[0] += 1
            lo0, hi0 = lb[k], ub[k]
            h.changeColBounds(k, math.ceil(val[k]), math.ceil(val[k])); h.run()
            if h.getModelStatus() != highspy.HighsModelStatus.kOptimal:
                h.changeColBounds(k, math.floor(val[k]), math.floor(val[k])); h.run()
                if h.getModelStatus() != highspy.HighsModelStatus.kOptimal: return 'fail'
            val = list(h.getSolution().col_value)

    def solve_mip(s):
        # Parity facts as x - 2k == p with a new integer k (as probe2).
        n = len(s.lb); lb, ub, obj, rows = list(s.lb), list(s.ub), list(s.obj), list(s.rows)
        for x, p in s.par.items():
            lb.append(-INF); ub.append(INF); obj.append(0.0)
            rows.append(({x: 1, n: -2}, p, p)); n += 1
        val = s._run(n, lb, ub, obj, rows, True)
        return val

    def parities(s):
        """Each variable's parity from the parity facts and the equality rows (mod 2), by propagation then GF(2) elimination."""
        n = len(s.lb)
        par = dict(s.par)
        for k in range(n):
            if s.lb[k] == s.ub[k]: par[k] = int(s.lb[k]) % 2
        eqs = [(d, int(lo)) for d, lo, hi in s.rows if lo == hi]
        changed = True
        while changed:
            changed = False
            for d, rhs in eqs:
                unk = [k for k, v in d.items() if v % 2 and k not in par]
                if len(unk) == 1:
                    par[unk[0]] = (rhs - sum(v * par[k] for k, v in d.items() if v % 2 and k in par)) % 2
                    changed = True
        und = [k for k in range(n) if k not in par]
        for k in und: par[k] = 0  # arbitrary; counted
        return par, len(und)

    def solve_plp(s, st, cache, key):
        t0 = time.perf_counter()
        if key in cache: par, und = cache[key]
        else: par, und = cache[key] = s.parities()
        st['parity_ms'] += (time.perf_counter() - t0) * 1000
        st['undetermined'] += und
        n = len(s.lb)
        lb = [math.ceil((s.lb[k] - par[k]) / 2) if s.lb[k] > -INF else -INF for k in range(n)]
        ub = [math.floor((s.ub[k] - par[k]) / 2) if s.ub[k] < INF else INF for k in range(n)]
        rows = []
        for d, lo, hi in s.rows:
            off = sum(v * par[k] for k, v in d.items())
            if lo == hi:
                r = lo - off
                if r % 2: return None  # parity-inconsistent
                rows.append((d, r / 2, r / 2))
            else:
                rows.append((d, math.ceil((lo - off) / 2), INF))
        st['lp'] += 1
        if DIVE:
            d0 = dive_n[0]
            m = s._run(n, lb, ub, list(s.obj), rows, False, dive=s.dive_order())
            if m is None: return None
            if m != 'fail':
                st['lp_integral'] += dive_n[0] == d0; st['dives'] = st.get('dives', 0) + dive_n[0] - d0
                return [2 * round(m[k]) + par[k] for k in range(n)]
            st['fallback'] += 1
            m = s._run(n, lb, ub, list(s.obj), rows, True)
            if m is None: return None
            return [2 * round(m[k]) + par[k] for k in range(n)]
        m = s._run(n, lb, ub, list(s.obj), rows, False)
        if m is None: return None
        if SCORE_ONLY: return [2 * x + par[k] for k, x in enumerate(m)]
        if all(abs(x - round(x)) < 1e-6 for x in m): st['lp_integral'] += 1
        else:
            st['fallback'] += 1
            m = s._run(n, lb, ub, list(s.obj), rows, True)
            if m is None: return None
        return [2 * round(m[k]) + par[k] for k in range(n)]


def t_topology(lanes):
    """Lines: name -> dict(start, end, dirs, mins, spare (indices of optional middle segments)); plus crossings."""
    L, ports = {}, {}
    ks = range(1, lanes + 1)
    def line(name, a, b, dirs, mins, spare=()):
        L[name] = dict(start=a, end=b, dirs=dirs, mins=mins, spare=set(spare))
    for k in ks:
        line(f'AB{k}', f'pAB{k}', f'sAB{k}', ['W'], [STUB])
        line(f'A{k}', f'sAB{k}', f'mAF{k}', ['SW', 'S', 'SW'], [2, 0, 2], [1])
        line(f'B{k}', f'sAB{k}', f'mCB{k}', ['W'], [2])
        line(f'CD{k}', f'pCD{k}', f'sCD{k}', ['N'], [STUB])
        line(f'C{k}', f'sCD{k}', f'mCB{k}', ['NW', 'N', 'NW'], [2, 0, 2], [1])
        line(f'D{k}', f'sCD{k}', f'mED{k}', ['N', 'NE', 'E', 'NE'], [2, 2, 0, 2], [2])
        line(f'EF{k}', f'pEF{k}', f'sEF{k}', ['E'], [STUB])
        line(f'E{k}', f'sEF{k}', f'mED{k}', ['E'], [2])
        line(f'F{k}', f'sEF{k}', f'mAF{k}', ['SE', 'E', 'SE', 'S'], [2, 0, 2, 2], [1])
        line(f'AF{k}', f'mAF{k}', f'pAF{k}', ['S'], [STUB])
        line(f'CB{k}', f'mCB{k}', f'pCB{k}', ['W'], [STUB])
        line(f'ED{k}', f'mED{k}', f'pED{k}', ['E'], [STUB])
    # Ports in boundary order (W side north->south, E side north->south, S side west->east), from the lane order rule.
    W = [f'pEF{k}' for k in ks] + [f'pCB{k}' for k in reversed(ks)]
    E = [f'pED{k}' for k in ks] + [f'pAB{k}' for k in reversed(ks)]
    S = [f'pCD{k}' for k in ks] + [f'pAF{k}' for k in reversed(ks)]
    cross = []
    for a in [f'B{k}' for k in ks]:
        for b in [f'D{k}' for k in ks] + [f'F{k}' for k in ks]: cross.append((a, b))
    for a in [f'D{k}' for k in ks]:
        for b in [f'F{k}' for k in ks]: cross.append((a, b))
    if lanes == 2:
        cross += [('A2', 'B1'), ('C2', 'D1'), ('E2', 'F1'), ('F1', 'A2'), ('C2', 'B1'), ('E2', 'D1')]
    return L, dict(W=W, E=E, S=S), cross


class Model:
    def __init__(self, lanes):
        import topo5 as topo_rule
        topo_rule.CHANNELS = '--channels' in sys.argv
        self.L, self.ports, self.cross, self.meta = topo_rule.from_json(lanes, STUB)
        nodes = {}
        for n, l in self.L.items():
            nodes.setdefault(l['start'], set()).add(n); nodes.setdefault(l['end'], set()).add(n)
        self.related = {frozenset((a, b)) for ls in nodes.values() for a, b in itertools.combinations(ls, 2)}
        self.cross_set = {frozenset(c) for c in self.cross}
        self.opts = []
        for p, q in self.cross:
            o = []
            for i, dp in enumerate(self.L[p]['dirs']):
                for j, dq in enumerate(self.L[q]['dirs']):
                    if parallel(dp, dq): continue
                    for over in (p, q):
                        if axis(dp if over == p else dq):
                            if DOUBLE and over in self.meta['prio_lines']: o += [(i, j, over, 1), (i, j, over, -1)]
                            else: o.append((i, j, over))
            self.opts.append(o)
        if GROUPS: self.build_units()

    def build_units(self):
        import re
        def fam(n):
            m = re.match(r'([A-Za-z]*?)(\d+)([a-z]*)$', n)
            return (m.group(1), m.group(3)) if m else (n, '')
        self.fam = fam
        free = lambda ln, i: i in self.L[ln].get('free', set())
        # Crossing options never use a free sideways S-bend piece.
        self.opts = [[o for o in os_ if not free(p, o[0]) and not free(q, o[1])] for (p, q), os_ in zip(self.cross, self.opts)]
        by = {}
        for c, (p, q) in enumerate(self.cross):
            key = tuple(sorted((fam(p), fam(q))))
            by.setdefault(key, []).append(c)
        units = []
        for key, cs in by.items():
            def side_lines(c, f):
                p, q = self.cross[c]
                return p if fam(p) == f else q
            uniform = len(cs) > 1 and key[0] != key[1] and all(
                len({tuple(self.L[side_lines(c, f)]['dirs']) for c in cs}) == 1 for f in key)
            if not uniform:
                units += [[c] for c in cs]; continue
            units.append(cs)
        self.units, self.gopts = [], []
        for cs in units:
            c0 = cs[0]; p0, q0 = self.cross[c0]
            gos = []
            for o in self.opts[c0]:
                go = {}
                ofam = fam(o[2])
                for c in cs:
                    p, q = self.cross[c]
                    i_, j_ = (o[0], o[1]) if fam(p) == fam(p0) else (o[1], o[0])
                    ov = p if fam(p) == ofam else q
                    go[c] = (i_, j_, ov) + tuple(o[3:])
                gos.append(go)
            self.units.append(cs); self.gopts.append(gos)

    def unit_name(self, u):
        return '+'.join(f'{self.cross[c][0]}x{self.cross[c][1]}' for c in self.units[u])

    RELAX = False

    BACKEND = 'mip'   # 'mip': integer vars as before; 'plp': parity folded into constants, plain LP (MIP fallback)
    pstats = dict(lp=0, lp_integral=0, fallback=0, undetermined=0, parity_ms=0.0, solve_ms=0.0)
    _pcache = {}

    def solve(self, fixed, spare, seps):
        """fixed: crossing index -> (i, j, over); spare: (line, seg) -> present; seps: list of (p, i, q, j, f, side, g)."""
        B = Builder()
        var = B.var
        X, Y = (var(c=W_TILE * AREA[1] / 4), var(c=W_TILE * AREA[0] / 4)) if (COST and AREA) else (var(c=1.0), var(c=1.0))
        node = {}
        for l in self.L.values():
            for n in (l['start'], l['end']):
                if n not in node: node[n] = (var(), var())
        for u, v in node.values():
            B.add(u <= X); B.add(v <= Y); B.add(v >= 1)
        for side, seq in self.ports.items():
            for n in seq:
                u, v = node[n]
                if side == 'W': B.add(u == 0); B.parity(v, 1)
                if side == 'E': B.add(u - X == 0); B.parity(v, 1)
                if side == 'S': B.add(v - Y == 0); B.parity(u, 1)
                if side == 'N': B.add(v == 0); B.parity(u, 1)
            for a, b in zip(seq, seq[1:]):
                ca, cb = node[a][0 if side == 'S' else 1], node[b][0 if side == 'S' else 1]
                B.add(cb - ca >= LANE)
                if self.meta['pgroup'][a] == self.meta['pgroup'][b]: B.add(cb - ca <= LANE)  # R16: one bundle's tracks on adjacent port lines
        self.seg = {}
        for name, l in self.L.items():
            u, v = node[l['start']]
            n = len(l['dirs'])
            for i, (d, m) in enumerate(zip(l['dirs'], l['mins'])):
                if i in l['spare'] and l.get('fan') and FREEFAN:  # fans: a free length (parity keeps it 0 or >= 2)
                    Lv = var(lb=0, c=seg_cost(d))
                elif i in l['spare']:
                    present = spare.get((name, i), False)
                    Lv = var(lb=(2 if (l.get('fan') and not axis(d)) else G) if present else 0, ub=INF if present else 0, c=seg_cost(d))
                else:
                    Lv = var(lb=m, ub=(m if i in l.get('exact', ()) else INF), c=(0.0 if (FREESTUB and m == STUB and (l['start'].startswith('p') or l['end'].startswith('p'))) else seg_cost(d)))
                if axis(d):
                    B.parity(u, 0 if D[d][1] == 0 else 1); B.parity(Lv, 0)
                if i == n - 1: ub, vb = node[l['end']]
                else: ub, vb = var(lb=-INF), var(lb=-INF)
                dx, dy = D[d]
                B.add(ub - u - dx * Lv == 0); B.add(vb - v - dy * Lv == 0)
                self.seg[(name, i)] = (u, v, Lv, d, ub, vb)
                u, v = ub, vb
            dirs = l['dirs']
            fr = l.get('free', set())
            for i in range(1, n - 1):
                if i in l['spare'] or i in fr or (i - 1) in fr or (i + 1) in fr: continue
                if dirs[i - 1] != dirs[i] and dirs[i + 1] != dirs[i]:
                    same_hand = (rot8(dirs[i - 1], dirs[i]) > 0) == (rot8(dirs[i], dirs[i + 1]) > 0)
                    B.add(self.seg[(name, i)][2] >= (G if same_hand else 2))
        for h_, members in self.meta.get('channels', []):  # siblings one lane apart, in order, on their channel runs
            fx, fy = D[topo_turn(h_, 2)]
            def run_pt(m):
                ln, end_ = m
                l_ = self.L[ln]
                sg = self.seg[(ln, l_['run_s'] if end_ == 'S' else l_['run_e'])]
                return (sg[0], sg[1]) if end_ == 'S' else (sg[4], sg[5])
            for ma, mb in zip(members, members[1:]):
                (ua, va), (ub_, vb_) = run_pt(ma), run_pt(mb)
                B.add(fx * ub_ + fy * vb_ - fx * ua - fy * va == LANE)
        cpar = {}
        self.xrel, self.xcross = set(), set()
        dgroups = {}
        for c, opt in fixed.items():
            i, j, over = opt[:3]
            p, q = self.cross[c]
            u0, v0, Lp, dp, _, _ = self.seg[(p, i)]; u1, v1, Lq, dq, _, _ = self.seg[(q, j)]
            under_d = dq if over == p else dp
            head = 3 if axis(under_d) else 4
            hp, hq = (head, 1) if over == p else (1, head)
            a, b = var(), var()
            B.add(a >= hp); B.add(Lp - a >= hp); B.add(b >= hq); B.add(Lq - b >= hq)
            (ex, ey), (fx, fy) = D[dp], D[dq]
            B.add(u0 + ex * a - u1 - fx * b == 0); B.add(v0 + ey * a - v1 - fy * b == 0)
            cpar[(c, p)] = (p, i, a, hp); cpar[(c, q)] = (q, j, b, hq)
            if len(opt) == 4:  # doubled crossing (D80/D309): collected per over segment, one span each
                if over == p: grp = ((p, i), (q, j, b, dq, u1, v1, Lq), a)
                else: grp = ((q, j), (p, i, a, dp, u0, v0, Lp), b)
                dgroups.setdefault(grp[0], []).append((c, opt[3], head, grp[1], grp[2]))
        # Split each segment's doubled crossings into runs consecutive in the over line's event order (any other crossing breaks a run).
        runs = {}
        for (over, oi), items in dgroups.items():
            ev = self.meta['events'][over]
            items.sort(key=lambda it: ev.index(it[0]))
            run_, prev = [], None
            for it in items:
                if prev is not None and ev.index(it[0]) != ev.index(prev) + 1:
                    runs[(over, oi, len(runs))] = run_; run_ = []
                run_.append(it); prev = it[0]
            runs[(over, oi, len(runs))] = run_
        for (over, oi, rk), items in runs.items():
            ou, ov, OL, od, _, _ = self.seg[(over, oi)]
            ex, ey = D[od]
            side = items[0][1]
            s1, s2 = var(), var()
            B.add(s1 >= 2); B.add(OL - s2 >= 2)
            for c, _, head, _, oa in items: B.add(oa - s1 >= head + 2); B.add(s2 - oa >= head + 2)
            if SPANCLEAR:  # other crossings on this segment lie before the split or after the rejoin, by event order
                ev = self.meta['events'][over]
                rset = {it[0] for it in items}
                e0, e1 = min(ev.index(x) for x in rset), max(ev.index(x) for x in rset)
                for (c2, ln), (l2, i2, a2, h2) in cpar.items():
                    if ln != over or i2 != oi or c2 >= 1000 or c2 in rset: continue
                    e = ev.index(c2)
                    if e < e0: B.add(s1 - a2 >= h2 + 2)
                    elif e > e1: B.add(a2 - s2 >= h2 + 2)
            cname = f'{over}~{oi}.{rk}'
            pu, pv = var(lb=-INF), var(lb=-INF)
            B.add(pu - ou - ex * s1 == 0); B.add(pv - ov - ey * s1 == 0)
            dirs = [topo_turn(od, side), od, topo_turn(od, -side)]
            lens = [var(lb=2, ub=2, c=seg_cost(dirs[0])), var(lb=2, c=seg_cost(dirs[1])), var(lb=2, ub=2, c=seg_cost(dirs[2]))]
            cu, cv = pu, pv
            for k_, (d_, L_) in enumerate(zip(dirs, lens)):
                if axis(d_): B.parity(cu, 0 if D[d_][1] == 0 else 1); B.parity(cv, 1 if D[d_][1] == 0 else 0); B.parity(L_, 0)
                nu, nv = var(lb=-INF), var(lb=-INF)
                B.add(nu - cu - D[d_][0] * L_ == 0); B.add(nv - cv - D[d_][1] * L_ == 0)
                self.seg[(cname, k_)] = (cu, cv, L_, d_, nu, nv)
                cu, cv = nu, nv
            B.add(cu - ou - ex * s2 == 0); B.add(cv - ov - ey * s2 == 0)
            self.xrel.add(frozenset((cname, over)))
            cu0, cv0 = self.seg[(cname, 1)][0], self.seg[(cname, 1)][1]
            for c, _, head, (U, uj, _, ud, uu, uv, UL), oa in items:
                ac, bc = var(), var()
                B.add(ac >= head); B.add(lens[1] - ac >= head); B.add(bc >= 1); B.add(UL - bc >= 1)
                B.add(cu0 + ex * ac - uu - D[ud][0] * bc == 0); B.add(cv0 + ey * ac - uv - D[ud][1] * bc == 0)
                cpar[(c + 1000, cname)] = (cname, 1, ac, head); cpar[(c + 1000, U)] = (U, uj, bc, 1)
                self.xcross.add(frozenset((cname, U)))
        for sp_ in seps:
            if sp_[0] == 'ord':
                _, line, c1, c2, gap = sp_
                B.add(cpar[(c2, line)][2] - cpar[(c1, line)][2] >= gap)
                continue
            (p, i, q, j, f, side, g) = sp_
            s, t = self.seg[(p, i)], self.seg[(q, j)]
            for (ua, va) in ((s[0], s[1]), (s[4], s[5])):
                for (ub, vb) in ((t[0], t[1]), (t[4], t[5])):
                    fa = ua * f[0] + va * f[1]; fb = ub * f[0] + vb * f[1]
                    B.add(side * (fb - fa) >= g)
        key = (tuple(sorted(fixed.items())), tuple(sorted(k for k, v in spare.items() if v)))
        t0 = time.perf_counter()
        val = B.solve_mip() if self.BACKEND == 'mip' else B.solve_plp(self.pstats, self._pcache, key)
        self.pstats['solve_ms'] += (time.perf_counter() - t0) * 1000
        if val is None: return None
        g_ = lambda x: round(val[x.i])
        geo = {k: (g_(s[0]), g_(s[1]), g_(s[2]), s[3]) for k, s in self.seg.items()}
        self.last_cpar = {k: (l, i, g_(a), hh) for k, (l, i, a, hh) in cpar.items()}
        if COST:
            bconst = sum(bridge_extra(3 if axis(self.L[self.cross[c][1] if o[2] == self.cross[c][0] else self.cross[c][0]]['dirs'][o[1] if o[2] == self.cross[c][0] else o[0]]) else 4) * (2 if len(o) == 4 else 1) for c, o in fixed.items())
            return sum(B.obj[k] * val[k] for k in range(len(B.obj))) + bconst, geo, (g_(X), g_(Y))
        return val[X.i] + val[Y.i] + 0.01 * sum(s[2] for s in geo.values()), geo, (g_(X), g_(Y))

    def score(self, fixed, spare, seps):
        global SCORE_ONLY
        SCORE_ONLY = True
        try: return self.solve(fixed, spare, seps)
        finally: SCORE_ONLY = False

    def all_spacing(self):
        out = []
        by = {}
        for (c, line), (l, i, a, hh) in self.last_cpar.items(): by.setdefault((l, i), []).append((a, c, hh))
        for (l, i), lst in by.items():
            lst.sort()
            for (a1, c1, h1), (a2, c2, h2) in zip(lst, lst[1:]):
                need = 2 if (MERGED and h1 > 1 and h2 > 1) else h1 + h2
                if a2 - a1 < need: out.append(('ord', l, c1, c2, need))
        return out

    def spacing(self):
        """Two crossings on one segment of a line closer than their needs (head if over, else 1, each): (line, c1, c2, gap) options."""
        by = {}
        for (c, line), (l, i, a, hh) in self.last_cpar.items(): by.setdefault((l, i), []).append((a, c, hh))
        for (l, i), lst in by.items():
            for (a1, c1, h1), (a2, c2, h2) in itertools.combinations(sorted(lst), 2):
                need = 2 if (MERGED and h1 > 1 and h2 > 1) else h1 + h2  # over at both: one deck spans both
                if a2 - a1 < need:
                    return [('ord', l, c1, c2, need), ('ord', l, c2, c1, need)]
        return None

    # ---- raster: pieces (tile, kind, endpoints) ----
    @staticmethod
    def pieces(u, v, L, d):
        dx, dy = D[d]
        out = []
        if axis(d):
            for t in range(0, L, 2):
                a = (u + dx * t, v + dy * t); b = (u + dx * (t + 2), v + dy * (t + 2))
                m = ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2)
                out.append(((int(m[0] // 2), int(m[1] // 2)), 'X' if dy == 0 else 'Y', (a, b)))
        else:
            for t in range(L):
                a = (u + dx * t, v + dy * t); b = (u + dx * (t + 1), v + dy * (t + 1))
                m = ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2)
                tile = (int(m[0] // 2), int(m[1] // 2))
                corner = ('N' if m[1] - 2 * tile[1] < 1 else 'S') + ('W' if m[0] - 2 * tile[0] < 1 else 'E')
                out.append((tile, corner, (a, b)))
        return out

    def clashes(self, geo):
        """Unrelated, non-crossing segment pairs that share an incompatible tile or an edge midpoint."""
        tiles, pts = {}, {}
        for (line, i), s in geo.items():
            for tile, kind, (a, b) in self.pieces(*s):
                tiles.setdefault(tile, []).append((line, i, kind))
                for p in (a, b): pts.setdefault(p, set()).add((line, i))
        ok_pair = {frozenset(('NW', 'SE')), frozenset(('NE', 'SW'))}
        bad = set()
        def check(x, y, why):
            (p, i), (q, j) = x, y
            if p == q: return
            if frozenset((p, q)) in self.related or frozenset((p, q)) in self.cross_set: return
            if frozenset((p, q)) in self.xrel or frozenset((p, q)) in self.xcross: return
            bad.add((min(x, y), max(x, y)))
        for occ in tiles.values():
            for (p, i, k1), (q, j, k2) in itertools.combinations(occ, 2):
                if frozenset((k1, k2)) in ok_pair: continue
                check((p, i), (q, j), 'tile')
        for occ in pts.values():
            for x, y in itertools.combinations(sorted(occ), 2): check(x, y, 'touch')
        return sorted(bad)

    def sep_options(self, geo, pair):
        """The 8 sectors: orientation f in {u, v, u+v, u-v}, side +-1; ordered by violation now (push)."""
        (p, i), (q, j) = pair
        sp = geo[(p, i)]; sq = geo[(q, j)]
        def ends(s):
            u, v, L, d = s; return [(u, v), (u + D[d][0] * L, v + D[d][1] * L)]
        out = []
        for f in ((1, 0), (0, 1), (1, 1), (1, -1)):
            g = SEP if f in ((1, 0), (0, 1)) else SEP
            fa = [a * f[0] + b * f[1] for a, b in ends(sp)]; fb = [a * f[0] + b * f[1] for a, b in ends(sq)]
            for side in (1, -1):
                viol = max(0, g - (min(fb) - max(fa))) if side == 1 else max(0, g - (min(fa) - max(fb)))
                out.append((viol, (p, i, q, j, f, side, g)))
        out.sort(key=lambda t: t[0])
        res = [o for _, o in out]
        if EMBED:
            base = lambda n: n.split('~')[0]
            sd = self.meta['side_of'](base(p), base(q)) if base(p) != base(q) else None
            if sd:
                h, sign = sd
                a, b = D[topo_turn(h, 2)]  # right of travel
                if a == 0 or b == 0: f, k = (abs(a), abs(b)), (a + b)
                elif a == b: f, k = (1, 1), a
                else: f, k = (1, -1), a
                emb = (p, i, q, j, f, sign * k, SEP)
                res = [emb] + [o for o in res if o != emb]
                self.embed_hits = getattr(self, 'embed_hits', 0) + 1
        return res


def run(lanes):
    M = Model(lanes)
    M.RELAX = False
    if "--plp" in sys.argv: Model.BACKEND = "plp"
    # Search order: core crossings (B, D, F) first, then fray; spare S-bends start absent and are switched on
    # only when a node fails (cheapest first).
    order = sorted(range(len(M.cross)), key=lambda c: (1 if M.cross[c] in M.meta['fray'] else 0, tuple(sorted(M.cross[c])) if '--byname' in sys.argv else c))
    if GROUPS:
        order = sorted(range(len(M.units)), key=lambda u: (1 if any(M.cross[c] in M.meta['fray'] for c in M.units[u]) else 0, -len(M.units[u]), M.unit_name(u)))
        print('UNITS', [M.unit_name(u) + f' ({len(M.gopts[u])} opts)' for u in order])
    if '--fray-first' in sys.argv: order = sorted(order, key=lambda c: (1 if all(x[0] in 'BDF' for x in M.cross[c]) else 0, c))
    if '--reverse' in sys.argv: order = list(reversed(order))
    stats = dict(solves=0, nodes=0, backtracks=0, sep_rounds=0, leaves=0)
    best = [None]
    global best_x
    t0 = time.perf_counter()
    spare_all = [(n, i) for n, l in M.L.items() for i in l['spare'] if not l.get('fan')]

    at_leaf = [False]
    global trace
    trace = []
    LAZY_LEAF = '--lazy-leaf' in sys.argv

    def lazy(fixed, spare, seps):
        """Solve, then add separations for clashes until clean; DFS over sectors on infeasibility. Returns (res, seps)."""
        stats['solves'] += 1
        r = M.solve(fixed, spare, seps)
        if r is None: return None, seps
        if LAZY_LEAF and not at_leaf[0]: return r, seps
        sp_opts = M.spacing()
        bad = M.clashes(r[1])
        if 'leaf_dump' not in stats and at_leaf[0]:
            stats['leaf_dump'] = 1; print('FIRST LEAF clashes', bad, 'spacing', M.all_spacing()); print(render(M, r[1], r[2])); print({M.cross[c]: o[2:] for c, o in fixed.items()})
        if not bad and not sp_opts: return r, seps
        if time.perf_counter() - t0 > TIME_BUDGET: return None, seps
        stats['sep_rounds'] += 1
        if BATCH:  # every clash and spacing fault at once, each by push; no backtracking
            add = []
            seen = set()
            for pr in bad:
                add.append(M.sep_options(r[1], pr)[0])
            if sp_opts: add.append(sp_opts[0])
            for sp in M.all_spacing(): add.append(sp)
            res, s2 = lazy(fixed, spare, seps + add)
            return res, s2
        opts = sp_opts if sp_opts else M.sep_options(r[1], bad[0])[:(4 if EMBED else 3)]
        for o in opts:
            res, s2 = lazy(fixed, spare, seps + [o])
            if res is not None: return res, s2
            stats['backtracks'] += 1
        return None, seps

    def dfs(k, fixed, spare, seps):
        stats['nodes'] += 1
        at_leaf[0] = k == len(order)
        if k > stats.get('maxk', -1):
            stats['maxk'] = k; stats['maxk_next'] = (M.unit_name(order[k]) if GROUPS else M.cross[order[k]]) if k < len(order) else None
            stats['maxk_fixed'] = {M.cross[c]: o[2:] for c, o in fixed.items()}
            stats['maxk_spare'] = sorted(k_ for k_, v in spare.items() if v)
        if time.perf_counter() - t0 > TIME_BUDGET: return
        r, seps = lazy(fixed, spare, seps)
        if r is None: return
        if TRACE and stats.get('leaves', 0) == 0: trace.append((k, (M.unit_name(order[k - 1]) if GROUPS else M.cross[order[k - 1]]) if k else None, fixed.get(order[k - 1]) if k else None, r[2][0] // 2 + 1, r[2][1] // 2 + 1, sorted(kk for kk, v in spare.items() if v and not M.L[kk[0]].get('fan'))))
        if best[0] and r[0] >= best[0][0] - 1e-9: return  # cost only grows as constraints are added
        if k == len(order):
            stats['leaves'] += 1
            if M.RELAX:  # positions: integer solve at the leaf only
                M.RELAX = False; ri, _ = lazy(fixed, spare, seps); M.RELAX = True
                if ri is None: stats['int_fail'] = stats.get('int_fail', 0) + 1; return
                r = ri
            if stats['leaves'] == 1: stats['first_leaf_s'] = round(time.perf_counter() - t0, 2); stats['first_size'] = (r[2][0] // 2 + 1, r[2][1] // 2 + 1)
            best[0] = (r[0], r, dict(fixed), dict(spare), time.perf_counter() - t0, dict(stats)); best_x[0] = (set(M.xrel), set(M.xcross), M.clashes(r[1]), dict(M.last_cpar))
            return
        if GROUPS:
            u = order[k]
            candg = []
            for go in M.gopts[u]:
                f2 = dict(fixed); f2.update(go)
                need = set()
                for cidx, o in go.items():
                    p_, q_ = M.cross[cidx]
                    for kk in ((p_, o[0]), (q_, o[1])):
                        if kk[1] in M.L[kk[0]]['spare'] and not spare.get(kk) and not M.L[kk[0]].get('fan'): need.add(kk)
                s2 = dict(spare)
                for kk in need: s2[kk] = True
                stats['solves'] += 1
                rr = M.solve(f2, s2, seps)
                if rr is not None: candg.append((rr[0], go, s2))
            for _, go, sp in sorted(candg, key=lambda t: t[0]):
                f2 = dict(fixed); f2.update(go)
                dfs(k + 1, f2, sp, seps)
            return
        c = order[k]
        # Try each option; order them by the cost after adding (push: cheapest first).
        cand = []
        cand_s = []
        opts = M.opts[c]
        if '--policy' in sys.argv and '--noscore' not in sys.argv:  # main lines (prio B, E) stay on the ground when the other line may go over
            pol = [o for o in opts if o[2] not in M.meta['prio_lines']]
            opts = pol or opts
        if '--noscore' in sys.argv:  # no scoring solves: policy preference, then static order
            pref = sorted(opts, key=lambda o: (o[2] in M.meta['prio_lines'], -o[0] if '--late' in sys.argv else 0))
            for o in pref:
                f2 = dict(fixed); f2[c] = o
                dfs(k + 1, f2, spare, seps)
            return
        for o in opts:
            f2 = dict(fixed); f2[c] = o
            stats['solves'] += 1
            if JOGS:  # an option on a jog that is off competes with the jog on
                p_, q_ = M.cross[c]
                keys = [(p_, o[0]), (q_, o[1])]
                need = [kk for kk in keys if kk[1] in M.L[kk[0]]['spare'] and not spare.get(kk) and not M.L[kk[0]].get('fan')]
                if need:
                    s2 = dict(spare)
                    for kk in need: s2[kk] = True
                    rr = M.solve(f2, s2, seps)
                    if rr is not None: cand_s.append((rr[0] + (DPEN if len(o) == 4 else 0), o, s2))
                    continue
            ts = time.perf_counter()
            if '--lpscore' in sys.argv:
                global DIVE
                d0 = DIVE; DIVE = False; Model.BACKEND, b0 = 'plp', Model.BACKEND
                rr = M.score(f2, spare, seps); DIVE = d0; Model.BACKEND = b0
            else:
                rr = M.solve(f2, spare, seps)
            stats['score_ms'] = stats.get('score_ms', 0) + (time.perf_counter() - ts) * 1000
            if rr is not None: cand.append((rr[0] + (DPEN if len(o) == 4 else 0), o))
        if not cand and not cand_s:
            # Switch a spare S-bend on (one level) and retry this crossing.
            p, q = M.cross[c]
            for key in spare_all:
                if key[0] in (p, q) and not spare.get(key):
                    s2 = dict(spare); s2[key] = True
                    stats['backtracks'] += 1
                    dfs(k, fixed, s2, seps)
            return
        allc = [(cst, o, spare) for cst, o in cand] + cand_s
        for _, o, sp in sorted(allc, key=lambda t: t[0]):
            f2 = dict(fixed); f2[c] = o
            dfs(k + 1, f2, sp, seps)

    root = {}
    if M.solve({}, {}, []) is None:  # base geometry infeasible: open the port fans
        root = {(n, i): True for n, l in M.L.items() if l.get('fan') for i in l['spare']}
        print('root infeasible without fans; fans on:', len(root) // 2)
    if '--area' in sys.argv:
        r0 = M.solve({}, root, [])
        global AREA
        AREA = (r0[2][0], r0[2][1]); globals()['AREA'] = AREA
        print('area linearised at', AREA)
    dfs(0, {}, root, [])
    dt = time.perf_counter() - t0
    return M, best[0], stats, dt


def render(M, geo, XY):
    W, Hh = XY[0] // 2 + 1, XY[1] // 2 + 1
    g = [['.'] * W for _ in range(Hh)]
    for (line, i), s in geo.items():
        for (tx, ty), kind, _ in M.pieces(*s):
            if 0 <= ty < Hh and 0 <= tx < W:
                ch = line[0].lower() if line in M.L and M.L[line]['mins'][0] == STUB and len(M.L[line]['dirs']) == 1 else line[0]
                g[ty][tx] = ch if g[ty][tx] in ('.', ch) else '#'
    return '\n'.join(''.join(r) for r in g)


if __name__ == '__main__':
    lanes = sys.argv[1]
    M, best, stats, dt = run(lanes)
    print(f'{M.meta["name"]}: {len(M.cross)} crossings, {sum(len(o) for o in M.opts)} options; search {dt:.2f} s; stats {stats}')
    if best is None:
        print('NO LAYOUT within budget'); sys.exit(1)
    cost, (c, geo, XY), fixed, spare, t_first, st = best
    print(f'best: {XY[0] // 2 + 1} x {XY[1] // 2 + 1} tiles, found at {t_first:.2f} s; spare S-bends on: {sorted(k for k, v in spare.items() if v)}')
    M.xrel, M.xcross, at_leaf_bad, best_cpar = best_x[0]
    print(f'clash check on best: {M.clashes(geo)} (at leaf: {at_leaf_bad})')
    print(render(M, geo, XY))
    print(f'backend {M.BACKEND}: {M.pstats}')
    po = [(M.cross[c], o[2]) for c, o in fixed.items() if o[2] in M.meta['prio_lines']]
    print(f'over lines that are prio (D80: need doubling): {len(po)} of {len(fixed)}: {po}')

    exp = next((a.split('=', 1)[1] for a in sys.argv if a.startswith('--export=')), None)
    if exp:
        import json
        BITS = {'X': 'X', 'Y': 'Y', 'NW': 'Upper', 'SE': 'Lower', 'NE': 'Left', 'SW': 'Right'}
        def tpar(seg, p):
            u0, v0, L, d = seg
            return max(abs(p[0] - u0), abs(p[1] - v0))
        # Bridge ranges per (line, seg): crossing param +- head on the over line (head > 1 marks the over entry).
        ranges = {}
        for (c, line), (l, i, a, hh) in best_cpar.items():
            if hh > 1: ranges.setdefault((l, i), []).append([a - hh, a + hh])
        removed, bridges = set(), []
        for (l, i), rs in ranges.items():
            rs.sort(); merged = []
            for lo, hi in rs:
                if merged and lo < merged[-1][1]: merged[-1][1] = max(merged[-1][1], hi)
                else: merged.append([lo, hi])
            seg = geo[(l, i)]
            pcs = M.pieces(*seg)
            for lo, hi in merged:
                inside = [k for k, (tile, kind, (pa, pb)) in enumerate(pcs) if tpar(seg, pa) >= lo and tpar(seg, pb) <= hi]
                if not inside: continue
                for k in inside: removed.add((l, i, k))
                t0, t1 = pcs[inside[0]][0], pcs[inside[-1]][0]
                bridges.append({'start': list(t0), 'end': list(t1)})
        tracks = {}
        for (l, i), seg in geo.items():
            for k, (tile, kind, _) in enumerate(M.pieces(*seg)):
                if (l, i, k) in removed: continue
                tracks.setdefault(tile, set()).add(BITS[kind])
        out = {'size': [XY[0] // 2 + 1, XY[1] // 2 + 1],
               'tracks': [{'x': t[0], 'y': t[1], 'bits': sorted(b)} for t, b in sorted(tracks.items())],
               'bridges': bridges, 'signals': []}
        json.dump(out, open(exp, 'w'))
        print(f'exported {exp}: {len(out["tracks"])} track tiles, {len(bridges)} bridges')

    # Measures of the best: track tiles, D153 surface + slivers, box.
    surf = set()
    for (l, i), seg in geo.items():
        for tile, kind, _ in M.pieces(*seg): surf.add(tile)
    Wt, Ht = XY[0] // 2 + 1, XY[1] // 2 + 1
    free = lambda x, y: not (0 <= x < Wt and 0 <= y < Ht) or (x, y) not in surf
    sliv = sum(1 for x in range(Wt) for y in range(Ht) if free(x, y) and not any(all(free(x + dx + ex, y + dy + ey) for ex in (0, 1) for ey in (0, 1)) for dx in (-1, 0) for dy in (-1, 0)))
    print(f'MEASURES box {Wt}x{Ht}={Wt*Ht} surface {len(surf)} slivers {sliv} D153 {len(surf)+sliv}')

    if TRACE:
        last = {}
        for row in trace:
            last[row[0]] = row
        for k_ in sorted(last): print('TRACE', last[k_])
    if '--stubs' in sys.argv:
        st = {n: geo[(n, i)][2] // 2 for n, l in M.L.items() for i, m in enumerate(l['mins']) if m == STUB and (l['start'].startswith('p') or l['end'].startswith('p'))}
        print('STUB tiles', st)
