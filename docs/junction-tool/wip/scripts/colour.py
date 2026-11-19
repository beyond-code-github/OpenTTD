import json,sys
# usage: colour.py layout.json pd-lines.txt out.json  (colour by line letter)
TYPES={'A':1,'B':6,'C':4,'D':8,'E':7,'F':5}
lay=json.load(open(sys.argv[1])); col={}
for ln in open(sys.argv[2]):
    p=ln.split()
    if len(p)<5: continue
    x,y,name=int(p[0]),int(p[1]),p[4]
    if name.startswith('prio:'): name=name[5:]
    col.setdefault((x,y),TYPES.get(name[0],0))
for t in lay['tracks']: t['rail_type']=col.get((t['x'],t['y']),0)
for b in lay['bridges']: b['rail_type']=col.get(tuple(b['start']),0)
json.dump(lay,open(sys.argv[3],'w'))
print('uncoloured tracks',sum(1 for t in lay['tracks'] if (t['x'],t['y']) not in col))
