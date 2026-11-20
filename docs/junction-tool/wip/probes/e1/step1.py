"""E1h-1b step 1: re-derived `cross` statements for walk combination A or B (E1h-1b brief, amendment and tie-break ruling).
Run: python3 -I step1.py A|B  -> cross-rewalked-A.txt / cross-rewalked-B.txt
Walks: D12e/D12f per combination, F1c second walk, every other line its single walk; D12g not used (chord, no crossings).
Pairs: the 121 of psg197.txt + the restored T49 pair (D12f x D12d in A, D12e x D12d in B).
Positions, points and marks: order.py's displaced polylines, order.py's position rule, marks.py's mark rule."""
import sys, io, re, contextlib, collections
HERE = '/home/pete/OpenTTD/docs/junction-tool/wip/probes/e1'
comb = sys.argv[1]
src = open(HERE + '/order.py').read(); g = {'__name__': 'order_mod'}
with contextlib.redirect_stdout(io.StringIO()):
    exec(compile(src, HERE + '/order.py', 'exec'), g)
index, walks, build_geo, inter, rowk, polyline = g['index'], g['walks'], g['build_geo'], g['inter'], g['rowk'], g['polyline']
TOP = '/home/pete/OpenTTD/tools/junction/topologies/psg197.txt'
lines = [l.rstrip('\n') for l in open(TOP)]
stm = []
for l in lines:
    m = re.match(r'cross (\w+): (.*)$', l)
    if m: stm.append((m.group(1), [re.sub(r'[<>]$', '', x.strip()) for x in m.group(2).split(',')], l))
pairs = {frozenset((L, M)) for L, ms, _ in stm for M in ms}
newpair = frozenset(('D12f', 'D12d')) if comb == 'A' else frozenset(('D12e', 'D12d'))
pairs.add(newpair)
choice_k = {'D12e': 0, 'D12f': 1} if comb == 'A' else {'D12e': 1, 'D12f': 0}
choice_k['F1c'] = 1
need = sorted({n for p in pairs for n in p})
choice = {}
for (n, f, t, tl) in index:
    if n not in need: continue
    w, capped = walks(f, t, tl, n)
    ws = sorted(w)
    if capped or not ws: raise SystemExit('STOP: walk of %s' % n)
    choice[n] = ws[choice_k.get(n, 0)] if n in choice_k else ws[0]
    if n not in choice_k and len(ws) != 1: raise SystemExit('STOP: %s ambiguous' % n)
G = build_geo(choice)
SPECIAL = {frozenset(('E2', 'E2b')): (82.5, 162.0), frozenset(('D12c', 'D23c')): (102.0, 178.5)}
T49 = (100.5, 185.5)
def unit(a, b):
    L = ((b[0]-a[0])**2 + (b[1]-a[1])**2) ** .5; return ((b[0]-a[0])/L, (b[1]-a[1])/L)
def mk(dL, dM):
    dot = dM[0]*dL[1] + dM[1]*(-dL[0])
    if abs(dot) < 1e-9: raise SystemExit('STOP: parallel')
    return '>' if dot < 0 else '<'
cross = collections.defaultdict(list); marks = {}; stops = []; notes = []
for pr in sorted(pairs, key=lambda p: sorted(p)):
    A, B = sorted(pr, key=lambda n: rowk[n])
    (P, SP), (Q, SQ) = G[A], G[B]; hs = []
    for i in range(len(P)-1):
        for j in range(len(Q)-1):
            r = inter(P[i], P[i+1], Q[j], Q[j+1])
            if r:
                t, u = r; pt = (P[i][0]+t*(P[i+1][0]-P[i][0]), P[i][1]+t*(P[i+1][1]-P[i][1]))
                hs.append((pt, i, j, SP[i]+t*(SP[i+1]-SP[i]), SQ[j]+u*(SQ[j+1]-SQ[j])))
    # one point per place (a vertex hit reported by two segments)
    d = []
    for h in hs:
        if not any(abs(h[0][0]-x[0][0]) < 1e-4 and abs(h[0][1]-x[0][1]) < 1e-4 for x in d): d.append(h)
    if pr in SPECIAL:
        sp = SPECIAL[pr]; d = [h for h in d if abs(h[0][0]-sp[0]) < 1e-3 and abs(h[0][1]-sp[1]) < 1e-3]
    elif pr == newpair:
        d = [h for h in d if abs(h[0][0]-T49[0]) < 1e-3 and abs(h[0][1]-T49[1]) < 1e-3]
    if len(d) != 1:
        stops.append('%s x %s: %d distinct intersections %s' % (A, B, len(d), [(round(h[0][0], 3), round(h[0][1], 3)) for h in d])); continue
    pt, i, j, sa, sb = d[0]
    dA = unit(P[i], P[i+1]); dB = unit(Q[j], Q[j+1])
    mBA = mk(dA, dB); mAB = mk(dB, dA)
    if mBA == mAB: stops.append('%s x %s: same marks' % (A, B))
    marks[(A, B)] = mBA; marks[(B, A)] = mAB
    cross[A].append((sa, B)); cross[B].append((sb, A))
if stops:
    print('STOPS'); [print(' ', s) for s in stops]; sys.exit(1)
out = []; old = {re.match(r'cross (\w+):', l).group(1): l for _, _, l in stm}
for L in sorted(cross):
    cr = sorted(cross[L])
    for x, y in zip(cr, cr[1:]):
        if y[0]-x[0] < 1e-12: raise SystemExit('STOP: tie on %s' % L)
    out.append('cross %s: %s' % (L, ', '.join(M + marks[(L, M)] for _, M in cr)))
open(HERE + '/cross-rewalked-%s.txt' % comb, 'w').write('\n'.join(out) + '\n')
print('combination', comb, 'statements', len(out), 'pairs', len(pairs))
for l in out:
    L = re.match(r'cross (\w+):', l).group(1)
    if old.get(L) != l: print('CHANGED\n  old:', old.get(L), '\n  new:', l)
