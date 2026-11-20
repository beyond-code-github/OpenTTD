"""E1d-1: PSG 197's Structure decisions file (tunnels, groups, copy, straight legs; D389).
Run: python3 -I structure.py > structure-out.txt   (writes tools/junction/topologies/psg197.structure.txt)
Walk and segment code copied from order.py (lines 4-85 and 98-139), unchanged.
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
def polyline(path, rec=None):
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
        if rec is not None: rec.append((pc, s, e))
        for p in (s, e):
            if not pts or dist(pts[-1], p) > 1e-12: pts.append(p)
        prev_exit = exit_
    return pts

# ---------------- E1d-1 ----------------
REPO = '/home/pete/OpenTTD'
OUTFILE = REPO + '/tools/junction/topologies/psg197.structure.txt'
topo = open(REPO + '/tools/junction/topologies/psg197.txt').read().splitlines()
breaches2 = []
def breach(s): breaches2.append(s); print('BREACH ' + s)

# cross statements
cross_stmt = {}
for l in topo:
    m = re.match(r'cross (\w+): (.*)$', l)
    if m: cross_stmt[m.group(1)] = [x.strip() for x in m.group(2).split(',')]
pairs121 = set()
for L, ms in cross_stmt.items():
    for M in ms: pairs121.add(frozenset((L, M)))
print('cross statements %d, pairs %d' % (len(cross_stmt), len(pairs121)))
for L, ms in cross_stmt.items():
    for M in ms:
        if L not in cross_stmt.get(M, []): breach('pair %s %s not listed by both lines' % (L, M))

# tunnel table: #, entrance, exit, middle, separator, group
ttab = []
for l in dec:
    m = re.match(r'\| (\d+) \| (\d+,\d+) \| (\d+,\d+) \| (\d+) \| (\w+) \| (G\d+) ', l)
    if m: ttab.append((int(m.group(1)), tile(m.group(2)), int(m.group(4)), m.group(5), m.group(6)))
bynum = {t[0]: t for t in ttab}; byent = {t[1]: t for t in ttab}
print('tunnel table rows', len(ttab))

# tile pair rows (decisions file, section "Crossing pairs from tile evidence")
members = collections.defaultdict(list)    # tunnel# -> crossers
pairmem = collections.defaultdict(int)
insec = False; ntile = 0; ntile_kept = 0
for l in dec:
    if l.startswith('## Crossing pairs'): insec = True; continue
    if l.startswith('## Line index'): insec = False
    if not insec: continue
    m = re.match(r'\| (\w+) \| (\w+) \| tunnel \| \d+ \| (\d+,\d+) \|', l)
    if not m: continue
    ntile += 1
    sep, cr, ent = m.group(1), m.group(2), tile(m.group(3))
    if frozenset((sep, cr)) not in pairs121: continue      # not among the 121
    ntile_kept += 1
    t = byent[ent]
    if t[3] != sep: breach('tile row %s %s: tunnel %d separator is %s' % (sep, cr, t[0], t[3]))
    members[t[0]].append(cr); pairmem[frozenset((sep, cr))] += 1
print('tile rows %d, in the 121: %d' % (ntile, ntile_kept))
# tunnel-under-tunnel
ntt = 0
for l in open(HERE + '/probes/e1/tunnel-tunnel.txt'):
    m = re.match(r'TT (\d+) (\w+) (\d+) (\w+) ', l)
    if not m: continue
    a, la, b, lb = int(m.group(1)), m.group(2), int(m.group(3)), m.group(4)
    ntt += 1
    if bynum[a][3] != la or bynum[b][3] != lb: breach('TT line %s: separators differ from table' % l.strip())
    if frozenset((la, lb)) not in pairs121: breach('TT pair %s %s not in the 121' % (la, lb))
    members[a].append(lb); members[b].append(la)
    pairmem[frozenset((la, lb))] += 2
ttpairs = set()
for l in open(HERE + '/probes/e1/tunnel-tunnel.txt'):
    m = re.match(r'TT (\d+) (\w+) (\d+) (\w+) ', l)
    if m: ttpairs.add(frozenset((m.group(2), m.group(4))))
print('TT rows %d, distinct TT pairs %d' % (ntt, len(ttpairs)))

# order crossers by the separator's cross statement; checks
stmts_t = []
tun_with = []
for (n, ent, mid_, sep, grp) in ttab:
    cs = members.get(n)
    if not cs: continue
    tun_with.append(n)
    lst = cross_stmt[sep]
    dup = [c for c in set(cs) if cs.count(c) > 1]
    if dup: breach('tunnel %d: duplicate crossers %s' % (n, dup))
    for c in cs:
        if c not in lst: breach('tunnel %d: crosser %s not in cross %s' % (n, c, sep))
        if frozenset((sep, c)) not in pairs121: breach('tunnel %d: %s %s not in the 121' % (n, sep, c))
    cs_sorted = sorted(set(cs), key=lambda c: lst.index(c))
    pos = [lst.index(c) for c in cs_sorted]
    if pos[-1] - pos[0] + 1 != len(pos): breach('tunnel %d (%s): crossers not contiguous in cross list, positions %s' % (n, sep, pos))
    stmts_t.append('tunnel T%d %s: %s' % (n, sep, ', '.join(cs_sorted)))
    print('T%d %s (%s) crossers: %s' % (n, sep, ent, ', '.join(cs_sorted)))
print('tunnels with a statement:', len(tun_with), '(expected 49)')
if len(tun_with) != 49: breach('tunnel statement count %d != 49' % len(tun_with))
if 49 in tun_with: breach('tunnel 49 has a statement')
total = sum(len(set(members[n])) for n in tun_with)
print('total memberships:', total, '(expected 135)')
if total != 135: breach('total memberships %d != 135' % total)
for p in pairs121:
    want = 2 if p in ttpairs else 1
    if pairmem[p] != want: breach('pair %s in %d tunnel statements, want %d' % (sorted(p), pairmem[p], want))
if len(ttpairs) != 14: breach('TT distinct pairs %d != 14' % len(ttpairs))

# groups
grp_members = collections.defaultdict(list)
for (n, ent, mid_, sep, grp) in ttab:
    if n in tun_with: grp_members[grp].append(n)
stmts_g = ['group %s: %s' % (g, ', '.join('T%d' % n for n in ns)) for g, ns in sorted(grp_members.items()) if len(ns) >= 2]
print('group statements:', len(stmts_g))
# copy (G18): tunnels 30 and 32 from the table's copy column
copy_t = [n for n in (30, 32)]
if not all(bynum[n][4] == 'G18' for n in copy_t): breach('tunnels 30, 32 not both G18')
stmt_copy = 'copy D3 over T30, T32'

# ---- straight legs ----
nodes = []
for l in topo:
    if not l.startswith('node '): continue
    lhs, rhs = l[5:].split(' -> ')
    L = [x.split('=')[0].strip() for x in lhs.split(',')]
    R = [x.split('=')[0].strip() for x in rhs.split(',')]
    if len(L) == 1 and len(R) == 2: nodes.append(('split', L[0], R))
    elif len(L) == 2 and len(R) == 1: nodes.append(('merge', R[0], L))
    else: breach('node shape not 1->2 or 2->1: %s' % l)
need = sorted({x for k, tr, lg in nodes for x in [tr] + lg})
idx = {n: (n, f, t, tl) for (n, f, t, tl) in index}
walk0 = {}; walkerr = {}
for n in need:
    if n not in idx: walkerr[n] = 'not in line index'; continue
    try:
        w, capped = walks(idx[n][1], idx[n][2], idx[n][3], n)
    except SystemExit as e:
        walkerr[n] = 'walk stop: %s' % e; continue
    if capped: walkerr[n] = 'walk capped'; continue
    if not w: walkerr[n] = 'no walk'; continue
    walk0[n] = sorted(w)[0]
def segs(n):
    """per-piece segments of n's first walk: [(tile, piece, heading)], zero-length segments dropped"""
    rec = []; polyline(walk0[n], rec); out = []
    for pc, a, b in rec:
        L = dist(a, b)
        if L > 1e-12: out.append((pc[0], pc, (round((b[0]-a[0])/L, 6) + 0.0, round((b[1]-a[1])/L, 6) + 0.0)))
    return out
