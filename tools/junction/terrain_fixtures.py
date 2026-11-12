#!/usr/bin/env python3
"""Dump terrain fixtures from generated maps (RT1): tools/junction/terrain/<map>-<n>.json.

Starts a dedicated server with the rig's helpers on its own ports and scratch run dir (never the engineer's rig on 4077/4079),
and for each map asks it for 16 rectangles of 80x40 tiles with the console command junction_terrain_dump.
Usage: terrain_fixtures.py [path of the openttd binary, default build-rel/openttd of this tree]
"""
import os, random, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(ROOT, "tools", "ottd"))

SCRATCH = os.environ.get("TERRAIN_SCRATCH", "/tmp/terrain-fixtures-rig")
os.makedirs(SCRATCH, exist_ok=True)
# The rig reads these at import; its own port pair 4177/4179, its own run dir and user data dir.
os.environ["OTTD_RIG_PORT"] = "4177"
os.environ["OTTD_RIG_GAME_PORT"] = "4179"
os.environ["OTTD_RIG_DIR"] = os.path.join(SCRATCH, "run") + "/"
os.environ["OTTD_RIG_BIN"] = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "build-rel", "openttd")
os.environ["XDG_DATA_HOME"] = os.path.join(SCRATCH, "xdg")

from ottd_core import admin as A, server as S

S.USERDATA = os.path.join(SCRATCH, "xdg", "openttd")
# The graphics set is read from the normal download directory (read only, through a link); everything else is scratch.
os.makedirs(S.USERDATA, exist_ok=True)
if not os.path.exists(os.path.join(S.USERDATA, "content_download")):
    os.symlink(os.path.expanduser("~/.local/share/openttd/content_download"), os.path.join(S.USERDATA, "content_download"))
# 256x256 maps, a landscape and terrain type per map, normal town count (the rig's config has 128x128 and one town).
S.CFG = S.CFG.replace("map_x = 7", "map_x = 8").replace("map_y = 7", "map_y = 8").replace("number_towns = %(towns)d", "number_towns = 2")

SIZE = 256
W, H = 80, 40
EDGE = 2
PER_MAP = 16
# name, seed, landscape, difficulty terrain_type (None: the rig's hilly settings)
MAPS = [
    ("temperate-hilly", 101, "temperate", None),
    ("temperate-mountainous", 202, "temperate", 3),
    ("arctic-hilly", 303, "arctic", None),
]


def configure(landscape, terrain_type):
    cfg = S.CFG.replace("[game_creation]\n", "[game_creation]\nlandscape = %s\n" % landscape)
    if terrain_type is not None:
        cfg = cfg.replace("terrain_type = 5", "terrain_type = %d" % terrain_type)
    S.CFG = cfg


def dump_map(name, seed, landscape, terrain_type):
    base_cfg = S.CFG
    configure(landscape, terrain_type)
    S.SEED = seed
    try:
        S.start(hilly=True)
    finally:
        S.CFG = base_cfg
    try:
        adm = A.Admin(port=S.PORT, password=S.PASSWORD)
        adm.rcon("start_ai %s" % S.AI_NAME)
        for _ in range(80):
            time.sleep(0.25)
            if any(line.startswith("#:") for line in adm.rcon("companies")):
                break
        rng = random.Random(seed)
        out_dir = os.path.join(HERE, "terrain")
        os.makedirs(out_dir, exist_ok=True)
        for n in range(PER_MAP):
            x = rng.randint(EDGE, SIZE - W - EDGE)
            y = rng.randint(EDGE, SIZE - H - EDGE)
            path = os.path.join(out_dir, "%s-%d.json" % (name, n))
            lines = adm.rcon("junction_terrain_dump %d %d %d %d %s" % (x, y, W, H, path))
            if not any("junction_terrain_dump ok" in line for line in lines):
                raise RuntimeError("dump %s failed: %s" % (path, lines))
        adm.close()
    finally:
        S.stop()


def main():
    for name, seed, landscape, terrain_type in MAPS:
        dump_map(name, seed, landscape, terrain_type)
        print("dumped", name)


if __name__ == "__main__":
    main()
