import sys,re,collections
sys.path.insert(0,'.')
import parse,q1,seg,seg2
INF=10**9
def band_range(h,b):
    R=len(h.rowrange)
    lo=-INF if b==0 else h.rowrange[b-1][1]+1
    hi=INF if b==R else h.rowrange[b][0]-1
    return lo,hi
def tiles_of(h,e):
    out=[]
    for o in h.d.occ:
        if o['edge']==e and q1.klass(o).startswith('band:'):
            for x in range(o['L'],o['R']+1) if o['L']<=o['R'] else []:
                out.append((o['y'],x,o['bits'],o['dbg']))
    return out
def segs(h,e,b):
    lo,hi=band_range(h,b)
    ts=[t for t in tiles_of(h,e) if lo<=t[0]<=hi]
    if not ts: return []
    ymin=min(t[0] for t in ts); ymax=max(t[0] for t in ts)
    cuts=sorted(set([ymin,ymax]+[ev[1] for ev in h.events[e] if ev[1] is not None and ymin<ev[1]<ymax]))
    out=[]
    for a,bb in zip(cuts,cuts[1:]):
        st=[t for t in ts if a<=t[0]<=bb]
        runs=sorted(set(t[1] for t in st if t[2]==2))
        out.append(dict(r0=a,r1=bb,tiles=st,runs=runs))
    if len(cuts)==1:
        out.append(dict(r0=ymin,r1=ymax,tiles=ts,runs=sorted(set(t[1] for t in ts if t[2]==2))))
    return out
def ccol(seg_,west):
    if seg_['runs']: return (min(seg_['runs']) if not west else max(seg_['runs'])),'run'
    cols=[t[1] for t in seg_['tiles']]
    return (max(cols) if west else min(cols)),'bend'
def evaluate(name,verbose=False):
    h=seg.build(name); d=h.d
    res=dict(i_hold=0,i_fail=0,ii_hold=0,ii_fail=0,pairs=0,pairs_nocoexist=0)
    fails=[]; info=[]
    for (b,w,e,ext) in d.neigh:
        lo,hi=band_range(h,b)
        if w not in h.bandedges or e not in h.bandedges:
            info.append(('nonband',b,w,e)); continue
        sw=segs(h,w,b); se=segs(h,e,b)
        res['pairs']+=1
        co=0
        for s1 in sw:
            for s2 in se:
                if s1['r1']<s2['r0'] or s2['r1']<s1['r0']: continue
                co+=1
                ca,ka=ccol(s1,True); cb,kb=ccol(s2,False)
                ok=ca+1<=cb
                res['i_hold' if ok else 'i_fail']+=1
                if not ok: fails.append(('i',b,w,e,(s1['r0'],s1['r1']),ka,ca,(s2['r0'],s2['r1']),kb,cb))
        if co==0: res['pairs_nocoexist']+=1
        info.append(('pair',b,w,e,ext,'segs',len(sw),len(se),'coexisting',co))
        # (ii) boundary rows: events of w or e in common row range
        ta=collections.defaultdict(list); tb=collections.defaultdict(list)
        for t in tiles_of(h,w):
            if lo<=t[0]<=hi: ta[t[0]].append(t[1])
        for t in tiles_of(h,e):
            if lo<=t[0]<=hi: tb[t[0]].append(t[1])
        if not ta or not tb: continue
        common=(max(min(ta),min(tb)),min(max(ta),max(tb)))
        rhos=set()
        for ed in (w,e):
            for ev in h.events[ed]:
                if ev[1] is not None and common[0]<=ev[1]<=common[1]: rhos.add((ev[1],ev[0],ed))
        for rho,typ,ed in sorted(rhos):
            if rho in ta and rho in tb:
                A=max(ta[rho]);B=min(tb[rho]); ok=A+1<=B
                res['ii_hold' if ok else 'ii_fail']+=1
                if not ok: fails.append(('ii',b,w,e,rho,typ,ed,A,B))
            else:
                res.setdefault('ii_notile',0); res['ii_notile']+=1
    return h,res,fails,info
if __name__=='__main__':
    for name in ['T','LLRR_LR','LR_LLRR']:
        h,res,fails,info=evaluate(name)
        print('=====',name,res)
        for f in fails: print('  FAIL',f)
