import sys, re, collections
sys.path.insert(0,'.')
import parse, q1, seg
def pieces_of(h,e):
    d=h.d
    pcs=collections.OrderedDict()
    for o in d.occ:
        if o['edge']!=e or not q1.klass(o).startswith('band:'): continue
        pcs.setdefault(o['dbg'],[]).append(o)
    out=[]
    for dbg,os_ in pcs.items():
        m=re.match(r'(\w+)#(\d+)',dbg); typ,i=m.group(1),int(m.group(2))
        ys=[o['y'] for o in os_]
        out.append(dict(dbg=dbg,typ=typ,i=i,ymin=min(ys),ymax=max(ys),cmin=min(o['L'] for o in os_),cmax=max(o['R'] for o in os_),n=len(os_),tiles=[(o['y'],o['L'],o['R'],o['bits']) for o in os_]))
    out.sort(key=lambda p:(p['i'],1 if p['typ']=='run' else 0))
    return out
def edge_report(h,e,verbose=True):
    evs=h.events[e]; pcs=pieces_of(h,e)
    straddles=[]
    for p in pcs:
        for ev in evs:
            y=ev[1]
            if y is None: continue
            if p['ymin']<y<p['ymax']:
                straddles.append((p['dbg'],ev[0],y,p['ymin'],p['ymax']))
    return evs,pcs,straddles
if __name__=='__main__':
    names=sys.argv[1:] or ['T']
    for name in names:
        h=seg.build(name)
        print('=====',name)
        tot=collections.Counter()
        for e in sorted(h.events):
            evs,pcs,st=edge_report(h,e)
            print('edge',e,'events',[(x[0],x[1],x[2]) for x in evs])
            for p in pcs: print('    ',p['dbg'],'rows',p['ymin'],p['ymax'],'cols',p['cmin'],p['cmax'],'n',p['n'])
            for s in st:
                print('   STRADDLE',s); tot[s[1]]+=1
        print('straddle totals by event type',dict(tot))
