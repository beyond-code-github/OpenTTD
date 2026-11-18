import sys, re, collections
sys.path.insert(0,'.')
import parse
def klass(o):
    dbg=o['dbg']
    if dbg=='node': return 'node'
    if dbg=='prio' or (dbg=='' and o['kind']=='prio'): return 'prio'
    if dbg.startswith('row'): return 'row:'+dbg.split('/')[1]+('('+o['kind'].replace(' ','_')+')')
    m=re.match(r'(\w+)#',dbg)
    return 'band:'+m.group(1)
def isband(o): return klass(o).startswith('band:')
def band_of_edge_sets(d):
    s=collections.defaultdict(set)
    for b,ls in d.lines.items():
        for i,l in ls.items():
            for e in l: s[b].add(e)
    return s
def row_ys(d):
    # item rows' y
    return {i:d.pt[p][1] for i,p in d.itempt.items()}
def run(name):
    d=parse.load(f'dump-{name}.txt')
    iy=row_ys(d); iys=set(iy.values())
    items_by_y=collections.defaultdict(list)
    for i,y in iy.items(): items_by_y[y].append(i)
    itemset=collections.defaultdict(set)
    chosen={c['i']:c['chosen'] for c in d.cpieces}
    for oc in d.itemocc:
        if oc['piece']>=0 and not chosen[oc['piece']]: continue
        itemset[oc['item']].add((oc['edge'],oc['copy'],oc['node'],oc['west'],oc['east']))
    def in_item(o,y):
        return any((o['edge'],o['copy'],o['node'],o['L'],o['R']) in itemset[i] for i in items_by_y.get(y,[]))
    # fan pair end rows
    fanrows={}
    for p in d.fanpairs:
        rows=set()
        for e in (p['west'],p['east']):
            for el in d.elems[(e,0)]:
                if el['k']=='Fan':
                    rows.add(d.pt[el['first']][1]); rows.add(d.pt[el['last']][1])
        fanrows[p['i']]=(p['west'],p['east'],rows)
    # also port tile rows of fan edges
    neighbours=set((w,e) for b,w,e,x in d.neigh)
    nb_unord=set(frozenset((w,e)) for b,w,e,x in d.neigh)
    res=[]
    for c in d.clashes:
        a,b,y=c['a'],c['b'],c['y']
        ka,kb=klass(a),klass(b)
        onitem = y in iys
        both_item = onitem and in_item(a,y) and in_item(b,y)
        o2 = False
        for pi,(w,e,rows) in fanrows.items():
            if {a['edge'],b['edge']}=={w,e} and y in rows: o2=True
        # O2 pairs are band? fans west/east are row edges' port fans; check edges
        res.append(dict(c=c,ka=ka,kb=kb,onitem=onitem,both_item=both_item,o2=o2,bb=isband(a) and isband(b) and a['edge']!=b['edge'],
            nbr=(frozenset((a['edge'],b['edge'])) in nb_unord)))
    return d,res
if __name__=='__main__':
    for name in ['T','LLRR_LR','LR_LLRR']:
        d,res=run(name)
        print('==',name,'clashes',len(res))
        cnt=collections.Counter()
        for r in res:
            cat = 'item-covered' if r['both_item'] else ('item-row, not both in item_rows' if r['onitem'] else ('O2-implied' if r['o2'] else 'non-item row'))
            cnt[cat]+=1
        print(dict(cnt))
        non=[r for r in res if not r['both_item']]
        print('not-covered by O1:',len(non))
        k=collections.Counter(tuple(sorted((r['ka'].split('(')[0],r['kb'].split('(')[0]))) for r in non)
        for kk,v in sorted(k.items(),key=lambda x:-x[1]): print('  ',v,kk)
