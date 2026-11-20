import sys, io, re, contextlib
HERE='/home/pete/OpenTTD/docs/junction-tool/wip/probes/e1'
src=open(HERE+'/order.py').read(); g={'__name__':'order_mod'}
with contextlib.redirect_stdout(io.StringIO()): exec(compile(src,HERE+'/order.py','exec'),g)
index=g['index']; walks=g['walks']
W={}
for (n,f,t,tl) in index:
    w,c=walks(f,t,tl,n)
    W[n]=sorted(w)
amb=[n for n in W if len(W[n])>1]
print('index',len(index),'ambiguous',[(n,len(W[n])) for n in amb], 'nowalk',[n for n in W if not W[n]])
def pcs(p): return set(p)
for n in amb:
    ends={W[n][0][0][0], W[n][0][-1][0]}
    for k,w in enumerate(W[n]):
        ends={w[0][0],w[-1][0]}
        sh={}
        for m in W:
            if m==n: continue
            for j,v in enumerate(W[m]):
                s=(set(w)&set(v)) 
                s={p for p in s if p[0] not in ends}
                if s: sh.setdefault(m,[]).append((j,len(s)))
        print(n,'walk',k,'len',len(w),'shares',sh)
print()
N=g['N']
for n in ['D12g','D12f','D12d']:
    for k,w in enumerate(W[n]): print(n,k,[(p[0],N.W.pname(p)) for p in w][:20])
print([x for x in index if x[0] in('D12g','D12e','D12f','D12d')])
pl=g['polyline']
for k,w in enumerate(W['D12g']): print('D12g poly',k,pl(w))
