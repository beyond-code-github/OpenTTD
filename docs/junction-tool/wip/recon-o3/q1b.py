import sys,collections
sys.path.insert(0,'.')
import q1
def short(k):
    if k.startswith('band:'): return 'band-'+k[5:]
    if k.startswith('row:'): return 'row-'+k[4:].split('(')[0]
    return k
def run(name):
    d,res=q1.run(name)
    cats=collections.defaultdict(list)
    for r in res:
        if r['both_item']: cat='A O1-covered (item row, both in item_rows)'
        elif r['o2']: cat='B O2 end row'
        elif r['onitem']: cat='C item-row y, not both item-row occupants'
        else: cat='D non-item row'
        cats[cat].append(r)
    print('=====',name,'clashes',len(res),'occupants',len(d.occ))
    for cat in sorted(cats):
        rs=cats[cat]
        bb=sum(r['bb'] for r in rs)
        print(' ',cat,len(rs),'| band-edge vs band-edge (2 edges):',bb)
        if cat[0] in 'CD':
            k=collections.Counter(' / '.join(sorted((short(r['ka']),short(r['kb'])))) for r in rs)
            for kk,v in sorted(k.items(),key=lambda x:-x[1]): print('      ',v,kk)
    # C breakdown: which side lacks item membership
    return d,res
for n in ['T','LLRR_LR','LR_LLRR']: run(n)
