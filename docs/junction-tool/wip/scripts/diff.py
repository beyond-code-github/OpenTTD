import json,sys
orig=json.load(open(sys.argv[1])); pete=json.load(open(sys.argv[2])); ox,oy=int(sys.argv[3]),int(sys.argv[4])
A={}
for t in orig['tracks']: A[(t['x']+ox,t['y']+oy)]=set(t['bits'])
for b in orig['bridges']:
    (x0,y0),(x1,y1)=b['start'],b['end']
    A.setdefault((x0+ox,y0+oy),set()).add('B'); A.setdefault((x1+ox,y1+oy),set()).add('B')
B={}
for t in pete['tiles']:
    if t.get('tile_type')=='rail': B[(t['x'],t['y'])]=set(t.get('tracks',[]))
    elif 'bridge' in t.get('kind','') or t.get('tile_type')=='tunnelbridge': B[(t['x'],t['y'])]={'B'}
xs=[k[0] for k in list(A)+list(B)]; ys=[k[1] for k in list(A)+list(B)]
ch={'X':'\\','Y':'/','Upper':'-','Lower':'-','Left':'|','Right':'|'}
for y in range(min(ys),max(ys)+1):
    row=''
    for x in range(min(xs),max(xs)+1):
        a,b=A.get((x,y)),B.get((x,y))
        if not a and not b: row+='.'
        elif a and not b: row+='o'
        elif b and not a: row+='+'
        elif a==b or 'B' in a and 'B' in b: row+='#'
        else: row+='~'
    print(f'{y:3d} '+row)
