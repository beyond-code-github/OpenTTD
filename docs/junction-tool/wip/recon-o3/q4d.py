import sys,collections,re
sys.path.insert(0,'.')
import seg,q4,q4b,q2,seg2
def incident(d,n,e):
    return [x for x,g in d.edges.items() if x!=e and (g['frm']=='n%d'%n or g['to']=='n%d'%n)]
def node_of_end(d,e,which):
    g=d.edges[e]; s=g['frm'] if which=='start' else g['to']
    return int(s[1:]) if s[0]=='n' else None
def partners(h,e,ev):
    d=h.d
    if ev[0] in('start','end'):
        n=node_of_end(d,e,ev[0])
        return set(incident(d,n,e)) if n is not None else set()
    return {ev[2]}
def bends(h,e):
    out=[]
    for p in seg2.pieces_of(h,e):
        if p['typ'] in('Corner','SBend','Fan'): out.append(p)
    return out
def roles(h,e):
    evs=h.events[e]; sy=evs[0][1]; ey=evs[-1][1]; down=ey>=sy
    out=[]
    for p in bends(h,e):
        lo,hi=p['ymin'],p['ymax']
        a,b=(lo,hi) if down else (hi,lo)  # start,end in travel
        sign=1 if down else -1
        prev=[ev for ev in evs if ev[1] is not None and (ev[1]-a)*sign<=0]
        nxt=[ev for ev in evs if ev[1] is not None and (ev[1]-b)*sign>=0]
        prev_ev=max(prev,key=lambda ev:ev[1]*sign) if prev else None
        next_ev=min(nxt,key=lambda ev:ev[1]*sign) if nxt else None
        r=[]
        if prev_ev and prev_ev[1]==a: r.append(('leaves',prev_ev))
        if next_ev: r.append(('approaches',next_ev))
        out.append((p,r))
    return out
if __name__=='__main__':
    for name in ['T','LLRR_LR','LR_LLRR']:
        h=seg.build(name); d=h.d
        tot=collections.Counter()
        for e in sorted(h.events):
            for p,rs in roles(h,e):
                for kind,ev in rs:
                    tot[(kind,ev[0].split('/')[0])]+=1
        print(name,dict(tot))
        # per neighbour pair: bends of each edge approaching a partner other than the other edge
        cnt=collections.Counter(); ex=[]
        for (b,w,e,ext) in d.neigh:
            if w not in h.bandedges or e not in h.bandedges: continue
            for a,o in ((w,e),(e,w)):
                for p,rs in roles(h,a):
                    for kind,ev in rs:
                        if kind!='approaches': continue
                        pt=partners(h,a,ev)
                        cnt['bend-approach instances (pair edge x bend)']+=1
                        if o in pt: cnt['partner == other edge of pair']+=1
                        elif pt: cnt['partner other than pair other edge']+=1
                        else: cnt['no partner (port/none)']+=1
        print('  ',dict(cnt))
