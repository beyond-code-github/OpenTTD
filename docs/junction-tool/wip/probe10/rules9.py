"""T3 with clash sides from two fixed rules, up front (no guessing). Usage: rules9.py FS JS [--noloop]"""
import sys, time
sys.argv += ['--n=3']
import probe8 as p8
import probe9 as p9
from probe8 import right, dot, D, Ex
GONE, KEEP = set(), set()
TAGS = {}
if '--tied' in sys.argv:   # one switch per channel transition (tag + heading) instead of one per lane
    _seg1 = p8.Model.seg
    def segtied(M, name, d, lo=0, hi=None, cost=1.0, opt=False):
        if not opt: return _seg1(M, name, d, lo, hi, cost, opt)
        sg = _seg1(M, name, d, lo=0, hi=hi if hi is not None else p8.OPTMAX, cost=cost, opt=False)
        if not hasattr(M, 'gz'): M.gz = {}
        key = (getattr(M, 'tag', '?'), d)
        if key not in M.gz:
            ON = next((a.split('=')[1] for a in sys.argv if a.startswith('--on=')), None)
            if ON is None: z = M.var(ub=1.0, c=p8.OPTW); M.ints.add(z.i)
            else:
                v = 1.0 if f'{key[0]}:{key[1]}' in ON.split(',') else 0.0
                z = M.var(lb=v, ub=v)
            M.gz[key] = z
        z = M.gz[key]
        M.add(sg.L - z * lo >= 0); M.add(z * (hi if hi is not None else p8.OPTMAX) - sg.L >= 0)
        sg.z = z
        return sg
    p8.Model.seg = segtied
_seg0 = p8.Model.seg
def _segt(M, *a, **k):
    sg = _seg0(M, *a, **k); TAGS[id(sg)] = getattr(M, 'tag', '?'); return sg
p8.Model.seg = _segt
if '--nobin' in sys.argv:   # every optional step becomes a plain length >= 0 (no switch); its curves always count for spacing
    _seg = p8.Model.seg
    def seg(M, name, d, lo=0, hi=None, cost=1.0, opt=False):
        if opt:
            key = (name, len(M.strands[name]))
            if key in GONE:
                sg = _seg(M, name, d, lo=0, hi=0, cost=cost, opt=False); sg.gone = True; return sg
            if key in KEEP: return _seg(M, name, d, lo=lo, hi=hi if hi is not None else p8.OPTMAX, cost=cost, opt=False)
            sg = _seg(M, name, d, lo=0, hi=hi if hi is not None else p8.OPTMAX, cost=cost, opt=False); sg.soft = True; sg.key = key; return sg
        return _seg(M, name, d, lo, hi, cost, opt)
    p8.Model.seg = seg
    from probe8 import rot, lift, TL, CL, G
    def route_curves(M, routes):
        """probe 8's curve rows with every curve always present, except that a step's own two curves need no gap (length 0 cancels them)."""
        seen = set()
        def addrow(e):
            key = (tuple(sorted(e.d.items())), e.c)
            if key not in seen: seen.add(key); M.add(e >= 0)
        for route in routes:
            for seq in p8.route_segs(M, route):
                curves, acc = [], Ex()
                for a, b in zip(seq, seq[1:]):
                    acc = acc + a.L
                    if a.d != b.d:
                        if getattr(a, 'gone', False) or getattr(b, 'gone', False): continue
                        sid = id(a) if getattr(a, 'soft', False) else id(b) if getattr(b, 'soft', False) else None
                        curves.append((acc, rot(a.d, b.d) > 0, sid))
                for i in range(len(curves)):
                    ci, hi_, si = curves[i]
                    mand = 0
                    for j in range(i + 1, len(curves)):
                        if mand >= 3: break
                        cj, hj, sj = curves[j]
                        if not (j == i + 1 and si is not None and si == sj):
                            e = cj - ci
                            addrow(e - (min(2 * TL - 1, (mand + 1) * CL) if mand >= 1 else (2 if hi_ != hj else G)))
                        mand += 1
    p8.route_curves = route_curves

def sep(M, sp, sq, f):
    """Segment sq lies at least 2 beyond segment sp along f (all four endpoint pairs)."""
    for a in (sp.a, sp.b):
        for b in (sq.a, sq.b):
            M.add(dot(f, b) - dot(f, a) >= 2)

