# Brief G3m (probe on probe-g3e): fork links in the slot layer pick each fork's straight leg (card F option 1)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (on `wt-tracks`). Run `git switch probe-g3e` (at the hash in your launch prompt; its head is G3l's `PLANAR_FORKMASK` commit). Commit only there. Build dir `build-rel`, target `openttd_test` only. Finish with `git switch wt-tracks` and rebuild `openttd_test` in build-rel.
- Time box: 90 minutes. One heavy job at a time. Each run under `timeout -s KILL 1800`.
- S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`.
- Read first (main worktree `/home/pete/OpenTTD/docs/junction-tool/`): STATUS.md DECISION CARD F; `wip/BRIEF-G3l-fork-mask-probe.md` and the G3l commit's diff (`git show <G3l hash>`: the fork numbering, the left/right leg order and the straight-leg mechanism you reuse); `src/junction/planar/planar_slots.cpp` lines 160-190 (the link loop) and the LinkStep struct above it.

## Why
Card F option (1): the slot layer's min-cost solve picks which leg of each fork runs straight. Today the link loop skips forks twice: a site with `sites[si].straight == UINT32_MAX`, and a node with `node_legs[n].straight == UINT32_MAX`. Each fork leg is in its own channel with a free offset `o[c]`, so charging both legs' steps alone costs 0. A row that holds the legs one slot apart makes one leg's step 0 and the other ±1, and the rest of the objective (the legs' later steps and links) decides which.

## Change 1: slot layer, behind `PLANAR_FORKLINKS=1` (read with `std::getenv`, no other parsing)
In the link loop of `BuildPlanarSlots`, for each split site (`Fray` or `LaneSplit`), including one whose `sites[si].straight == UINT32_MAX`, and for each node `n` of it that is a fork as G3l defines it (`node_legs[n].straight == UINT32_MAX`, exactly two legs):
- For each of the two legs `L`: a charged link exactly like the existing one (`up`, `dn` with cost 1; row `slot(L, ct, 0) - slot(in[0], cf, last(cf)) - up + dn = 0`; push a `LinkStep {in[0], L, up, dn, 0}`).
- One row: `slot(right, c_right, 0) - slot(left, c_left, 0) = 1`, with left and right the legs in G3l's order (left = the leg G2 rule 2 gives -1). If that order is not computable from the slot layer's inputs (`topology`, `channels`, `sites`), stop and report what it is computed from.
- Nodes that are not forks, and all sites when the variable is unset, keep today's behaviour unchanged.

## Change 2: `TrackComposer`, behind the same variable
For each fork (G3l's numbering), the straight leg is the leg whose `PlanarSlotLink` from the fork's in line has `step == 0`. Feed that choice into G3l's mechanism in place of the mask bit. If neither or both legs have step 0, stop and report the fork and both steps. `PLANAR_FORKMASK` still wins when both are set. The probe test prints one line per fork: index, node, in line, the straight leg, both link steps.

## Measure
1. `PLANAR_COMPOSER=tracks PLANAR_SLOTBEND=1 PLANAR_FORKLINKS=1 tools/junction/pv run build-rel LL2 --tags <S>/ll2-12.txt --out <S>/g3m-pv-LL2.txt`: ok / failed by reason, best (area, size, track, wasted, empty, ms), largest ms; beside `<S>/lo1-pv-LL2-after.txt` (MappedComposer, best 2162.25). For `o0t1f0s1`: the fork lines from the probe test, the mask they amount to, and how that mask did in `<S>/g3l-masks.txt`.
2. T2 and T3 with the same variables into `<S>/g3m-pv-T2.txt`, `-T3.txt`: list any variant that differs from `<S>/g3h-pv-T2.txt` / `-T3.txt` (they have no forks; expect none).
3. The slot layer's own tests: `tools/junction/jt fast build-rel '[planar]'` with and without `PLANAR_FORKLINKS=1` (count, failures).

## Do not
- Add a variant bit, an optional piece, a cost other than the two charged links, or any row besides the one spacing row. Change links of non-fork nodes, the map, G2's rules, the solve outside the slot layer, or `MappedComposer`. Touch ports 4077/4079 or start a game client. Use git stash.

## Commit and report
- One commit on probe-g3e: `Probe: Junction fork links in the slot layer pick the straight leg (PLANAR_FORKLINKS)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Report: references read; commit hash; the fork lines for o0t1f0s1; measures 1-3; any stop; `git status --porcelain`; `git log -1`. Finish with `git switch wt-tracks` and the rebuild. When you have reported, stop.
