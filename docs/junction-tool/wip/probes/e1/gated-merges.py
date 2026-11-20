"""E1j: PSG 197's six gated merges, the pre-signal blocks traced. Data only.
Run: python3 -I gated-merges.py > gated-merges-out.txt
Block exploration = OpenTTD ExploreSegment (as in tools/sigsim.py) ported onto the trace's piece graph:
from a signal's exit edge, flood over all connected pieces (both directions, one-way ignored), stop at any piece with a signal
(a signal piece bounds the block; it belongs to both blocks). A bounding signal faces INTO the block if its exit edge is the edge
the flood entered by, OUT of the block if its exit edge is the piece's other edge."""
import importlib.util as u, re
OD = '/mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad/psg/anatomy/psg197/trace'
s = u.spec_from_file_location('core', OD + '/core.py'); core = u.module_from_spec(s); s.loader.exec_module(core)
N = core.build(); W = N.W
PL = lambda pc: core.pl(N, pc)
MERGES = ['86,151r', '98,171l', '107,173x', '100,174d', '110,176x', '94,195u']
PRE = ('entry', 'exit', 'combo')
# tunnel table: entrance tile -> number
TUN = {}
for ln in open('/home/pete/OpenTTD/docs/junction-tool/wip/topologies/psg197-decisions.md'):
    m = re.match(r'\| (\d+) \| (\d+),(\d+) \| \d+,\d+ \| \d+ \|', ln)
    if m: TUN[(int(m.group(2)), int(m.group(3)))] = int(m.group(1))
byname = {PL(pc): pc for pc in N.pieces}
def sigs(pc): return [(sg['type'], sg['exit_edge']) for sg in N.sig.get(pc, [])]
def sgtxt(pc): return ' '.join('%s>%s' % (t[:3], e) for t, e in sigs(pc))
def other(pc, e):
    a, b = W.pedges(pc); return b if e == a else a
def nbrs(pc, e):  # pieces across edge e, in box, as (piece, entered edge)
    return [(o, e2) for o, e2 in W.across(pc, e) if N.inb(o[0])]
def explore(p, e):
    """block ahead of signal piece p exiting at edge e -> (block pieces, boundary {piece: [enter edges]}, escaped count)"""
    block, bound, esc = set(), {}, 0
    todo = list(nbrs(p, e)); seen = set()
    if len(todo) < len(W.across(p, e)): esc += 1
    while todo:
        pc, ein = todo.pop()
        if (pc, ein) in seen: continue
        seen.add((pc, ein))
        if pc in N.sig:
            bound.setdefault(pc, []).append(ein); continue
        block.add(pc)
        eo = other(pc, ein)
        allx = W.across(pc, eo); nb = nbrs(pc, eo)
        if len(nb) < len(allx): esc += 1
        todo += nb
    return block, bound, esc
def back_leg(pc0, depth=8):
    """pieces upstream of pred pc0 (inclusive) by N.P, BFS to depth (pc0 = distance 1)"""
    dist = {pc0: 1}; fr = [pc0]
    for d in range(2, depth + 1):
        nf = []
        for a in fr:
            for b, e in N.P[a]:
                if b not in dist: dist[b] = d; nf.append(b)
        fr = nf
    return dist
def reach(starts, edges, allowed):
    seen = set(starts); fr = list(starts)
    while fr:
        a = fr.pop()
        for b, e in edges[a]:
            if b in allowed and b not in seen: seen.add(b); fr.append(b)
    return seen
def tunnel_ahead(pc, e):
    """tunnel head within 0..3 tiles ahead of signal piece pc travelling out of edge e -> (tunnel#, tiles) or None"""
    cur, ce = pc, e
    for d in range(0, 4):
        h = W.heads.get(cur[0])
        if h and h[0] == ce and h[1] is not None:
            return TUN.get(cur[0]), d
        nx = [b for b, ee in N.S[cur] if ee == ce]
        if len(nx) != 1: 
            if not nx: return None
        cur = nx[0]; ce = other(cur, [x for x in W.pedges(cur) if any(o == (cur, 0) or True for o in [0])][0]) if False else None
        # next exit edge: the edge of cur not shared with the one we came across
        ce = next(iter(N.exits[cur])) if len(N.exits[cur]) == 1 else None
        if ce is None: return None
    return None
