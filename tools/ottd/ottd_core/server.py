"""Server lifecycle: run dir, config files, start/stop of the openttd-spike dedicated server."""
import os, shutil, signal, socket, subprocess, time

RUNDIR = os.environ.get(
    "OTTD_RIG_DIR",
    "/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/ottd-rig/")
BINARY = os.environ.get("OTTD_RIG_BIN", "/home/pete/OpenTTD/build/openttd-spike")
PORT = 3977
PASSWORD = "rigpw"
GS_NAME = "JunctionTestGS"
AI_NAME = "IdleAI"
SEED = int(os.environ.get("OTTD_RIG_SEED", "3"))

TOOLS = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
USERDATA = os.path.expanduser("~/.local/share/openttd")

CFG = """[version]
ini_version = 8
[network]
server_admin_port = %(port)d
allow_insecure_admin_login = true
[difficulty]
terrain_type = 5
number_towns = %(towns)d
industry_density = 0
vehicle_breakdowns = 0
max_loan = 2000000000
quantity_sea_lakes = %(lakes)d
[game_creation]
map_x = 7
map_y = 7
custom_town_number = 1
custom_terrain_type = %(terrain)d
custom_sea_level = 1
water_borders = 0
water_border_presets = 0
tgen_smoothness = %(smooth)d
amount_of_rivers = 0
variety = %(variety)d
[economy]
larger_towns = 0
[ai]
ai_in_multiplayer = true
[game_scripts]
%(gs)s =
"""
SECRETS = "[network]\nadmin_password = %s\n"


def _p(name):
    return os.path.join(RUNDIR, name)


def map_size():
    """Map width in tiles, from the written config (map_x is a power of two)."""
    with open(_p("openttd.cfg")) as f:
        for line in f:
            if line.strip().startswith("map_x"):
                return 1 << int(line.split("=")[1])
    raise RuntimeError("no map_x in %s" % _p("openttd.cfg"))


def pid():
    try:
        with open(_p("server.pid")) as f:
            n = int(f.read().strip())
        os.kill(n, 0)
        return n
    except (OSError, ValueError):
        return None


def _port_open():
    try:
        socket.create_connection(("127.0.0.1", PORT), timeout=0.5).close()
        return True
    except OSError:
        return False


def _install(src, dst):
    if os.path.isdir(dst):
        shutil.rmtree(dst)
    shutil.copytree(src, dst, ignore=shutil.ignore_patterns("__pycache__"))


def start(save=None, scenario=None, timeout=60, hilly=False):
    if pid():
        raise RuntimeError("server already running (pid %d)" % pid())
    os.makedirs(RUNDIR, exist_ok=True)
    _install(os.path.join(TOOLS, "gs"), os.path.join(USERDATA, "game", GS_NAME))
    _install(os.path.join(TOOLS, "ai", AI_NAME), os.path.join(USERDATA, "ai", AI_NAME))
    with open(_p("openttd.cfg"), "w") as f:
        terrain = dict(terrain=3, variety=2, smooth=1, lakes=2, towns=4) if hilly else dict(terrain=1, variety=0, smooth=0, lakes=4, towns=4)
        f.write(CFG % dict(port=PORT, gs=GS_NAME, **terrain))
    with open(_p("secrets.cfg"), "w") as f:
        f.write(SECRETS % PASSWORD)
    args = [BINARY, "-D", "127.0.0.1", "-c", _p("openttd.cfg"), "-g"]
    blank = os.path.join(TOOLS, "scenarios", "blank.scn")
    if not save and not scenario and os.path.isfile(blank):
        scenario = blank
    if save:
        args.append(os.path.abspath(save))
    elif scenario:
        args.append(os.path.abspath(scenario))
    else:
        args += ["-G", str(SEED)]  # fixed map seed so test maps are reproducible
    log = open(_p("server.log"), "w")
    proc = subprocess.Popen(args, cwd=RUNDIR, stdout=log, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL,
                            start_new_session=True)
    with open(_p("server.pid"), "w") as f:
        f.write(str(proc.pid))
    deadline = time.time() + timeout
    while time.time() < deadline:
        if proc.poll() is not None:
            raise RuntimeError("server exited early (code %s), see %s" % (proc.returncode, _p("server.log")))
        if _port_open():
            time.sleep(1.5)  # let the game and GameScript start ticking
            return proc.pid
        time.sleep(0.3)
    raise RuntimeError("admin port did not open in %ds, see %s" % (timeout, _p("server.log")))


def stop(timeout=10):
    n = pid()
    if not n:
        return False
    os.kill(n, signal.SIGTERM)
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            os.kill(n, 0)
        except OSError:
            break
        time.sleep(0.2)
    else:
        os.kill(n, signal.SIGKILL)
    try:
        os.remove(_p("server.pid"))
    except OSError:
        pass
    return True