scache = {}
def S(n):
    if n not in scache:
        try: scache[n] = segs(n)
        except SystemExit as e: scache[n] = 'polyline stop: %s' % e
    return scache[n]
PORTDIR = {'N': (0, -1), 'S': (0, 1), 'E': (1, 0), 'W': (-1, 0)}
portstmt = {}   # (side, 'in'/'out') -> travel heading
for l in topo:
    m = re.match(r'port (\w) (in|out) (\w):', l)
    if m: portstmt[(m.group(1), m.group(2))] = PORTDIR[m.group(3)]
def fh(h): return '(%g, %g)' % (h[0] + 0.0, h[1] + 0.0)
def fs(sg): return '%s,%s %s' % (sg[0][0], sg[0][1], fh(sg[2])) if False else '%d,%d %s %s' % (sg[0][0], sg[0][1], N.W.pname(sg[1]), fh(sg[2]))

def trunk_heading(kind, trunk):
    """heading of the trunk's last (split) / first (merge) segment off the node tile; port heading if none"""
    sg = S(trunk); w = walk0[trunk]
    nt = w[-1][0] if kind == 'split' else w[0][0]
    off = [x for x in sg if x[0] != nt]
    if off:
        x = off[-1] if kind == 'split' else off[0]
        return x[2], 'segment ' + fs(x)
    ep = idx[trunk][1] if kind == 'split' else idx[trunk][2]
    if not ep.startswith('port '): raise SystemExit('trunk %s has no off-node segment and no port end' % trunk)
    side = ep[5]; d = portstmt[(side, 'in' if kind == 'split' else 'out')]
    return d, 'port %s %s' % (side, 'in' if kind == 'split' else 'out')

