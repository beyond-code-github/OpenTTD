import json, sys, subprocess, os
x1, y1, x2, y2, out = int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4]), sys.argv[5]
BITS = {1: 'X', 2: 'Y', 4: 'Upper', 8: 'Lower', 16: 'Left', 32: 'Right'}
tracks, bridges = [], set()
for y in range(y1, y2 + 1):
    r = json.loads(subprocess.check_output(['/home/pete/OpenTTD/tools/ottd/ottd', 'gs', json.dumps({'cmd': 'rail_row', 'x1': x1, 'x2': x2, 'y': y})]))
    row = r['row']
    for i in range(0, len(row), 2):
        b = int(row[i:i + 2], 16)
        if b: tracks.append({'x': x1 + i // 2 - x1, 'y': y - y1, 'bits': [n for k, n in BITS.items() if b & k]})
    for x, ex, ey in r['bridges']:
        a, c = (x - x1, y - y1), (ex - x1, ey - y1)
        bridges.add(tuple(sorted([a, c])))
json.dump({'size': [x2 - x1 + 1, y2 - y1 + 1], 'tracks': tracks, 'bridges': [{'start': list(a), 'end': list(c)} for a, c in sorted(bridges)], 'signals': []}, open(out, 'w'))
print(len(tracks), 'tracks', len(bridges), 'bridges')
