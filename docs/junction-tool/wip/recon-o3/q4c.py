import sys,collections
sys.path.insert(0,'.')
import seg,q4,q4b,q2
for name in ['T','LLRR_LR','LR_LLRR']:
    h=seg.build(name); d=h.d
    cnt=collections.Counter(); 
    seen=set()
    for (b,w,e,ext) in d.neigh:
        for ed in (w,e):
            if (b,ed) in seen or ed not in h.bandedges: continue
            seen.add((b,ed))
            for s in q4.segs(h,ed,b):
                cnt['segments']+=1
                n=len(s['runs'])
                cnt['run-less' if n==0 else ('1 run column' if n==1 else '>=2 run columns')]+=1
    print(name,dict(cnt))
    # row-restricted (i)
    r=collections.Counter(); fails=[]
    for (b,w,e,ext) in d.neigh:
        if w not in h.bandedges or e not in h.bandedges: continue
        lo,hi=q4.band_range(h,b)
        ys=set(t[0] for t in q4.tiles_of(h,w) if lo<=t[0]<=hi)&set(t[0] for t in q4.tiles_of(h,e) if lo<=t[0]<=hi)
        S=q4b.adjacent_rows(h,b,w,e,sorted(ys))
        if not S: continue
        sw=q4.segs(h,w,b); se=q4.segs(h,e,b)
        for s1 in sw:
            for s2 in se:
                rr=sorted(set(range(max(s1['r0'],s2['r0']),min(s1['r1'],s2['r1'])+1))&set(S))
                if not rr: continue
                def col(s,west):
                    ts=[t for t in s['tiles'] if t[0] in rr]
                    runs=[t[1] for t in ts if t[2]==2]
                    pool=runs if runs else [t[1] for t in ts]
                    return (max(pool) if west else min(pool))
                ca=col(s1,True); cb=col(s2,False)
                r['pairs']+=1
                if ca+1<=cb: r['hold']+=1
                else: r['fail']+=1; fails.append((b,w,e,rr,ca,cb))
    print('   row-restricted (i):',dict(r),fails)
