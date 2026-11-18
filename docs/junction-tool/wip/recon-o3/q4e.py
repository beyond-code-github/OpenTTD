import sys,collections
sys.path.insert(0,'.')
import seg,q4,q4b
def segs_all(h,e):
    ts=q4.tiles_of(h,e)
    if not ts: return []
    ymin=min(t[0] for t in ts); ymax=max(t[0] for t in ts)
    cuts=sorted(set([ymin,ymax]+[ev[1] for ev in h.events[e] if ev[1] is not None and ymin<ev[1]<ymax]))
    out=[]
    pairs=list(zip(cuts,cuts[1:])) or [(ymin,ymax)]
    for a,b in pairs:
        st=[t for t in ts if a<=t[0]<=b]
        out.append(dict(r0=a,r1=b,tiles=st,runs=sorted(set(t[1] for t in st if t[2]==2))))
    return out
for name in ['T','LLRR_LR','LR_LLRR']:
    h=seg.build(name); d=h.d
    cnt=collections.Counter(); fails=[]
    for (b,w,e,ext) in d.neigh:
        if w not in h.bandedges or e not in h.bandedges: continue
        tw=collections.defaultdict(list); te=collections.defaultdict(list)
        for t in q4.tiles_of(h,w): tw[t[0]].append(t[1])
        for t in q4.tiles_of(h,e): te[t[0]].append(t[1])
        S=set(y for y in set(tw)&set(te) if max(tw[y])<min(te[y]))
        # rows where shared and the opposite order physically (w east of e)
        if not S: cnt['pairs: no row with w west of e']+=1; continue
        cnt['pairs evaluated']+=1
        for s1 in segs_all(h,w):
            for s2 in segs_all(h,e):
                rr=set(range(max(s1['r0'],s2['r0']),min(s1['r1'],s2['r1'])+1))&S
                if not rr: continue
                ca,ka=q4.ccol(s1,True); cb,kb=q4.ccol(s2,False)
                cnt['(i) segment pairs']+=1
                if ca+1<=cb: cnt['(i) hold']+=1
                else: cnt['(i) fail']+=1; fails.append((b,w,e,(s1['r0'],s1['r1']),ka,len(s1['runs']),ca,(s2['r0'],s2['r1']),kb,len(s2['runs']),cb,min(rr)))
        rhos=set()
        for ed in (w,e):
            for ev in h.events[ed]:
                if ev[1] in S: rhos.add((ev[1],ed,ev[0]))
        for rho,ed,typ in sorted(rhos):
            cnt['(ii) rows']+=1
            if max(tw[rho])+1<=min(te[rho]): cnt['(ii) hold']+=1
            else: cnt['(ii) fail']+=1
    print(name,dict(cnt))
    for f in fails: print('   FAIL',f)
