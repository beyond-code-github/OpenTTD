import sys,re,collections
sys.path.insert(0,'.')
import parse,q1,seg,seg2,q2,q4
def apply(line,removed,inserted):
    n=len(removed)
    for i in range(len(line)-n+1):
        if line[i:i+n]==removed: return line[:i]+inserted+line[i+n:]
    return None
def line_at_y(h,b,y,strict=True):
    d=h.d; ey=q2.eventy(h,b)
    line=list(d.lines[b][0])
    for ev in d.events[b]:
        yy=ey[ev['i']]
        if yy is None: continue
        if (yy>y) if strict else (yy>=y):
            rem,ins=d.repl[b][ev['i']]
            nl=apply(line,rem,ins)
            if nl is None: return None
            line=nl
    return line
def adjacent_rows(h,b,w,e,rows):
    out=[]
    for y in rows:
        for strict in (True,False):
            ln=line_at_y(h,b,y,strict)
            if ln and w in ln and e in ln and ln.index(w)+1==ln.index(e): out.append(y); break
    return out
def evaluate(name):
    h=seg.build(name); d=h.d
    res=collections.Counter(); fails=[]; rowsinfo=[]
    for (b,w,e,ext) in d.neigh:
        lo,hi=q4.band_range(h,b)
        if w not in h.bandedges or e not in h.bandedges: res['nonband pairs']+=1; continue
        sw=q4.segs(h,w,b); se=q4.segs(h,e,b)
        ys=set(t[0] for t in q4.tiles_of(h,w) if lo<=t[0]<=hi)&set(t[0] for t in q4.tiles_of(h,e) if lo<=t[0]<=hi)
        S=set(adjacent_rows(h,b,w,e,sorted(ys)))
        res['neighbour pairs']+=1
        if not S: res['pairs with no adjacent coexisting row at the seed']+=1; rowsinfo.append((b,w,e,ext,'none',sorted(ys))); continue
        res['pairs evaluated']+=1
        for s1 in sw:
            for s2 in se:
                rr=set(range(max(s1['r0'],s2['r0']),min(s1['r1'],s2['r1'])+1))&S
                if not rr: continue
                ca,ka=q4.ccol(s1,True); cb,kb=q4.ccol(s2,False)
                res['(i) segment pairs']+=1
                if ca+1<=cb: res['(i) hold']+=1
                else:
                    res['(i) fail']+=1; fails.append(('i',b,w,e,(s1['r0'],s1['r1']),ka,ca,(s2['r0'],s2['r1']),kb,cb,sorted(rr)[:1]))
        ta=collections.defaultdict(list); tb=collections.defaultdict(list)
        for t in q4.tiles_of(h,w):
            if lo<=t[0]<=hi: ta[t[0]].append(t[1])
        for t in q4.tiles_of(h,e):
            if lo<=t[0]<=hi: tb[t[0]].append(t[1])
        rhos=set()
        for ed in (w,e):
            for ev in h.events[ed]:
                if ev[1] in S: rhos.add((ev[1],ev[0],ed))
        for rho,typ,ed in sorted(rhos):
            res['(ii) boundary rows']+=1
            A=max(ta[rho]);B=min(tb[rho])
            if A+1<=B: res['(ii) hold']+=1
            else: res['(ii) fail']+=1; fails.append(('ii',b,w,e,rho,typ,ed,A,B))
    return h,res,fails,rowsinfo
if __name__=='__main__':
    for name in ['T','LLRR_LR','LR_LLRR']:
        h,res,fails,info=evaluate(name)
        print('=====',name,dict(res))
        for f in fails: print('  FAIL',f)
        for i in info: print('  NOCO',i)
