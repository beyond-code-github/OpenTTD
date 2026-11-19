import json,sys
def load_orig(p,ox,oy):
    d=json.load(open(p));A={}
    for t in d['tracks']: A[(t['x']+ox,t['y']+oy)]=set(t['bits'])
    for b in d['bridges']:
        (x0,y0),(x1,y1)=b['start'],b['end']
        for x in range(min(x0,x1),max(x0,x1)+1):
            for y in range(min(y0,y1),max(y0,y1)+1):
                if (x+ox,y+oy) not in A: A[(x+ox,y+oy)]={'BR'}
    return A
def load_exp(p):
    d=json.load(open(p));B={}
    for t in d['tiles']:
        if t.get('tile_type')=='rail': B[(t['x'],t['y'])]=set(t.get('tracks',[]))
        else: B[(t['x'],t['y'])]={'BR'}
    return B
def g(s):
    if not s: return '.'
    if 'BR' in s: return '='
    if s=={'X'}: return '\\'
    if s=={'Y'}: return '/'
    if s<= {'Upper','Lower'}: return '-'
    if s<= {'Left','Right'}: return '|'
    return '+'
A=load_orig(sys.argv[1],100,8); B=load_exp(sys.argv[2]); x0,x1,y0,y1=map(int,sys.argv[3:7])
for y in range(y0,y1+1):
    print(f'{y:3d} '+' '.join(g(A.get((x,y))) for x in range(x0,x1+1))+'   |   '+' '.join(g(B.get((x,y))) for x in range(x0,x1+1)))
