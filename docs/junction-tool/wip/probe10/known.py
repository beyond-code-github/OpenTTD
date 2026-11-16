"""Upper-bound test: LL2 -1F1w1 solved with every clash side known up front (from its own finished solution)."""
import time, sys
import probe8 as p8
cap = {}
orig = p8.solve_lazy1
def wrap(M, t0, seps0):
    r, why = orig(M, t0, seps0)
    if r is not None: cap['seps'] = r[4]; cap['val'] = r[0][0]
    return r, why
p8.solve_lazy1 = wrap
M = p8.build_t2(-1, 'F', 1, 1)
t = time.perf_counter(); res, why = p8.solve_lazy(M, t); tl = time.perf_counter() - t
print(f'today: lazy loop + area, area {M.area:.0f}, {tl:.2f}s, {len(cap["seps"])} separation rows, {len(M.rows)} base rows, {len(M.ints)} binaries')
val = res[0][0]
seps = cap['seps']
def timed(label, **kw):
    M.last = None
    t = time.perf_counter(); r = M.solve(seps); dt = time.perf_counter() - t
    a = abs(p8.area_of(r[0], p8.outline(M))) / 4 if r else None
    geo, decks = p8.geometry(M, r[0]); nb = len(p8.clashes(M, geo, decks))
    print(f'{label}: {dt*1000:.0f} ms, area {a}, clashes left {nb}')
timed('A: sides known, one solve (bends free, all integer)')
lb0, ub0 = list(M.lb), list(M.ub)
for z in M.ints: M.lb[z] = M.ub[z] = float(val[z])
timed('B: sides known + bend choices fixed (all integer)')
p8.INTALL = False; ints0 = M.ints; M.ints = set()
timed('C: B as a pure LP (integrality only on parity aux)')
M.par_saved = M.par; M.par = {}
timed('D: B as a pure LP, no parity rows')
