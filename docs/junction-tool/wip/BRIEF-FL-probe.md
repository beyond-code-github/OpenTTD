# Brief FL (probe, throwaway): a straight core may climb after its bridges

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`. Run `git switch -c probe-fl probe-pd` (probe-pd is at {HEAD}; it has the per-line dump `PLANAR_LINEDUMP`). Commit only there. Finish with `git switch wt-tracks` and rebuild `openttd_test` in build-rel.
- Time box: 90 minutes. Single-variant runs `timeout -s KILL 30`; pv runs `timeout -s KILL 600`; end to end `timeout -s KILL 60`. One run at a time.
- S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`.
- Background: `/home/pete/OpenTTD/docs/junction-tool/wip/ANALYSIS-PD.md` (the engineer's hand edit of T3 o1t3f1s0: the EF lanes run low, split low, E crosses F low on short bridges and climbs to its port rows after the bridges; 40 tiles less track on the six E/F routes). The engineer: "If we can make the architecture support it, then yes these sound like cases where we are too rigid!"

## Read first
- `src/junction/planar/planar_components.cpp` on probe-pd: `Fray` (~990-1030: the prio in-lane rise `M.AddSeg(x, Lt, 2, std::nullopt, 1.0)`, the `fray gap` rows), `build_core` (~1440-1540: offsets, `Double`, the copy rejoin, `run`, `run offset`, `piece_tails`), and where cores are chosen (~1402).
- `src/junction/planar/planar_model.h`: `AddSeg` (its lo/hi meaning), `PieceCurves` and how curve rows treat a zero-length segment.

## Changes (each behind one letter of the environment variable `PLANAR_LATE`, e.g. `PLANAR_LATE=rc`; unset = today exactly)
- **`r` (rise may be zero):** in `Fray`, the prio in-lane rise `AddSeg(x, Lt, 2, ...)` gets lower bound 0 instead of 2.
- **`c` (core climbs late):** in `build_core`, when the core's channel is the straight leg of a prio fray (the fray whose in-lane rise is `Lt = Turn(h_fray, -1)`), after the `run` segments add per lane `AddSeg(l, Turn(h, -1), 0)` then `AddSeg(l, h, 0)`, with the diagonal lengths of all lanes equal (rows under `TagScope("late climb")`). `piece_tails` keeps `run`. If finding "the straight leg of a prio fray" for a core needs anything beyond the site data `build_core` already has, stop and report what is missing.
- **`g` (fray gap at least):** the `fray gap` rows become `Ge` (at least the gap) instead of `Eq`.
If the model refuses a zero-length diagonal between axis segments (curve rows, `AddPiece`), stop and report the refusal text.

## Measure
Variant run (T3 test `"planar components: T3 from its topology text (mapped) at the hand composition's area"`): `PLANAR_ONLY=o1t3f1s0 PLANAR_TIMES=1 <env> timeout -s KILL 30 build-rel/openttd_test "<T3 test>" -s > <S>/fl-<name>.txt 2>&1`: area, size, track, ms or failure, last `slack` line.
1. No env: 1796.25 81x46 track 1283.15.
2. `PLANAR_LATE` = `r`, `c`, `g`, `rc`, `rcg`: each.
3. For the best of 2 by track length (state which), end to end with the line dump: `PLANAR_LATE=<it> PLANAR_ONLY=o1t3f1s0 PLANAR_LINEDUMP=<S>/fl-lines.txt JUNCTION_COMPONENTS_TOPOLOGY=T3 JUNCTION_COMPONENTS_OUT=<S>/fl-T3.json timeout -s KILL 60 build-rel/openttd_test "planar: dump a mapped component layout" -s > <S>/fl-e2e-T3.txt 2>&1`: plan result, `MAPPED`, `VIOLATION` lines grouped by rule; and the same without the variant pin (best variant).
4. `pv run build-rel T3` and `pv run build-rel T2` with that `PLANAR_LATE` and without (`--out <S>/fl-pv-<hub>[-base].txt`): built/failed with reasons grouped, best variant and area, per variant areas side by side, max ms (the budget is 15 s per variant).
5. For the result of 3 (pinned), lengths of E1-3 and F1-3 and EF1-3 in tiles from `fl-lines.txt` (pieces per line), against the solver baseline in ANALYSIS-PD.md (E1 46, F1 53, E2 46, F2 49, E3 44, F3 45).

## Do not
- Change anything else (slot layer, paths, signal pass, checker, other composer rules).
- Touch ports 4077/4079 or start a game client. Use git stash. Run two heavy jobs at once.

## Commit and report
- Commit on probe-fl: `Probe: Junction late climb after a core's bridges, free prio fray rise (throwaway)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Write `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-FL.md` (do not commit): changes and measurements 1-5.
- Reply with the measurements, the commit hash, `git status --porcelain`.

When you have reported, stop.
