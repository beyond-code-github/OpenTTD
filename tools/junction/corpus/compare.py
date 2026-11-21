#!/usr/bin/env python3
"""Golden check: python3 -I compare.py psg197 <dir>.  Re-runs the extraction in memory (about 2 s), checks that <dir>'s text
files equal a fresh write, and compares with tools/junction/topologies/psg197*.txt and psg197-decisions.md, mapping the
hand file's names to the extractor's lines by tiles (the probes' rules: x1c lines and nodes, x5 crossings, x6 structure and
shape).  Prints the probes' numbers and every difference class with examples."""
import os, re, sys, tempfile, filecmp, itertools
from collections import defaultdict
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import extract

ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
TOPO = os.path.join(ROOT, 'tools/junction/topologies/')
DEC = os.path.join(ROOT, 'docs/junction-tool/wip/topologies/psg197-decisions.md')
# the hand file's lane numbering (left to right across travel) and movement letters, from its header
INL = {'N': {1: 'N91', 2: 'N90'}, 'E': {1: 'E174', 2: 'E173', 3: 'E169'}, 'S': {1: 'S81', 2: 'S82', 3: 'S83'}}
LET = {'A': ('N', 'S'), 'B': ('N', 'E'), 'C': ('E', 'N'), 'D': ('E', 'S'), 'E': ('S', 'N'), 'F': ('S', 'E')}
out = []
def P(*a):
    s = ' '.join(str(x) for x in a); out.append(s); print(s)
def tile(s): x, y = s.split(','); return (int(x), int(y))
near = lambda a, b: max(abs(a[0] - b[0]), abs(a[1] - b[1])) <= 1


