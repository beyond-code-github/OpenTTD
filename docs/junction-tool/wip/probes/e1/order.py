"""E1b: each PSG 197 line's tile sequence in travel order, and its crossings ordered along it.
Run: python3 -I order.py   (reads psg197-decisions.md, psg197-crossings-resolved.md; imports the trace's core.py)
"""
import importlib.util, re, sys, collections
HERE = '/home/pete/OpenTTD/docs/junction-tool/wip'
OD = '/mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad/psg/anatomy/psg197/trace'
spec = importlib.util.spec_from_file_location('core', OD + '/core.py')
core = importlib.util.module_from_spec(spec); spec.loader.exec_module(core)
N = core.build()

def tile(s):
    x, y = s.split(','); return (int(x), int(y))

dec = open(HERE + '/topologies/psg197-decisions.md').read().splitlines()
res = open(HERE + '/probes/e1/psg197-crossings-resolved.md').read().splitlines()

# tunnel table: entrance -> exit (rows `| # | ent | exit | middle | ...`)
tunnels = {}
for l in dec:
    m = re.match(r'\| \d+ \| (\d+,\d+) \| (\d+,\d+) \| (\d+) \|', l)
    if m: tunnels[tile(m.group(1))] = (tile(m.group(2)), int(m.group(3)))
# line index
index = []   # (line, from, to, tunnels)
inidx = False
for l in dec:
    if l.startswith('## Line index'): inidx = True; continue
    if inidx and l.startswith('| ') and not l.startswith('| line') and not l.startswith('|---'):
        c = [x.strip() for x in l.strip('|').split('|')]
        tl = [] if c[4] == '-' else [tile(t) for t in c[4].split()]
        index.append((c[0], c[2], c[3], tl))
# crossing pairs
pairs = []
for l in res:
    m = re.match(r'\| (\w+) \| (\w+) \| tunnel \|', l)
    if m: pairs.append((m.group(1), m.group(2)))
pairset = {frozenset(p) for p in pairs}

lanes = {'%s%d' % (L['side'], L['pos']): L['pc'] for L in N.lanes}
core_pieces = N.core
bytile = collections.defaultdict(list)
for pc in core_pieces: bytile[pc[0]].append(pc)

def endpoint(s):
    if s.startswith('port '):
        pc = lanes[s[5:]]; return pc[0], [pc]
    t = tile(s); return t, bytile[t]

def between(a, b):
    dx = (b[0] > a[0]) - (b[0] < a[0]); dy = (b[1] > a[1]) - (b[1] < a[1])
    out = []; p = (a[0] + dx, a[1] + dy)
    while p != b: out.append(p); p = (p[0] + dx, p[1] + dy)
    return out

def walks(frm, to, tl, name):
    ft, fpcs = endpoint(frm); tt, _ = endpoint(to)
    ntl = len(tl); full = (1 << ntl) - 1
    # k is a bitmask of the line's listed tunnels used so far (the index lists them in no fixed order)
    def succ(pc, k):
        for b in N.CS[pc]:
            if abs(pc[0][0]-b[0][0]) + abs(pc[0][1]-b[0][1]) > 1:   # tunnel jump
                if pc[0] in tl and not k >> tl.index(pc[0]) & 1: yield b, k | 1 << tl.index(pc[0])
            else: yield b, k
    goal = [(pc, full) for pc in bytile[tt]]
    states = [(pc, k) for pc in core_pieces for k in range(full + 1)]
    rev = collections.defaultdict(list)
    for s in states:
        for t in succ(*s): rev[t].append(s)
    good = set(goal); st = list(goal)
    while st:
        s = st.pop()
        for p in rev[s]:
            if p not in good: good.add(p); st.append(p)
    found = set(); cap = [0]
    def dfs(pc, k, path, seen):
        if cap[0] > 20000: return
        if pc[0] == tt and k == full:
            cap[0] += 1; found.add(tuple(path)); return
        for b, k2 in succ(pc, k):
            if (b, k2) in good and b[0] not in seen:
                seen.add(b[0]); path.append(b); dfs(b, k2, path, seen); path.pop(); seen.discard(b[0])
    for pc in fpcs:
        if (pc, 0) in good: dfs(pc, 0, [pc], {pc[0]})
    return found, cap[0] > 20000


breaches = []
used = sorted({l for p in pairs for l in p})
allwalks = {}
for (name, frm, to, tl) in index:
    if name not in used: continue
    w, capped = walks(frm, to, tl, name)
    if capped: breaches.append('BREACH walk of %s: more than 20000 walks (capped)' % name)
    if not w: breaches.append('BREACH walk of %s: none found' % name)
    else: allwalks[name] = w
    for t in tl:
        if t not in tunnels: breaches.append('BREACH %s: tunnel %s not in table' % (name, t))

