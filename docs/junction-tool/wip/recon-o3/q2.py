import sys,re,collections
sys.path.insert(0,'.')
import parse,q1,seg,seg2
def eventy(h,b):
    """y of each event of band b, by event index"""
    d=h.d; out={}
    # node event: y of node point; find via edges: edge from node -> start_pt ; to node -> end_pt
    for ev in d.events[b]:
        if ev['node']:
            n=ev['a']; y=None
            for e,g in d.edges.items():
                if g['frm']=='n%d'%n and e in h.start_pt: y=h.pty(h.start_pt[e]);break
                if g['to']=='n%d'%n and e in h.end_pt: y=h.pty(h.end_pt[e]);break
            out[ev['i']]=y
        else:
            a,bb=ev['a'],ev['b']
            # rho from h.events of a
            ys=[x[1] for x in h.events.get(a,[]) if x[0].startswith('bandX') and x[2]==bb]
            out[ev['i']]=ys[0] if ys else None
    return out
def line_at(h,b,y,ey):
    d=h.d
    k=0
    for ev in d.events[b]:
        yy=ey[ev['i']]
        if yy is not None and yy>y: k=ev['i']+1
        # events are in sweep order: stop at first not applied
        else: break
    return d.lines[b][k],k
def run(name):
    h=seg.build(name); d=h.d
    nb=collections.defaultdict(set)
    for b,w,e,x in d.neigh: nb[b].add((w,e))
    print('=====',name)
    for b in sorted(d.events|{}.keys() if False else set(x[0] for x in d.neigh)):
        pairs=sorted(p for p in d.neigh if p[0]==b)
        print(' band',b,'events',[(ev['i'],'N%d'%ev['a'] if ev['node'] else 'X%d,%d'%(ev['a'],ev['b'])) for ev in d.events[b]],'neighbour pairs',len(pairs),'extension-only',sum(p[3] for p in pairs))
        print('   ',[(w,e,'ext' if x else '') for _,w,e,x in pairs])
    return h
def clash_pairs(name):
    h=seg.build(name); d=h.d
    nbset=collections.defaultdict(set)
    for b,w,e,x in d.neigh: nbset[b].add(frozenset((w,e)))
    allnb=set().union(*nbset.values()) if nbset else set()
    ey={b:eventy(h,b) for b in d.events}
    res=[]
    for c in d.clashes:
        a,b=c['a'],c['b']
        if not(q1.isband(a) and q1.isband(b)) or a['edge']==b['edge']: continue
        y=c['y']; bd=h.band_of_y(y)
        pair=frozenset((a['edge'],b['edge']))
        if isinstance(bd,tuple):
            res.append(dict(c=c,band=bd,nbr=pair in allnb,between=None,line=None)); continue
        nbr=pair in nbset.get(bd,set())
        between=None; line=None
        if not nbr and bd in d.events:
            line,k=line_at(h,bd,y,ey[bd])
            if a['edge'] in line and b['edge'] in line:
                ia,ib=line.index(a['edge']),line.index(b['edge'])
                lo,hi=sorted((ia,ib)); between=line[lo+1:hi]
        res.append(dict(c=c,band=bd,nbr=nbr,between=between,line=line,anynbr=pair in allnb))
    return h,res
if __name__=='__main__':
    for name in ['T','LLRR_LR','LR_LLRR']:
        run(name)
        h,res=clash_pairs(name)
        print(' band-vs-band clashes of different edges:',len(res))
        print('   in stack rows (band undefined):',sum(isinstance(r['band'],tuple) for r in res),'neighbour in band:',sum(r['nbr'] for r in res if not isinstance(r['band'],tuple)),'non-neighbour:',sum((not r['nbr']) for r in res if not isinstance(r['band'],tuple)))
        for r in res:
            c=r['c']
            if isinstance(r['band'],tuple): 
                print('     STACK y',c['y'],r['band'],c['a']['edge'],c['a']['dbg'],c['b']['edge'],c['b']['dbg'],'nbr-anywhere',r['nbr'])
            elif not r['nbr']:
                print('     NONNBR y',c['y'],'band',r['band'],c['a']['edge'],c['a']['dbg'],c['b']['edge'],c['b']['dbg'],'between',r['between'],'line',r['line'],'nbr-in-other-band',r.get('anynbr'))
