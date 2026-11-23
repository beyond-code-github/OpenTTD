"""domain-39: triage of V's PSG 197 domain breaks (artefact or domain gap).
python3 -I triage197.py <dump.json> [corpus-root]   (dump: PLANAR_DOMAIN_DUMP_197=<path> build/openttd_test '[planardomain197]')
Compares our rasterised pieces with V's real tiles on each break's tile and its 8 neighbours."""
import json, sys, os, collections
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..', '..', 'tools', 'junction', 'corpus'))
import load
NAME = {'X': 0, 'Y': 1, 'Upper': 2, 'Lower': 3, 'Left': 4, 'Right': 5}
PN = 'X Y Upper Lower Left Right'.split()
dump = json.load(open(sys.argv[1]))
root = sys.argv[2] if len(sys.argv) > 2 else None
T = load.load_tiles(197, root)
lowx, lowy = dump['low'][0] // 2, dump['low'][1] // 2
def axis(d): return 'X' if d in ('NE', 'SW') else 'Y'
V = collections.defaultdict(set); heads = {}
for (x, y), t in T.items():
    tt = t['tile_type']
    if tt == 'rail' and t.get('kind') in ('track', 'signals'): V[(x, y)] |= {NAME[n] for n in t['tracks']}
    elif tt == 'road' and t.get('kind') == 'level_crossing': V[(x, y)].add(NAME[t['rail_axis']])
    elif tt == 'station' and t.get('axis') in ('X', 'Y'): V[(x, y)].add(NAME[t['axis']])
    elif tt == 'tunnelbridge' and t.get('transport') == 'rail':
        V[(x, y)].add(NAME[axis(t['direction'])]); heads[(x, y)] = t
# tiles strictly between two heads (tunnel / bridge middles): our pieces there are in the structure
mid = {}   # tile -> axis pieces of the tunnels/bridges passing through it
for (x, y), t in heads.items():
    fe = t['far_end']
    if not fe: continue
    fx, fy = fe['x'], fe['y']
    for k in range(1, max(abs(fx - x), abs(fy - y))):
        mid.setdefault((x + (fx > x) * k - (fx < x) * k, y + (fy > y) * k - (fy < y) * k), set()).add(NAME[axis(t['direction'])])
O = collections.defaultdict(set); OT = collections.defaultdict(lambda: collections.defaultdict(set))
for p in dump['pieces']:
    g = (p['x'] + lowx, p['y'] + lowy)
    O[g].add(p['piece']); OT[g][p['piece']].add(p['track'] + '#%d' % p['k'])
def vs(g): return V.get(g, set())
def os_(g):  # on a tunnel/bridge middle our structure piece is not rail track: dropped unless V has it too
    return O.get(g, set()) - (mid.get(g, set()) - V.get(g, set()))
def fmt(s): return '{' + ','.join(PN[i] for i in sorted(s)) + '}' if s else '{}'
box = (76, 137, 128, 198)
# global match
inbox = [g for g in set(O) | set(V) if box[0] <= g[0] <= box[2] and box[1] <= g[1] <= box[3]]
same = sum(1 for g in inbox if os_(g) == vs(g))
print('tiles compared (box, tunnel middles adjusted): %d, identical %d, differing %d' % (len(inbox), same, len(inbox) - same))
diff = sorted(g for g in inbox if os_(g) != vs(g))
rows = []
cls = collections.Counter(); rule_cls = collections.Counter()
for b in dump['breaks']:
    g = (b['x'] + lowx, b['y'] + lowy)
    nine = [(g[0] + dx, g[1] + dy) for dy in (-1, 0, 1) for dx in (-1, 0, 1)]
    d = [(h, os_(h), vs(h)) for h in nine if os_(h) != vs(h)]
    c = 'ARTEFACT' if d else 'DOMAIN GAP'
    cls[c] += 1; rule_cls[(b['rule'], c)] += 1
    rows.append((b, g, c, d))
for b, g, c, d in rows:
    print('%s %s shifted(%d,%d) game(%d,%d) %s: %s' % (c, b['rule'], b['x'], b['y'], g[0], g[1], ','.join(b['tracks']),
          '; '.join('(%d,%d) ours %s V %s' % (h[0], h[1], fmt(o), fmt(v)) for h, o, v in d) or 'all nine tiles identical'))
