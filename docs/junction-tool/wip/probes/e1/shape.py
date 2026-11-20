"""E1h-1: PSG 197's shape from the save: per line its lifted headings and the piece of each crossing.
Run: python3 -I shape.py > shape-out.txt   (writes tools/junction/topologies/psg197.shape.txt)
Walks, polylines and crossing points are order.py's and marks.py's (imported by executing order.py; marks.py's point rule copied)."""
import sys, io, re, contextlib, itertools
HERE = '/home/pete/OpenTTD/docs/junction-tool/wip/probes/e1'
src = open(HERE + '/order.py').read()
g = {'__name__': 'order_mod'}
with contextlib.redirect_stdout(io.StringIO()):
    exec(compile(src, HERE + '/order.py', 'exec'), g)
G, inter, index, walks, polyline, rowk, N = g['G'], g['inter'], g['index'], g['walks'], g['polyline'], g['rowk'], g['N']
TOP = '/home/pete/OpenTTD/tools/junction/topologies/psg197.txt'
OUT = '/home/pete/OpenTTD/tools/junction/topologies/psg197.shape.txt'
tl = [l.rstrip('\n') for l in open(TOP)]
stm = []
for l in tl:
    m = re.match(r'cross (\w+): (.*)$', l)
    if m: stm.append((m.group(1), [re.sub(r'[<>]$', '', x.strip()) for x in m.group(2).split(',')]))
cross = dict(stm)
# line names of psg197.txt
names = set()
for l in tl:
    m = re.match(r'port \w \w \w: (.*)$', l)
    if m: names |= {x.split('=')[0].strip() for x in m.group(1).split(',')}
    m = re.match(r'node (.*) -> (.*)$', l)
    if m:
        names |= {x.split('=')[0].strip() for x in m.group(1).split(',')} | {x.split('=')[0].strip() for x in m.group(2).split(',')}
    m = re.match(r'node ([^>]*)$', l)
    if m: names |= {x.split('=')[0].strip() for x in m.group(1).split(',')}
# the trunk of a node is on its left (split) or right (merge); a name used as a leg of one node and a trunk of another is one line
idx = {n: (n, f, t, tlx) for (n, f, t, tlx) in index}
missing_idx = sorted(n for n in names if n not in idx)
# walks of A (E1h-1b): D12e first walk, D12f second, F1c second, D12g chord-only, others single
CH = {'D12e': 0, 'D12f': 1, 'F1c': 1}
EIGHTH = {(0, -1): 0, (1, -1): 1, (1, 0): 2, (1, 1): 3, (0, 1): 4, (-1, 1): 5, (-1, 0): 6, (-1, -1): 7}
def sgn(v): return (v > 1e-9) - (v < -1e-9)
def heading(a, b):
    dx, dy = b[0]-a[0], b[1]-a[1]
    if abs(dx) > 1e-9 and abs(dy) > 1e-9 and abs(abs(dx)-abs(dy)) > 1e-9: raise SystemExit('STOP: segment %s %s is not an eighth' % (a, b))
    return EIGHTH[(sgn(dx), sgn(dy))]
def lift(hs):
    out = []
    for h in hs:
        if not out: out.append(h); continue
        t = (h - out[-1]) % 8
        if t > 4: t -= 8
        out.append(out[-1] + t)
    return out
def tl2(s): x, y = s.split(','); return (int(x), int(y))
chosen = {}; ambiguous = []; nowalk = []; poly = {}; chord = []
for n in sorted(names, key=lambda n: rowk.get(n, 999)):
    if n not in idx: nowalk.append((n, 'not in the line index')); continue
    _, f, t, tlx = idx[n]
    w, capped = walks(f, t, tlx, n)
    if capped or not w: nowalk.append((n, 'capped' if capped else 'no walk')); continue
    ws = sorted(w)
    if len(ws) > 1: ambiguous.append((n, len(ws), CH.get(n, 'chord') ))
    if n == 'D12g':
        a, b = tl2(f), tl2(t); poly[n] = [(a[0]+.5, a[1]+.5), (b[0]+.5, b[1]+.5)]; chosen[n] = (ws[0], 0); chord.append(n); continue
    if len(ws) > 1 and n not in CH: raise SystemExit('STOP: unexpected ambiguous %s' % n)
    k = CH.get(n, 0); chosen[n] = (ws[k], k); poly[n] = polyline(ws[k])
