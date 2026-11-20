"""X1 probe: route-set line/node rule on PSG 197's trace graph. python3 -I lines.py [nodir]"""
import importlib.util as u,re,sys,os
from collections import defaultdict,deque
TR="/mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad/psg/anatomy/psg197/trace/core.py"
ROOT="/home/pete/OpenTTD/.claude/ng1"
def load():
    s=u.spec_from_file_location("core",TR);c=u.module_from_spec(s);s.loader.exec_module(c);return c.build()
def bfs(starts,adj):
    seen=set(starts);q=deque(starts)
    while q:
        a=q.popleft()
        for b in adj.get(a,()):
            if b not in seen: seen.add(b);q.append(b)
    return seen
def analyse(N):
    CS=N.CS;CP=N.CP
    fw={};bw={}
    for L in N.ins: fw[N.lab[L['id']]]=bfs([L['pc']],CS)
    arms=defaultdict(list)
    for L in N.outs: arms[N.lab[L['id']][0]].append(L['pc'])
    for a,l in arms.items(): bw[a]=bfs(l,CP)
    RS={}
    for pc in N.core:
        RS[pc]=frozenset((i,a) for i in fw for a in bw if pc in fw[i] and pc in bw[a])
    return fw,bw,RS
INL={'N':{1:'N91',2:'N90'},'E':{1:'E174',2:'E173',3:'E169'},'S':{1:'S81',2:'S82',3:'S83'}}
LET={'A':('N','S'),'B':('N','E'),'C':('E','N'),'D':('E','S'),'E':('S','N'),'F':('S','E')}
def rname(pair):
    i,a=pair
    for l,(s,t) in LET.items():
        if i[0]==s and a==t:
            d=[k for k,v in INL[s].items() if v==i][0]; return l+str(d)
def rpair(n):
    s,t=LET[n[0]]; return (INL[s][int(n[1])],t)