def main():
    hubid = sys.argv[1]; d = sys.argv[2]
    if hubid != 'psg197': raise SystemExit('only psg197 has a golden file')
    Rr = extract.run(hubid)
    # files in <dir> equal a fresh write
    tmp = tempfile.mkdtemp(); extract.write_all(Rr, tmp)
    diff = [f for f in sorted(os.listdir(tmp)) if not os.path.exists(os.path.join(d, f)) or not filecmp.cmp(os.path.join(tmp, f), os.path.join(d, f), shallow=False)]
    P('files in %s equal a fresh extraction: %s' % (d, 'yes' if not diff else 'NO, differ: %s' % diff))
    N, G, Ln, X5, S6, Sh, T = Rr.N, Rr.G, Rr.Ln, Rr.X5, Rr.S6, Rr.Sh, Rr.T
    lines, RS, bounds, LID, isnode, ninfo, CS, CP = Ln.lines, Ln.RS, Ln.bounds, Ln.LID, Ln.isnode, Ln.ninfo, G.CS, G.CP
    lane_in = {x['label']: x['pc'] for x in Rr.lanes if x['dir'] == 'in'}; lane_out = {x['label']: x['pc'] for x in Rr.lanes if x['dir'] == 'out'}
    outport = {x['side']: x['port'] for x in Rr.lanes if x['dir'] == 'out'}
    def rpair(n):
        s, t = LET[n[0]]; return (INL[s][int(n[1])], outport[t])
    def hn_route_name(pair):                                   # extractor route key -> hand route name
        for l, (s, t) in LET.items():
            for dno, lab in INL[s].items():
                if pair[0] == lab and pair[1] == outport[t]: return l + str(dno)
    lset = lambda k: frozenset(hn_route_name(x) for x in RS[lines[k][0]])
    # ---------------- hand lines (x1c)
    txt = open(TOPO + 'psg197.txt').read().splitlines()
    tab = {}
    for l in open(DEC).read().split('## Line index')[1].splitlines():
        m = re.match(r'\| (\S+) \| (\S+) \| (.+?) \| (.+?) \| (.*?) \|', l)
        if m and m.group(1) != 'line' and '---' not in m.group(1): tab[m.group(1)] = (set(m.group(2).split('+')), m.group(3), m.group(4))
    def ptile(s, o):
        m = re.match(r'port (\w+)', s)
        if m: return (lane_out if o else lane_in)[m.group(1)][0]
        return tile(s)
    cand = {}
    for n, (rt, fr, to) in tab.items():
        ft, tt = ptile(fr, 0), ptile(to, 1); pairs = {rpair(r) for r in rt}
        cand[n] = [k for k in range(len(lines)) if pairs == set(RS[lines[k][0]]) and any(near(ft, t) for t in bounds[k][0]) and any(near(tt, t) for t in bounds[k][1])]
    hl_unique = [n for n, c in cand.items() if len(c) == 1]
    e2h = defaultdict(list)
    for n, c in cand.items():
        for k in c: e2h[k].append(n)
    one = [n for n in hl_unique if len(e2h[cand[n][0]]) == 1]
    none_ = [n for n, c in cand.items() if not c]; multi = [n for n, c in cand.items() if len(c) > 1]
    nm = lambda k: 'L%d' % (k + 1)
    # ---------------- hand nodes (x1c)
    hn = []
    def lsets(n): return tab[n][0] if n in tab else ({'A2'} if n == 'A2m' else set())
    for l in txt:
        if not l.startswith('node '): continue
        lhs, rhs = l[5:].split(' -> ')
        ins = [x.strip().split('=')[0] for x in lhs.split(',')]; legs = [x.strip().split('=')[0] for x in rhs.split(',')]
        def st(x): return frozenset(x.split('=')[1].split('+')) if '=' in x else frozenset(lsets(x))
        ins_s = [st(x.strip()) for x in lhs.split(',')]; legs_s = [st(x.strip()) for x in rhs.split(',')]
        kd = 'split' if len(ins) == 1 and len(legs) > 1 else 'merge' if len(legs) == 1 and len(ins) > 1 else 'other'
        t = None
        if kd == 'split': t = ptile(tab[legs[0]][1], 0) if legs[0] in tab else None
        elif kd == 'merge': t = ptile(tab[ins[0]][2], 1) if ins[0] in tab else ptile(tab[legs[0]][1], 0)
        if t is None:
            for x in legs:
                if x in tab: t = ptile(tab[x][1], 0); break
        if t is None:
            for x in ins:
                if x in tab: t = ptile(tab[x][2], 1); break
        fl = ('fray' if len(set(legs_s)) > 1 else 'lane') if kd == 'split' else ('join' if len(set(ins_s)) > 1 else 'lane')
        hn.append((l, kd, fl, t, len(ins), len(legs)))
    used = set(); matched = []; unh = []
    for l, kd, fl, t, a, b in hn:
        if t is None: unh.append((l, kd, fl, t)); continue
        c = [p for p in isnode if near(t, p[0]) and ninfo[p][0] == kd]
        cf = [p for p in c if ninfo[p][1] == fl]
        if cf: matched.append((l, [Ln_pl(N, p) for p in cf], 'kind+fl')); used |= set(cf)
        elif c: matched.append((l, [Ln_pl(N, p) for p in c], 'kind only %s vs %s' % (ninfo[c[0]][1], fl))); used |= set(c)
        else: unh.append((l, kd, fl, t))
    mk_ = [m for m in matched if m[2] == 'kind+fl']
    ext_un = [p for p in isnode if p not in used]
    # ---------------- crossings (x5)
    X, EX = X5.X, X5.EX
    HC = {}
    for l in open(TOPO + 'psg197.txt'):
        m = re.match(r'cross (\w+): (.*)$', l.strip())
        if m: HC[m.group(1)] = [(x.strip().rstrip('<>'), x.strip()[-1]) for x in m.group(2).split(',')]
    hpairs = {frozenset((a, b)) for a, v in HC.items() for b, _ in v}
    HM = {(a, b): m for a, v in HC.items() for b, m in v}
    def exmark(x, y): return [m for s_, o, m, t in EX[x] if o == y]
    assign = {n: c[0] for n, c in cand.items() if len(c) == 1}
    amb = sorted(n for n, c in cand.items() if len(c) > 1)
    def score(asg):
        sc = 0
        for hp in hpairs:
            a, b = sorted(hp)
            if a in asg and b in asg and frozenset((asg[a], asg[b])) in X and asg[a] != asg[b]:
                sc += 1
                ma = exmark(asg[a], asg[b]); mb = exmark(asg[b], asg[a])
                sc += 0.5 * (HM.get((a, b)) == ma[0]) + 0.5 * (HM.get((b, a)) == mb[0])
        return sc
    best = None
    for combo in itertools.product(*[cand[n] for n in amb]):
        if len(set(combo)) < len(combo): continue
        asg = dict(assign); asg.update(zip(amb, combo)); sc = score(asg)
        if best is None or sc > best[0]: best = (sc, asg)
    assign = best[1]
    E2H = defaultdict(list)
    for n, k in assign.items(): E2H[k].append(n)
    hname = lambda k: '+'.join(E2H.get(k, [])) or nm(k)
    common = []; missing = []
    for hp in sorted(hpairs, key=lambda s: sorted(s)):
        a, b = sorted(hp)
        if a in assign and b in assign and assign[a] != assign[b] and frozenset((assign[a], assign[b])) in X: common.append(hp)
        else: missing.append(hp)
    covered = {frozenset((assign[a], assign[b])) for hp in common for a, b in [sorted(hp)]}
    extra = [ks for ks in X if ks not in covered]
    markok = 0; markbad = []
    for hp in common:
        a, b = sorted(hp); x, y = assign[a], assign[b]
        ma = exmark(x, y)[0]; mb = exmark(y, x)[0]
        if HM.get((a, b)) == ma and HM.get((b, a)) == mb: markok += 1
        else: markbad.append((a, b))
    ordok = 0; ordbad = []; short = 0; cs = set(common)
    for a in sorted(HC):
        if a not in assign: continue
        hl = [b for b, _ in HC[a] if frozenset((a, b)) in cs]
        if len(hl) < 2: short += 1; continue
        key = [[s_ for s_, o, m, t in EX[assign[a]] if o == assign[b]][0] for b in hl]
        exo = [b for _, b in sorted(zip(key, hl))]
        if exo == hl: ordok += 1
        else: ordbad.append(a)
    # ---------------- structure (x6)
    def hand_t():
        ttab = {}
        for l in open(DEC).read().splitlines():
            m = re.match(r'\| (\d+) \| (\d+,\d+) \| (\d+,\d+) \| (\d+) \| (\w+) \| (G\d+) ', l)
            if m: ttab[int(m.group(1))] = (tile(m.group(2)), tile(m.group(3)), int(m.group(4)), m.group(5))
        H_T, H_G, H_S = {}, [], []
        for l in open(TOPO + 'psg197.structure.txt').read().splitlines():
            m = re.match(r'tunnel T(\d+) (\w+): (.*)$', l)
            if m: H_T[int(m.group(1))] = (m.group(2), [x.strip() for x in m.group(3).split(',')])
            m = re.match(r'group (\w+): (.*)$', l)
            if m: H_G.append([int(x.strip()[1:]) for x in m.group(2).split(',')])
            m = re.match(r'straight (\w+): (\w+)$', l)
            if m: H_S.append((m.group(1), None if m.group(2) == 'none' else m.group(2)))
        return ttab, H_T, H_G, H_S
    ttab, H_T, H_G, H_S = hand_t()
    ET = S6.T          # entrance -> (sep line, crossers, exit, is_bridge)
    cls = defaultdict(list); tmatch = sepok = crok = 0
    for n, (sep, cr) in sorted(H_T.items()):
        ent = ttab[n][0]
        if ent not in ET: cls['tunnels: no ext tunnel at head'].append(n); continue
        k, ec, ex_, br = ET[ent]
        if assign.get(sep) != k: cls['tunnels: separator differs'].append('T%d hand %s ext %s' % (n, sep, hname(k))); continue
        sepok += 1
        hl = [assign.get(c) for c in cr]
        if None in hl or sorted(hl) != sorted(ec):
            miss = [c for c, a in zip(cr, hl) if a not in ec]; extra_ = [hname(e) for e in ec if e not in hl]
            cls['tunnels: crosser set differs'].append('T%d %s: hand-only %s, ext-only %s' % (n, sep, miss, extra_)); continue
        crok += 1
        if hl != ec: cls['tunnels: crosser order differs'].append('T%d %s' % (n, sep)); continue
        tmatch += 1
    eg = set(S6.groups); hg = {frozenset(ttab[n][0] for n in g_) for g_ in H_G}
    # straights
    topo = txt; hnodes = []
    for l in topo:
        if not l.startswith('node '): continue
        lhs, rhs = l[5:].split(' -> ')
        Lh = [x.split('=')[0].strip() for x in lhs.split(',')]; Rh = [x.split('=')[0].strip() for x in rhs.split(',')]
        if len(Lh) == 1 and len(Rh) == 2: hnodes.append(('split', Lh[0], Rh))
        elif len(Lh) == 2 and len(Rh) == 1: hnodes.append(('merge', Rh[0], Lh))
    base = [n for n in hnodes if n[1] not in ('A2', 'A2m')]
    assert len(H_S) == len(base) + 2, (len(H_S), len(base))
    hand = []
    for n, (tn, st_) in zip(base, H_S[:len(base)]):
        assert tn == n[1]; hand.append((n[0], n[1], n[2], st_))
    hand.append(('split', 'A2', ['A2b', 'A2c', 'A2d'], 'A2b'))
    ext_nodes = S6.nodes; used_n = set(); res = defaultdict(list); nmatch = sm = 0
    for kd, tn, legs, st_ in hand:
        tk = assign.get(tn); lk = {assign.get(l) for l in legs}
        cands = [e for e in ext_nodes if e[1] == kd and set(e[3]) == lk and None not in lk and (tk is None and e[2] is None or e[2] == tk)]
        if not cands and tk is None and None not in lk: cands = [e for e in ext_nodes if e[1] == kd and set(e[3]) == lk]
        if not cands: res['straights: hand node with no ext node'].append('%s %s -> %s' % (kd, tn, legs)); continue
        nmatch += 1; e = cands[0]; used_n.add(e[0]); r = e[4]; hst = assign.get(st_) if st_ else None
        if r[0] == 'straight' and st_ and r[1] == hst: sm += 1
        elif r[0] == 'none' and st_ is None: sm += 1
        else: res['straights: straight leg differs (%s)' % r[0]].append('%s %s: hand %s ext %s' % (tn, legs, st_, r))
    # ---------------- shape (x7)
    H_shape, H_at, H_steps = {}, {}, {}
    for l in open(TOPO + 'psg197.shape.txt'):
        m = re.match(r'shape (\w+): (.*)$', l.strip())
        if m: H_shape[m.group(1)] = [int(x) for x in m.group(2).split()]
        m = re.match(r'at (\w+): (.*)$', l.strip())
        if m: H_at[m.group(1)] = [(a.strip().split('@')[0], int(a.strip().split('@')[1])) for a in m.group(2).split(',')]
    for l in open(TOPO + 'psg197.steps.txt'):
        m = re.match(r'step (\w+) (\d+): ([\d ]+)', l)
        if m: H_steps[(m.group(1), int(m.group(2)))] = [int(x) for x in m.group(3).split()]
    pj_ok = st_ok = both = pc_ok = pc_n = 0; mcls = defaultdict(list); sh_ext = defaultdict(list)
    for n in H_shape:
        k = assign.get(n)
        if k is None or k not in Sh.ES: mcls['shape: hand line with no ext line'].append(n); continue
        sh_ext[k].append(n)
        raw, at, proj, newp, steps = Sh.ES[k]
        ok = proj == H_shape[n]; hst = {kk: v for (nn, kk), v in H_steps.items() if nn == n}; sok = steps == hst
        pj_ok += ok; st_ok += sok; both += ok and sok
        if not ok: mcls['shape: projection differs'].append('%s (%s) hand %s ext %s' % (n, nm(k), H_shape[n], proj))
        if not sok: mcls['shape: steps differ'].append('%s (%s) hand %s ext %s' % (n, nm(k), hst, steps))
        pm = {o: newp[i] for i, (o, p, m) in enumerate(at)}
        for M, p in H_at.get(n, []):
            km = assign.get(M)
            if km is None or km not in pm: continue
            pc_n += 1
            if pm[km] == p: pc_ok += 1
            else: mcls['shape: crossing piece differs'].append('%s x %s hand @%d ext @%d' % (n, M, p, pm[km]))
    # ---------------- print
    P('== GOLDEN (probe numbers in brackets)')
    P('nodes: %d of %d hand nodes matched by tile+kind+fray/lane [62 of 62]; ext node pieces matching no hand node %d [7 + 1]' % (len(mk_), len(hn), len(ext_un)))
    P('lines: %d of %d hand lines one-to-one [89 of 100]; one candidate shared %d [2]; several candidates %d [4]; none %d [5]; ext lines matching no hand line %d [15]' % (len(one), len(tab), len(hl_unique) - len(one), len(multi), len(none_), len([k for k in range(len(lines)) if k not in e2h])))
    P('crossings: common %d [117], only in hand %d [5], only in extractor %d [6]; ext pairs %d [123], hand pairs %d [122]' % (len(common), len(missing), len(extra), len(X), len(hpairs)))
    P('crossing order: %d lines identical, %d differ [37 identical, 0 differ]; marks equal %d of %d [117 of 117]' % (ordok, len(ordbad), markok, len(common)))
    P('tunnels: %d of %d identical [42 of 50]; separator equal %d [49]; crosser set equal %d [42]' % (tmatch, len(H_T), sepok, crok))
    P('groups: ext %d hand %d identical %d [18 18 18]' % (len(eg), len(hg), len(eg & hg)))
    P('straights: hand nodes matched to an ext node %d of %d; straight leg equal %d [55 of 55]' % (nmatch, len(hand), sm))
    P('shape: lines with a match %d of %d; identical projection %d [94 of 95]; steps %d [90]; both %d [89]; crossing pieces equal %d of %d [234 of 234]' % (len(H_shape) - len(mcls['shape: hand line with no ext line']), len(H_shape), pj_ok, st_ok, both, pc_ok, pc_n))
    P('== DIFFERENCE CLASSES')
    for l, ps, k in matched:
        if k != 'kind+fl': P('  nodes: fray/lane differs', l[:70], ps, k)
    for m in unh: P('  nodes: hand node with no ext node', m)
    for p in sorted(ext_un, key=lambda p: p[0]): P('  nodes: ext node piece with no hand node', Ln_pl(N, p), ninfo[p][:2], 'in', [Ln.rset(a) for a in CP[p]], 'out', [Ln.rset(b) for b in CS[p]])
    P('  lines: hand lines with no ext line', none_); P('  lines: several candidates', {n: [nm(k) for k in cand[n]] for n in multi})
    P('  lines: ext lines matched by no hand line', [nm(k) for k in range(len(lines)) if k not in e2h])
    def cm(hp):
        a, b = sorted(hp); ua = [n for n in (a, b) if n not in assign]
        if ua: return 'hand line without ext line: ' + ','.join(ua)
        if assign[a] == assign[b]: return 'both hand lines on one ext line'
        return 'both lines matched, no ext crossing between them'
    mc = defaultdict(list)
    for hp in missing: mc[cm(hp)].append(' x '.join(sorted(hp)))
    for k, v in mc.items(): P('  crossings: only in hand (%d) %s: %s' % (len(v), k, v[:10]))
    ec = defaultdict(list)
    for ks in extra:
        k1, k2 = sorted(ks); u = [k for k in (k1, k2) if not E2H.get(k)]
        ec[('ext line matches no hand line' if u else 'both ext lines matched, hand has no such pair')].append('%s x %s (%s) at %.1f,%.1f' % (nm(k1), nm(k2), X[ks][7], X[ks][4][0], X[ks][4][1]))
    for k, v in ec.items(): P('  crossings: only in extractor (%d) %s: %s' % (len(v), k, v[:10]))
    for hp in markbad: P('  crossings: marks differ', hp)
    for a in ordbad: P('  crossings: order differs', a)
    for c, v in list(cls.items()) + list(res.items()) + list(mcls.items()): P('  %s (%d): %s' % (c, len(v), v[:8]))
    for g_ in sorted(eg - hg, key=sorted): P('  groups: ext-only', sorted(g_))
    for g_ in sorted(hg - eg, key=sorted): P('  groups: hand-only', sorted(g_))
    hand_heads = {ttab[n][0] for n in H_T}
    P('  tunnels: ext tunnels with crossers but no hand statement:', [(hd, hname(v[0]), [hname(c) for c in v[1]]) for hd, v in ET.items() if v[1] and hd not in hand_heads])
    P('  straights: ext nodes unused:', [(e[0][0], e[1], e[4]) for e in ext_nodes if e[0] not in used_n])
    P('  hand lines sharing one ext line:', {nm(k): v for k, v in sh_ext.items() if len(v) > 1})
    open(os.path.join(d, 'compare.txt'), 'w').write('\n'.join(out) + '\n')


def Ln_pl(N, p): return '%d,%d%s' % (p[0][0], p[0][1], N.W.pname(p)[:2])


if __name__ == '__main__':
    main()
