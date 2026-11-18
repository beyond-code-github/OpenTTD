import sys, re, collections
sys.path.insert(0,'.')
import parse, q1
OVER,UNDER,TUN,ABOVE=3,4,5,6
def kindname(k): return {0:'Fan',1:'Corner',2:'SBend',3:'Over',4:'Under',5:'Tunnel',6:'Above'}[k]
class Hub: pass
def build(name):
    d=parse.load(f'dump-{name}.txt')
    h=Hub(); h.d=d; h.name=name
    h.pty=lambda p:d.pt[p][1]; h.ptx=lambda p:d.pt[p][0]
    # band membership of edges
    h.band_edges=q1.band_of_edge_sets(d)
    h.rowedge_set=set(); 
    # row edges: edges with row pieces in OCC
    rowe=set(); bande=set()
    for o in d.occ:
        k=q1.klass(o)
        if o['edge']<0: continue
        (bande if k.startswith('band:') else rowe).add(o['edge'])
    h.rowedges=rowe; h.bandedges=bande
    # crossing pairs of band events
    h.bandcross={}
    for b,evs in d.events.items():
        for ev in evs:
            if not ev['node']: h.bandcross[frozenset((ev['a'],ev['b']))]=(b,ev['i'])
    # item row ys
    h.iy={i:d.pt[p][1] for i,p in d.itempt.items()}
    # band of a y
    rowsy=collections.defaultdict(list)
    for i,(kind,row) in d.itemkind.items(): rowsy[row].append(h.iy[i])
    h.rowrange={r:(min(v),max(v)) for r,v in rowsy.items()}
    R=len(h.rowrange)
    def band_of_y(y):
        for r in range(R):
            lo,hi=h.rowrange[r]
            if y<lo: return r
            if y<=hi: return ('row',r)
        return R
    h.band_of_y=band_of_y
    # chain end points & y
    h.start_pt={};h.end_pt={}
    for e in d.edges:
        el=d.elems.get((e,0))
        if el: h.start_pt[e]=el[0]['first']; h.end_pt[e]=el[-1]['last']
    # events per edge in travel order
    h.cpieces_items=collections.defaultdict(list)
    for c in d.cpieces:
        if c['item'] not in h.cpieces_items[c['edge']]: h.cpieces_items[c['edge']].append(c['item'])
    h.events={}
    for e in sorted(h.bandedges):
        prog=d.prog[(e,0)]
        chain=d.elems[(e,0)]
        heads=[el for el in chain if el['k']=='Heads']
        sy=h.pty(h.start_pt[e]); ey=h.pty(h.end_pt[e])
        down = ey>=sy
        evs=[('start',sy,h.start_pt[e],None)]
        overs=0
        # row-crossing items sorted in travel order
        ritems=sorted(h.cpieces_items[e],key=lambda i:h.iy[i],reverse=not down)
        rq=0
        for t in prog:
            if t['kind'] in (OVER,TUN):
                p=t['index']; hd=heads[overs]; overs+=1
                rho=min(h.pty(hd['first']),h.pty(hd['last']))+1
                typ='bandX' if frozenset((e,p)) in h.bandcross else 'rowX'
                evs.append((typ+'/over',rho,p,t['tok']))
            elif t['kind'] in (UNDER,ABOVE):
                p=t['index']
                if frozenset((e,p)) in h.bandcross:
                    pc=[el for el in d.elems[(p,0)] if el['k']=='Heads']
                    # which of p's overs is e?
                    pp=[x for x in d.prog[(p,0)] if x['kind'] in (OVER,TUN)]
                    idx=[x['index'] for x in pp].index(e)
                    hd=pc[idx]; rho=min(h.pty(hd['first']),h.pty(hd['last']))+1
                    evs.append(('bandX/under',rho,p,t['tok']))
                else:
                    y=h.iy[ritems[rq]] if rq<len(ritems) else None; rq+=1
                    evs.append(('rowX/under',y,p,t['tok']))
        evs.append(('end',ey,h.end_pt[e],None))
        h.events[e]=evs
    return h
if __name__=='__main__':
    for name in ['T']:
        h=build(name)
        for e,evs in h.events.items(): print(e,evs)
