exec(open('/home/pete/OpenTTD/docs/junction-tool/wip/probes/e1/joint.py').read().split("res=[]")[0])
inter=g['inter']; pl=g['polyline']
import math
def innode(pt):
    return any(t[0]-1e-9<=pt[0]<=t[0]+1+1e-9 and t[1]-1e-9<=pt[1]<=t[1]+1+1e-9 for t in node)
def hits(a,b):
    out=[]
    for i in range(len(a)-1):
        for j in range(len(b)-1):
            r=inter(a[i],a[i+1],b[j],b[j+1])
            if r:
                t,u=r; out.append((a[i][0]+t*(a[i+1][0]-a[i][0]),a[i][1]+t*(a[i+1][1]-a[i][1])))
    return out
for ch in ({'D12e':0,'D12f':1},{'D12e':1,'D12f':0}):
    ch=dict(ch,F1c=1)
    P={m:pl(W[m][ch[m]] if m in ch else W[m][0]) for m in W if m!='D12g'}
    P2={m:P[m] for m in P}
    print(ch)
    for a in ('D12e','D12f','F1c'):
        for m in P:
            if m==a or (m in ch and m<a): continue
            h=hits(P[a],P[m])
            far=[x for x in h if not innode(x)]
            if far: print('  ',a,m,len(h),'off-node',len(far),far[:3])
for e,f in ((0,1),(1,0)):
    h=hits(pl(W['D12e'][e]),pl(W['D12f'][f])); print('ExF',e,f,len(h),h)
