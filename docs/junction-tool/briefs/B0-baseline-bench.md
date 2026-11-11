# B0: baseline hover timings of today's generator on the Buborough save (D183 §9)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any file or mechanism the brief does not name. If something is not covered, stop and report. Keep your context small: send command output to files and report only what matters.

## Why
Before the node model replaces projection we need a before-picture: GenerateJunctionNear (the hover search) at a few hundred random spots on a real large map. Nothing in the source changes.

## Setup
- Binary: `/home/pete/OpenTTD/.claude/play/build-play/openttd` (the engineer's play build; do not rebuild it, do not change its worktree).
- Rig: `tools/ottd/ottd` (read `tools/ottd/README.md` once: start --save, company --id 0, rcon, stop). Use your own run dir and ports: `OTTD_RIG_DIR=/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/b0rig/ OTTD_RIG_PORT=3991 OTTD_RIG_GAME_PORT=3993 OTTD_RIG_BIN=<binary>`. Never touch ~/.config/openttd.
- Save: `~/.local/share/openttd/save/Buborough Bay Transport, 2243-02-19.sav`. If it fails to load (missing NewGRFs etc.), stop and report the error.
- As soon as the server is up, print the line `JOIN: <binary> -n 127.0.0.1:3993` in your report-in-progress file and in your final report (the engineer may spectate).

## Work
1. New script `tools/junction/bench_anchors.py` (Python 3 stdlib, run with `python3 -I`): args `--n` (default 200), `--seed` (default 1), `--out` (JSONL path). It reads the map size via rcon (or takes `--size 512x1024`), draws N tiles uniformly with the seed, rotation 0-3 and mirror 0/1 uniformly, and for each calls, through `tools/ottd/ottd rcon`, `junction_export <tile> <rot> <mirror> <tmp json path> L_R L_R 4 1 train=14194,1090000,561,25,196 prio=14` (the test spec's tl and coverage; near 1 = GenerateJunctionNear). It records per call: tile x,y, rot, mirror, the console reply's first line (ok / error / failed and the error name), the timing the reply prints (if any), and the client-side wall time in ms. Read `junction_export`'s help in src/console_cmds.cpp ~320-330 for the exact syntax first; if the reply has no timing field, use wall time only and say so.
2. Smoke run with `--n 5`; then the full run `--n 200` in the background, writing to `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/b0/anchors.jsonl`.
3. Summary file `docs/junction-tool/BENCH-B0.md` (at most ~60 lines): binary and head (`git -C .claude/play log -1 --oneline`), save, parameters, counts per outcome, wall-time p50/p90/p99/max overall and per outcome, the share over 100 ms, the 10 slowest anchors (tile, rot, outcome, ms), and how long the whole run took. Note that another agent may have been building during the run (timings may be inflated 20-50 %).
4. Stop the rig.

## Do not
No source changes, no builds, no commits other than one at the end: `Doc: Junction baseline hover benchmark on the Buborough save` adding the script and BENCH-B0.md, on the current branch of `/home/pete/OpenTTD` (feature/junction-tool), ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Commit only those two files (`git add` them by path). No `pkill`, broad `kill`, bare `git stash`; stop the server only with `tools/ottd/ottd stop` under your env.

## Report (short)
The JOIN line; outcome counts; p50/p90/p99/max; share over 100 ms; slowest 3; total run time; commit hash. When you have reported, stop.
