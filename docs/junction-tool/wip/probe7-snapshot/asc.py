import json,sys
L=json.load(open(sys.argv[1])); x0,y0,x1,y1=map(int,sys.argv[2:6])
g={}
for t in L['tracks']:
    b=set(t['bits']) if isinstance(t['bits'],list) else t['bits']
    g[(t['x'],t['y'])]=b
br={}
for b in L['bridges']:
    (sx,sy),(ex,ey)=b['start'],b['end']
    if sx==ex:
        for y in range(min(sy,ey),max(sy,ey)+1): br[(sx,y)]='H' if y in (sy,ey) else '"'
    else:
        for x in range(min(sx,ex),max(sx,ex)+1): br[(x,sy)]='H' if x in (sx,ex) else '='
sig={(s['x'],s['y']) if 'x' in s else tuple(s['offset']) for s in L.get('signals',[])}
print('    '+''.join(str(x%10) for x in range(x0,x1+1)))
for y in range(y0,y1+1):
    r=''
    for x in range(x0,x1+1):
        c=' '
        b=g.get((x,y))
        if b:
            b=set(b)
            if b<= {'X'}: c='-'
            elif b<={'Y'}: c='|'
            elif b<={'Left','Right'}: c='\\'
            elif b<={'Upper','Lower'}: c='/'
            else: c='+'
        if (x,y) in br: c=br[(x,y)]
        if (x,y) in sig: c='s' if c in '-|\\/' else '$'
        r+=c
    print(f'{y:3} '+r)
