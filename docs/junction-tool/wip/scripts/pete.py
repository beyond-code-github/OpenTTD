import json,sys
# usage: pete.py export.json solver_coloured.json ox oy out.json : export -> layout at origin (ox,oy); kept tiles take solver colour, changed tiles Railway(0)
e=json.load(open(sys.argv[1])); sol=json.load(open(sys.argv[2])); ox,oy=int(sys.argv[3]),int(sys.argv[4])
S={(t['x']+ox,t['y']+oy):t for t in sol['tracks']}
tracks=[];bridges=[];seen=set()
for t in e['tiles']:
    x,y=t['x'],t['y']
    if t['tile_type']=='rail':
        s=S.get((x,y)); rt=s['rail_type'] if s and set(s['bits'])==set(t['tracks']) else 0
        tracks.append({'x':x-ox,'y':y-oy,'bits':t['tracks'],'rail_type':rt})
    elif t['kind']=='bridge':
        a=(x,y); b=(t['far_end']['x'],t['far_end']['y']); k=tuple(sorted([a,b]))
        if k in seen: continue
        seen.add(k); rt=0
        for sb in sol['bridges']:
            if {tuple(sb['start']),tuple(sb['end'])}=={(k[0][0]-ox,k[0][1]-oy),(k[1][0]-ox,k[1][1]-oy)}: rt=sb['rail_type']
        bridges.append({'start':[k[0][0]-ox,k[0][1]-oy],'end':[k[1][0]-ox,k[1][1]-oy],'rail_type':rt})
xs=[t['x'] for t in tracks];ys=[t['y'] for t in tracks]
json.dump({'size':[max(xs)+1,max(ys)+1],'tracks':tracks,'bridges':bridges,'signals':[]},open(sys.argv[5],'w'))
print(len(tracks),'tracks',len(bridges),'bridges',sum(1 for t in tracks if t['rail_type']==0),'changed')
