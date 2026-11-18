import sys,re,collections
sys.path.insert(0,'.')
import parse,seg,q7
OVER,TUN=3,5
def run(name):
    h=seg.build(name); d=h.d
    rels=q7.load_y(name)
    clear=[r for r in rels if r['reason']=='bridge middle clearance']
    item_of_pt={p:i for i,p in d.itempt.items()}
    cnt=collections.Counter(); bad=[]; kinds=collections.Counter()
    for r in clear:
        # the relation has head pt on one side, line pt on the other
        v,u=r['v'],r['u']
        if u in item_of_pt: head,line,south=v,u,True
        elif v in item_of_pt: head,line,south=u,v,False
        else: cnt['line pt not an item pt']+=1; continue
        # head owner
        own=[(e,c,i) for (e,c),els in d.elems.items() for i,el in enumerate(els) if head in (el['first'],el['last']) and el['k'] in('Heads','HeadsPair')]
        if not own: cnt['head owner unknown']+=1; continue
        e,c,i=own[0]
        els=d.elems[(e,c)]
        k=[j for j,el in enumerate(els) if el['k']=='Heads' ]
        if els[i]['k']!='Heads': kinds['head of a doubled bridge (HeadsPair), edge %d'%e]+=1; cnt['head of doubled bridge']+=1; continue
        ordinal=k.index(i)
        overs=[t for t in d.prog[(e,0)] if t['kind'] in (OVER,TUN)]
        crossed=overs[ordinal]['index']
        band=h.bandcross.get(frozenset((e,crossed)))
        row=d.itemkind[item_of_pt[line]][1]
        if band is None: cnt['single bridge crossing not a band event']+=1; continue
        b=band[0]
        expect_south = b>row
        ok = expect_south==south
        cnt['side = band side (south iff band > row)' if ok else 'side contradicts band index']+=1
        if not ok: bad.append((r,e,b,row,south))
        kinds[(b,row)]+=1
    return cnt,kinds,bad,len(clear)
for name in ['T','LR_LLRR','LLRR_LR','LL2_100100000']:
    cnt,kinds,bad,n=run(name)
    print(name,n,dict(cnt))
    print('    (band,row) of relations',dict(kinds))
    for b in bad[:5]: print('   BAD',b)
