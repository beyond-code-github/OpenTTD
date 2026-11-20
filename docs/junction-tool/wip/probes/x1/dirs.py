"""X1 measurement 3: direction fixes. LOCAL=1 gives the save's own per-piece direction (anatomy local_dirs) vs the trace's build2 (signals only + KEEP lane dirs)."""
import os,importlib.util as u
os.environ['LOCAL']='1'
TR="/mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad/psg/anatomy/psg197/trace/net.py"
s=u.spec_from_file_location("net",TR);net=u.module_from_spec(s);s.loader.exec_module(net)
N=net.build2()
print('lanes raw (LOCAL=1 dir) vs KEEP:')
KEEP={('N',84):'out',('N',85):'out',('N',90):'in',('N',91):'in',('S',81):'in',('S',82):'in',('S',83):'in',('S',88):'out',('S',89):'out',('S',90):'out',('E',164):'out',('E',166):'out',('E',167):'out',('E',169):'in',('E',173):'in',('E',174):'in'}
seen=set()
for L in N.lanes:
    k=(L['side'],L['pos'])
    if k in KEEP:
        seen.add(k); print(' ',k,'raw',L['dir'],'KEEP',KEEP[k],'DIFF' if L['dir']!=KEEP[k] else '')
print('  KEEP lanes absent from raw lanes:',sorted(set(KEEP)-seen))
print('  raw lanes not in KEEP:',sorted((L['side'],L['pos'],L['dir']) for L in N.lanes if (L['side'],L['pos']) not in KEEP))
sig=0;dis=[];free=0;nol=0
for pc in N.pieces:
    e=N.ed(pc)
    ex=N.exits[pc]
    if pc in N.sig1:
        sig+=1
        if e is None: nol+=1
        elif e not in ex: dis.append(pc)
    else:
        free+=1
print('pieces',len(N.pieces),'signal-directed',sig,'free(unsignalled both ways)',free)
print('signal-directed pieces where the save-local direction is undefined',nol,'and where it contradicts the signal',len(dis))
for pc in dis[:20]: print('  ',pc[0],N.W.pname(pc),'local',N.ed(pc),'signal',N.exits[pc])
fr=[pc for pc in N.pieces if pc not in N.sig1]
und=sum(1 for pc in fr if N.ed(pc) is None);print('free pieces: local direction undefined',und,'defined',len(fr)-und)
bad=[pc for pc in fr if N.ed(pc) is not None]
print('free pieces given a direction by LOCAL (would restrict):',len(bad))
