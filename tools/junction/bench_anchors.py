#!/usr/bin/env python3
"""Baseline hover benchmark: junction_export (near 1) at random tiles through the ottd rig.
Run with python3 -I; the rig env (OTTD_RIG_*) must be set and the server running."""
import argparse, json, os, random, subprocess, sys, time, tempfile

OTTD = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "ottd", "ottd")
TRAIN = "train=14194,1090000,561,25,196 prio=14"


def rcon(cmd):
    t0 = time.perf_counter()
    r = subprocess.run([sys.executable, "-I", OTTD, "rcon", cmd], capture_output=True, text=True)
    ms = (time.perf_counter() - t0) * 1000
    return (r.stdout.splitlines() or [r.stderr.strip()])[0], ms


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--n", type=int, default=200)
    p.add_argument("--seed", type=int, default=1)
    p.add_argument("--out", required=True)
    p.add_argument("--size", required=True, help="WxH of the loaded map")
    a = p.parse_args()
    w, h = map(int, a.size.lower().split("x"))
    rng = random.Random(a.seed)
    tmp = os.path.join(tempfile.gettempdir(), "bench_anchors_%d.json" % os.getpid())
    echo = sorted(rcon("echo x")[1] for _ in range(20))
    sys.stderr.write("rig overhead median %.1f ms\n" % ((echo[9] + echo[10]) / 2))
    t_all = time.perf_counter()
    with open(a.out, "w") as f:
        for _ in range(a.n):
            x, y = rng.randrange(w), rng.randrange(h)
            rot, mir = rng.randrange(4), rng.randrange(2)
            line, ms = rcon("junction_export %d %d %d %s L_R L_R 4 1 %s" % (y * w + x, rot, mir, tmp, TRAIN))
            f.write(json.dumps({"x": x, "y": y, "rot": rot, "mirror": mir, "reply": line, "ms": round(ms, 1)}) + "\n")
            f.flush()
    sys.stderr.write("total %.1f s\n" % (time.perf_counter() - t_all))
    json.dump({"rig_overhead_median_ms": (echo[9] + echo[10]) / 2}, open(a.out + ".meta", "w"))


main()
