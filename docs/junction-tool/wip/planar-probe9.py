"""Probe 9: the T family at n lanes (lane keeps lane, D323), the T2 branch of probe 8 generalised over lanes.
n=2 must reproduce probe 8's T2 (935). Run: python3 probe9.py --n=2 [--only=+1w-1] [--routes=path]"""
import sys, time
import probe8 as p8
from probe8 import Model, Ex, turn, right, axis, dot, D, P, TL, STUB, FANW, TUCK, ev, solve_lazy, render, export_routes, head_u

N = int(next((a.split('=')[1] for a in sys.argv if a.startswith('--n=')), 2))
FREEJ = '--freej' in sys.argv
MAXMID = 10  # the spec's max_bridge_middle (test spec)  # join diagonals free length (else the T2 lengths generalised)


def core_lines(n):
    """B core: lane j's line and its copy side (D312 mirrored pairs; an odd middle lane copies left, the way the W join needs the lines)."""
    sg = [-1 if j < (n + 1) // 2 else +1 for j in range(n)]
    pos = [0]
    for j in range(1, n):
        pos.append(pos[-1] + 1 + (1 if sg[j - 1] > 0 else 0) + (1 if sg[j] < 0 else 0))
    return sg, pos


def join_entry(n):
    """A prio join's straight lanes arrive on these lines (far lane 0, toward the joiners): every crossed lane steps one line out, the merges two lines apart with a prio row between."""
    return [0] + [2 * j - 1 for j in range(1, n)]


def build_tn(n, fray_s, join_s):
    M = Model()
    X, Y = M.var(), M.var()
    M.X, M.Y = X, Y
    M.fan_in = []
    L = lambda s: [f'{s}{k}' for k in range(1, n + 1)]
    R_ = lambda s: L(s)[::-1]

    def port_in(name, side):
        M.strand(name)
        u, v = M.start[name]
        M.fan_in.append(name)
        if side == 'W': M.add(u == 0)
        if side == 'E': M.add(u - X == 0)
        if side == 'S': M.add(v - Y == 0)

    for x in L('EF'): port_in(x, 'W')
    for x in L('AB'): port_in(x, 'E')
    for x in L('CD'): port_in(x, 'S')
    outs = {x: 'E' for x in L('ED')} | {x: 'W' for x in L('CB')} | {x: 'S' for x in L('AF')}
    bundles = {}

    def double(l, h, sg, suffix, first):
        """The parallelogram on lane l (copy toward sg): lane deck, step sg; copy steps sg, deck; tie. Returns (d0, d1); the lane ends on its copy's line."""
        M.seg(l, h, lo=first)
        cs = M.end(l); c = l + suffix
        M.strand(c, cs, label=l); M.node(l, c)
        d0 = M.seg(l, h, lo=2)
        M.seg(l, turn(h, sg), lo=2, hi=2)
        M.seg(c, turn(h, sg), lo=2, hi=2)
        d1 = M.seg(c, h, lo=2)
        M.tie(M.end(c), M.end(l)); M.copies[c] = (l, d0.i, d0.i + 2)
        return c, d0, d1

    def fray(h, sigma, ins, S, V, prio):
        """n-lane fray, lanes far..near (far: away from sigma). V_k crosses every nearer S_j; each crossed S_j decks over all farther V."""
        Lt = turn(h, -1); r = right(h)
        for x in ins:
            M.seg(x, h, lo=STUB)
            M.seg(x, Lt, lo=2, opt=True)
            M.seg(x, h, lo=0, cost=FANW)
        sp = [M.end(x) for x in ins]
        for x, s, v, p in zip(ins, S, V, sp):
            M.strand(s, p); M.strand(v, p); M.node(x, s, v); M.pred[s] = x; M.pred[v] = x
        for a, b in zip(sp, sp[1:]):
            M.add((dot(r, b) - dot(r, a)) * sigma >= (4 if prio else 2))
        M.seg(S[0], h, lo=2)
        vs = [M.seg(v, turn(h, sigma), lo=2) for v in V]
        for j in range(1, n):
            far_v = vs[:j]
            if prio:
                c, d0, d1 = double(S[j], h, -sigma, "'", 4)
                M.seg(S[j], h, lo=2)
                n0, f0 = M.deck(d0, [M.cross(d0, v, 4) for v in far_v], 4, 0)
                n1, f1 = M.deck(d1, [M.cross(d1, v, 4) for v in far_v], 0, 4)
                M.add(f0 - n0 - (f1 - n1) == 0)
                M.add(dot(D[h], d0.a) + n0 - dot(D[h], d1.a) - n1 == 2)
            else:
                M.seg(S[j], h, lo=2)
                M.seg(S[j], Lt, lo=2, opt=True)
                d0 = M.seg(S[j], h, lo=2)
                M.deck(d0, [M.cross(d0, v, 4) for v in far_v], n_min=2)

    def join(h, sigma, S, J, O, prio):
        """n-lane join: J_k lands on S_k from side sigma, crossing every nearer S_j (lanes far..near)."""
        r = right(h)
        M.seg(S[0], h, lo=2 * P + 2 if prio else 2)
        js = []
        for k in range(n):
            M.seg(J[k], h, lo=2)
            lo = 4 if (prio and k > 0) else 6           # T2: J_far 6, prio J_near 4, plain J_near 6
            hi = lo if (n == 2 and not FREEJ) else None  # n > 2: J_k's diagonal spans its own crossings, free
            if FREEJ: lo = 2
            js.append(M.seg(J[k], turn(h, -sigma), lo=lo, hi=hi))
        M.tie(M.end(J[0]), M.end(S[0]))
        for j in range(1, n):
            near_j = js[:j]
            if prio:
                # Joiners far from the merge cross before this lane's coverage, each P tiles apart: one doubled stretch
                # per crossed joiner in series (D322), the earlier ones stepping back, the last one the join's outward step.
                for k in range(j - 1):
                    c, d0, d1 = double(S[j], h, sigma, "''" + "'" * (k + 1), 2)
                    M.seg(S[j], h, lo=2)
                    M.seg(S[j], turn(h, -sigma), lo=2, hi=2)
                    n0, f0 = M.deck(d0, [M.cross(d0, near_j[k], 4)], 4, 0)
                    n1, f1 = M.deck(d1, [M.cross(d1, near_j[k], 4)], 0, 4)
                    M.add(f0 - n0 - (f1 - n1) == 0)
                    M.add(dot(D[h], d0.a) + n0 - dot(D[h], d1.a) - n1 == 2)
                c, d0, d1 = double(S[j], h, sigma, "''", 2)
                M.seg(S[j], h, lo=2 * P + 2)
                n0, f0 = M.deck(d0, [M.cross(d0, near_j[-1], 4)], 4, 0)
                n1, f1 = M.deck(d1, [M.cross(d1, near_j[-1], 4)], 0, 4)
                M.add(f0 - n0 - (f1 - n1) == 0)
                M.add(dot(D[h], d0.a) + n0 - dot(D[h], d1.a) - n1 == 2)
            else:
                d0 = M.seg(S[j], h, lo=2)
                M.deck(d0, [M.cross(d0, x, 4) for x in near_j], tail_min=4)
            M.tie(M.end(J[j]), M.end(S[j]))
        ms = [M.end(s) for s in S]
        for s, x, o, m in zip(S, J, O, ms):
            M.strand(o, m); M.node(s, x, o); M.pred[o] = s
        if prio:
            for s, m in zip(S, ms): M.prio_rows.append((s, m, h, -sigma))
            for j in range(1, n):
                M.seg(O[j], h, lo=2)
                M.seg(O[j], turn(h, -sigma), lo=2, hi=2 * j)
        st = {}
        for o in O:
            M.seg(o, h, lo=2, cost=FANW)
            st[o] = M.seg(o, turn(h, +1), lo=2, opt=True)
            M.seg(o, h, lo=STUB)
        for f, g in zip(O, O[1:]):
            a, b = (f, g) if turn(h, +1) == turn(h, sigma) else (g, f)
            M.add(dot(D[h], st[a].a) - dot(D[h], st[b].a) >= 0)
            M.add(dot(r, M.end(g)) - dot(r, M.end(f)) == 2 * sigma)

    def flex(name, lanes, h, offs=None):
        offs = offs or list(range(len(lanes)))
        segs = {l: [M.seg(l, h, lo=0), M.seg(l, turn(h, -1), lo=2, opt=True, cost=TUCK), M.seg(l, h, lo=0),
                    M.seg(l, turn(h, +1), lo=2, opt=True, cost=TUCK), M.seg(l, h, lo=0)] for l in lanes}
        r = right(h)
        for j, l in enumerate(lanes[1:], 1):
            for p in ('a', 'b'):
                M.add(dot(r, getattr(segs[l][4], p)) - dot(r, getattr(segs[lanes[0]][4], p)) == 2 * offs[j])
        bundles[name] = (lanes, [h], segs)
        return segs

    def bundle(name, lanes, heads, opt0=False):
        segs = {l: [] for l in lanes}
        for k, h in enumerate(heads):
            r = right(h)
            for l in lanes:
                o = opt0 and k == 0      # a fray's farther legs cross every nearer deck: the near lane's run grows with n
                segs[l].append(M.seg(l, h, lo=0, hi=p8.OPTMAX * (n - 1) if o else None, cost=1.0 if axis(h) else TUCK, opt=o))
            for j, l in enumerate(lanes[1:], 1):
                M.add(dot(r, segs[l][k].a) - dot(r, segs[lanes[0]][k].a) == 2 * j)
                M.add(dot(r, segs[l][k].b) - dot(r, segs[lanes[0]][k].b) == 2 * j)
        bundles[name] = (lanes, heads, segs)
        return segs

    M.pending = []
    M.tag = 'frayW'; fray('E', +1, L('EF'), L('E'), L('F'), True)
    M.tag = 'frayE'; fray('W', -1, R_('AB'), R_('B'), R_('A'), True)
    M.tag = 'frayS'
    if fray_s > 0: fray('N', +1, L('CD'), L('C'), L('D'), False)
    else: fray('N', -1, R_('CD'), R_('D'), R_('C'), False)

    M.tag = 'bundles'
    M.tag = 'bE'; flex('E', L('E'), 'E', join_entry(n))
    M.tag = 'bF'; sF = bundle('F', L('F'), ['E', 'SE', 'S'], opt0=True)
    M.tag = 'bD'; sD = bundle('D', L('D'), ['N', 'NE', 'N', 'NE', 'E'], opt0=fray_s > 0)
    M.tag = 'bC'; bundle('C', L('C'), ['N', 'NW', 'W'], opt0=fray_s < 0)
    M.tag = 'bA'; bundle('A', L('A'), ['W', 'SW', 'S'], opt0=True)
    M.tag = 'bundleB'
    sgB, posB = core_lines(n)
    flex('B', L('B'), 'W', posB)

    def core_decks(dk, first, last):
        xs_F = [M.cross(dk, sF[f][-1], 3) for f in L('F')]
        xs_D = [M.cross(dk, sD[d][2], 3) for d in L('D')]
        nF, fF = M.deck(dk, xs_F, None, last)
        nD, fD = M.deck(dk, xs_D, first, None)
        M.add(nF - fD >= 2)
        return fF - nF, fD - nD, head_u(dk, nF), head_u(dk, nD)

    M.tag = 'coreB'
    for l, sg in zip(L('B'), sgB):
        h = 'W'
        c, d0, d1 = double(l, h, sg, '"', 2)
        M.seg(l, h, lo=2)
        M.seg(l, turn(h, -sg), lo=2, hi=2)
        sp0 = core_decks(d0, 4, 0); sp1 = core_decks(d1, 0, 4)
        M.add(sp0[0] - sp1[0] == 0); M.add(sp0[1] - sp1[1] == 0)
        M.add(sp0[2] - sp1[2] == -2); M.add(sp0[3] - sp1[3] == -2)
    run2 = {l: M.seg(l, 'W', lo=0) for l in L('B')}
    for j, l in enumerate(L('B')[1:], 1):
        M.add(dot(right('W'), run2[l].a) - dot(right('W'), run2['B1'].a) == 2 * posB[j])
    M.tag = 'coreDF'
    for f in L('F'): M.deck(sF[f][-1], [M.cross(sF[f][-1], sD[d][1], 4) for d in L('D')])
    M.tag = 'joinE'; join('E', +1, L('E'), L('D'), L('ED'), True)
    M.tag = 'joinW'; join('W', -1, R_('B'), R_('C'), R_('CB'), True)
    M.tag = 'joinS'
    if join_s > 0: join('S', +1, L('A'), L('F'), L('AF'), False)
    else: join('S', -1, R_('F'), R_('A'), R_('AF'), False)

    M.tag = 'ports'
    for x, side in outs.items():
        u, v = M.end(x)
        if side == 'W': M.add(u == 0)
        if side == 'E': M.add(u - X == 0)
        if side == 'S': M.add(v - Y == 0)
    vv = lambda x, s: M.start[x] if s else M.end(x)
    order = {'W': [(x, 1) for x in L('EF')] + [(x, 0) for x in R_('CB')],
             'E': [(x, 0) for x in L('ED')] + [(x, 1) for x in R_('AB')],
             'S': [(x, 1) for x in L('CD')] + [(x, 0) for x in R_('AF')]}
    M.gaps = []
    for side, seq in order.items():
        k = 0 if side == 'S' else 1
        for idx, ((a, sa), (b, sb)) in enumerate(zip(seq, seq[1:])):
            M.add(vv(b, sb)[k] - vv(a, sa)[k] == (4 if idx == n - 1 else 2))
    M.tag = 'box'
    for name, segs in M.strands.items():
        for p in [M.start[name]] + [s.b for s in segs]:
            M.add(p[0] >= 0); M.add(X - p[0] >= 0); M.add(p[1] >= 1); M.add(Y - p[1] >= 0)
    M.tag = 'prio'
    for main, m, h, side in M.prio_rows:
        r = right(h); dx, dy = D[h]
        start = (m[0] - dx * 2 * P + side * r[0] * 2, m[1] - dy * 2 * P + side * r[1] * 2)
        nm = 'prio:' + main
        M.strand(nm, M.point(), label=nm)
        M.tie(M.start[nm], start)
        M.seg(nm, h, lo=2 * P, hi=2 * P, cost=0)
        for p in (M.start[nm], M.end(nm)):
            M.add(p[0] >= 0); M.add(X - p[0] >= 0); M.add(p[1] >= 1); M.add(Y - p[1] >= 0)
    M.routes = [(a + str(k), b + str(k), c + str(k)) for k in range(1, n + 1) for a, b, c in
                (('EF', 'E', 'ED'), ('EF', 'F', 'AF'), ('AB', 'B', 'CB'), ('AB', 'A', 'AF'), ('CD', 'C', 'CB'), ('CD', 'D', 'ED'))]
    M.outline_routes = [('EF1', 'E1', 'ED1'), ('AB1', 'A1', 'AF1'), ('CD1', 'C1', 'CB1')]
    M.tag = 'bridge'
    for dseg, n_, f_, xs in M.decks: M.add(f_ - n_ <= 2 * (MAXMID + 2))   # a deck is two heads and at most MAXMID middle tiles
    M.tag = 'curve'; p8.route_curves(M, M.routes)
    M.bundles = bundles
    return M


def solve_lazy1(M, t0, seps0):
    """probe 8's lazy loop with the separating solve reused as the next round's solution (no second identical solve)."""
    seps = list(seps0)
    r = M.solve(seps)
    if r is None: return None, 'base infeasible'
    for rnd in range(60):
        geo, decks = p8.geometry(M, r[0])
        under_bad = []
        for over, under, a, bb in M.crossings:
            bv = ev(r[0], bb)
            for dseg, n_, f_, xs in M.decks:
                if dseg is under and ev(r[0], n_) - 3 < bv < ev(r[0], f_) + 3:
                    under_bad.append([M.row(bb <= n_ - 3), M.row(bb >= f_ + 3)])
        if under_bad:
            for rows in under_bad[0]:
                r2 = M.solve(seps + [rows])
                if r2 is not None: seps.append(rows); r = r2; break
            else: return None, 'under-deck conflict not separable'
            continue
        bad = p8.clashes(M, geo, decks)
        if not bad: return (r, geo, decks, rnd, seps), None
        if time.perf_counter() - t0 > p8.BUDGET: return None, f'budget at round {rnd}, {len(bad)} clashes'
        for f, rows in p8.sep_options(M, geo, bad[0])[:4]:
            r2 = M.solve(seps + rows)
            if r2 is not None: seps += rows; r = r2; break
        else: return None, f'clash {bad[0]} not separable'
    return None, 'rounds exhausted'


p8.solve_lazy1 = solve_lazy1


if __name__ == '__main__':
    t0 = time.perf_counter()
    results = []
    combos = [(fs, js) for fs in (1, -1) for js in (1, -1)]
    only = next((a.split('=')[1] for a in sys.argv if a.startswith('--only=')), None)
    if only: combos = [c for c in combos if f'{c[0]:+d}w{c[1]}' == only]
    for fs, js in combos:
        ts = time.perf_counter()
        M = build_tn(N, fs, js)
        res, why = solve_lazy(M, ts)
        tag = f'{fs:+d}w{js}'
        if res is None:
            print(f'T{N} {tag}: NO ({why}) {time.perf_counter() - ts:.2f}s', flush=True); continue
        (val, obj), geo, decks, rnd = res
        W, H = round(ev(val, M.X)) // 2 + 1, round(ev(val, M.Y)) // 2 + 1
        print(f'T{N} {tag}: area {getattr(M, "area", 0):.0f} {W}x{H} sep rounds {rnd} {time.perf_counter() - ts:.2f}s', flush=True)
        results.append((M.area, tag, M, val, geo, decks))
    print(f'total {time.perf_counter() - t0:.2f}s')
    if results:
        results.sort(key=lambda r: r[0])
        a, tag, M, val, geo, decks = results[0]
        print('best', tag, f'{a:.0f}')
        print(render(M, geo, decks, val))
        rexp = next((a.split('=', 1)[1] for a in sys.argv if a.startswith('--routes=')), None)
        if rexp: export_routes(M, geo, decks, val, rexp); print(f'routes {rexp}')
