"""X5: crossings, order and marks from x1c's lines on PSG 197, compared with psg197.txt. python3 -I cross.py"""
import os, re, io, sys, contextlib
from collections import defaultdict
ROOT = "/home/pete/OpenTTD/.claude/ng1"
HERE = os.path.dirname(os.path.abspath(__file__))
src = open(ROOT + "/docs/junction-tool/wip/probes/x1c/nodes.py").read()
w = "open(os.path.dirname(os.path.abspath(__file__))+'/nodes-out.txt','w').write("
assert w in src
src = src.replace(w, "(lambda s:None)(")
g = {'__name__': 'x1c', '__file__': ROOT + '/docs/junction-tool/wip/probes/x1c/nodes.py'}
sys.argv = ['x']
with contextlib.redirect_stdout(io.StringIO()):
    exec(compile(src, 'nodes.py', 'exec'), g)
N = g['N']; W = N.W; lines = g['lines']; LID = g['LID']; isnode = g['isnode']; CS = g['CS']; CP = g['CP']
ninfo = g['ninfo']; cand = g['cand']; tab = g['tab']; core = g['core']
out = []
def P(*a): out.append(' '.join(str(x) for x in a)); print(*a)

# ---- geometry (order.py's pieces as sub-tile segments, tunnels as centrelines)
SIX = {'X': ('NE', 'SW'), 'Y': ('NW', 'SE'), 'Upper': ('NE', 'NW'), 'Lower': ('SW', 'SE'), 'Left': ('SW', 'NW'), 'Right': ('NE', 'SE')}
def mid(t, e):
    x, y = t; return {'NE': (x, y+.5), 'SW': (x+1, y+.5), 'NW': (x+.5, y), 'SE': (x+.5, y+1)}[e]
def ctr(t): return (t[0]+.5, t[1]+.5)
def dist(a, b): return ((a[0]-b[0])**2+(a[1]-b[1])**2)**.5
def ends(pc): return [mid(pc[0], e) for e in SIX[W.pname(pc)]]
def isjump(a, b): return abs(a[0][0]-b[0][0])+abs(a[0][1]-b[0][1]) > 1
def shared(a, b):
    s = [p for p in ends(a) if p in ends(b)]; return s[0] if len(s) == 1 else None

# chain order of each line
chains = []
for k, ps in enumerate(lines):
    S = set(ps)
    st = [p for p in ps if not any(q in S for q in CP[p])]
    if len(st) != 1: chains.append(None); continue
    ch = [st[0]]
    while True:
        nx = [b for b in CS[ch[-1]] if b in S]
        if len(nx) != 1 or nx[0] in ch: break
        ch.append(nx[0])
    chains.append(ch if len(ch) == len(ps) else None)
P('lines', len(lines), 'with a simple chain', sum(1 for c in chains if c), 'without', [k for k, c in enumerate(chains) if not c])

def segs_of(k):
    """list of (a, b, kind, piece) in travel order; kind 'g' ground, 'j' jump"""
    ch = chains[k]; n = len(ch); res = []
    pre = [a for a in CP[ch[0]]]; nxt = list(CS[ch[-1]])
    for i, pc in enumerate(ch):
        E = ends(pc); c = ctr(pc[0])
        injump = i > 0 and isjump(ch[i-1], pc)
        outjump = i+1 < n and isjump(pc, ch[i+1])
        # exit
        if outjump: ex = None
        elif i+1 < n: ex = shared(pc, ch[i+1])
        elif nxt:
            sh = {shared(pc, b) for b in nxt}; ex = sh.pop() if len(sh) == 1 and None not in sh else None
        else: ex = None
        if injump: en = None
        elif i > 0: en = shared(ch[i-1], pc)
        elif pre:
            sh = {shared(a, pc) for a in pre}; en = sh.pop() if len(sh) == 1 and None not in sh else None
        else: en = None
        if ex is None and not outjump:
            ex = [p for p in E if p != en][0] if en else E[1]
        if en is None and not injump:
            en = [p for p in E if p != ex][0]
        if en is not None and ex is not None and en == ex: raise SystemExit('STOP entry==exit %s' % (pc,))
        if injump: res.append((c, ex, 'g', pc)) if ex else None
        elif outjump: res.append((en, c, 'g', pc))
        else: res.append((en, ex, 'g', pc))
        if injump and outjump: raise SystemExit('STOP head both')
        if outjump: res.append((c, ctr(ch[i+1][0]), 'j', (pc, ch[i+1])))
    return res
