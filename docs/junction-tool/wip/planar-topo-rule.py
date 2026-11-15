"""Direction-sequence rule (DESIGN-planar §2b candidate): every line's segment directions from the topology alone.

Rules:
1. Node heading: all routes through the node exit on one side -> that out port's heading; else all enter from one side ->
   that in port's heading; else the topology is refused (no rule yet).
2. Straight leg: at a merge the prio leg; else (and at splits) the leg whose far end needs the smaller rotation;
   ties -> the right leg (across travel). The other leg leaves/joins at 45 degrees on its own side.
3. A line runs from its start heading to its end heading by the shortest rotation, one segment per heading.
   A port line is one segment at the port heading (STUB). Each diagonal segment carries an optional axis jog (spare S-bend).
"""
import json

HEAD = 'N NE E SE S SW W NW'.split()
SIDES = 'NESW'


def rot(a, b):
    """Signed rotation a -> b in 45-degree steps, -3..4 (4 = reversal)."""
    d = (HEAD.index(b) - HEAD.index(a)) % 8
    return d - 8 if d > 4 else d


def turn(h, k): return HEAD[(HEAD.index(h) + k) % 8]


def from_json(path, STUB=4):
    t = json.load(open(path))
    Ls, nodes, ports, routes = t['lines'], t['nodes'], t['ports'], t['routes']
    nm = [l['name'] for l in Ls]
    prio_r = {i for i, r in enumerate(routes) if r['prio']}
    # Route entry/exit ports.
    entry, exits = {}, {}
    for pi, p in enumerate(ports):
        for li in p['lines']:
            for r in Ls[li]['routes']:
                if p['in']: entry[r] = pi
                else: exits.setdefault(r, set()).add(pi)
    def node_routes(n): return set(r for li in nodes[n]['in'] + nodes[n]['out'] for r in Ls[li]['routes'])
    nh = {}
    for n in range(len(nodes)):
        rs = node_routes(n)
        ex = set().union(*(exits[r] for r in rs))
        en = {entry[r] for r in rs}
        if len({ports[p]['side'] for p in ex}) == 1: nh[n] = HEAD[ports[next(iter(ex))]['heading']]
        elif len({ports[p]['side'] for p in en}) == 1: nh[n] = HEAD[ports[next(iter(en))]['heading']]
        else: raise ValueError(f'node {n}: no heading rule')
    # Far-end heading of a line seen from one end (for the straight-leg rotation test).
    def end_heading(e):
        return HEAD[ports[e['index']]['heading']] if e['port'] else nh[e['index']]
    def assign(choice):
        """choice: node -> straight leg index (None = by rule). Returns leg headings and straight legs."""
        leg_head, straight = {}, {}
        for n, node in enumerate(nodes):
            h = nh[n]
            split = len(node['out']) == 2
            legs = node['out'] if split else node['in']
            if choice.get(n) is not None: s = choice[n]
            elif not split and any(set(Ls[li]['routes']) & prio_r for li in legs) and not all(set(Ls[li]['routes']) & prio_r for li in legs):
                s = next(i for i, li in enumerate(legs) if set(Ls[li]['routes']) & prio_r)
            else:
                far = [abs(rot(h, end_heading(Ls[li]['to'] if split else Ls[li]['from']))) for li in legs]
                s = 0 if far[0] < far[1] else 1 if far[1] < far[0] else None
                if s is None: tie.add(n); s = 1
            straight[n] = legs[s]
            for i, li in enumerate(legs):
                k = 0 if i == s else ((-1 if i == 0 else 1) * (1 if split else -1))
                leg_head[(li, 'start' if split else 'end')] = turn(h, k)
            trunk = node['in'][0] if split else node['out'][0]
            leg_head[(trunk, 'end' if split else 'start')] = h
        return leg_head, straight
    def total(lh):
        return sum(abs(rot(lh[(li, 'start')], lh[(li, 'end')])) for li, l in enumerate(Ls) if not (l['from']['port'] or l['to']['port']))
    tie = set()
    choice = {}
    leg_head, straight = assign(choice)
    # Tied nodes: local search on the lines' total turning (ties stay on the right leg).
    best = total(leg_head); improved = True
    while improved:
        improved = False
        for n in sorted(tie):
            for s in (0, 1):
                c2 = dict(choice); c2[n] = s
                lh, _ = assign(c2)
                if total(lh) < best: best, choice, improved = total(lh), c2, True
    leg_head, straight = assign(choice)
    L = {}
    def pname(li): return 'p' + nm[li]
    def ename(li, which):
        e = Ls[li][which]
        return pname(li) if e['port'] else f'n{e["index"]}'
    for li, l in enumerate(Ls):
        a, b = ename(li, 'from'), ename(li, 'to')
        if l['from']['port'] or l['to']['port']:
            pe = l['from'] if l['from']['port'] else l['to']
            p = ports[pe['index']]; h = HEAD[p['heading']]
            k, j = len(p['lines']), p['lines'].index(li)
            side = -1 if j < (k - 1) / 2 else (1 if j > (k - 1) / 2 else 0)  # left lines fan left inside the box
            if k < 2 or side == 0:
                L[nm[li]] = dict(start=a, end=b, dirs=[h], mins=[STUB], spare=set()); continue
            if p['in']:  # [stub, fan diagonal, run], the diagonal moving to the line's side
                L[nm[li]] = dict(start=a, end=b, dirs=[h, turn(h, side), h], mins=[STUB, 0, 0], spare={1, 2}, fan=True)
            else:        # [run, fan diagonal back toward the port row, stub]
                L[nm[li]] = dict(start=a, end=b, dirs=[h, turn(h, -side), h], mins=[0, 0, STUB], spare={0, 1}, fan=True)
            continue
        hs, he = leg_head[(li, 'start')], leg_head[(li, 'end')]
        r = rot(hs, he)
        if abs(r) == 4: raise ValueError(f'line {nm[li]}: reversal')
        seq = [turn(hs, k * (1 if r > 0 else -1)) for k in range(abs(r) + 1)]
        dirs, mins, spare = [], [], set()
        for d in seq:
            if len(dirs) and False: pass
            dirs.append(d); mins.append(2)
            if len(d) == 2:  # diagonal: optional axis jog, then the diagonal continues
                spare.add(len(dirs)); dirs.append(d[0] if d[0] in 'NS' else d[1]); mins.append(0)
                dirs.append(d); mins.append(2)
        L[nm[li]] = dict(start=a, end=b, dirs=dirs, mins=mins, spare=spare)
    # Ports in reading order (W/E north->south, N/S west->east): a port's lines are left->right across travel.
    P, pgroup = {}, {}
    for pi, p in enumerate(ports):
        side, h = SIDES[p['side']], HEAD[p['heading']]
        lines = list(p['lines'])
        # Left of travel is reading-first when heading E (W/E sides: north) or N (N/S sides: west).
        if h in ('W', 'S'): lines.reverse()
        for li in lines:
            P.setdefault(side, []).append(pname(li)); pgroup[pname(li)] = pi
    cross = [(nm[a], nm[b]) for a, b in t['crossings']]
    def fray(a, b):
        ra, rb = Ls[nm.index(a)]['routes'], Ls[nm.index(b)]['routes']
        return bool({entry[r] for r in ra} & {entry[r] for r in rb}) or bool(set().union(*(exits[r] for r in ra)) & set().union(*(exits[r] for r in rb)))
    meta = dict(pgroup=pgroup, fray={c for c in cross if fray(*c)}, prio_lines={nm[i] for i, l in enumerate(Ls) if set(l['routes']) & prio_r},
                node_heading=nh, straight={n: nm[li] for n, li in straight.items()}, name=t['name'])
    return L, P, cross, meta


if __name__ == '__main__':
    import sys
    L, P, cross, meta = from_json(sys.argv[1])
    for n, l in L.items():
        if len(l['dirs']) > 1 or l['mins'][0] != 4:
            print(f"  {n:6} {l['start']:>5}->{l['end']:<5} {' '.join(d + ('*' if i in l['spare'] else '') for i, d in enumerate(l['dirs']))}")
    print('ports', P)
    print('fray', sorted(meta['fray']))
    print('node headings', meta['node_heading'])
