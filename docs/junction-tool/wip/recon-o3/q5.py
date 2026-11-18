import sys,collections
sys.path.insert(0,'.')
import seg,q2
for name in (sys.argv[1:] or ['T','LLRR_LR','LR_LLRR']):
    h=seg.build(name); d=h.d
    print('=====',name,'EVPAIRs',len(d.evpair))
    cnt=collections.Counter()
    evset=collections.defaultdict(set)
    for b,f,s in d.evpair:
        ey=q2.eventy(h,b)
        yf,ys_=ey[f],ey[s]
        rel='first south of second' if yf>ys_ else ('same row' if yf==ys_ else 'first NORTH of second (against sweep order)')
        cnt[rel]+=1
        evset[b]|={f,s}
        evf=d.events[b][f]; evs=d.events[b][s]
        print('   band',b,'pair',(f,s),[('N%d'%x['a'] if x['node'] else 'X%d,%d'%(x['a'],x['b'])) for x in (evf,evs)],'y',yf,ys_,rel)
    print('  ',dict(cnt))
    # conditional neighbour pairs
    cond=collections.Counter(); condl=[]
    for (b,w,e,ext) in d.neigh:
        lines=d.lines[b]; n=len(lines)
        adj=[k for k in range(n) if w in lines[k] and e in lines[k] and lines[k].index(w)+1==lines[k].index(e)]
        if ext or not adj:
            cond['conditional (extension-only: adjacent under another order only)']+=1; condl.append((b,w,e,'ext')); continue
        k0,k1=min(adj),max(adj)
        be=(k0-1) if k0>0 else None   # event creating adjacency
        en=k1 if k1<n-1 else None     # event ending adjacency (event index k1)
        inv=[x for x in (be,en) if x is not None and x in evset[b]]
        if inv: cond['conditional (begin/end event is in an event-order pair)']+=1; condl.append((b,w,e,'begin',be,'end',en))
        else: cond['not conditional (begin/end events fixed by the order)']+=1
        # also adjacency non contiguous
        if adj!=list(range(k0,k1+1)): cond['(non-contiguous adjacency in the sweep lines)']+=1
    print('  ',dict(cond))
    for c in condl: print('     ',c)