def displace(pts, d):
    segn = []
    for a, b in zip(pts, pts[1:]):
        L = dist(a, b); dx, dy = (b[0]-a[0])/L, (b[1]-a[1])/L; segn.append((-dy, dx))
    o = []
    for i, p in enumerate(pts):
        if i == 0: n = segn[0]; o.append((p[0]+d*n[0], p[1]+d*n[1])); continue
        if i == len(pts)-1: n = segn[-1]; o.append((p[0]+d*n[0], p[1]+d*n[1])); continue
        n1, n2 = segn[i-1], segn[i]; kk = d/(1+n1[0]*n2[0]+n1[1]*n2[1])
        o.append((p[0]+kk*(n1[0]+n2[0]), p[1]+kk*(n1[1]+n2[1])))
    return o
EXT = {}
for p in isnode:
    kd = ninfo[p][0]
    if kd == 'split': ls = {LID.get(a) for a in CP[p]}; pos = 'end'
    elif kd == 'merge': ls = {LID.get(b) for b in CS[p]}; pos = 'start'
    else: continue
    if len(ls) == 1 and None not in ls: EXT[ls.pop()] = (pos, p)
SEG = {}; ARC = {}; RAWPTS = {}
for k, ch in enumerate(chains):
    if not ch: continue
    s = segs_of(k)
    if k in EXT:
        pos, nd = EXT[k]
        if pos == 'end':
            sh = shared(ch[-1], nd)
            if sh is not None: s.append((sh, [p for p in ends(nd) if p != sh][0], 'g', nd))
        else:
            sh = shared(nd, ch[0])
            if sh is not None: s.insert(0, ([p for p in ends(nd) if p != sh][0], sh, 'g', nd))
    pts = [s[0][0]] + [x[1] for x in s]
    for i in range(len(s)-1):
        if dist(s[i][1], s[i+1][0]) > 1e-9: raise SystemExit('STOP discontinuous line %d' % k)
    RAWPTS[k] = {(round(p[0], 6), round(p[1], 6)) for p in pts}
    dp = displace(pts, 1e-6*(k+1))
    s = [(dp[i], dp[i+1], s[i][2], s[i][3]) for i in range(len(s))]
    SEG[k] = s
    a = [0.0]
    for x, y, kd, pc in s: a.append(a[-1]+dist(x, y))
    ARC[k] = a
njump = sum(1 for k in SEG for s in SEG[k] if s[2] == 'j')
P('segments: lines', len(SEG), 'tunnel jumps', njump, '(psg197-decisions: 50 tunnels, 0 bridges)')

def proper(p1, p2, q1, q2):
    r = (p2[0]-p1[0], p2[1]-p1[1]); q = (q2[0]-q1[0], q2[1]-q1[1])
    den = r[0]*q[1]-r[1]*q[0]
    if abs(den) < 1e-12: return None
    wv = (q1[0]-p1[0], q1[1]-p1[1])
    t = (wv[0]*q[1]-wv[1]*q[0])/den; u = (wv[0]*r[1]-wv[1]*r[0])/den
    e = 1e-9
    if e < t < 1-e and e < u < 1-e: return t, u
    return None
def unit(a, b):
    L = dist(a, b); return ((b[0]-a[0])/L, (b[1]-a[1])/L)

# node pieces' segments (ground) for the trunk rule
node_seg = {}
for p in isnode:
    E = ends(p); node_seg[p] = (E[0], E[1])
def trunk_of(p):
    """(line, 'end'|'start') the D390 (1) trunk: a split's in line (its end), a merge's out line (its start)"""
    kd = ninfo[p][0]
    if kd == 'split': ls = {LID.get(a) for a in CP[p]}; pos = 'end'
    elif kd == 'merge': ls = {LID.get(b) for b in CS[p]}; pos = 'start'
    else: return None, kd
    if len(ls) == 1 and None not in ls: return ls.pop(), pos
    return None, kd

# ---- collect hits
hits = defaultdict(list)   # frozenset(k1,k2) -> list of (k1, s1, k2, s2, point, d1, d2, type)
cnt = defaultdict(int); notes = []
items = []   # (line, seg index)
for k in SEG:
    for i, sg in enumerate(SEG[k]): items.append((k, i))