def kids(M, x):
    return [c for c, p in M.pred.items() if p == x and c in M.strands and not c.startswith('prio:')]

def rules(M, n):
    L = lambda s: [f'{s}{k}' for k in range(1, n + 1)]
    cnt = 0
    # Rule 1 (fray component): in-port lanes left to right; the right lane's fan and what follows stay right of and ahead of the left lane.
    ONLY = next((a.split('=')[1] for a in sys.argv if a.startswith('--rule=')), 'EF,AB,CD,E,S').split(',')
    for port, h in [x for x in (('EF', 'E'), ('AB', 'W'), ('CD', 'N')) if x[0] in ONLY]:
        lanes = sorted(L(port), key=lambda x: dot(right(h), (0, 0)))  # placeholder, replaced below
        r = right(h)
        # left to right across travel from the port's reading order: W/E sides read N->S (+y), S reads W->E (+x)
        read = (0, 1) if h in ('E', 'W') else (1, 0)
        lanes = L(port) if dot(read, r) > 0 else L(port)[::-1]
        if port == 'AB': lanes = L(port)  # AB lanes: AB1 is left of travel (south) .. see probe9 order R_('AB') N->S
        f = (r[0] + D[h][0], r[1] + D[h][1])
        for i, p in enumerate(lanes):
            ps = M.strands[p]
            for q in lanes[i + 1:]:
                qs = M.strands[q]
                later = qs[1:] + [s for c in kids(M, q) for s in M.strands[c][:2]]
                for sq in later: sep(M, ps[0], sq, f); cnt += 1
                for sq in qs[2:] + [s for c in kids(M, q) for s in M.strands[c][:2]]: sep(M, ps[1], sq, f); cnt += 1
    # Rule 2 (ports on one side): the joiners' last run into an out-port stays on its side of the neighbouring in-port.
    for side, ins, joiners, f in [x for x in (('E', L('AB'), L('D'), (0, 1)), ('S', L('CD'), L('F'), (1, 0))) if x[0] in ONLY]:
        for p in ins:
            for q in joiners:
                for sp in M.strands[p]:
                    for sq in M.strands[q][-2:]:
                        sep(M, sq, sp, f); cnt += 1
    return cnt

fs, js = int(sys.argv[1]), int(sys.argv[2])
M = p9.build_tn(3, fs, js)
k = rules(M, 3)
print(f'T3 {fs:+d}w{js}: {k} rule pairs added up front')
if '--noloop' in sys.argv:
    t = time.perf_counter(); r = M.solve(); dt = time.perf_counter() - t
    if r is None: print('infeasible'); sys.exit()
    geo, decks = p8.geometry(M, r[0]); bad = p8.clashes(M, geo, decks)
    print(f'one solve {dt:.2f}s, clashes left {len(bad)}', bad[:6])
elif '--twopass' in sys.argv:
    t = time.perf_counter(); res, why = p8.solve_lazy(M, t)
    a1 = getattr(M, 'area', None); val = res[0][0] if res else None
    soft = [sg for st in M.strands.values() for sg in st if getattr(sg, 'soft', False)]
    for sg in soft: (GONE if round(p8.ev(val, sg.L)) == 0 else KEEP).add(sg.key)
    M2 = p9.build_tn(3, fs, js); rules(M2, 3)
    res2, why2 = p8.solve_lazy(M2, t)
    print(f'pass 1 area {a1}; {len(GONE)} steps gone, {len(KEEP)} kept; pass 2', why2, f'area {getattr(M2, "area", None)}', f'total {time.perf_counter() - t:.2f}s')
else:
    t = time.perf_counter(); res, why = p8.solve_lazy(M, t)
    if '--groups' in sys.argv and res:
        for k, z in M.gz.items(): print('  group', k, 'ON' if round(p8.ev(res[0][0], z)) else '-')
    if '--steps' in sys.argv and res:
        val = res[0][0]
        for name, segs in M.strands.items():
            for sg in segs:
                if sg.z is not None:
                    z, L = round(p8.ev(val, sg.z)), round(p8.ev(val, sg.L))
                    print(f'  step {name}[{sg.i}] {sg.d:>2} tag={TAGS.get(id(sg), "?"):8} {"PRESENT len %d" % L if z else "-"}')
    print(why, f'area {getattr(M, "area", None)}', f'{time.perf_counter() - t:.2f}s', 'rounds', res[3] if res else None)
