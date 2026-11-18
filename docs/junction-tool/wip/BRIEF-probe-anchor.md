# Probe: hard anchor (one port point pinned, box low edges free) in the mapped composer

Throwaway probe: nothing committed in ng1. Time box: 75 minutes; at the box, report what you have. Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only `openttd_test` in `build-rel`. One heavy job at a time. Outputs under `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/probe-an/`; read code with offset/limit; command output to files. If anything does not fit the code as described, stop and report; do not invent another rule. Do not start background work that outlives your report.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, clean at `95fe3d99c7`. Read `src/junction/planar/CLAUDE.md` "Component solver" (objective, `PLANAR_PULL`).

## Why (DESIGN-tracks 8.4: "a hard anchor (one port coordinate fixed) instead of only the north-west pull, to remove the hub's sliding symmetry")
In `MappedComposer` the box is [0, X] x [1, Y] (every point: `Ge(p[0], 0)`, `Ge(p[1], 1)`, `Ge(M.X - p[0], 0)`, `Ge(M.Y - p[1], 0)`, ~1735-1755) and port lines sit on its edges (~1680-1690: W at 0, E at X, S at Y, N at the low y edge). The outline area and track length do not change when the whole layout translates, so on a hub whose ports do not pin both axes (the T: nothing pins y but the box floor) only the tiny pull separates equal optima.

## Change (MappedComposer path only, behind env `PLANAR_ANCHOR=1`; unset = today exactly)
1. Two new model variables X0, Y0 (box low edges; add them as `M.X`/`M.Y` are made; if adding a variable needs a new Model member, add exactly `X0` and `Y0` beside `X`, `Y`).
2. Every mapped box row uses them: `p[0] >= X0`, `p[1] >= Y0 + 1`; port sides W `u[0] == X0`, N `u[1] == Y0 + 1` (read what N uses today and keep the same offset from the low edge); E and S unchanged.
3. Pin: the first port in topology order, its first line's port end point (the `u` the side rows use), fixed to (P0, P0) with P0 = 2 * the box bound in half-tiles the composer already uses (so the box fits on either side). Everything else is free to translate around it.
4. Every place that reads the box size or an absolute coordinate for output subtracts X0/Y0: width/height (`(X - X0)/2 + 1`, ~2367, ~2643), the box bound rows (`X - X0 <= bound`), the pull (~2165: weight on X - X0 and Y - Y0 instead of X and Y; the per-coordinate pull terms stay), and the route programs' tile coordinates (shift by X0, Y0 so the output is identical in form to today). List each site you change; if there are sites outside these families, stop and report them.

## Runs (sequential)
- `/home/pete/OpenTTD/tools/junction/pv run build-rel <hub>` for T2, T3 and LL² without and with the env (pv writes per-variant area or reason; read its file), plus the T2 and T3 mapped tests and the LL² mapped test (`-s`) for wall time and best, both ways.
- Per hub: best area/tag both ways; every variant whose area or reason changed; wall time (full test) and per-variant solve times (pv, or `PLANAR_TIMES=1` on the three best tags) both ways.
- Areas must be identical where both solve (translation changes nothing); report every difference.

## Report (findings under 30 lines in `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-anchor.md`, committed alone on feature/junction-tool in `/home/pete/OpenTTD`, message "Doc: Junction probe of a hard anchor", ending "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>")
Then in ng1: `git diff > /home/pete/OpenTTD/docs/junction-tool/wip/probe-anchor.patch` (commit with the findings), `git checkout -- .`, `git status --porcelain` empty, rebuild `openttd_test`. Report in under 12 lines. When you have reported, stop.
