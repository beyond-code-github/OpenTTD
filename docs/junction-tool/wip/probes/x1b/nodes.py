"""X1b: switch-node/line rule on PSG 197 with LOCAL directions on free pieces. python3 -I nodes.py"""
import os,re,sys
os.environ['LOCAL']='1'
import importlib.util as u
from collections import defaultdict,deque
D="/mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad/psg/anatomy/psg197/trace/"
ROOT="/home/pete/OpenTTD/.claude/ng1"
s=u.spec_from_file_location("net",D+"net.py");net=u.module_from_spec(s);s.loader.exec_module(net)
sl=u.spec_from_file_location("l1",ROOT+"/docs/junction-tool/wip/probes/x1/lines.py")
src=open(ROOT+"/docs/junction-tool/wip/probes/x1/lines.py").read().replace("\nmain()","\n")
L1={};exec(compile(src,"lines.py","exec"),L1)
bfs=L1['bfs'];rname=L1['rname'];rpair=L1['rpair']
N=net.build2();W=N.W
# directions: signal-directed as before; free pieces: local direction if defined, else both ways
undef=[];restricted=0
for pc in N.pieces:
    if pc in N.sig1: continue
    e=N.ed(pc)
    if e is not None and e in set(W.pedges(pc)):
        N.exits[pc]={e};restricted+=1
    else: undef.append(pc)
N.S=defaultdict(list);N.P=defaultdict(list)
for a in N.pieces:
    for e in N.exits[a]:
        for (o,e2) in W.across(a,e):
            if not N.inb(o[0]): continue
            oe=[x for x in W.pedges(o) if x!=e2]
            if oe and oe[0] in N.exits[o]:
                N.S[a].append((o,e));N.P[o].append((a,e))
# core as core.py
KEEP={('N',84):'out',('N',85):'out',('N',90):'in',('N',91):'in',('S',81):'in',('S',82):'in',('S',83):'in',('S',88):'out',('S',89):'out',('S',90):'out',('E',164):'out',('E',166):'out',('E',167):'out',('E',169):'in',('E',173):'in',('E',174):'in'}
N.lanes=[L for L in N.lanes if (L['side'],L['pos']) in KEEP]
for L in N.lanes: L['dir']=KEEP[(L['side'],L['pos'])]
N.lab={L['id']:'%s%d'%(L['side'],L['pos']) for L in N.lanes}
N.ins=sorted([L for L in N.lanes if L['dir']=='in'],key=lambda L:(L['side'],L['pos']))
N.outs=sorted([L for L in N.lanes if L['dir']=='out'],key=lambda L:(L['side'],L['pos']))
fw=bfs([L['pc'] for L in N.ins],{a:[b for b,_ in v] for a,v in N.S.items()})
bw=bfs([L['pc'] for L in N.outs],{a:[b for b,_ in v] for a,v in N.P.items()})
core=fw&bw;N.core=core
CS={a:sorted(set(b for b,e in N.S[a] if b in core)) for a in core}
CP=defaultdict(list)
for a in core:
    for b in CS[a]: CP[b].append(a)
N.CS=CS;N.CP=CP
fwd,bwd,RS=L1['analyse'](N)
out=[];P=lambda *a:out.append(' '.join(str(x) for x in a))
tl=lambda p:'%d,%d'%p[0]
pl=lambda p:'%d,%d%s'%(p[0][0],p[0][1],W.pname(p)[:2])
P('core pieces',len(core),'free restricted by LOCAL',restricted,'undefined (both ways)',len(undef),'; undefined in core',sum(1 for p in undef if p in core))
undef_core=sorted(set(tl(p) for p in undef if p in core))
P('undefined tiles in core:',' '.join(undef_core))
# nodes
isnode={p for p in core if len(CS[p])>=2 or len(CP[p])>=2}
def kind(p):
    s,m=len(CS[p])>=2,len(CP[p])>=2
    return 'cross' if s and m else 'split' if s else 'merge'
# lines: components of non-node pieces
par={p:p for p in core}
def f(x):
    while par[x]!=x: par[x]=par[par[x]];x=par[x]
    return x
