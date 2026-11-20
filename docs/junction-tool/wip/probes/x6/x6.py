"""X6 structure + X7 shape from x5's lines/crossings on PSG 197, compared with psg197.structure/shape/steps. python3 -I x6.py (~15 s)"""
import os, re, io, sys, contextlib, bisect
from collections import defaultdict
ROOT = "/home/pete/OpenTTD/.claude/ng1"
HERE = ROOT + "/docs/junction-tool/wip/probes/x6"
TOPO = ROOT + "/tools/junction/topologies/"
src = open(ROOT + "/docs/junction-tool/wip/probes/x5/cross.py").read()
w = "HERE = os.path.dirname(os.path.abspath(__file__))"
assert w in src
src = src.replace(w, "HERE = %r" % HERE)
G = {'__name__': 'x5', '__file__': ROOT + "/docs/junction-tool/wip/probes/x5/cross.py"}
with contextlib.redirect_stdout(io.StringIO()):
    exec(compile(src, 'cross.py', 'exec'), G)
NG = G['g']
SEG, ARC, X, EX, assign, E2H = G['SEG'], G['ARC'], G['X'], G['EX'], G['assign'], G['E2H']
chains, lines, isnode, ninfo, CS, CP, LID = G['chains'], G['lines'], G['isnode'], G['ninfo'], G['CS'], G['CP'], G['LID']
segs_of, shared, ends, ctr = G['segs_of'], G['shared'], G['ends'], G['ctr']
ports_in, ports_out = NG['ports_in'], NG['ports_out']
out = []
def P(*a):
    out.append(' '.join(str(x) for x in a)); print(*a)
EIGHTH = {(0, -1): 0, (1, -1): 1, (1, 0): 2, (1, 1): 3, (0, 1): 4, (-1, 1): 5, (-1, 0): 6, (-1, -1): 7}
def head(a, b):
    dx, dy = b[0]-a[0], b[1]-a[1]
    sx = 0 if abs(dx) < 1e-4 else (1 if dx > 0 else -1); sy = 0 if abs(dy) < 1e-4 else (1 if dy > 0 else -1)
    if sx and sy and abs(abs(dx)-abs(dy)) > 1e-4: raise SystemExit('STOP not an eighth %s %s' % (a, b))
    return EIGHTH[(sx, sy)]
LET = {'N': 0, 'E': 2, 'S': 4, 'W': 6}
INH = {'N': 4, 'E': 6, 'S': 0}      # heading of an in lane on that side
# ---------------- hand files
def tile(s): x, y = s.split(','); return (int(x), int(y))
dec = open(ROOT + '/docs/junction-tool/wip/topologies/psg197-decisions.md').read().splitlines()
ttab = {}
for l in dec:
    m = re.match(r'\| (\d+) \| (\d+,\d+) \| (\d+,\d+) \| (\d+) \| (\w+) \| (G\d+) ', l)
    if m: ttab[int(m.group(1))] = (tile(m.group(2)), tile(m.group(3)), int(m.group(4)), m.group(5))
hs = open(TOPO + 'psg197.structure.txt').read().splitlines()
H_T, H_G, H_S = {}, [], []
for l in hs:
    m = re.match(r'tunnel T(\d+) (\w+): (.*)$', l)
    if m: H_T[int(m.group(1))] = (m.group(2), [x.strip() for x in m.group(3).split(',')])
    m = re.match(r'group (\w+): (.*)$', l)
    if m: H_G.append([int(x.strip()[1:]) for x in m.group(2).split(',')])
    m = re.match(r'straight (\w+): (\w+)$', l)
    if m: H_S.append((m.group(1), None if m.group(2) == 'none' else m.group(2)))
# ---------------- X6 tunnels
jumps = {}
for k in SEG:
    for i, sg in enumerate(SEG[k]):
        if sg[2] == 'j': jumps[(k, i)] = (sg[3][0][0], sg[3][1][0])
def segidx(k, s):
    i = bisect.bisect_right(ARC[k], s) - 1; return min(max(i, 0), len(SEG[k]) - 1)
mem = defaultdict(list)
for ks, h in X.items():
    k1, s1, k2, s2 = h[0], h[1], h[2], h[3]
    i1, i2 = segidx(k1, s1), segidx(k2, s2)
    if (k1, i1) in jumps: mem[(k1, i1)].append((s1, k2))
    if (k2, i2) in jumps: mem[(k2, i2)].append((s2, k1))
ties = 0
EXT_T = {}   # head tile -> (k, [crossers in order], exit)
for key, hd in jumps.items():
    lst = sorted(mem.get(key, []))
    for a, b in zip(lst, lst[1:]):
        if abs(a[0]-b[0]) < 1e-9: ties += 1
    EXT_T[hd[0]] = (key[0], [c for _, c in lst], hd[1])