import itertools
def sat(k, i, t): return ARC[k][i]+t*(ARC[k][i+1]-ARC[k][i])
for (k1, i1), (k2, i2) in itertools.combinations(items, 2):
    if k1 == k2: continue
    a = SEG[k1][i1]; b = SEG[k2][i2]
    node_a = a[2] == 'g' and a[3] in isnode; node_b = b[2] == 'g' and b[3] in isnode
    if a[2] == 'g' and b[2] == 'g': ty = 'a'
    elif a[2] == 'j' and b[2] == 'j': ty = 'c'
    else: ty = 'b'
    if min(a[0][0], a[1][0]) > max(b[0][0], b[1][0]) or min(b[0][0], b[1][0]) > max(a[0][0], a[1][0]): continue
    r = proper(a[0], a[1], b[0], b[1])
    if not r: continue
    t, u = r
    pt = (a[0][0]+t*(a[1][0]-a[0][0]), a[0][1]+t*(a[1][1]-a[0][1]))
    shp = RAWPTS[k1] & RAWPTS[k2]
    if any(dist(pt, (q[0], q[1])) < 1e-3 for q in shp): cnt['spur'] += 1; continue
    hits[frozenset((k1, k2))].append((k1, sat(k1, i1, t), k2, sat(k2, i2, u), pt, unit(a[0], a[1]), unit(b[0], b[1]), ty))
    if ty == 'b' and (node_a or node_b): ty = 'n'
    if ty == 'a' and (node_a or node_b): ty = 'na'
    hits[frozenset((k1, k2))][-1] = hits[frozenset((k1, k2))][-1][:7] + (ty,)
    cnt[ty] += 1
# node pieces over jumps
nodehits = []
for p, (n0, n1) in []:
    for k in SEG:
        for i, sg in enumerate(SEG[k]):
            if sg[2] != 'j': continue
            r = proper(n0, n1, sg[0], sg[1])
            if r:
                t, u = r
                pt = (n0[0]+t*(n1[0]-n0[0]), n0[1]+t*(n1[1]-n0[1]))
                nodehits.append((p, k, sat(k, i, u), pt, unit(n0, n1), unit(sg[0], sg[1])))
P('raw segment crossings: a ground x ground (level)', cnt['a'], '; a at a trunk node piece', cnt['na'], '; b ground x tunnel', cnt['b'], '; n trunk node piece x tunnel', cnt['n'], '; c tunnel x tunnel', cnt['c'], '; spurious near shared endpoints dropped', cnt['spur'])
# node piece vs ground pieces of lines / other nodes (level crossings at a node)
lv_nodes = 0; lvl = []
for p, (n0, n1) in node_seg.items():
    for k in SEG:
        for i, sg in enumerate(SEG[k]):
            if sg[2] != 'g': continue
            if proper(n0, n1, sg[0], sg[1]): lv_nodes += 1; lvl.append((p[0], k))
for p, q in itertools.combinations(node_seg, 2):
    if proper(*node_seg[p], *node_seg[q]): lv_nodes += 1; lvl.append((p[0], q[0]))
P('node piece crossing a ground piece of a line or another node piece (level):', lv_nodes, lvl[:6])
lvl_a = [(k1, k2, h[4]) for ks, hl in hits.items() for h in hl for k1, k2 in [tuple(sorted(ks))] if h[7] in ('a','na')]
P('level crossings (a) among lines:', len(lvl_a), [(a, b, '%.1f,%.1f' % p) for a, b, p in lvl_a[:8]])

# ---- crossings per line pair; D390 (1) for node-over-tunnel
X = {}   # frozenset -> (k1,s1,k2,s2,pt,d1,d2,type)
multi = []
for ks, hl in hits.items():
    k1, k2 = sorted(ks); hl2 = sorted([h if h[0] == k1 else (h[2], h[3], h[0], h[1], h[4], h[6], h[5], h[7]) for h in hl], key=lambda h: h[1])
    if len(hl2) > 1: multi.append((k1, k2, len(hl2)))
    X[ks] = hl2[0]
trunk_pairs = []; trunk_drop = []
for (p, k, s, pt, dn, dj) in nodehits:
    tk, pos = trunk_of(p)
    if tk is None:
        trunk_drop.append((p[0], ninfo[p][:2], k)); continue
    if tk == k: trunk_drop.append((p[0], 'trunk is the tunnel line itself', k)); continue
    ks = frozenset((tk, k))
    s_t = 0.0 if pos == 'start' else ARC[tk][-1]
    # direction on the trunk at the node: the node piece direction is the trunk's end/start direction; use the piece segment
    h = (tk, s_t, k, s, pt, dn, dj, 'n')
    if ks in X: trunk_drop.append((p[0], 'pair already crossed elsewhere', tk, k)); continue
    X[ks] = h; trunk_pairs.append((p[0], tk, k))