def leg_segment(kind, trunk, leg):
    tw = {x[1] for x in S(trunk)}
    own = [x for x in S(leg) if x[1] not in tw]
    if not own: raise SystemExit('leg %s has no segment outside trunk %s' % (leg, trunk))
    return own[0] if kind == 'split' else own[-1]

def bad_walks(names):
    out = []
    for x in names:
        if x in walkerr: out.append('%s: %s' % (x, walkerr[x]))
        elif not isinstance(S(x), list): out.append('%s: %s' % (x, S(x)))
    return out

stmts_s = []; nstraight = nfork = nund = 0; und = []
def decide(label, kind, trunk, legs):
    """returns (kind_of_result, leg) after printing; kind_of_result in straight/none/und"""
    global nstraight, nfork, nund
    bad = bad_walks([trunk] + legs)
    if not bad:
        try:
            h, src = trunk_heading(kind, trunk)
            lsg = [(lg, leg_segment(kind, trunk, lg)) for lg in legs]
        except SystemExit as e:
            bad = [str(e)]
    if bad:
        why = '; '.join(bad)
        print('node %s %s: UNDETERMINED %s' % (kind, label, why)); return 'und', why
    ok = [lg for lg, sg in lsg if sg[2] == h]
    print('node %s trunk %s heading %s from %s; legs %s -> %s' % (kind, label, fh(h), src,
          '; '.join('%s %s' % (lg, fs(sg)) for lg, sg in lsg), ok if ok else 'none'))
    if len(ok) == 1: return 'straight', ok[0]
    if not ok: return 'none', None
    return 'und', 'more than one leg keeps the trunk heading: ' + ', '.join(ok)

def emit(trunk, r, v):
    global nstraight, nfork, nund
    if r == 'straight': stmts_s.append('straight %s: %s' % (trunk, v)); nstraight += 1
    elif r == 'none': stmts_s.append('straight %s: none' % trunk); nfork += 1
    else: stmts_s.append('# straight %s: undetermined (%s)' % (trunk, v)); und.append((trunk, v)); nund += 1

skip = set()
for i, (kind, trunk, legs) in enumerate(nodes):
    if trunk == 'A2m' or (trunk == 'A2' and legs == ['A2d', 'A2m']): continue
    r, v = decide(trunk, kind, trunk, legs); emit(trunk, r, v)
# the 3-way split at 92,158 (A2 -> A2d, A2m; A2m -> A2b, A2c), in place of its two nodes
r, v = decide('A2 (3-way A2b, A2c, A2d)', 'split', 'A2', ['A2b', 'A2c', 'A2d'])
if r == 'straight' and v == 'A2d': emit('A2', 'straight', 'A2d'); emit('A2m', 'none', None)
elif r == 'straight': emit('A2', 'straight', 'A2m'); emit('A2m', 'straight', v)
else:
    w = v if r == 'und' else 'no leg keeps the trunk heading'
    emit('A2', 'und', w); emit('A2m', 'und', w)
print('nodes %d: straight %d, none %d, undetermined %d' % (len(nodes), nstraight, nfork, nund))
for t, w in und: print('UNDETERMINED %s: %s' % (t, w))
if len(nodes) != nstraight + nfork + nund: breach('node count mismatch')

hdr = ['# PSG 197 Structure decisions (D389, wip/CARD-E1d-psg197-variant.md); generated by docs/junction-tool/wip/probes/e1/structure.py.']
with open(OUTFILE, 'w') as f:
    f.write('\n'.join(hdr + stmts_t + stmts_g + [stmt_copy] + stmts_s) + '\n')
print('wrote', OUTFILE)
print('breaches: %d' % len(breaches2))
