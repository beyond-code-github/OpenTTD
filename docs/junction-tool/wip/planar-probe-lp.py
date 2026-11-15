"""Riskiest-first probe (DESIGN-planar §2b idea): choices first, then positions by LP.

1-lane T, right-hand traffic, sides W/E/S. Each line is a chain of segments with FIXED directions
(octilinear, 45-degree steps only); lengths and node positions are LP variables. Discrete choices
(which segment of each line hosts each crossing, and which line is over) are enumerated; each
combination is one LP. Simplifications (probe only): tile units, a diagonal step = 1 tile,
head floor H each side for the over line and 1 for the under line, middle bend segments may be 0,
curve gaps and signals not modelled. Clashes are detected after by rasterising (no lazy rounds yet).
"""
import itertools, time
import highspy

INF = highspy.kHighsInf
D = {'E': (1, 0), 'W': (-1, 0), 'N': (0, -1), 'S': (0, 1),
     'NE': (1, -1), 'NW': (-1, -1), 'SE': (1, 1), 'SW': (-1, 1)}
H = 2

# line: (start node, end node, segment directions, min length per segment)
LINES = {
    'WIN':  ('pW_in', 'sp1', ['E'], [1]),
    'A':    ('sp1', 'mg2', ['E'], [1]),
    'B':    ('sp1', 'mg4', ['SE', 'S'], [1, 1]),
    'EOUT': ('mg2', 'pE_out', ['E'], [1]),
    'EIN':  ('pE_in', 'sp2', ['W'], [1]),
    'E':    ('sp2', 'mg3', ['W'], [1]),
    'F':    ('sp2', 'mg4', ['SW', 'S', 'SW'], [1, 0, 1]),
    'SOUT': ('mg4', 'pS_out', ['S'], [1]),
    'SIN':  ('pS_in', 'sp3', ['N'], [1]),
    'C':    ('sp3', 'mg2', ['NE', 'N', 'NE'], [1, 0, 1]),
    'D':    ('sp3', 'mg3', ['NW', 'N', 'NW'], [1, 0, 1]),
    'WOUT': ('mg3', 'pW_out', ['W'], [1]),
}
CROSSINGS = [('A', 'D'), ('A', 'F'), ('D', 'F')]  # derived by interleaving (spike.derived_crossings)
RELATED = {frozenset(p) for p in [('WIN', 'A'), ('WIN', 'B'), ('A', 'B'), ('A', 'EOUT'), ('C', 'EOUT'), ('A', 'C'),
                                  ('EIN', 'E'), ('EIN', 'F'), ('E', 'F'), ('B', 'SOUT'), ('F', 'SOUT'), ('B', 'F'),
                                  ('SIN', 'C'), ('SIN', 'D'), ('C', 'D'), ('E', 'WOUT'), ('D', 'WOUT'), ('E', 'D')]}


def parallel(d1, d2):
    (a, b), (c, e) = D[d1], D[d2]
    return a * e - b * c == 0


def axis(d): return 0 in D[d]


def options():
    """Per crossing: (seg of p, seg of q, over) with the over segment axis-aligned and the two not parallel."""
    per = []
    for p, q in CROSSINGS:
        opts = []
        for i, dp in enumerate(LINES[p][2]):
            for j, dq in enumerate(LINES[q][2]):
                if parallel(dp, dq): continue
                for over in (p, q):
                    if axis(dp if over == p else dq): opts.append((i, j, over))
        per.append(opts)
    return per


