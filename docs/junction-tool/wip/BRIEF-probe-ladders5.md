# Probe: ladders 5: stretches within a run, bridge copies as walls with deck middles exempt

Throwaway probe: nothing committed. Time box: 60 minutes; at the box, report what you have. Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only `openttd_test` in `build-rel`, never `openttd`. Outputs under `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/ladders5/`; code read with offset/limit; command output to files. If anything does not fit the code as described, stop and report; do not invent another rule.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, clean at `{HEAD}`, Release `build-rel`. Apply `/home/pete/OpenTTD/docs/junction-tool/wip/probe-ladders3.patch` then `wip/probe-ladders4.patch` (PLANAR_LADDER=3). Read `wip/PROBE-ladders4.md` and `wip/PROBE-ladders2.md` (both short).
- Two gaps remain: (1) T3 o1t2f1s0/s1 first pass: AF3#1/#2 vs D3#0/#1. D3#0 and AF3#0 lie in one-wall box face 15 (one run: skipped today); a single run can fold so that two of its stretches face each other. (2) Area pass on T2/T3 o1t{0..3}f1s1: A vs B1" (copy strands are not walls). Probe 2 put copies in walls but treated whole copies as ground, which over-constrained (+12 to +186): a deck middle may pass over ground track.

## Changes
1. **Stretches within a run:** split every run into stretches (maximal pieces of one line, in walk order). For every ordered pair of stretches (X, Y) in the same run that are not consecutive in walk order (cyclically, for a one-run face), add rows exactly as run-vs-run does: X's end points against Y's facing segment, same nearest-facing rule on the rough solve, same off-wall drop, same `RungSpacing`, target side from Y's run direction. One-run faces are no longer skipped (they get only these rows). Run-vs-run rows unchanged. Count rows added by this per hub.
2. **Copies as walls, deck middles exempt:** a copy strand (from `M.copies`) joins the walls exactly as in probe 2 (side test from the rough solve), but read first how the lazy loop / `JunctionTracksMayShareTile` lets a deck middle share a tile with ground track, and use exactly that marking: a copy segment that is a deck middle is not a facing segment for ground points and its end points get no rows against ground segments. Ramp/head segments and deck-vs-deck pairs get rows as normal. If the code has no per-segment deck-middle marking you can read, stop and report what it has.
3. Everything else unchanged (flipped face map, rows from the rough solve, lazy loop as the safety net, up to 3 re-read passes).

## Runs
- First T3 o1t2f1s1 and T2 o1t0f1s1 (the two gap cases); then full T2 and T3 sets and LL² on the 201 tags in `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/ll2-tags.txt` (reuse probe 4's harness/parse.py from scratchpad `ladders4/`).
- Per hub, same table as probe 4 (solved rough/ladder, first-pass r0 and variants at 0, area-pass r0, area b/s/w with every worse delta, ladder ms med/max), rows violated on the rough solve (must be 0; list any), rows from change 1 and change 2 separately, every surviving clash pair.

## Report (findings under 35 lines in `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-ladders5.md`)
Then `git diff > /home/pete/OpenTTD/docs/junction-tool/wip/probe-ladders5.patch` (the full diff on HEAD), `git checkout -- .`, `git status --porcelain` empty, rebuild `openttd_test`. Report in under 12 lines. When you have reported, stop.
