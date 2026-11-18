import sys,re,collections
def model(f):
    chans={};prio={};net=collections.defaultdict(int);copy=set();row=set()
    for l in open(f):
        p=l.split()
        if l.startswith("info channel"):
            c=int(p[2].rstrip(':')); toks=l.split(':',1)[1].split()
            ci=toks.index('crossings'); chans[c]=toks[:ci]
            prio[c]=toks[toks.index('prio')+1]=='1' if 'prio' in toks else False
        elif l.startswith("slot ") and p[4]!='-' and p[5] not in ('-',):
            net[p[1]]+=int(p[5])
        elif l.startswith("link "):
            a,b=p[1].split("->"); net[b]+=int(p[2])
        elif l.startswith("track "):
            (copy if p[1]=='copy' else row).add(p[2])
    return chans,prio,net,copy,row
def obs(f):
    out={}
    for l in open(f):
        if not l.startswith("seq ") or l.startswith("seq prio"): continue
        name,rest=l[4:].split(":",1); name=name.strip()
        m=[]
        for x in rest.split():
            d,L=map(int,x.split(":"))
            if m and m[-1][0]==d: m[-1][1]+=L
            else: m.append([d,L])
        turning = m[0][0]!=m[-1][0]
        n=0
        for i in range(1,len(m)-1):
            a,b,c=m[i-1][0],m[i][0],m[i+1][0]
            if a==c and (b-a)%8 in (1,7): n+= (m[i][1]//2)*(1 if (b-a)%8==1 else -1)
        out[name]=(n,turning)
    return out
mf,of=sys.argv[1],sys.argv[2]
chans,prio,net,copy,row=model(mf); O=obs(of)
ok=bad=skip=0; mism=[]
for c,lines in sorted(chans.items()):
    for a,b in zip(lines,lines[1:]):
        if a not in O or b not in O or O[a][1] or O[b][1]: skip+=1; continue
        pr=net[b]-net[a]; ob=O[b][0]-O[a][0]
        if pr==ob: ok+=1
        else:
            bad+=1; fl=[x for x,f in (("prio",prio[c]),("doubled",a in copy or b in copy),("row",a in row or b in row)) if f]
            mism.append(f"ch{c} {a}/{b} pred {pr:+d} obs {ob:+d} {','.join(fl)}")
print(f"matched {ok} mismatched {bad} skipped {skip}")
for m in mism: print("  "+m)