def main():
    N=load();fw,bw,RS=analyse(N)
    CS=N.CS
    # lines
    par={p:p for p in N.core}
    def f(x):
        while par[x]!=x: par[x]=par[par[x]];x=par[x]
        return x
    for a in N.core:
        for b in CS[a]:
            if RS[a]==RS[b]: par[f(a)]=f(b)
    comp=defaultdict(list)
    for p in N.core: comp[f(p)].append(p)
    LID={};lines=[]
    for k,(r,ps) in enumerate(sorted(comp.items(),key=lambda kv:min(kv[1]))):
        for p in ps: LID[p]=k
        lines.append(ps)
    lanepc={}
    for L in N.lanes: lanepc[L['pc']]=N.lab[L['id']]
    tl=lambda p:'%d,%d'%p[0]
    pl=lambda p:'%d,%d%s'%(p[0][0],p[0][1],N.W.pname(p)[:2])
    out=[]
    P=lambda *a:out.append(' '.join(str(x) for x in a))
    info=[]
    for k,ps in enumerate(lines):
        S=set(ps); st=[p for p in ps if not any(q in S for q in N.CP[p])]
        en=[p for p in ps if not any(q in S for q in CS[p])]
        fr=sorted(set(('port '+lanepc[p]) if p in lanepc and p in N.ins_pc else tl(p) for p in st)) if False else None
        info.append((st,en))
    N.ins_pc={L['pc']:1 for L in N.ins}; N.outs_pc={L['pc']:1 for L in N.outs}
    def end_lbl(p,out_):
        if p in lanepc and ((not out_ and p in N.ins_pc) or (out_ and p in N.outs_pc)): return 'port '+lanepc[p]
        return tl(p)
    # nodes: cross edges
    cross=[(a,b) for a in N.core for b in CS[a] if RS[a]!=RS[b]]
    # split/merge events at pieces
    npar={}
    def g(x):
        npar.setdefault(x,x)
        while npar[x]!=x: npar[x]=npar[npar[x]];x=npar[x]
        return x
    for a,b in cross:
        g(('p',a));g(('p',b)); npar[g(('p',a))]=g(('p',b))
    # also join cross edges that share a piece (done); cluster
    cl=defaultdict(lambda:[set(),set(),[]])
    for a,b in cross:
        c=g(('p',a)); cl[c][0].add(LID[a]); cl[c][1].add(LID[b]); cl[c][2].append((a,b))
    nodes=list(cl.values())
    ar=defaultdict(int)
    for i,o,_ in nodes:
        ar['%d->%d'%(len(i),len(o))]+=1
    lset=lambda k:frozenset(rname(x) for x in RS[lines[k][0]])
    P('lines',len(lines),'nodes',len(nodes),'arity',dict(sorted(ar.items())))
    splits=sum(1 for i,o,_ in nodes if len(i)==1 and len(o)>1);merges=sum(1 for i,o,_ in nodes if len(o)==1 and len(i)>1)
    P('splits(1->k)',splits,'merges(k->1)',merges,'other',len(nodes)-splits-merges)
    P('\nLINES: id  from  to  routes  npieces')
    for k,ps in enumerate(lines):
        st,en=info[k]
        P('L%d'%k,('|'.join(sorted(set(end_lbl(p,False) for p in st)))[:60] or '(loop)'),'->',('|'.join(sorted(set(end_lbl(p,True) for p in en)))[:60] or '(loop)'),'+'.join(sorted(lset(k))),len(ps))
    P('\nNODES: in lines -> out lines')
    for i,o,_ in sorted(nodes,key=lambda n:min(n[0]|n[1])):
        P(sorted(i),'->',sorted(o),' edges:',' '.join('%s>%s'%(pl(a),pl(b)) for a,b in _[:4]))
    # ---- hand
    txt=open(ROOT+'/tools/junction/topologies/psg197.txt').read().splitlines()
    tab={}
    for l in open(ROOT+'/docs/junction-tool/wip/topologies/psg197-decisions.md').read().split('## Line index')[1].splitlines():
        m=re.match(r'\| (\S+) \| (\S+) \| (.+?) \| (.+?) \| (.*?) \|',l)
        if m and m.group(1)!='line' and '---' not in m.group(1): tab[m.group(1)]=(set(m.group(2).split('+')),m.group(3),m.group(4))
    P('\nhand lines',len(tab))
    bytile=defaultdict(list)
    for p in N.core: bytile[p[0]].append(p)
    lane_in={N.lab[L['id']]:L['pc'] for L in N.ins}; lane_out={N.lab[L['id']]:L['pc'] for L in N.outs}
    def ptiles(s,out_):
        m=re.match(r'port (\w+)',s)
        if m: return [(lane_out if out_ else lane_in)[m.group(1)]]
        x,y=map(int,s.split(',')); return bytile[(x,y)]
    touched=set();res={}
    P('\nMATCH: hand line, hand routes, ext line ids touched by its interior path (route-carrying), ext route sets'); 
    for n,(rt,fr,to) in tab.items():
        pairs=set(rpair(r) for r in rt)
        ok=lambda p:pairs<=RS[p]
        F=[p for p in ptiles(fr,False) if ok(p)];T=[p for p in ptiles(to,True) if ok(p)]
        adj={a:[b for b in CS[a] if ok(b)] for a in N.core if ok(a)}
        radj={a:[b for b in N.CP[a] if ok(b)] for a in N.core if ok(a)}
        path=bfs(F,adj)&bfs(T,radj)
        ft={p[0] for p in F}|{p[0] for p in T}
        inter=[p for p in path if p[0] not in ft] or list(path)
        ids=sorted({LID[p] for p in inter}); touched|=set(ids)
        sets=[lset(i) for i in ids]
        res[n]=(rt,ids,sets,bool(path))
    exact=0;multi=[];nopath=[]
    for n,(rt,ids,sets,ok) in res.items():
        if not ok: nopath.append(n);continue
        if len(ids)==1 and set(sets[0])==rt: exact+=1
        else: multi.append(n)
    # ext line -> hand lines
    e2h=defaultdict(list)
    for n,(rt,ids,sets,ok) in res.items():
        for i in ids: e2h[i].append(n)
    P('exact one-to-one (single ext line, same route set):',exact,'; multi/other:',len(multi),'; no path:',nopath)
    for n in sorted(res):
        rt,ids,sets,ok=res[n]
        P(n,'+'.join(sorted(rt)),['L%d'%i for i in ids],['+'.join(sorted(s)) for s in sets] if len(ids)>1 or (ids and set(sets[0])!=rt) else '')
    P('\next lines shared by several hand lines (under-split):')
    for i,h in sorted(e2h.items()):
        if len(h)>1: P('L%d'%i,'+'.join(sorted(lset(i))),len(lines[i]),'pieces <-',h)
    extra=[i for i in range(len(lines)) if i not in touched]
    P('\next lines touched by no hand line:',len(extra))
    for i in extra: P('L%d'%i,'+'.join(sorted(lset(i))),len(lines[i]),'pieces',sorted(set(tl(p) for p in lines[i]))[:6])
    # hand nodes
    ln=lambda s:tab[s][0] if s in tab else ({'A2'} if s=='A2m' else None)
    hn=[]
    for l in txt:
        if not l.startswith('node '):continue
        lhs,rhs=l[5:].split(' -> ')
        ins=[x.strip() for x in lhs.split(',')]
        legs=[x.strip() for x in rhs.split(',')]
        ins_n=[x.split('=')[0] for x in ins]
        in_sets=[]
        for x in ins:
            nm=x.split('=')[0]
            in_sets.append(frozenset(x.split('=')[1].split('+')) if '=' in x else frozenset(ln(nm) or print("MISSING",nm,l) or []))
        out_sets=[]
        for x in legs:
            nm=x.split('=')[0]
            out_sets.append(frozenset(x.split('=')[1].split('+')) if '=' in x else frozenset(ln(nm) or print("MISSING",nm,l) or []))
        hn.append((l,in_sets,out_sets))
    chg=[h for h in hn if any(s!=h[1][0] for s in h[1]+h[2])]
    # same-set: all ins and outs equal sets
    P('\nhand nodes',len(hn),'; set-changing',len(chg),'; same-set (choice splits / plain joins):',len(hn)-len(chg))
    def sig(i,o): return (frozenset(i),frozenset(o))
    hsig=defaultdict(list)
    for l,i,o in chg: hsig[sig(i,o)].append(l)
    esig=defaultdict(list)
    for i,o,_ in nodes: esig[sig([lset(k) for k in i],[lset(k) for k in o])].append((sorted(i),sorted(o)))
    P('signatures (in route sets, out route sets): hand set-changing',sum(len(v) for v in hsig.values()),'ext',sum(len(v) for v in esig.values()))
    P('hand sigs not in ext:')
    for s,v in hsig.items():
        if s not in esig: P(' ',v)
    P('ext sigs not in hand set-changing:')
    for s,v in esig.items():
        if s not in hsig: P(' ',v,[sorted('+'.join(sorted(x)) for x in z) for z in s])
    P('same-set hand nodes:')
    for l,i,o in hn:
        if (l,i,o) not in chg: P(' ',l)
    open(os.path.join(os.path.dirname(os.path.abspath(__file__)),'lines-out.txt' if len(sys.argv)<2 else 'lines-out-%s.txt'%sys.argv[1]),'w').write('\n'.join(out)+'\n')
    print('\n'.join(out[:3]));print(out[out.index('')+0:][0] if False else '')
main()
