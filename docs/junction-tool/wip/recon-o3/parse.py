import re, collections, sys
class D: pass
def load(path):
    d = D()
    d.pt = {}; d.itempt = {}; d.nodes = {}; d.edges = {}; d.ends = {}; d.prog = collections.defaultdict(list)
    d.rows = {}; d.events = collections.defaultdict(list); d.lines = collections.defaultdict(dict); d.repl = collections.defaultdict(dict)
    d.neigh = []; d.evpair = []; d.unders = []; d.bridges = []; d.elems = collections.defaultdict(list)
    d.bends = {}; d.clashes = []; d.occ = []; d.itemocc = []; d.cpieces = []; d.fanpairs = []; d.fpieces = []; d.formgroups = []
    d.portrow = []; d.itemkind = {}
    for ln in open(path):
        ln = ln.rstrip('\n'); f = ln.split()
        if not f: continue
        k = f[0]
        if k == 'PT': d.pt[int(f[1])] = (int(f[2]), int(f[3]))
        elif k == 'ITEMPT': d.itempt[int(f[1])] = int(f[2]); d.itemkind[int(f[1])] = (int(f[4]), int(f[6]))
        elif k == 'NODE': d.nodes[int(f[1])] = dict(split=int(f[3]), slot=int(f[5]), level=int(f[7]))
        elif k == 'EDGE':
            e = int(f[1]); d.edges[e] = dict(kind=int(f[3]), frm=f[5], to=f[7], paths=[int(x) for x in f[9:]])
        elif k == 'ENDS': d.ends[int(f[1])] = ln
        elif k == 'PROG':
            e, c, t = int(f[1]), int(f[2]), int(f[3])
            d.prog[(e, c)].append(dict(tok=t, kind=int(f[5]), hand=int(f[7]), index=int(f[9]), copy=int(f[11]), role=int(f[13])))
        elif k == 'ROW': d.rows[int(f[1].rstrip(':'))] = f[2:]
        elif k == 'EVENT': d.events[int(f[1])].append(dict(i=int(f[2]), node=f[3] == 'node', a=int(f[4]), b=int(f[5]), layer=int(f[7])))
        elif k == 'LINE': d.lines[int(f[1])][int(f[2].rstrip(':'))] = [int(x) for x in f[3:]]
        elif k == 'REPL':
            i = f.index('inserted'); d.repl[int(f[1])][int(f[2])] = ([int(x) for x in f[4:i]], [int(x) for x in f[i+1:]])
        elif k == 'NEIGH': d.neigh.append((int(f[1]), int(f[2]), int(f[3]), int(f[5])))
        elif k == 'EVPAIR': d.evpair.append((int(f[1]), int(f[2]), int(f[3])))
        elif k == 'UNDER': d.unders.append(ln)
        elif k == 'BRIDGE': d.bridges.append(ln)
        elif k == 'ELEM':
            d.elems[(int(f[2]), int(f[4]))].append(dict(k=f[6], first=int(f[8]), last=int(f[10]), hin=int(f[12]), hout=int(f[14]), bend=int(f[16])))
        elif k == 'BEND':
            d.bends[int(f[2])] = dict(e=int(f[4]), copy=int(f[6]), tok=int(f[8]), kind=f[10], src=f[12], hand=int(f[14]), hin=int(f[16]), first=int(f[18]), last=int(f[20]), m=int(f[22]), floor=int(f[24]), group=int(f[26]))
        elif k == 'FORMGROUP': d.formgroups.append((int(f[2]), int(f[4]), int(f[6])))
        elif k == 'CLASH':
            a, b = ln.split(' | ', 1)[0], None
            head, rest = ln.split(' | ', 1)
            # rest: "y=Y occA | occB"  (occ itself has no ' | ')
            ya = rest.split(' | ')
            y = int(ya[0].partition(' ')[0][2:]); occa = ya[0].partition(' ')[2]; occb = ya[1]
            hf = head.split()
            d.clashes.append(dict(i=int(hf[1]), gap=int(hf[3]), gaprev=int(hf[5]), tie=int(hf[7]), y=y, a=occ(occa), b=occ(occb)))
        elif k == 'OCC':
            y = int(f[1][2:]); o = occ(ln.split(' ',2)[2]); o['y'] = y; d.occ.append(o)
        elif k == 'ITEMOCC':
            m = re.match(r'ITEMOCC (\d+) kind (.*) edge (-?\d+) copy (-?\d+) node (-?\d+) west (-?\d+) east (-?\d+) rank (-?\d+) piece (-?\d+) bits (\d+)', ln)
            g = m.groups(); d.itemocc.append(dict(item=int(g[0]), kind=g[1], edge=int(g[2]), copy=int(g[3]), node=int(g[4]), west=int(g[5]), east=int(g[6]), rank=int(g[7]), piece=int(g[8]), bits=int(g[9])))
        elif k == 'CPIECE': d.cpieces.append(dict(i=int(f[1]), edge=int(f[3]), item=int(f[5]), bend=int(f[7]), seg=int(f[9]), chosen=int(f[11])))
        elif k == 'FANPAIR': d.fanpairs.append(dict(i=int(f[1]), west=int(f[3]), east=int(f[5]), eastside=int(f[7]), south=int(f[9])))
        elif k == 'FPIECE': d.fpieces.append(dict(i=int(f[1]), pair=int(f[3]), end=int(f[5]), edge=int(f[7]), kind=f[9], chosen=int(f[11])))
        elif k == 'PORTROW': d.portrow = [int(x) for x in f[1:f.index('offset')]]; d.portoffset = int(f[-1])
    return d
def occ(s):
    p = s.split(',')
    edge, copy, node, dbg, kind, L, R, bits, fp = int(p[0]), int(p[1]), int(p[2]), p[3], p[4], int(p[5]), int(p[6]), int(p[7]), int(p[8])
    return dict(edge=edge, copy=copy, node=node, dbg=dbg, kind=kind, L=L, R=R, bits=bits, fp=fp)