# crossing points on the displaced polylines of A's walks (order.py / step1.py rules)
dis = {}
for n in poly:
    d = poly[n]
    dis[n] = g['displace'](d, 1e-6 * (rowk[n] + 1))
pairs = {frozenset((L, M)) for L, ms in stm for M in ms}
SPECIAL = {frozenset(('E2', 'E2b')): (82.5, 162.0), frozenset(('D12c', 'D23c')): (102.0, 178.5), frozenset(('D12f', 'D12d')): (100.5, 185.5)}
point = {}
for pr in pairs:
    A, B = sorted(pr, key=lambda n: rowk[n])
    P, Q = dis[A], dis[B]; hs = []
    for i in range(len(P)-1):
        for j in range(len(Q)-1):
            r = inter(P[i], P[i+1], Q[j], Q[j+1])
            if r:
                t, u = r; pt = (P[i][0]+t*(P[i+1][0]-P[i][0]), P[i][1]+t*(P[i+1][1]-P[i][1]))
                if not any(abs(pt[0]-x[0][0]) < 1e-4 and abs(pt[1]-x[0][1]) < 1e-4 for x in hs): hs.append((pt, i, j))
    if pr in SPECIAL:
        sp = SPECIAL[pr]; hs = [h for h in hs if abs(h[0][0]-sp[0]) < 1e-3 and abs(h[0][1]-sp[1]) < 1e-3]
    if len(hs) != 1: raise SystemExit('STOP: %s x %s: %d intersections' % (A, B, len(hs)))
    pt, i, j = hs[0]; point[pr] = (pt, {A: i, B: j})
# segments, pieces
pieces = {}; segpiece = {}; bends = []; tiles_of = {}
for n, pts in poly.items():
    sg = [(pts[i], pts[i+1], heading(pts[i], pts[i+1])) for i in range(len(pts)-1)]
    raw = []; sp = []
    for a, b, h in sg:
        if raw and raw[-1][0] == h: raw[-1][1].append((a, b))
        else: raw.append((h, [(a, b)]))
        sp.append(len(raw) - 1)
    pieces[n] = raw; segpiece[n] = sp
    lifted = lift([h for h, _ in raw])
    pieces[n] = (lifted, [s for _, s in raw])
    for k2 in range(len(lifted) - 1):
        d = lifted[k2+1] - lifted[k2]
        if abs(d) >= 2:
            p = pieces[n][1][k2][-1][1]
            bends.append((n, k2, d, '%.1f,%.1f' % p))
def pieceof(n, pr):
    pt, segs = point[pr]
    w, k = chosen[n]
    if k == 0 and n in G and G[n][0] and len(G[n][0]) - 1 == len(segpiece[n]) and False: pass
    return segpiece[n][segs[n]], None
    # second walk: find the segment by geometry
    pts = poly[n]; cands = []
    for i in range(len(pts)-1):
        a, b = pts[i], pts[i+1]; L2 = (b[0]-a[0])**2 + (b[1]-a[1])**2
        t = max(0, min(1, ((pt[0]-a[0])*(b[0]-a[0]) + (pt[1]-a[1])*(b[1]-a[1])) / L2))
        d = ((a[0]+t*(b[0]-a[0])-pt[0])**2 + (a[1]+t*(b[1]-a[1])-pt[1])**2) ** .5
        if d < 5e-4: cands.append(i)
    if not cands: raise SystemExit('STOP: crossing point %s of %s x %s lies on no segment of %s' % (pt, n, [m for m in pr if m != n][0], n))
    return segpiece[n][cands[0]], cands