# ---- geometry (E1b2, Ruling 1) ----
SIX = {'X': ('NE', 'SW'), 'Y': ('NW', 'SE'), 'Upper': ('NE', 'NW'), 'Lower': ('SW', 'SE'),
       'Left': ('SW', 'NW'), 'Right': ('NE', 'SE')}
def mid(t, e):
    x, y = t
    return {'NE': (x, y + .5), 'SW': (x + 1, y + .5), 'NW': (x + .5, y), 'SE': (x + .5, y + 1)}[e]
def ctr(t): return (t[0] + .5, t[1] + .5)
def dist(a, b): return ((a[0]-b[0])**2 + (a[1]-b[1])**2) ** .5
def isjump(a, b): return abs(a[0][0]-b[0][0]) + abs(a[0][1]-b[0][1]) > 1
pnames = set()
def ends(pc):
    n = N.W.pname(pc); pnames.add(n)
    if n not in SIX: raise SystemExit('STOP: piece name %s not one of the six' % n)
    return [mid(pc[0], e) for e in SIX[n]]
def polyline(path):
    n = len(path); inj = [i > 0 and isjump(path[i-1], path[i]) for i in range(n)]
    outj = [i + 1 < n and isjump(path[i], path[i+1]) for i in range(n)]
    pts = []; prev_exit = None
    for i, pc in enumerate(path):
        E = ends(pc); c = ctr(pc[0])
        if outj[i]: tgt = ctr(path[i+1][0]); exit_ = None
        elif i + 1 < n:
            nE = ends(path[i+1]); sh = [p for p in E if p in nE]
            if len(sh) != 1: raise SystemExit('STOP: pieces %s %s share %d points' % (pc, path[i+1], len(sh)))
            exit_ = sh[0]
        else:
            ref = ctr(path[i-1][0]) if i else None
            exit_ = max(E, key=lambda p: dist(p, ref)) if ref else E[1]
        if inj[i]: entry = None
        elif prev_exit is not None: entry = prev_exit
        else:
            entry = [p for p in E if p != exit_][0] if exit_ else min(E, key=lambda p: dist(p, tgt)) if False else \
                    max(E, key=lambda p: dist(p, tgt))
        if not inj[i] and entry not in E: raise SystemExit('STOP: entry %s not on piece %s' % (entry, pc))
        if inj[i] and not outj[i] and exit_ is None: raise SystemExit('STOP')
        s = c if inj[i] else entry
        e = c if outj[i] else exit_
        if inj[i] and not outj[i] and i + 1 < n: pass
        for p in (s, e):
            if not pts or dist(pts[-1], p) > 1e-12: pts.append(p)
        prev_exit = exit_
    return pts
def displace(pts, d):
    segn = []
    for a, b in zip(pts, pts[1:]):
        L = dist(a, b); dx, dy = (b[0]-a[0])/L, (b[1]-a[1])/L
        segn.append((-dy, dx))
    out = []
    for i, p in enumerate(pts):
        if i == 0: n = segn[0]; out.append((p[0]+d*n[0], p[1]+d*n[1])); continue
        if i == len(pts) - 1: n = segn[-1]; out.append((p[0]+d*n[0], p[1]+d*n[1])); continue
        n1, n2 = segn[i-1], segn[i]; k = d / (1 + n1[0]*n2[0] + n1[1]*n2[1])
        out.append((p[0]+k*(n1[0]+n2[0]), p[1]+k*(n1[1]+n2[1])))
    return out
def arcs(pts):
    s = [0.0]
    for a, b in zip(pts, pts[1:]): s.append(s[-1] + dist(a, b))
    return s
def pointat(pts, S, s):
    for i in range(len(pts) - 1):
        if S[i] - 1e-12 <= s <= S[i+1] + 1e-12:
            t = (s - S[i]) / (S[i+1] - S[i]); a, b = pts[i], pts[i+1]
            return (a[0] + t*(b[0]-a[0]), a[1] + t*(b[1]-a[1]))
def inter(p1, p2, q1, q2):
    r = (p2[0]-p1[0], p2[1]-p1[1]); q = (q2[0]-q1[0], q2[1]-q1[1])
    den = r[0]*q[1] - r[1]*q[0]
    if abs(den) < 1e-15: return None
    w = (q1[0]-p1[0], q1[1]-p1[1])
    t = (w[0]*q[1] - w[1]*q[0]) / den; u = (w[0]*r[1] - w[1]*r[0]) / den
    if 0 <= t <= 1 and 0 <= u <= 1: return t, u
rowk = {n: k for k, (n, *_) in enumerate(index)}

def build_geo(choice):
    G = {}
    for n, pth in choice.items():
        d = polyline(pth); dd = displace(d, 1e-6 * (rowk[n] + 1)); S = arcs(dd)
        G[n] = (dd, S)
    return G
