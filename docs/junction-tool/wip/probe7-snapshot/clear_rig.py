import json, sys, importlib.machinery, importlib.util, time
sys.path.insert(0, '/home/pete/OpenTTD/tools/ottd')
loader = importlib.machinery.SourceFileLoader('ottd', '/home/pete/OpenTTD/tools/ottd/ottd')
spec = importlib.util.spec_from_loader('ottd', loader); O = importlib.util.module_from_spec(spec); loader.exec_module(O)
jobs = [('pete-ll2d.json', 6, 50)]
tiles = set()
for f, ox, oy in jobs:
    L = json.load(open(f))
    for t in L['tracks']: tiles.add((ox + t['x'], oy + t['y']))
    for b in L['bridges']:
        tiles.add((ox + b['start'][0], oy + b['start'][1]))
adm = O.connect()
ok = fail = 0; t0 = time.time()
try:
    for x, y in sorted(tiles):
        try: O.gs(adm, {'cmd': 'demolish', 'x': x, 'y': y}, timeout=30); ok += 1
        except Exception as e: fail += 1
finally: adm.close()
print('demolished', ok, 'failed', fail, f'{time.time()-t0:.1f}s')
