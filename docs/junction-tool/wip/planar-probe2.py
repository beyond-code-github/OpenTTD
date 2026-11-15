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
import itertools, sys, time
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
        self.L, self.ports, self.cross = t_topology(lanes)
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
                        if axis(dp if over == p else dq): o.append((i, j, over))
            self.opts.append(o)

    RELAX = False

    def solve(self, fixed, spare, seps):
        """fixed: crossing index -> (i, j, over); spare: (line, seg) -> present; seps: list of (p, i, q, j, f, side, g)."""
        h = highspy.Highs(); h.silent()
        h.setOptionValue('time_limit', 5.0)
        def var(lb=0.0, ub=INF, c=0.0, integer=True):
            x = h.addVariable(lb=lb, ub=ub, obj=c)
            if integer and not self.RELAX: h.changeColIntegrality(x.index, highspy.HighsVarType.kInteger)
            return x
        X, Y = var(c=1.0), var(c=1.0)
        node = {}
        for l in self.L.values():
            for n in (l['start'], l['end']):
                if n not in node: node[n] = (var(), var())
        for u, v in node.values():
            h.addConstr(u <= X); h.addConstr(v <= Y); h.addConstr(v >= 1)
        # Ports: W at u = 0, E at u = X, S at v = Y, in boundary order, LANE apart; parity via a half variable.
        for side, seq in self.ports.items():
            for n in seq:
                u, v = node[n]
                k = var()
                if side == 'W': h.addConstr(u == 0); h.addConstr(v - 2 * k == 1)
                if side == 'E': h.addConstr(u - X == 0); h.addConstr(v - 2 * k == 1)
                if side == 'S': h.addConstr(v - Y == 0); h.addConstr(u - 2 * k == 1)
            for a, b in zip(seq, seq[1:]):
                ia = 1 if side == 'S' else 1
                ca, cb = node[a][0 if side == 'S' else 1], node[b][0 if side == 'S' else 1]
                h.addConstr(cb - ca >= LANE)
        self.seg = {}
        for name, l in self.L.items():
            u, v = node[l['start']]
            n = len(l['dirs'])
            for i, (d, m) in enumerate(zip(l['dirs'], l['mins'])):
                if i in l['spare']:
                    present = spare.get((name, i), False)
                    Lv = var(lb=G if present else 0, ub=INF if present else 0, c=0.01)
                else:
                    Lv = var(lb=m, c=0.01)
                if axis(d):  # axis start on the right parity: horizontal u even, vertical u odd; length even
                    k = var(); h.addConstr(u - 2 * k == (0 if D[d][1] == 0 else 1))
                    k2 = var(); h.addConstr(Lv - 2 * k2 == 0)
                if i == n - 1: ub, vb = node[l['end']]
                else: ub, vb = var(lb=-INF), var(lb=-INF)
                dx, dy = D[d]
                h.addConstr(ub - u - dx * Lv == 0); h.addConstr(vb - v - dy * Lv == 0)
                self.seg[(name, i)] = (u, v, Lv, d, ub, vb)
                u, v = ub, vb
            # Curve gaps between consecutive bends: a bend is a direction change (or the line's first piece at a split
            # when it diverges). Segment i sits between bends when both its neighbours differ from it.
            dirs = l['dirs']
            for i in range(1, n - 1):
                if i in l['spare']: continue
                if dirs[i - 1] != dirs[i] and dirs[i + 1] != dirs[i]:
                    h.addConstr(self.seg[(name, i)][2] >= G)
        cpar = {}
        for c, (i, j, over) in fixed.items():
            p, q = self.cross[c]
            u0, v0, Lp, dp, _, _ = self.seg[(p, i)]; u1, v1, Lq, dq, _, _ = self.seg[(q, j)]
            under_d = dq if over == p else dp
            head = 3 if axis(under_d) else 4
            hp, hq = (head, 1) if over == p else (1, head)
            a, b = var(), var()
            h.addConstr(a >= hp); h.addConstr(Lp - a >= hp); h.addConstr(b >= hq); h.addConstr(Lq - b >= hq)
            (ex, ey), (fx, fy) = D[dp], D[dq]
            h.addConstr(u0 + ex * a - u1 - fx * b == 0); h.addConstr(v0 + ey * a - v1 - fy * b == 0)
            cpar[(c, p)] = (p, i, a, hp); cpar[(c, q)] = (q, j, b, hq)
        for sp_ in seps:
            if sp_[0] == 'ord':  # along one segment, crossing c2 lies at least gap after c1
                _, line, c1, c2, gap = sp_
                h.addConstr(cpar[(c2, line)][2] - cpar[(c1, line)][2] >= gap)
                continue
            (p, i, q, j, f, side, g) = sp_
            s, t = self.seg[(p, i)], self.seg[(q, j)]
            for (ua, va) in ((s[0], s[1]), (s[4], s[5])):
                for (ub, vb) in ((t[0], t[1]), (t[4], t[5])):
                    fa = ua * f[0] + va * f[1]; fb = ub * f[0] + vb * f[1]
                    h.addConstr(side * (fb - fa) >= g)
        h.run()
        if h.getModelStatus() != highspy.HighsModelStatus.kOptimal: return None
        val = h.getSolution().col_value
        g_ = lambda x: round(val[x.index])
        geo = {k: (g_(s[0]), g_(s[1]), g_(s[2]), s[3]) for k, s in self.seg.items()}
        self.last_cpar = {k: (l, i, g_(a), hh) for k, (l, i, a, hh) in cpar.items()}
        return val[X.index] + val[Y.index] + 0.01 * sum(s[2] for s in geo.values()), geo, (g_(X), g_(Y))

    def spacing(self):
        """Two crossings on one segment of a line closer than their needs (head if over, else 1, each): (line, c1, c2, gap) options."""
        by = {}
        for (c, line), (l, i, a, hh) in self.last_cpar.items(): by.setdefault((l, i), []).append((a, c, hh))
        for (l, i), lst in by.items():
            for (a1, c1, h1), (a2, c2, h2) in itertools.combinations(sorted(lst), 2):
                if a2 - a1 < h1 + h2:
                    return [('ord', l, c1, c2, h1 + h2), ('ord', l, c2, c1, h1 + h2)]
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
        return [o for _, o in out]