for a in core:
    if a in isnode: continue
    for b in CS[a]:
        if b not in isnode: par[f(a)]=f(b)
comp=defaultdict(list)
for p in core:
    if p not in isnode: comp[f(p)].append(p)
lines=sorted(comp.values(),key=lambda ps:min(ps))
LID={}
for k,ps in enumerate(lines):
    for p in ps: LID[p]=k
bad=[k for k,ps in enumerate(lines) if len({RS[p] for p in ps})>1]
ports_in={L['pc']:N.lab[L['id']] for L in N.ins};ports_out={L['pc']:N.lab[L['id']] for L in N.outs}
# line ends: start pieces (no pred inside line), end pieces
def ends(k):
    S=set(lines[k])
    st=[p for p in lines[k] if not any(q in S for q in CP[p])]
    en=[p for p in lines[k] if not any(q in S for q in CS[p])]
    return st,en
# nodes: leg comparisons
nk=defaultdict(list);fray=defaultdict(int)
ninfo={}
for p in sorted(isnode):
    kd=kind(p);ar=(len(CP[p]),len(CS[p]))
    succ=frozenset(RS[b] for b in CS[p]);pred=frozenset(RS[a] for a in CP[p])
    if kd=='split': fl='fray' if len(succ)>1 else 'lane'
    elif kd=='merge': fl='join' if len(pred)>1 else 'lane'
    else: fl=('fray' if len(succ)>1 else 'lane')+'/'+('join' if len(pred)>1 else 'lane')
    ninfo[p]=(kd,fl,ar)
cnt=defaultdict(int)
for p,(kd,fl,ar) in ninfo.items(): cnt[(kd,fl,'%d->%d'%ar)]+=1
P('lines',len(lines),'nodes (pieces)',len(isnode),'inconsistent-route-set lines',bad)
P('node kinds:',dict(sorted(cnt.items())))
# adjacent node clusters
npar={p:p for p in isnode}
def g(x):
    while npar[x]!=x: npar[x]=npar[npar[x]];x=npar[x]
    return x
for a in isnode:
    for b in CS[a]:
        if b in isnode: npar[g(a)]=g(b)
cl=defaultdict(list)
for p in isnode: cl[g(p)].append(p)
P('node-piece clusters (adjacent node pieces joined):',len(cl),'sizes',sorted(len(v) for v in cl.values()))
P('3-way pieces:',[(pl(p),ninfo[p]) for p in isnode if max(ninfo[p][2])>=3])
# 2-cycles and undirected
two=[(a,b) for a in core for b in CS[a] if a in CS[b] and a<b]
P('2-cycles in core:',len(two),'pieces in them',len({x for t in two for x in t}))
P('\nNODES (piece, kind, fray/lane, arity, routes in->out)')
lset=lambda p:'+'.join(sorted(rname(x) for x in RS[p]))
for p in sorted(isnode,key=lambda p:p[0]):
    P(pl(p),ninfo[p],'in',[lset(a) for a in CP[p]],'out',[lset(b) for b in CS[p]])
P('\nLINES: id from-> to routes npieces')
for k,ps in enumerate(lines):
    st,en=ends(k)
    sl=lambda p,o: ('port '+(ports_out if o else ports_in)[p]) if p in (ports_out if o else ports_in) else tl(p)
    P('L%d'%k,'|'.join(sorted(set(sl(p,0) for p in st)))[:50] or '(loop)','->','|'.join(sorted(set(sl(p,1) for p in en)))[:50] or '(loop)',lset(ps[0]),len(ps))
# ---------- hand
txt=open(ROOT+'/tools/junction/topologies/psg197.txt').read().splitlines()
tab={}
for l in open(ROOT+'/docs/junction-tool/wip/topologies/psg197-decisions.md').read().split('## Line index')[1].splitlines():
    m=re.match(r'\| (\S+) \| (\S+) \| (.+?) \| (.+?) \| (.*?) \|',l)
    if m and m.group(1)!='line' and '---' not in m.group(1): tab[m.group(1)]=(set(m.group(2).split('+')),m.group(3),m.group(4))