P('trunk rule (D390 (1)): node-piece-over-tunnel hits recorded on the trunk line only: see n count above; trunk lines extended by their node piece:', len(EXT))
P('pairs with >1 hit (first hit along the lower-numbered line kept):', len(multi), multi[:8])
def mk(dL, dM):
    dot = dM[0]*dL[1]+dM[1]*(-dL[0]); return '>' if dot < 0 else '<'
EX = defaultdict(list)   # line -> [(s, other, mark, type)]
for ks, h in X.items():
    k1, s1, k2, s2, pt, d1, d2, ty = h
    EX[k1].append((s1, k2, mk(d1, d2), ty)); EX[k2].append((s2, k1, mk(d2, d1), ty))
for k in EX: EX[k].sort()
tyc = defaultdict(int)
for h in X.values(): tyc[h[7]] += 1
P('extractor crossing pairs', len(X), dict(tyc), '(n = node over tunnel via trunk rule)')

# ---- hand
HC = {}  # line -> [(M, mark)]
for l in open(ROOT+'/tools/junction/topologies/psg197.txt'):
    m = re.match(r'cross (\w+): (.*)$', l.strip())
    if m: HC[m.group(1)] = [(x.strip().rstrip('<>'), x.strip()[-1]) for x in m.group(2).split(',')]
hpairs = {frozenset((a, b)) for a, v in HC.items() for b, _ in v}
P('hand pairs', len(hpairs), 'statements', len(HC))
# hand name -> ext candidates; ambiguous (several candidate) lines get an injective assignment maximising agreement
H2E = {n: list(c) for n, c in cand.items()}
HM = {}
for a, v in HC.items():
    for b, m in v: HM[(a, b)] = m
def exmark(x, y):
    return [m for s_, o, m, t in EX[x] if o == y]
assign = {n: c[0] for n, c in H2E.items() if len(c) == 1}
amb = sorted(n for n, c in H2E.items() if len(c) > 1)
import itertools as it
def score(asg):
    sc = 0
    for hp in hpairs:
        a, b = sorted(hp)
        if a in asg and b in asg and frozenset((asg[a], asg[b])) in X and asg[a] != asg[b]:
            sc += 1
            ma = exmark(asg[a], asg[b]); mb = exmark(asg[b], asg[a])
            sc += 0.5*(HM.get((a, b)) == ma[0]) + 0.5*(HM.get((b, a)) == mb[0])
    return sc
best = None
for combo in it.product(*[H2E[n] for n in amb]):
    if len(set(combo)) < len(combo): continue
    asg = dict(assign); asg.update(zip(amb, combo)); sc = score(asg)
    if best is None or sc > best[0]: best = (sc, asg)
assign = best[1]
P('ambiguous hand lines', {n: H2E[n] for n in amb}, '-> assigned', {n: assign[n] for n in amb})
E2H = defaultdict(list)
for n, k in assign.items(): E2H[k].append(n)
common = []; missing = []
for hp in sorted(hpairs, key=lambda s: sorted(s)):
    a, b = sorted(hp)
    if a in assign and b in assign and assign[a] != assign[b] and frozenset((assign[a], assign[b])) in X: common.append(hp)
    else: missing.append(hp)
covered = {frozenset((assign[a], assign[b])) for hp in common for a, b in [sorted(hp)]}
extra = [ks for ks in X if ks not in covered]
P('hand pairs found by extractor (common):', len(common), '; only in hand file (missing):', len(missing), '; only in extractor:', len(extra))
def cls_missing(hp):
    a, b = sorted(hp)
    ua = [n for n in (a, b) if n not in assign]
    if ua: return 'hand line without ext line: ' + ','.join(ua)
    if assign[a] == assign[b]: return 'both hand lines on one ext line'
    return 'both lines matched, no ext crossing between them'
mc = defaultdict(list)
for hp in missing: mc[cls_missing(hp)].append(sorted(hp))
for k, v in mc.items(): P('  MISSING', len(v), k, v[:12])
ec = defaultdict(list)
def cls_extra(ks):
    k1, k2 = sorted(ks)
    u = [k for k in (k1, k2) if not E2H.get(k)]
    return ('ext line matches no hand line: L%s' % ','.join(map(str, u))) if u else 'both ext lines matched to hand lines, hand has no such pair'
ecd = defaultdict(list)
for ks in extra:
    ecd[cls_extra(ks)].append((tuple(sorted(ks)), X[ks][7], [E2H.get(k, ['-']) for k in sorted(ks)], '%.1f,%.1f' % X[ks][4]))
