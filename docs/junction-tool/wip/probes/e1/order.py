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
            cap[0] += 1; found.add(tuple(expand(path))); return
        for b, k2 in succ(pc, k):
            if (b, k2) in good and b not in seen:
                seen.add(b); path.append(b); dfs(b, k2, path, seen); path.pop(); seen.discard(b)
    for pc in fpcs:
        if (pc, 0) in good: dfs(pc, 0, [pc], {pc})
    return found, cap[0] > 20000

def expand(path):
    seq = []
    def add(t):
        if not seq or seq[-1] != t: seq.append(t)
    for i, pc in enumerate(path):
        add(pc[0])
        if i + 1 < len(path):
            a, b = pc[0], path[i+1][0]
            if abs(a[0]-b[0]) + abs(a[1]-b[1]) > 1:
                for m in between(a, b): add(m)
    return seq

breaches = []
used = sorted({l for p in pairs for l in p})
seqs = {}
for (name, frm, to, tl) in index:
    if name not in used: continue
    w, capped = walks(frm, to, tl, name)
    if capped: breaches.append('BREACH walk of %s: more than 20000 walks (capped)' % name)
    if len(w) != 1:
        breaches.append('BREACH walk of %s: %d distinct walks %s->%s tunnels %s' % (name, len(w), frm, to, tl))
        for x in sorted(w)[:4]: breaches.append('    %s' % (x,))
    if w: seqs[name] = list(sorted(w)[0])
    # tunnel-table check: middle length matches
    for t in tl:
        if t not in tunnels: breaches.append('BREACH %s: tunnel %s not in table' % (name, t))

# crossings
cross = collections.defaultdict(list)   # line -> [(pos, M, tile)]
info = []
emitted = set()
for L, M in pairs:
    if L not in seqs or M not in seqs: continue
    sh = set(seqs[L]) & set(seqs[M])
    if not sh:
        breaches.append('BREACH pair %s x %s: no shared tile' % (L, M)); continue
    if len(sh) > 1: info.append('INFO pair %s x %s: %d shared tiles %s' % (L, M, len(sh), sorted(sh)))
    pl_ = min(i for i, t in enumerate(seqs[L]) if t in sh)
    pm_ = min(i for i, t in enumerate(seqs[M]) if t in sh)
    cross[L].append((pl_, M, seqs[L][pl_])); cross[M].append((pm_, L, seqs[M][pm_]))
    emitted.add(frozenset((L, M)))

if emitted != pairset:
    breaches.append('BREACH emitted pair set != the file pair set (%d vs %d)' % (len(emitted), len(pairset)))
if len(pairs) != len(pairset): breaches.append('BREACH file has duplicate/reversed pairs: %d rows, %d unique' % (len(pairs), len(pairset)))
for L, M in pairs:
    if not any(m == M for _, m, _ in cross[L]) or not any(m == L for _, m, _ in cross[M]):
        breaches.append('BREACH pair %s x %s missing from a line list' % (L, M))
order = [n for (n, *_) in index if n in cross]
stmts = []
for L in order:
    cr = sorted(cross[L])
    pos = collections.defaultdict(list)
    for p, m, t in cr: pos[p].append(m)
    for p, ms in pos.items():
        if len(ms) > 1: breaches.append('BREACH tie on %s at position %d tile %s: %s' % (L, p, seqs[L][p], ms))
    print('%s (%d tiles, %s -> %s):' % (L, len(seqs[L]), seqs[L][0], seqs[L][-1]))
    for p, m, t in cr: print('    pos %3d tile %s  x %s' % (p, t, m))
    stmts.append('cross %s: %s' % (L, ', '.join(m for _, m, _ in cr)))
print('\nSTATEMENTS')
for s in stmts: print(s)
print()
for i in info: print(i)
print('lines walked %d, cross statements %d, pairs %d' % (len(seqs), len(stmts), len(pairset)))
for b in breaches: print(b)
print('breaches: %d' % sum(1 for b in breaches if b.startswith('BREACH')))