P('X6 tunnels: ext jumps', len(jumps), 'with >=1 crosser', sum(1 for v in EXT_T.values() if v[1]), 'crossing memberships', sum(len(v[1]) for v in EXT_T.values()), 'ties in order', ties)
hand_mem = sum(len(set(c)) for _, c in H_T.values())
P('hand: tunnel statements', len(H_T), 'memberships', hand_mem)
def hn(k): return '+'.join(E2H.get(k, [])) or 'L%d' % k
cls = defaultdict(list); tmatch = 0; sepok = 0; crok = 0; ordok = 0
for n, (sep, cr) in sorted(H_T.items()):
    ent = ttab[n][0]
    if ent not in EXT_T: cls['no ext tunnel at head'].append(n); continue
    k, ec, ex = EXT_T[ent]
    if assign.get(sep) != k:
        cls['separator differs'].append('T%d hand %s ext %s' % (n, sep, hn(k))); continue
    sepok += 1
    hl = [assign.get(c) for c in cr]
    if None in hl or sorted(hl) != sorted(ec):
        miss = [c for c, a in zip(cr, hl) if a not in ec]
        extra = [hn(e) for e in ec if e not in hl]
        cls['crosser set differs'].append('T%d %s: hand-only %s, ext-only %s' % (n, sep, miss, extra)); continue
    crok += 1
    if hl != ec: cls['crosser order differs'].append('T%d %s hand %s ext %s' % (n, sep, cr, [hn(e) for e in ec])); continue
    tmatch += 1
hand_heads = {ttab[n][0] for n in H_T}
extra_t = [(hd, hn(v[0]), [hn(c) for c in v[1]]) for hd, v in EXT_T.items() if v[1] and hd not in hand_heads]
P('X6 tunnels matched (separator, crossers, order all equal):', tmatch, 'of', len(H_T), '; separator equal', sepok, '; crosser set equal', crok)
for c, v in cls.items(): P('  CLASS', c, len(v), v[:6])
P('  ext tunnels with crossers but no hand statement:', len(extra_t), extra_t)
# groups: same heading and length, entrance tiles adjacent across the axis (side by side)
def tinfo(hd):
    a, b = hd, EXT_T[hd][2]; d = (b[0]-a[0], b[1]-a[1])
    return (d[0] > 0) - (d[0] < 0), (d[1] > 0) - (d[1] < 0), abs(d[0]) + abs(d[1])
def adj(a, b):
    ia, ib = tinfo(a), tinfo(b)
    if ia != ib: return False
    dx, dy, L = ia
    return (dx == 0 and a[1] == b[1] and abs(a[0]-b[0]) == 1) or (dy == 0 and a[0] == b[0] and abs(a[1]-b[1]) == 1)
par = {h: h for h in EXT_T}
def f(x):
    while par[x] != x: par[x] = par[par[x]]; x = par[x]
    return x
withc = [h for h, v in EXT_T.items() if v[1]]
for a in withc:
    for b in withc:
        if a < b and adj(a, b): par[f(a)] = f(b)
gm = defaultdict(set)
for h in withc: gm[f(h)].add(h)
eg = {frozenset(v) for v in gm.values() if len(v) >= 2}
hg = {frozenset(ttab[n][0] for n in g_) for g_ in H_G}
P('X6 groups: ext', len(eg), 'hand', len(hg), 'identical', len(eg & hg))
for g_ in sorted(eg - hg, key=sorted): P('  ext-only group', sorted(g_), [hn(EXT_T[h][0]) for h in sorted(g_)])
for g_ in sorted(hg - eg, key=sorted): P('  hand-only group', sorted(g_))
# ---------------- X6 straight legs
def offnode(k):
    return [sg for sg in SEG[k] if not (sg[2] == 'g' and sg[3] in isnode)]