P('\nhand lines',len(tab))
bytile=defaultdict(list)
for p in core: bytile[p[0]].append(p)
lane_in={N.lab[L['id']]:L['pc'] for L in N.ins};lane_out={N.lab[L['id']]:L['pc'] for L in N.outs}
def ptile(s,o):
    m=re.match(r'port (\w+)',s)
    if m: return (lane_out if o else lane_in)[m.group(1)][0]
    return tuple(map(int,s.split(',')))
near=lambda a,b:max(abs(a[0]-b[0]),abs(a[1]-b[1]))<=1
# line boundary tiles: own start tile or the node tile before; end likewise
def bnd(k):
    st,en=ends(k);S=set();E=set()
    for p in st:
        S.add(p[0])
        for a in CP[p]: S.add(a[0])
    for p in en:
        E.add(p[0])
        for b in CS[p]: E.add(b[0])
    return S,E
B=[bnd(k) for k in range(len(lines))]
cand={}
for n,(rt,fr,to) in tab.items():
    ft,tt=ptile(fr,0),ptile(to,1)
    pairs={rpair(r) for r in rt}
    c=[k for k in range(len(lines)) if RS[lines[k][0]]=={x for x in RS[lines[k][0]]} and pairs==set(RS[lines[k][0]]) and any(near(ft,t) for t in B[k][0]) and any(near(tt,t) for t in B[k][1])]
    cand[n]=c
hl_unique=[n for n,c in cand.items() if len(c)==1]
e2h=defaultdict(list)
for n,c in cand.items():
    for k in c: e2h[k].append(n)
one=[n for n in hl_unique if len(e2h[cand[n][0]])==1]
none_=[n for n,c in cand.items() if not c];multi=[n for n,c in cand.items() if len(c)>1]
P('hand lines: one-to-one',len(one),'; exactly one cand but ext line shared',len(hl_unique)-len(one),'; several cands',len(multi),'; none',len(none_))
P('  none:',none_);P('  several:',{n:cand[n] for n in multi})
P('  shared ext lines:',{k:v for k,v in e2h.items() if len(v)>1})
P('ext lines matched by no hand line:',[k for k in range(len(lines)) if k not in e2h])
# hand nodes
hn=[]
def lsets(n):
    return tab[n][0] if n in tab else ({'A2'} if n=='A2m' else set())
for l in txt:
    if not l.startswith('node '):continue
    lhs,rhs=l[5:].split(' -> ')
    ins=[x.strip().split('=')[0] for x in lhs.split(',')];legs=[x.strip().split('=')[0] for x in rhs.split(',')]
    def st(x):
        return frozenset(x.split('=')[1].split('+')) if '=' in x else frozenset(lsets(x))
    ins_s=[st(x.strip()) for x in lhs.split(',')];legs_s=[st(x.strip()) for x in rhs.split(',')]
    # tile
    if ins[0].startswith(('AB','CD','EF')) and len(ins)==1 or ins[0] not in tab:
        t=ptile(tab[legs[0]][1],0) if legs[0] in tab else None
    kd='split' if len(ins)==1 and len(legs)>1 else 'merge' if len(legs)==1 and len(ins)>1 else 'other'
    if kd=='split': t=ptile(tab[legs[0]][1],0) if legs[0] in tab else None
    elif kd=='merge': t=ptile(tab[ins[0]][2],1) if ins[0] in tab else ptile(tab[legs[0]][1],0)
    else: t=None
    if t is None:
        for x in legs:
            if x in tab: t=ptile(tab[x][1],0);break
    if t is None:
        for x in ins:
            if x in tab: t=ptile(tab[x][2],1);break
    if t is None: print('NOTILE',l)
    fl=('fray' if len(set(legs_s))>1 else 'lane') if kd=='split' else ('join' if len(set(ins_s))>1 else 'lane')
    hn.append((l,kd,fl,t,len(ins),len(legs)))
