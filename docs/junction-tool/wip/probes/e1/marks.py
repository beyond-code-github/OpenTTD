"""E1f-1: crossing direction marks for psg197 `cross` statements. Run: python3 -I marks.py
Imports order.py's code by executing it (its own report goes to the null device)."""
import sys, io, re, contextlib
HERE = '/home/pete/OpenTTD/docs/junction-tool/wip/probes/e1'
src = open(HERE + '/order.py').read()
g = {'__name__': 'order_mod'}
with contextlib.redirect_stdout(io.StringIO()):
    exec(compile(src, HERE + '/order.py', 'exec'), g)
G, inter = g['G'], g['inter']
TOP = '/home/pete/OpenTTD/tools/junction/topologies/psg197.txt'
lines = [l.rstrip('\n') for l in open(TOP)]
cross = [l for l in lines if l.startswith('cross ')]
stm = []
for l in cross:
    m = re.match(r'cross (\w+): (.*)$', l); stm.append((m.group(1), [x.strip() for x in m.group(2).split(',')]))
pairs = {frozenset((L, M)) for L, ms in stm for M in ms}
SPECIAL = {frozenset(('E2', 'E2b')): (82.5, 162.0), frozenset(('D12c', 'D23c')): (102.0, 178.5)}
def hitsof(L, M):
    P, _ = G[L]; Q, _ = G[M]; out = []
    for i in range(len(P)-1):
        for j in range(len(Q)-1):
            r = inter(P[i], P[i+1], Q[j], Q[j+1])
            if r:
                t, u = r; pt = (P[i][0]+t*(P[i+1][0]-P[i][0]), P[i][1]+t*(P[i+1][1]-P[i][1]))
                out.append((pt, i, j))
    return out
def unit(a, b):
    L = ((b[0]-a[0])**2 + (b[1]-a[1])**2) ** .5; return ((b[0]-a[0])/L, (b[1]-a[1])/L)
def mk(dL, dM):
    dot = dM[0]*dL[1] + dM[1]*(-dL[0])   # dot(dM, left(dL)), left=(y,-x)
    if abs(dot) < 1e-9: raise SystemExit('STOP: parallel at P')
    return '>' if dot < 0 else '<'
info = {}; out = []; bad = []
rk = g['rowk']
for pr in sorted(pairs, key=lambda p: sorted(p)):
    A, B = sorted(pr, key=lambda n: rk[n])
    hs = hitsof(A, B)
    if not hs: raise SystemExit('STOP: no intersection %s %s' % (A, B))
    if pr in SPECIAL:
        sp = SPECIAL[pr]; c = [h for h in hs if abs(h[0][0]-sp[0]) < 1e-3 and abs(h[0][1]-sp[1]) < 1e-3]
        if len(c) < 1: raise SystemExit('STOP: named point missing %s %s %s' % (A, B, hs))
        h = c[0]
    else: h = hs[0]
    pt, i, j = h
    dA = unit(G[A][0][i], G[A][0][i+1]); dB = unit(G[B][0][j], G[B][0][j+1])
    mBinA = mk(dA, dB); mAinB = mk(dB, dA)
    if mBinA == mAinB: bad.append((A, B))
    info[(A, B)] = mBinA; info[(B, A)] = mAinB    # (L, M) -> mark of M in L's statement
    out.append('%s x %s  P=(%.3f, %.3f)  d%s=(%.3f, %.3f) d%s=(%.3f, %.3f)  mark %s in %s: %s  mark %s in %s: %s  hits=%d' %
               (A, B, pt[0], pt[1], A, *dA, B, *dB, B, A, mBinA, A, B, mAinB, len(hs)))
new = []
for L, ms in stm: new.append('cross %s: %s' % (L, ', '.join(M + info[(L, M)] for M in ms)))
open(HERE + '/psg197-cross-marked.txt', 'w').write('\n'.join(new) + '\n')
open(HERE + '/marks-out.txt', 'w').write('\n'.join(out) + '\n')
strip = [re.sub(r'([<>])(?=,|$)', '', l) for l in new]
print('pairs', len(pairs), 'statements', len(stm), 'same-mark pairs', bad)
print('strip equals original:', strip == cross)
print('> count', sum(l.count('>') for l in new), '< count', sum(l.count('<') for l in new))