def run(lanes):
    M = Model(lanes)
    M.RELAX = '--relax' in sys.argv
    # Search order: core crossings (B, D, F) first, then fray; spare S-bends start absent and are switched on
    # only when a node fails (cheapest first).
    order = sorted(range(len(M.cross)), key=lambda c: (0 if all(x[0] in 'BDF' for x in M.cross[c]) else 1, c))
    stats = dict(solves=0, nodes=0, backtracks=0, sep_rounds=0, leaves=0)
    best = [None]
    t0 = time.perf_counter()
    spare_all = [(n, i) for n, l in M.L.items() for i in l['spare']]

    def lazy(fixed, spare, seps):
        """Solve, then add separations for clashes until clean; DFS over sectors on infeasibility. Returns (res, seps)."""
        stats['solves'] += 1
        r = M.solve(fixed, spare, seps)
        if r is None: return None, seps
        sp_opts = M.spacing()
        bad = M.clashes(r[1])
        if not bad and not sp_opts: return r, seps
        if time.perf_counter() - t0 > TIME_BUDGET: return None, seps
        stats['sep_rounds'] += 1
        opts = sp_opts if sp_opts else M.sep_options(r[1], bad[0])[:3]
        for o in opts:
            res, s2 = lazy(fixed, spare, seps + [o])
            if res is not None: return res, s2
            stats['backtracks'] += 1
        return None, seps

    def dfs(k, fixed, spare, seps):
        stats['nodes'] += 1
        if time.perf_counter() - t0 > TIME_BUDGET: return
        r, seps = lazy(fixed, spare, seps)
        if r is None: return
        if best[0] and r[0] >= best[0][0] - 1e-9: return  # cost only grows as constraints are added
        if k == len(order):
            stats['leaves'] += 1
            if M.RELAX:  # positions: integer solve at the leaf only
                M.RELAX = False; ri, _ = lazy(fixed, spare, seps); M.RELAX = True
                if ri is None: stats['int_fail'] = stats.get('int_fail', 0) + 1; return
                r = ri
            if stats['leaves'] == 1: stats['first_leaf_s'] = round(time.perf_counter() - t0, 2); stats['first_size'] = (r[2][0] // 2 + 1, r[2][1] // 2 + 1)
            best[0] = (r[0], r, dict(fixed), dict(spare), time.perf_counter() - t0, dict(stats))
            return
        c = order[k]
        # Try each option; order them by the cost after adding (push: cheapest first).
        cand = []
        for o in M.opts[c]:
            f2 = dict(fixed); f2[c] = o
            stats['solves'] += 1
            rr = M.solve(f2, spare, seps)
            if rr is not None: cand.append((rr[0], o))
        if not cand:
            # Switch a spare S-bend on (one level) and retry this crossing.
            p, q = M.cross[c]
            for key in spare_all:
                if key[0] in (p, q) and not spare.get(key):
                    s2 = dict(spare); s2[key] = True
                    stats['backtracks'] += 1
                    dfs(k, fixed, s2, seps)
            return
        for _, o in sorted(cand, key=lambda t: t[0]):
            f2 = dict(fixed); f2[c] = o
            dfs(k + 1, f2, spare, seps)

    dfs(0, {}, {}, [])
    dt = time.perf_counter() - t0
    return M, best[0], stats, dt


def render(M, geo, XY):
    W, Hh = XY[0] // 2 + 1, XY[1] // 2 + 1
    g = [['.'] * W for _ in range(Hh)]
    for (line, i), s in geo.items():
        for (tx, ty), kind, _ in M.pieces(*s):
            if 0 <= ty < Hh and 0 <= tx < W:
                ch = line[0] if len(line) == 2 and line[1].isdigit() else line[0].lower()
                g[ty][tx] = ch if g[ty][tx] in ('.', ch) else '#'
    return '\n'.join(''.join(r) for r in g)


if __name__ == '__main__':
    lanes = int(sys.argv[1]) if len(sys.argv) > 1 else 1
    M, best, stats, dt = run(lanes)
    print(f'T{lanes}: {len(M.cross)} crossings, {sum(len(o) for o in M.opts)} options; search {dt:.2f} s; stats {stats}')
    if best is None:
        print('NO LAYOUT within budget'); sys.exit(1)
    cost, (c, geo, XY), fixed, spare, t_first, st = best
    print(f'best: {XY[0] // 2 + 1} x {XY[1] // 2 + 1} tiles, found at {t_first:.2f} s (stats then {st}); spare S-bends on: {sorted(k for k, v in spare.items() if v)}')
    print(f'clash check on best: {M.clashes(geo)}')
    print(render(M, geo, XY))
