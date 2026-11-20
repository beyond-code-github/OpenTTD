import importlib.util as u,json
s=u.spec_from_file_location("core","/mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad/psg/anatomy/psg197/trace/core.py");c=u.module_from_spec(s);s.loader.exec_module(c)
N=c.build()
json.dump(sorted('%d,%d,%d'%(p[0][0],p[0][1],p[1]) for p in N.core),open('x1core.json','w'))
