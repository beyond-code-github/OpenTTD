"""Which pairs LL2's lazy loop separates, the direction it picked, and the strands' headings there."""
import time
import probe8 as p8
calls = []
orig_sep = p8.sep_options
def rec(M, geo, pr):
    out = orig_sep(M, geo, pr); calls.append((pr, out, {k: geo[k] for k in pr})); return out
p8.sep_options = rec
cap = {}
orig = p8.solve_lazy1
def wrap(M, t0, seps0):
    r, why = orig(M, t0, seps0)
    if r is not None: cap['seps'] = r[4]
    return r, why
p8.solve_lazy1 = wrap
M = p8.build_t2(-1, 'F', 1, 1)
res, why = p8.solve_lazy(M, time.perf_counter())
final = cap['seps']
key = lambda rw: (tuple(sorted(rw[0].items())), rw[1])
fk = {key(r) for r in final}
seen = set()
for pr, out, g in calls:
    for f, rows in out:
        if all(key(r) in fk for r in rows) and (pr, f) not in seen:
            seen.add((pr, f))
            (p, i), (q, j) = pr
            print(f'{p}[{i}] heading {g[pr[0]][3]:>2}  vs  {q}[{j}] heading {g[pr[1]][3]:>2}  ->  {q} pushed {f} of {p}')
