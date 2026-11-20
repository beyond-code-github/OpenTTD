import re, sys, itertools, collections
T=open(sys.argv[1]).read().splitlines(); D=open(sys.argv[2]).read().splitlines()
routes={}
def rs(name):
    if '=' in name: n,r=name.split('='); return n.strip(), r.strip().split('+')
    m=re.match(r'([A-Z]+)(\d)([a-z]*)$',name.strip()); return name.strip(), [c+m.group(2) for c in m.group(1)]
lines={}; edges=collections.defaultdict(list); portin=[]; portout=[]
def add(tok):
    if "=" not in tok and tok.strip() in lines: return tok.strip()
    n,r=rs(tok)
    if n not in lines or '=' in tok: lines[n]=r
    return n
for l in T:
    if l.startswith('port'):
        head,body=l.split(':'); io=head.split()[2]
        for t in body.split(','):
            n=add(t); (portin if io=='in' else portout).append(n)
    elif l.startswith('node'):
        a,b=l[5:].split('->'); ins=[add(t) for t in a.split(',')]; outs=[add(t) for t in b.split(',')]
        for i in ins:
            for o in outs:
                if set(lines[o])&set(lines[i]): edges[i].append(o)
ring='NO1 NO2 AB2 AB1 EO1 EO2 EO3 CD3 CD2 CD1 SO1 SO2 SO3 EF3 EF2 EF1'.split()
pos={n:i for i,n in enumerate(ring)}
allr=sorted({r for n in portin for r in lines[n]})
paths=[]
for r in allr:
    st=[n for n in portin if r in lines[n]][0]
    def walk(n,acc):
        if n in portout: paths.append((r,acc+[n])); return
        for o in edges[n]:
            if r in lines[o]: walk(o,acc+[n])
    walk(st,[])
cross=collections.Counter()
for l in D:
    m=re.match(r'\| (\w+) \| (\w+) \| tunnel \|',l)
    if m: cross[frozenset((m.group(1),m.group(2)))]+=1
print('lines',len(lines),'paths',len(paths),'crossings',sum(cross.values()))
def inter(a,b,c,d):
    s=lambda x,y,z: (pos[y]-pos[x])%16 < (pos[z]-pos[x])%16
    return s(a,c,b)!=s(a,d,b)
bad=collections.Counter(); ok=0; ex=[]
for (r1,p1),(r2,p2) in itertools.combinations(paths,2):
    if r1==r2 or set(p1)&set(p2): continue
    k=sum(cross[frozenset((x,y))] for x in p1 for y in p2)
    need=inter(p1[0],p1[-1],p2[0],p2[-1])
    if (k%2)!=need:
        bad[(r1,r2)]+=1
        if len(ex)<12: ex.append((r1,r2,p1[0],p1[-1],p2[0],p2[-1],k,need,[tuple(c) for c in cross if any(x in c for x in p1) and any(y in c for y in p2) and len(c&set(p1))==1]))
    else: ok+=1
print('disjoint path pairs ok',ok,'bad',sum(bad.values()))
for k,v in sorted(bad.items()): print(' ',k,v)
for e in ex: print(e)