stops = []
at = {}; unordered = []; vertex = []
for n in poly:
    if n not in cross: continue
    row = []
    for m in cross[n]:
        try: p, c = pieceof(n, frozenset((n, m)))
        except SystemExit as e: stops.append(str(e)); p, c = None, None
        row.append((m, p))
        pt, segs = point[frozenset((n, m))]; i = segs[n]; D = dis[n]
        for v in (i, i+1):
            if abs(D[v][0]-pt[0]) < 1e-4 and abs(D[v][1]-pt[1]) < 1e-4:
                nb = [segpiece[n][x] for x in (v-1, v) if 0 <= x < len(segpiece[n])]
                if len(set(nb)) > 1: vertex.append((n, m, 'at vertex between pieces', nb, 'used', p))
        if c and len(set(segpiece[n][i] for i in c)) > 1: vertex.append((n, m, c))
    at[n] = row
    ps = [p for _, p in row if p is not None]
    bad = [(row[i][0], row[i][1], row[i+1][0], row[i+1][1]) for i in range(len(row)-1) if row[i][1] is not None and row[i+1][1] is not None and row[i+1][1] < row[i][1]]
    if bad: unordered.append((n, bad))
# outputs
hdr = '# PSG 197 shape from the save (D389, E1h-1b): per line its lifted headings and the piece of each crossing; generated by docs/junction-tool/wip/probes/e1/shape.py.'
out = [hdr]
for n in sorted(poly, key=lambda n: rowk[n]):
    out.append('shape %s: %s' % (n, ' '.join(str(h) for h in pieces[n][0])))
    if n in at: out.append('at %s: %s' % (n, ', '.join('%s@%s' % (m, p) for m, p in at[n])))
if not stops: open(OUT, 'w').write('\n'.join(out) + '\n')
# report
print('lines of psg197.txt %d; shaped %d; cross statements %d' % (len(names), len(poly), len(stm)))
print('pieces total %d' % sum(len(v[0]) for v in pieces.values()))
print('\nPIECES (heading: tiles covered; each piece = its segments start..end points)')
def tr(p): return '(%.1f,%.1f)' % p
for n in sorted(poly, key=lambda n: rowk[n]):
    print('%s%s:' % (n, '  [walk %d]' % (chosen[n][1] + 1) if chosen[n][1] else ''))
    for i, (h, ss) in enumerate(zip(*pieces[n])):
        ts = sorted({(int(a[0]), int(a[1])) for a, b in ss} | {(int(b[0]), int(b[1])) for a, b in ss}) if False else None
        tiles = []
        for a, b in ss:
            for p in ((a[0]+b[0])/2, (a[1]+b[1])/2), a, b:
                pass
            for (x, y) in (((a[0]+b[0])/2, (a[1]+b[1])/2),):
                t = (int(x // 1), int(y // 1))
                if t not in tiles: tiles.append(t)
        print('  piece %d heading %d: %s .. %s, tiles %s' % (i, h, tr(ss[0][0]), tr(ss[-1][1]), ' '.join('%d,%d' % t for t in tiles) if len(tiles) <= 8 else '%d,%d .. %d,%d (%d tiles)' % (*tiles[0], *tiles[-1], len(tiles))))
print('\nFINDINGS')
print('Bends of 2 or more eighths: %d' % len(bends))
for n, k, d, t in bends: print('  %s: piece %d -> %d turn %+d at point %s (tile %s)' % (n, k, k+1, d, t, '%d,%d' % tuple(int(float(x)//1) for x in t.split(','))))
print('Ambiguous walks: %d' % len(ambiguous))
for n, c, k in ambiguous: print('  %s: %d walks, %s used' % (n, c, 'the chord (node tiles only)' if k == 'chord' else 'walk %d' % (k + 1)))
print('Lines with no walk: %d' % len(nowalk))
for n, w in nowalk: print('  %s: %s' % (n, w))
print('Crossing orders the shape cannot hold: %d lines' % len(unordered))
for n, b in unordered:
    print('  %s: %s' % (n, '; '.join('%s@%d then %s@%d' % x for x in b)))
print('Crossings on a vertex between two pieces: %d' % len(vertex))
for v in vertex: print('  ', v)
print('Stops: %d' % len(stops))
for s in stops: print('  ', s)
print('crossings emitted %d' % sum(len(v) for v in at.values()))
