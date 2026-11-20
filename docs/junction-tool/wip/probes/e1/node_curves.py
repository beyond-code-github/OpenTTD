"""D404 data fix: PSG 197's raw shape (psg197.shape-raw.txt) counted each switch tile's curve on the trunk, so a non-straight leg at a topology node started (ended) at the trunk heading, a cusp. Insert the turned heading as the leg's first piece (split) or last piece (merge), shift its `at` pieces by 1 for a split, then regenerate psg197.shape.txt and psg197.steps.txt with split_shape.py.
Input node-curves-in.txt: one line per leg at a topology node that Shape's checker reports as a cusp (D404): `leg node split|merge turned-heading` (the turned heading is one eighth toward the leg's own side, read as CheckH3 does; the 61 messages of the E1h test before this fix).
Run: python3 -I /home/pete/OpenTTD/.claude/ng1/docs/junction-tool/wip/probes/e1/node_curves.py"""
import re, subprocess, sys
HERE = '/home/pete/OpenTTD/.claude/ng1/docs/junction-tool/wip/probes/e1/'
RAW = HERE + 'psg197.shape-raw.txt'
todo = [l.split() for l in open(HERE + 'node-curves-in.txt') if l.strip()]
lines = open(RAW).read().split('\n')
shape_at, at_at = {}, {}
for i, l in enumerate(lines):
    m = re.match(r'shape (\w+): ', l)
    if m: shape_at[m.group(1)] = i
    m = re.match(r'at (\w+): ', l)
    if m: at_at[m.group(1)] = i
count = 0
for leg, node, kind, turned in todo:
    turned = int(turned)
    i = shape_at[leg]
    raw = [int(x) for x in lines[i].split(': ')[1].split()]
    near = raw[0] if kind == 'split' else raw[-1]
    cand = [v for v in (near - 1, near + 1) if (v - turned) % 8 == 0]
    if len(cand) != 1:
        sys.exit('no turned piece for %s %s' % (leg, node))
    if kind == 'split':
        raw.insert(0, cand[0])
        if leg in at_at:
            m = re.match(r'at (\w+): (.*)$', lines[at_at[leg]])
            entries = [(a, int(b) + 1) for a, b in (x.strip().split('@') for x in m.group(2).split(','))]
            lines[at_at[leg]] = 'at %s: %s' % (leg, ', '.join('%s@%d' % e for e in entries))
    else:
        raw.append(cand[0])
    lines[i] = 'shape %s: %s' % (leg, ' '.join(map(str, raw)))
    count += 1
open(RAW, 'w').write('\n'.join(lines))
print('inserted', count)
sys.exit(subprocess.call([sys.executable, '-I', HERE + 'split_shape.py']))