def solve(choice):
    h = highspy.Highs(); h.silent()
    v = lambda lb=0.0, ub=INF, c=0.0: h.addVariable(lb=lb, ub=ub, obj=c)
    X, Y = v(c=1.0), v(c=1.0)
    nodes = {n: (v(), v()) for l in LINES.values() for n in (l[0], l[1])}
    for x, y in nodes.values():
        h.addConstr(x <= X); h.addConstr(y <= Y)
    # Ports on the boundary, in the embedding's order with one tile between.
    h.addConstr(nodes['pW_in'][0] == 0); h.addConstr(nodes['pW_out'][0] == 0)
    h.addConstr(nodes['pE_in'][0] == X); h.addConstr(nodes['pE_out'][0] == X)
    h.addConstr(nodes['pS_in'][1] == Y); h.addConstr(nodes['pS_out'][1] == Y)
    h.addConstr(nodes['pW_in'][1] >= nodes['pW_out'][1] + 1)
    h.addConstr(nodes['pE_out'][1] >= nodes['pE_in'][1] + 1)
    h.addConstr(nodes['pS_in'][0] >= nodes['pS_out'][0] + 1)
    seg = {}  # (line, i) -> (x0, y0, L, dir)
    for name, (a, b, dirs, mins) in LINES.items():
        x, y = nodes[a]
        for i, (d, m) in enumerate(zip(dirs, mins)):
            L = v(lb=m, c=0.01)
            seg[(name, i)] = (x, y, L, d)
            dx, dy = D[d]
            if i == len(dirs) - 1:
                xb, yb = nodes[b]
            else:
                xb, yb = v(), v()
            h.addConstr(xb - x - dx * L == 0); h.addConstr(yb - y - dy * L == 0)
            x, y = xb, yb
    for (p, q), (i, j, over) in zip(CROSSINGS, choice):
        x0, y0, Lp, dp = seg[(p, i)]; x1, y1, Lq, dq = seg[(q, j)]
        a, b = v(), v()
        hp, hq = (H, 1) if over == p else (1, H)
        h.addConstr(a >= hp); h.addConstr(Lp - a >= hp); h.addConstr(b >= hq); h.addConstr(Lq - b >= hq)
        (ex, ey), (fx, fy) = D[dp], D[dq]
        h.addConstr(x0 + ex * a - x1 - fx * b == 0); h.addConstr(y0 + ey * a - y1 - fy * b == 0)
    h.run()
    if h.getModelStatus() != highspy.HighsModelStatus.kOptimal: return None
    val = h.getSolution().col_value
    get = lambda var: val[var.index]
    geo = {k: (get(x0), get(y0), get(L), d) for k, (x0, y0, L, d) in seg.items()}
    return get(X) + get(Y), get(X), get(Y), geo


def raster(geo):
    cells = {}
    for (line, i), (x0, y0, L, d) in geo.items():
        dx, dy = D[d]
        for t in range(int(round(L)) + 1):
            cells.setdefault((round(x0 + dx * t), round(y0 + dy * t)), set()).add(line)
    return cells


def clashes(geo):
    bad = set()
    for c, ls in raster(geo).items():
        for p, q in itertools.combinations(sorted(ls), 2):
            if frozenset((p, q)) in RELATED or (p, q) in CROSSINGS or (q, p) in CROSSINGS: continue
            bad.add((p, q))
    return bad


def render(geo, X, Y):
    g = [['.'] * (int(X) + 1) for _ in range(int(Y) + 1)]
    for (x, y), ls in raster(geo).items():
        g[y][x] = '#' if len(ls) > 1 else min(ls)[0]
    return '\n'.join(''.join(r) for r in g)


if __name__ == '__main__':
    per = options()
    combos = list(itertools.product(*per))
    t0 = time.perf_counter()
    results, infeasible = [], 0
    for ch in combos:
        r = solve(ch)
        if r is None: infeasible += 1; continue
        results.append((r[0], len(clashes(r[3])), ch, r))
    dt = time.perf_counter() - t0
    clean = [r for r in results if r[1] == 0]
    print(f'combos {len(combos)}, infeasible {infeasible}, feasible {len(results)}, clash-free {len(clean)}; '
          f'{dt * 1000:.0f} ms total, {dt * 1000 / len(combos):.1f} ms per LP')
    pick = min(clean or results, key=lambda r: (r[1], r[0]))
    cost, nclash, ch, (_, X, Y, geo) = pick
    frac = [k for k, (x0, y0, L, d) in geo.items() if abs(L - round(L)) > 1e-6 or abs(x0 - round(x0)) > 1e-6]
    print(f'best: size {X + 1:.0f} x {Y + 1:.0f}, clashes {nclash} {sorted(clashes(geo))}, choice {ch}, fractional {frac}')
    print(render(geo, X, Y))