hc=defaultdict(int)
for l,kd,fl,t,a,b in hn: hc[(kd,fl,'%d->%d'%(a,b))]+=1
P('hand nodes',len(hn),dict(sorted(hc.items())))
used=set();matched=[];unh=[]
for l,kd,fl,t,a,b in hn:
    if t is None: unh.append((l,kd,fl,t));continue
    c=[p for p in isnode if near(t,p[0]) and ninfo[p][0]==kd]
    cf=[p for p in c if ninfo[p][1]==fl]
    if cf: matched.append((l,[pl(p) for p in cf],'kind+fl'));used|=set(cf)
    elif c: matched.append((l,[pl(p) for p in c],'kind only %s vs %s'%(ninfo[c[0]][1],fl)));used|=set(c)
    else: unh.append((l,kd,fl,t))
mk=[m for m in matched if m[2]=='kind+fl'];mo=[m for m in matched if m[2]!='kind+fl']
P('hand nodes matched by tile(<=1)+kind+fray/lane:',len(mk),'; tile+kind but fray/lane differs:',len(mo),'; none:',len(unh))
for m in mo: P('  fl-diff',m)
for m in unh: P('  no ext node',m)
ext_un=[p for p in isnode if p not in used]
P('extractor node pieces matching no hand node:',len(ext_un))
for p in sorted(ext_un,key=lambda p:p[0]): P('  ',pl(p),ninfo[p],'in',[lset(a) for a in CP[p]],'out',[lset(b) for b in CS[p]])
# N90 three-way: hand A2 splits near tile of A2
t=ptile(tab['A2m'][1],0) if 'A2m' in tab else None
P('N90 three-way: A2m from',tab.get('A2m'),'A2 from',tab['A2'][1:]);
P('ext split/cross pieces within 2 tiles of',tab['A2'][1],[ (pl(p),ninfo[p]) for p in isnode if max(abs(p[0][0]-ptile(tab['A2'][1],0)[0]),abs(p[0][1]-ptile(tab['A2'][1],0)[1]))<=3])
# 2-cycle/undirected pieces location vs dropped pairs
P('\n2-cycle pairs (tiles):',sorted({(tl(a),tl(b)) for a,b in two}))
open(os.path.dirname(os.path.abspath(__file__))+'/nodes-out.txt','w').write('\n'.join(out)+'\n')
print('\n'.join(out[:10]))
import sys
if len(sys.argv)>1:
    for a in sys.argv[1:]:
        x,y=map(int,a.split(','))
        for p in sorted(N.pieces):
            if p[0]==(x,y):
                print(a,pl(p),'core' if p in core else 'NOT core','sig' if p in N.sig1 else ('undef' if p in undef else 'free'),'exits',sorted(N.exits[p]),'S',[pl(b) for b,_ in N.S[p]],'P',[pl(b) for b,_ in N.P[p]],'CS',[pl(b) for b in CS.get(p,[])],'CP',[pl(b) for b in CP.get(p,[])] if p in core else '')
import json
if os.path.exists(os.path.dirname(os.path.abspath(__file__))+'/x1core.json'):
    old=set(json.load(open(os.path.dirname(os.path.abspath(__file__))+'/x1core.json')))
    new={'%d,%d,%d'%(p[0][0],p[0][1],p[1]) for p in core}
    lost=sorted(old-new);print('x1 core',len(old),'x1b core',len(new),'lost',len(lost),'gained',len(new-old))
    from collections import Counter
    print('routes in x1b union:',len({x for p in core for x in RS[p]}),sorted({rname(x) for p in core for x in RS[p]}))
    tl_=sorted({tuple(map(int,x.split(',')[:2])) for x in lost});print('lost tiles',len(tl_),tl_)
print('distinct ext node pieces used',len(used),'of',len(isnode))
mp=defaultdict(list)
for l,ps,k in matched:
    for q in ps: mp[q].append(l.split(' ->')[0][5:])
print('ext pieces matched by >1 hand node:',{q:v for q,v in mp.items() if len(v)>1})
print('hand->ext N90:',[(m[0][:40],m[1]) for m in matched if m[0].startswith(('node A2 ','node A2m'))])
print('undef tiles',len(undef),sorted({tl(p) for p in undef}))
old_u=[p for p in undef if '%d,%d,%d'%(p[0][0],p[0][1],p[1]) in old]
print('undef pieces that were in x1 core:',len(old_u),sorted({tl(p) for p in old_u}))
# lost pieces: hand lines with no match -> tiles