def hd0(sg): return head(sg[0], sg[1])
ext_nodes = []   # (piece, kind, trunk_k, legs_k, result)
for p in sorted(isnode, key=lambda p: p[0]):
    kd = ninfo[p][0]
    if kd == 'cross': ext_nodes.append((p, kd, None, (), ('skip', 'cross node'))); continue
    if kd == 'split': tr_p, lg_p = CP[p], CS[p]
    else: tr_p, lg_p = CS[p], CP[p]
    trs = {LID.get(a) for a in tr_p}; lgs = tuple(sorted(LID.get(b, -1) for b in lg_p))
    if -1 in lgs or len(trs) > 1 or None in trs and tr_p:
        ext_nodes.append((p, kd, None, lgs, ('und', 'trunk/leg is a node piece'))); continue
    tk = trs.pop() if trs else None
    if tk is not None:
        sg = offnode(tk)
        if not sg: ext_nodes.append((p, kd, tk, lgs, ('und', 'trunk has no off-node piece'))); continue
        h = hd0(sg[-1] if kd == 'split' else sg[0])
    else:
        if kd == 'split' and p in ports_in: h = INH[ports_in[p][0]]
        elif kd == 'merge' and p in ports_out: h = LET[ports_out[p][0]]
        else: ext_nodes.append((p, kd, None, lgs, ('und', 'no trunk'))); continue
    ok = []
    for lk in lgs:
        sg = offnode(lk)
        if not sg: ext_nodes.append((p, kd, tk, lgs, ('und', 'leg has no off-node piece'))); break
        if hd0(sg[0] if kd == 'split' else sg[-1]) == h: ok.append(lk)
    else:
        ext_nodes.append((p, kd, tk, lgs, ('straight', ok[0]) if len(ok) == 1 else ('none', None) if not ok else ('und', 'several legs keep heading')))
rc = defaultdict(int)
for e in ext_nodes: rc[e[4][0]] += 1
P('X6 ext nodes', len(ext_nodes), dict(rc))
# hand nodes in file order
topo = open(TOPO + 'psg197.txt').read().splitlines()
hnodes = []
for l in topo:
    if not l.startswith('node '): continue
    lhs, rhs = l[5:].split(' -> ')
    L = [x.split('=')[0].strip() for x in lhs.split(',')]; R = [x.split('=')[0].strip() for x in rhs.split(',')]
    if len(L) == 1 and len(R) == 2: hnodes.append(('split', L[0], R))
    elif len(L) == 2 and len(R) == 1: hnodes.append(('merge', R[0], L))
base = [n for n in hnodes if n[1] not in ('A2', 'A2m')]
assert len(H_S) == len(base) + 2, (len(H_S), len(base))
hand = []
for n, (tn, st) in zip(base, H_S[:len(base)]):
    assert tn == n[1], (tn, n)
    hand.append((n[0], n[1], n[2], st))
hand.append(('split', 'A2', ['A2b', 'A2c', 'A2d'], 'A2b'))   # the 3-way: A2 -> A2m, A2m -> A2b
P('X6 hand nodes', len(hand), '(60 two-leg + the 3-way A2)')
used = set(); res = defaultdict(list); nm = 0; sm = 0
for kd, tn, legs, st in hand:
    tk = assign.get(tn); lk = {assign.get(l) for l in legs}
    cands = [e for e in ext_nodes if e[1] == kd and set(e[3]) == lk and None not in lk and (tk is None and e[2] is None or e[2] == tk)]
    if not cands and tk is None and None not in lk:
        cands = [e for e in ext_nodes if e[1] == kd and set(e[3]) == lk]
    if not cands:
        res['hand node with no ext node (lines unmatched or other wiring)'].append('%s %s -> %s' % (kd, tn, legs)); continue
    nm += 1; e = cands[0]; used.add(e[0])
    r = e[4]
    hst = assign.get(st) if st else None
    if r[0] == 'straight' and st and r[1] == hst: sm += 1
    elif r[0] == 'none' and st is None: sm += 1
    else: res['straight differs (%s)' % r[0]].append('%s %s -> %s: hand %s ext %s %s' % (kd, tn, legs, st, hn(r[1]) if r[1] is not None and r[0] == 'straight' else r[1], e[0][0]))
P('X6 straight: hand nodes matched to an ext node', nm, 'of', len(hand), '; straight leg equal', sm)
for c, v in res.items(): P('  CLASS', c, len(v), v[:8])
P('  ext nodes unused:', [(e[0][0], e[1], e[4]) for e in ext_nodes if e[0] not in used])
# ---------------- X7 shape
def shape_segs(k):
    s = segs_of(k); ch = chains[k]
    pre = [a for a in CP[ch[0]] if a in isnode]; nxt = [b for b in CS[ch[-1]] if b in isnode]
    if len(pre) == 1 and len(CP[ch[0]]) == 1:
        nd = pre[0]; sh = shared(nd, ch[0])
        if sh is not None: s.insert(0, ([q for q in ends(nd) if q != sh][0], sh, 'g', nd))
    if len(nxt) == 1 and len(CS[ch[-1]]) == 1:
        nd = nxt[0]; sh = shared(ch[-1], nd)
        if sh is not None: s.append((sh, [q for q in ends(nd) if q != sh][0], 'g', nd))
    return s
def lift(hs):
    o = []
    for h in hs:
        if not o: o.append(h); continue
        t = (h - o[-1]) % 8
        if t > 4: t -= 8
        o.append(o[-1] + t)
    return o
