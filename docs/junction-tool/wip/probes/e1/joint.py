import itertools,re
exec(open('/home/pete/OpenTTD/docs/junction-tool/wip/probes/e1/excl.py').read().split("print('index'")[0])
def tl(s):
    if s.startswith('port '): return None
    x,y=s.split(','); return (int(x),int(y))
node={tl(f) for (n,f,t,_) in index}|{tl(t) for (n,f,t,_) in index}
node.discard(None)
single={n:W[n][0] for n in W if n not in amb}
res=[]
for combo in itertools.product(*[range(2)]*4):
    ch=dict(zip(amb,combo)); tot=0
    for a in amb:
        wa={p for p in W[a][ch[a]] if p[0] not in node}
        for m in W:
            if m==a: continue
            wm=W[m][ch[m]] if m in ch else single[m]
            tot+=len(wa&{p for p in wm if p[0] not in node})
    res.append((tot,ch))
res.sort(key=lambda x:x[0])
for r in res[:6]: print(r)
print(len(node))