for k, v in ecd.items(): P('  EXTRA', len(v), k, v[:10])
# marks
markok = 0; markbad_l = []
for hp in common:
    a, b = sorted(hp); x, y = assign[a], assign[b]
    ma = exmark(x, y)[0]; mb = exmark(y, x)[0]
    if HM.get((a, b)) == ma and HM.get((b, a)) == mb: markok += 1
    else: markbad_l.append((a, b, HM.get((a, b)), ma, HM.get((b, a)), mb, X[frozenset((x, y))][7]))
P('mark agreement on common pairs: equal', markok, 'differ', len(markbad_l))
for m in markbad_l[:20]: P('   marks differ (hand a,b: mark b in a, ext; mark a in b, ext)', m)
# order per hand line, only common pairs, partners by hand name
ordok = 0; ordbad_l = []; short = 0
for a in sorted(HC):
    if a not in assign: continue
    hl = [b for b, _ in HC[a] if frozenset((a, b)) in set(common)]
    if len(hl) < 2: short += 1; continue
    key = [[s_ for s_, o, m, t in EX[assign[a]] if o == assign[b]][0] for b in hl]
    exo = [b for _, b in sorted(zip(key, hl))]
    if exo == hl: ordok += 1
    else:
        inv = sum(1 for i in range(len(hl)) for j in range(i+1, len(hl)) if key[i] > key[j])
        ordbad_l.append((a, len(hl), inv, hl, exo))
P('order agreement per hand line (>=2 common pairs):', ordok, 'identical', len(ordbad_l), 'differ; lines with <2 common pairs', short, '; hand lines with no ext line', sorted(n for n in HC if n not in assign))
for o in ordbad_l: P('   order differs', o[0], 'n=%d inversions=%d' % (o[1], o[2]), '\n      hand', o[3], '\n      ext ', o[4])
open(HERE+'/cross-out.txt', 'w').write('\n'.join(out)+'\n')
with open(HERE+'/ext-cross.txt', 'w') as f:
    for k in sorted(EX):
        f.write('L%d (%s): %s\n' % (k, '+'.join(E2H.get(k, [])) or '-', ', '.join('L%d%s%s' % (o, m, t if t != 'b' else '') for s_, o, m, t in EX[k])))
if os.environ.get('DBG2'):
    for n in ['D12d','D12e','D12f','D12b','D12g','D3','C2','C2b']: print(n, tab[n])
    for k in [67,68,69,72,79,95,97,98,92]:
        ch=chains[k]; print('L%d'%k, [ (p[0],W.pname(p)[:2]) for p in ch][:2], '..', ch[-1][0], len(ch), 'jumps', sum(1 for s_ in SEG[k] if s_[2]=='j'), g['lset'](ch[0]))
    print([ (tuple(sorted(ks)),h[7],'%.1f,%.1f'%h[4]) for ks,h in X.items() if h[7]=='n'])
    # node pieces on tunnel middle tiles
    mids=set()
    for k in SEG:
        for s_ in SEG[k]:
            if s_[2]=='j':
                a,b=s_[3]; (x0,y0),(x1,y1)=a[0],b[0]; dx=(x1>x0)-(x1<x0); dy=(y1>y0)-(y1<y0)
                t=(x0+dx,y0+dy)
                while t!=(x1,y1): mids.add(t); t=(t[0]+dx,t[1]+dy)
    print('node pieces on tunnel middle tiles:',[(p[0],W.pname(p)[:2],ninfo[p][:2], trunk_of(p)) for p in isnode if p[0] in mids])
if os.environ.get('DBG3'):
    for t in [(100,165),(97,185),(109,171)]:
        for k in SEG:
            for s_ in SEG[k]:
                if s_[2]=='j':
                    a,b=s_[3]; (x0,y0),(x1,y1)=a[0],b[0]
                    if (x0==x1==t[0] and min(y0,y1)<t[1]<max(y0,y1)) or (y0==y1==t[1] and min(x0,x1)<t[0]<max(x0,x1)):
                        nd=[p for p in isnode if p[0]==t][0]; tk=trunk_of(nd)[0]
                        legs=[LID.get(q) for q in (CS[nd] if ninfo[nd][0]=='split' else CP[nd])]
                        print(t,'tunnel line',k,a[0],'->',b[0],'node',W.pname(nd),ninfo[nd][:2],'trunk',tk,'legs',legs,'pairs in X: trunk',frozenset((tk,k)) in X,'legs',[frozenset((l,k)) in X for l in legs], 'trunk piece names', [W.pname(q) for q in (CP[nd] if ninfo[nd][0]=='split' else CS[nd])], 'tunnel axis', 'x' if y0==y1 else 'y')
