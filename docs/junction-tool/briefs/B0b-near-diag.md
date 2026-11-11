# B0b: why the in-game hover search returns NoLayout (read-only diagnosis)

You execute this brief and nothing else. Diagnose only: no source changes are committed. If something is not covered, stop and report. Keep your context small: send command and test output to files and report only what matters.

## Facts measured by the orchestrator (play binary at 7110983abc, Release)
- Rig new flat 64x64 game (`tools/ottd/ottd start; tools/ottd/ottd company`), tile 32*64+32, rot 0, mirror 0:
  - `junction_export <tile> 0 0 <json> L_R L_R 4 0 train=14194,1090000,561,25,196 prio=14` -> `ok origin=5,15 size=41x17 snapshot_us=115 gen_us=956702`
  - same with near 1 -> `error NoLayout snapshot_us=100 gen_us=19073`
- Buborough save (512x1024): 238 random tiles, near 1 -> all NoLayout, gen_us ~18000 each; near 0 at tile 600*512+300 -> NoLayout gen_us ~38000.
- Unit tests build junctions near terrain fine (src/tests/junction_generator.cpp, junction_map.cpp).

## Questions, in order
1. Why does near 1 (GenerateJunctionNear, called from ConJunctionExport in src/console_cmds.cpp ~320-440) return NoLayout in ~19 ms on flat land where near 0 builds? Find the exact return path. Likely places: the snapshot margin (MakeJunctionTerrain(..., near ? 4 : 0)), GenerateJunctionNear's offsets and its use of the snapshot, the cancel flag, or a tier that only tries the library. Reproduce in a unit test if you can (a scratch test in your worktree, not committed): MakeJunctionTerrain is game-side, so build an equivalent JunctionTerrain with the same margin by hand.
2. Why does near 0 fail on Buborough at 38 ms at tile (300,600)? Report what the snapshot holds there (use the rig: e.g. `junction_export` with near 0 at a few tiles; read `GenerateJunction`'s log if one is reachable) and which check fails first. If the honest answer is "the tile is not buildable land", say how you know.
3. Where do the 957 ms of the flat near-0 success go (one sentence per big phase, from the existing timing fields in JunctionTMapResult / GenerateJunction if they exist; no new profiling).

## Setup
- Worktree: make your own detached worktree `git -C /home/pete/OpenTTD worktree add /home/pete/OpenTTD/.claude/b0b feature/junction-tool --detach` and build there (`cmake -S . -B build -DCMAKE_BUILD_TYPE=Release` then `cmake --build build --target openttd openttd_test -j20`, output to a file) if you need a binary or tests. Do not touch `.claude/play` or its build.
- Rig: `OTTD_RIG_DIR=/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/b0b-rig/ OTTD_RIG_PORT=3995 OTTD_RIG_GAME_PORT=3997 OTTD_RIG_BIN=<binary>`; read tools/ottd/README.md once. Save: `~/.local/share/openttd/save/Buborough Bay Transport, 2243-02-19.sav`. Never touch ~/.config/openttd. Stop the rig with `tools/ottd/ottd stop` under your env when done.
- Tests: run through `tools/junction/jt` (read TESTS.md once); never hand-typed tag filters over the whole suite.

## Output
`docs/junction-tool/DIAG-near.md` (at most ~50 lines): each question, the answer with file:line, the evidence, and what fix would follow (described, not made). Commit only that file on feature/junction-tool in /home/pete/OpenTTD: `Doc: Junction diagnosis of the in-game hover NoLayout`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Remove your worktree afterwards (`git worktree remove --force`).

## Do not
No source commits, no `pkill`/broad kill, no bare `git stash`.

## Report (short)
The three answers in one line each, the commit hash, `git status --porcelain` of /home/pete/OpenTTD. When you have reported, stop.
