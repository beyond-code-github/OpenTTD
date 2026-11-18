import sys,re,collections
sys.path.insert(0,'.')
import parse,q1,seg,seg2
def run(name,verbose=True):
    h=seg.build(name); d=h.d
    out=[]
    for e in sorted(h.events):
        evs,pcs,st=seg2.edge_report(h,e)
        chain=d.elems[(e,0)]
        for p in pcs:
            if p['typ'] not in ('Corner','SBend','Fan'): continue
            el=chain[p['i']]; b=d.bends.get(el['bend'])
            for ev in evs:
                y=ev[1]
                if y is None: continue
                if p['ymin']<y<p['ymax']:
                    out.append(dict(edge=e,piece=p['dbg'],bend=el['bend'],group=b['group'] if b else None,kind=b['kind'] if b else None,src=b['src'] if b else None,ev=ev[0],y=y,rows=(p['ymin'],p['ymax']),partner=ev[2]))
    return h,out
if __name__=='__main__':
    for name in ['T','LLRR_LR','LR_LLRR']:
        h,out=run(name)
        print('==',name,'bend-straddles',len(out))
        for o in out: print('  ',o)