def key_of(sg): return (sg[2], sg[3])
ES = {}   # k -> (lifted, [(other, piece)], proj, newpieces, steps, notes)
notes = []
for k in sorted(SEG):
    ss = shape_segs(k)
    runs = []; segrun = []
    for a, b, kd, pc in ss:
        h = head(a, b)
        if runs and runs[-1] == h: pass
        else: runs.append(h)
        segrun.append(len(runs) - 1)
    raw = lift(runs)
    keyidx = {key_of(sg): i for i, sg in enumerate(ss)}
    at = []
    for s_, o, m, ty in EX[k]:
        i = segidx(k, s_); sg = SEG[k][i]
        at.append((o, segrun[keyidx[key_of(sg)]], m))
    # port end
    ch = chains[k]
    if ch[-1] in ports_out and not CS[ch[-1]]:
        P_ = LET[ports_out[ch[-1]][0]]; hh = raw[-1]
        if (hh - P_) % 8:
            for d in (-1, 1):
                if (hh + d - P_) % 8 == 0: raw.append(hh + d); break
            else: notes.append('L%d port end needs more than one eighth' % k)
    inport = ch[0] in ports_in and not CP[ch[0]]
    outport = ch[-1] in ports_out and not CS[ch[-1]]
    ends_ = [p for _, p, _ in at] + [len(raw) - 1]
    proj, newp, steps = [raw[0]], [], {}
    start = 0
    for kk, e in enumerate(ends_):
        seg = raw[start:e + 1]; to = raw[e]
        wig = sum(abs(seg[i + 1] - seg[i]) for i in range(len(seg) - 1))
        if wig > abs(to - raw[start]): steps[kk] = seg
        while proj[-1] != to: proj.append(proj[-1] + (1 if to > proj[-1] else -1))
        if kk + 1 < len(ends_): newp.append(len(proj) - 1)
        start = e
    ES[k] = (raw, at, proj, newp, steps)
P('X7 ext lines shaped', len(ES), 'notes', notes)
P('  ext pieces total', sum(len(v[0]) for v in ES.values()), 'steps', sum(len(v[4]) for v in ES.values()), 'crossings', sum(len(v[1]) for v in ES.values()))
# hand
H_shape, H_at, H_steps = {}, {}, {}
for l in open(TOPO + 'psg197.shape.txt'):
    m = re.match(r'shape (\w+): (.*)$', l.strip())
    if m: H_shape[m.group(1)] = [int(x) for x in m.group(2).split()]
    m = re.match(r'at (\w+): (.*)$', l.strip())
    if m: H_at[m.group(1)] = [(a.strip().split('@')[0], int(a.strip().split('@')[1])) for a in m.group(2).split(',')]
for l in open(TOPO + 'psg197.steps.txt'):
    m = re.match(r'step (\w+) (\d+): ([\d ]+)', l)
    if m: H_steps[(m.group(1), int(m.group(2)))] = [int(x) for x in m.group(3).split()]
P('hand: lines', len(H_shape), 'steps', len(H_steps), 'crossing entries', sum(len(v) for v in H_at.values()))
pj_ok = 0; st_ok = 0; both = 0; pc_ok = 0; pc_n = 0; mcls = defaultdict(list); shared_ext = defaultdict(list)
for n in H_shape:
    k = assign.get(n)
    if k is None: mcls['hand line with no ext line'].append(n); continue
    shared_ext[k].append(n)
    raw, at, proj, newp, steps = ES[k]
    ok = proj == H_shape[n]
    hst = {kk: v for (nn, kk), v in H_steps.items() if nn == n}
    sok = steps == hst
    pj_ok += ok; st_ok += sok; both += ok and sok
    if not ok: mcls['projection differs'].append('%s (L%d) hand %s ext %s' % (n, k, H_shape[n], proj))
    if not sok: mcls['steps differ'].append('%s (L%d) hand %s ext %s' % (n, k, hst, steps))
    pm = {o: newp[i] for i, (o, p, m) in enumerate(at)}
    for M, p in H_at.get(n, []):
        km = assign.get(M)
        if km is None or km not in pm: continue
        pc_n += 1
        if pm[km] == p: pc_ok += 1
        else: mcls['crossing piece differs'].append('%s x %s hand @%d ext @%d' % (n, M, p, pm[km]))
P('X7 lines with a match', len(H_shape) - len(mcls['hand line with no ext line']), 'of', len(H_shape), '; identical projection', pj_ok, '; identical steps', st_ok, '; both', both)
P('X7 crossing pieces equal on common pairs', pc_ok, 'of', pc_n)
for c, v in mcls.items(): P('  CLASS', c, len(v)); [P('     ', x) for x in v[:40]]
P('  hand names sharing one ext line:', {k: v for k, v in shared_ext.items() if len(v) > 1})
open(HERE + '/x6-out.txt', 'w').write('\n'.join(out) + '\n')