out = []; table = []
for mn in MERGES:
    m = byname[mn]
    out.append('=' * 100); out.append('MERGE %s  piece %s signals [%s]' % (mn, m, sgtxt(m)))
    preds = [(a, e) for a, e in N.P[m]]
    legs = {a: back_leg(a) for a, e in preds}
    # 1. entry signal on a leg's last 8 pieces, facing along the leg
    cand = []
    for a, e in preds:
        for pc, d in sorted(legs[a].items(), key=lambda x: x[1]):
            for t, ex in sigs(pc):
                if t == 'entry' and any(b in legs[a] or b == m for b, e2 in N.S[pc] if e2 == ex):
                    cand.append((a, pc, ex, d))
    out.append('legs (preds): ' + ', '.join('%s' % PL(a) for a, e in preds))
    out.append('entry signals in the last 8 pieces of a leg: ' + '; '.join('leg %s: %s dist %d facing %s' % (PL(a), PL(pc), d, ex) for a, pc, ex, d in cand))
    allpre = []
    gated = None
    for a, pc, ex, d in cand:
        gated = (a, pc, ex, d)
        out.append('-- ENTRY SIGNAL on leg %s: tile %d,%d piece %s (%s), signals [%s]' % (PL(a), pc[0][0], pc[0][1], core.tname(N, pc), PL(pc), sgtxt(pc)))
        out.append('   1. type entry, faces exit edge %s (travelling toward merge %s, %d pieces before it)' % (ex, mn, d))
        block, bound, esc = explore(pc, ex)
        bl = sorted(block)
        out.append('   3. bound %s tile %d,%d type entry exit edge %s faces INTO block (the entry signal itself)' % (PL(pc), pc[0][0], pc[0][1], ex))
        out.append('   2. block: %d pieces (+ %d bounding signal pieces%s): %s' % (len(block), len(bound), ', %d edges leave the box' % esc if esc else '', ' '.join(PL(b) for b in bl)))
        # relations to merge
        allowed = block | set(bound)
        up = {a2: reach([a2], N.P, allowed | {m}) for a2, e2 in preds}
        past = reach([m], N.S, allowed)
        outs = []
        for b, eins in sorted(bound.items()):
            for sg in N.sig[b]:
                typ, ex2 = sg['type'], sg['exit_edge']
                face = 'INTO' if ex2 in eins else 'OUT of'
                if face == 'OUT of':
                    rel = None
                    if b in past or b == m: rel = 'past merge'
                    for a2, e2 in preds:
                        if b in up[a2] and b != m: rel = 'upstream on leg %s%s' % (PL(a2), ' (the gated leg)' if a2 == a else ' (other leg)')
                    tun = tunnel_ahead(b, ex2)
                    where = rel or 'elsewhere in block'
                    if tun: where += ', %d tile(s) before tunnel T%s entrance' % (tun[1], tun[0])
                    if typ in ('exit', 'combo'): outs.append((PL(b), typ, where, tun, rel))
                else: where = ''
                out.append('   3. bound %s tile %d,%d type %s exit edge %s faces %s block%s%s' % (PL(b), b[0][0], b[0][1], typ, ex2, face, '; ' + where if where else '', '  [decides the aspect]' if face == 'OUT of' and typ in ('exit', 'combo') else ''))
        out.append('   4. exit/combo facing out: ' + ('; '.join('%s %s: %s' % (x[0], x[1], x[2]) for x in outs) or 'none'))
        allpre.append((pc, outs))
    # 5. holding legs
    hold = []
    for a, e in preds:
        if gated and a == gated[0]: continue
        found = [(d, pc, '%s>%s%s' % (t[:3], ex, ', two-way' if len(sigs(pc)) > 1 else '')) for pc, d in sorted(legs[a].items(), key=lambda x: x[1]) for t, ex in sigs(pc) if t in PRE and any((b in legs[a] and legs[a][b] < d) or b == m for b, e2 in N.S[pc] if e2 == ex)]
        out.append('   5. holding leg %s: pre-signals (entry/exit/combo) in last 8 pieces: %s' % (PL(a), '; '.join('%s dist %d [%s]' % (PL(pc), d, t) for d, pc, t in found) or 'none'))
        hold.append('%s: %s' % (PL(a), '; '.join('%s [%s]' % (PL(pc), t) for d, pc, t in found) or 'none'))
    if gated is None:
        out.append('   NO entry signal found on a leg'); table.append((mn, 'no entry signal found', ''))
    else:
        ex_by = []
        for pc, outs in allpre:
            for x in outs:
                lab = 'other leg' if 'other leg' in (x[4] or '') else ('past merge' if x[4] == 'past merge' else (x[4] or 'in block'))
                ex_by.append('%s %s %s%s' % (x[0], x[1][:3], lab, ' / T%s@%d' % (x[3][0], x[3][1]) if x[3] else ''))
        table.append((mn, '; '.join(ex_by) or 'none', ' | '.join(hold)))
hdr = ['TABLE: merge | exit/combo signals facing out of the entry signal\'s block, by place | holding leg pre-signals (entry/exit/combo, last 8 pieces)']
for a, b, c in table: hdr.append('%-9s | %s | %s' % (a, b, c))
print('\n'.join(hdr)); print(); print('\n'.join(out))