print(dict(cls)); print({'%s/%s' % k: v for k, v in sorted(rule_cls.items())})
if os.environ.get('TRIAGE_TILES'):
    for g in diff: print('tile (%d,%d) ours %s V %s ours-tracks %s' % (g[0], g[1], fmt(os_(g)), fmt(vs(g)), sorted(set(t for p in OT[g].values() for t in p))[:6]))
if os.environ.get('TRIAGE_AT'):
    gx, gy = map(int, os.environ['TRIAGE_AT'].split(','))
    for dy in range(-2, 3):
        for dx in range(-2, 3):
            g = (gx + dx, gy + dy)
            print('(%d,%d) V %s ours %s' % (g[0], g[1], fmt(vs(g)), {PN[k]: sorted(v) for k, v in OT[g].items()}), '[mid %s]' % fmt(mid[g]) if g in mid else '', '[head]' if g in heads else '')
if os.environ.get('TRIAGE_SPAN'):
    import re
    byk = collections.defaultdict(dict)
    for p in dump['pieces']: byk[p['track']][p['k']] = p
    for b in dump['breaks']:
        if b['rule'] != 'span': continue
        a, z = map(int, re.search(r'pieces (\d+)\.\.(\d+)', b['why']).groups()); tr = b['tracks'][0]
        row = []
        for k in range(a, z + 1):
            p = byk[tr][k]; g = (p['x'] + lowx, p['y'] + lowy)
            row.append('%d:%s%s%s' % (k, PN[p['piece']][0] + (PN[p['piece']][1] if p['piece'] > 1 else ''), 'H' if g in heads else '', '' if (g in mid) or g in heads else '.'))
        print(tr, a, z, ' '.join(row))
if os.environ.get('TRIAGE_SPAN'):
    print('heights (head h, tile outside h, tile inside h)')
    for b in dump['breaks']:
        if b['rule'] != 'span': continue
        a, z = map(int, re.search(r'pieces (\d+)\.\.(\d+)', b['why']).groups()); tr = b['tracks'][0]
        hs = [(k, (byk[tr][k]['x'] + lowx, byk[tr][k]['y'] + lowy)) for k in range(a, z + 1) if (byk[tr][k]['x'] + lowx, byk[tr][k]['y'] + lowy) in heads]
        out = []
        for k, g in hs:
            pk = byk[tr].get(k - 1) if k == hs[0][0] else byk[tr].get(k + 1)
            nk = byk[tr].get(k + 1) if k == hs[0][0] else byk[tr].get(k - 1)
            f = lambda p: T.get((p['x'] + lowx, p['y'] + lowy), {}).get('height', '-') if p else '-'
            out.append('head %s h%s outside h%s inside h%s' % (g, T[g]['height'], f(pk), f(nk)))
        print(tr, a, z, ' | '.join(out))
if os.environ.get('TRIAGE_LINES'):
    byk2 = collections.defaultdict(dict)
    for p in dump['pieces']: byk2[p['track']][p['k']] = p
    for tr in os.environ['TRIAGE_LINES'].split(','):
        print(tr, ' '.join('%d:%s(%d,%d)%s%s' % (k, PN[p['piece']], p['x'] + lowx, p['y'] + lowy, 'H' if (p['x'] + lowx, p['y'] + lowy) in heads else '', 'm' if (p['x'] + lowx, p['y'] + lowy) in mid else '') for k, p in sorted(byk2[tr].items())))
# cascade check: a diamond / connection / node break that involves a separator whose structure the domain dropped (span break or "no room" note)
DROPPED = {b['tracks'][0] for b in dump['breaks'] if b['rule'] == 'span'} | set(os.environ.get('TRIAGE_NOROOM', 'L5,L61,L70').split(','))
for rule in ('diamond', 'connection', 'node'):
    for b, g, c, d in rows:
        if b['rule'] == rule:
            hit = DROPPED & set(b['tracks'])
            print('cascade-check %s %s %s %s' % (rule, c, ','.join(b['tracks']), 'dropped-structure separator: ' + ','.join(sorted(hit)) if hit else 'NO dropped separator'))