def crossings(G, pairs, breaches, report):
    cross = collections.defaultdict(list); multi = []
    for L, M in pairs:
        if rowk[L] > rowk[M]: L, M = M, L
        (P, SP), (Q, SQ) = G[L], G[M]
        hits = []
        for i in range(len(P) - 1):
            for j in range(len(Q) - 1):
                r = inter(P[i], P[i+1], Q[j], Q[j+1])
                if r:
                    t, u = r
                    hits.append((SP[i] + t*(SP[i+1]-SP[i]), SQ[j] + u*(SQ[j+1]-SQ[j])))
        # merge duplicate hits at shared vertices
        hits.sort(); h2 = []
        for h in hits:
            if not h2 or abs(h[0]-h2[-1][0]) > 1e-9: h2.append(h)
        hits = h2
        if not hits:
            if report: breaches.append('BREACH pair %s x %s: displaced polylines do not intersect' % (L, M))
            continue
        if len(hits) > 1: multi.append((L, M, len(hits)))
        if len(hits) % 2 == 0 and report: breaches.append('BREACH pair %s x %s: %d intersections (even)' % (L, M, len(hits)))
        sl, sm = hits[0]
        a = pointat(P, SP, sl); b = pointat(Q, SQ, sm)
        if dist(a, b) > 1e-9 and report: breaches.append('BREACH pair %s x %s: point mismatch %s %s' % (L, M, a, b))
        cross[L].append((sl, M, a)); cross[M].append((sm, L, a))
    return cross, multi

# choose walks: baseline first walk of each line
walkset = {n: sorted(w) for n, w in allwalks.items()}
choice = {n: w[0] for n, w in walkset.items()}
G = build_geo(choice)
cross, multi = crossings(G, pairs, breaches, True)
amb = [n for n, w in walkset.items() if len(w) > 1]
def orders(cr): return {n: [m for _, m, _ in sorted(v)] for n, v in cr.items()}
base = orders(cross)
amb_report = []
import itertools
combos = 1
for n in amb: combos *= len(walkset[n])
variants = []
if combos <= 5000:
    for combo in itertools.product(*[walkset[n] for n in amb]):
        c2 = dict(choice); c2.update(dict(zip(amb, combo))); variants.append(c2)
else:
    for n in amb:
        for w in walkset[n]:
            c2 = dict(choice); c2[n] = w; variants.append(c2)
for n in amb:
    diff = False
    for c2 in variants:
        cr2, _ = crossings(build_geo(c2), pairs, [], False)
        o2 = orders(cr2)
        for ln in o2:
            if o2[ln] != base.get(ln):
                diff = True
                breaches.append('BREACH order differs on %s when walks vary (ambiguous line %s)' % (ln, n))
                breaches.append('    base %s' % base.get(ln)); breaches.append('    alt  %s' % o2[ln])
                breaches.append('    walks of %s: %s' % (n, [[(p[0], N.W.pname(p)) for p in c2[n]]]))
                break
        if diff: break
    amb_report.append('ambiguous %s: %d walks%s' % (n, len(walkset[n]), '' if diff else ', order-invariant'))
print('variants checked: %d (%s)' % (len(variants), 'product' if combos <= 5000 else 'one line at a time'))
for a in amb_report: print(a)
print('pieces seen:', sorted(pnames))

order = [n for (n, *_) in index if n in cross]
stmts = []
for L in order:
    cr = sorted(cross[L])
    ss = [c[0] for c in cr]
    for x, y in zip(ss, ss[1:]):
        if y - x < 1e-12: breaches.append('BREACH tie on %s at s=%.9f' % (L, y))
    print('%s (%d segs, length %.3f):' % (L, len(G[L][0]) - 1, G[L][1][-1]))
    for s, m, p in cr: print('    s %9.4f at (%.3f, %.3f)  x %s' % (s, p[0], p[1], m))
    stmts.append('cross %s: %s' % (L, ', '.join(m for _, m, _ in cr)))
print('\nSTATEMENTS')
for s in stmts: print(s)
print()
for L, M, k in multi: print('INFO pair %s x %s: %d intersections' % (L, M, k))
print('lines walked %d, cross statements %d, pairs %d' % (len(choice), len(stmts), len(pairset)))
emitted = {frozenset((L, M)) for L in cross for _, M, _ in cross[L]}
if emitted != pairset: breaches.append('BREACH emitted pair set != file pair set (%d vs %d)' % (len(emitted), len(pairset)))
if len(pairs) != len(pairset): breaches.append('BREACH duplicate/reversed pairs: %d rows, %d unique' % (len(pairs), len(pairset)))
for b in breaches: print(b)
print('breaches: %d' % sum(1 for b in breaches if b.startswith('BREACH')))
