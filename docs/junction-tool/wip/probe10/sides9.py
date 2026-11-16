import sys, time
sys.argv += ['--n=3']
import probe8 as p8
import probe9 as p9
calls = []
orig_sep = p8.sep_options
def rec(M, geo, pr):
    out = orig_sep(M, geo, pr); calls.append((pr, out, {k: geo[k] for k in pr})); return out
p8.sep_options = rec
p8.BUDGET = 60
cap = {}
orig = p8.solve_lazy1
def wrap(M, t0, seps0):
    r, why = orig(M, t0, seps0)
    if r is not None: cap['seps'] = r[4]
    return r, why
p8.solve_lazy1 = wrap
M = p9.build_tn(3, 1, -1)
t = time.perf_counter(); res, why = p8.solve_lazy(M, t)
print('T3 +1w-1', why, M.area if res else None, f'{time.perf_counter()-t:.1f}s')
final = cap['seps']
key = lambda rw: (tuple(sorted(rw[0].items())), rw[1])
fk = {key(r) for r in final}
seen = set()
for pr, out, g in calls:
    for f, rows in out:
        if all(key(r) in fk for r in rows) and (pr[0][0], pr[1][0], f) not in seen:
            seen.add((pr[0][0], pr[1][0], f))
            (p, i), (q, j) = pr
            print(f'{p}[{i}] {g[pr[0]][3]:>2} | {q}[{j}] {g[pr[1]][3]:>2} -> {f}')
