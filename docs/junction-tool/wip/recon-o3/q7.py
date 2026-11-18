import sys,re,collections
sys.path.insert(0,'.')
import parse
def load_y(name):
    rels=[]
    for ln in open(f'dump-{name}.txt'):
        if ln.startswith('YREL'):
            m=re.match(r'YREL v (-?\d+) u (-?\d+) c (-?\d+) bend (-?\d+) a (-?\d+) b (-?\d+) nl (\d) zb (\d) grp (-?\d+) alt (-?\d+) over (-?\d+) reason (.*)',ln.strip())
            g=m.groups(); rels.append(dict(v=int(g[0]),u=int(g[1]),c=int(g[2]),bend=int(g[3]),a=int(g[4]),b=int(g[5]),nl=int(g[6]),zb=int(g[7]),grp=int(g[8]),alt=int(g[9]),over=int(g[10]),reason=g[11]))
    return rels
def run(name):
    d=parse.load(f'dump-{name}.txt')
    rels=load_y(name)
    edges=[]
    for r in rels:
        if r['reason']=='bridge middle clearance': continue
        if r['zb'] or r['grp']>=0 or r['alt']>=0 or r['over']>=0 or r['nl']: continue
        c=r['c']
        if r['bend']>=0:
            fl=d.bends[r['bend']]['floor']
            if r['a']<0: continue
            c=r['a']*fl+r['b']
        edges.append((r['u'],r['v'],c))
    clear=[r for r in rels if r['reason']=='bridge middle clearance']
    n=max(max(e[0],e[1]) for e in edges)+1
    cnt=collections.Counter(); out=[]
    cache={}
    for r in clear:
        u,v=r['u'],r['v']
        if u not in cache:
            dist={u:0}
            for _ in range(n+1):
                ch=False
                for a,b,c in edges:
                    if a in dist and dist[a]+c>dist.get(b,-10**9): dist[b]=dist[a]+c; ch=True
                if not ch: break
            cache[u]=dist
        dd=cache[u].get(v)
        implied = dd is not None and dd>=r['c']
        cnt['implied by the other unconditional Y rows (at floor sizes)' if implied else 'not implied']+=1
        out.append((u,v,r['c'],dd,implied,d.pt.get(u),d.pt.get(v)))
    return cnt,out,len(clear)
for name in ['T','LR_LLRR','LLRR_LR','LL2_100100000']:
    cnt,out,n=run(name)
    print(name,'Y clearance relations',n,dict(cnt))
    for o in out:
        if not o[4]: print('   notimplied',o)
