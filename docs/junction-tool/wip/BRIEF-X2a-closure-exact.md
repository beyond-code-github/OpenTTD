# BRIEF X2a: does stretching still close once Slots' least exact answer is fixed? (probe, Sonnet)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch wt-slots, head {HEAD}. Release build dir /home/pete/OpenTTD/.claude/ng1/build-rel (persistent).
- Implements: wip/SPEC-slots.md §0.3, §4 L8 and §7 X2 (riskiest first, time-boxed), stage A: the exact quantities of Slots' least answer at decks, ports and cusps, without bundle (frame) rows. Stage B (frames as face strips) waits for this result. A probe: one hidden test case; no product code.

## Read first (once, with offset/limit)
- /home/pete/OpenTTD/docs/junction-tool/wip/SPEC-slots.md §0 and §2.
- /home/pete/OpenTTD/.claude/ng1/src/tests/junction/planar/planar_shape_layer.cpp: `ClosureLp` (~lines 261-400) and the sweep probe (last TEST_CASE).
- /home/pete/OpenTTD/.claude/ng1/src/junction/planar/planar_structure.h (`PlanarStructure`, `PlanarStructureVariant::side_gap`), planar_shape_layer.h, planar_regions.h (`PlanarTrackMap`; `expanded.ring`).
- /home/pete/OpenTTD/.claude/ng1/src/junction/planar/planar_model.h lines 40-80 (`STEP`, `Right`, `RungSpacing`: a lane on an axis is 2 units along `Right(h)`, on a diagonal 1 unit of the diagonal step).
- CODINGSTYLE.md (C++ part).

## Units
`ClosureLp` works in half-tile units (a piece of length L moves L * STEP[h]). One lane sideways is 2 * STEP[Right] on an axis heading and 1 * STEP[Right] on a diagonal heading. One tile along an axis deck is 2 * STEP[axis].

## The change
In src/tests/junction/planar/planar_shape_layer.cpp:
1. Give `ClosureLp` a second parameter `bool exact` (default false, so the existing callers are unchanged). With `exact` true it adds, after its existing rows:
   - **A1 cusps:** each cusp opening `w` is fixed to one lane: `w = 2` when the trunk heading h is axis (h even), `w = 1` when diagonal (tag `cusp <node>`).
   - **A2 decks:** for each structure s of `v.structure.structures` and each span: the separator span (the points `point[s.separator][EventIndex(map.events[s.separator], s.crossings[k]) + 1]` for k = 0..n-1) and, when `s.copy != UINT32_MAX`, the copy span (the points of the copy's events, `point[s.copy][i + 1]` for every i): consecutive points k-1, k differ by exactly one tile along the deck: `p_k - p_{k-1} = 2 * STEP[deck heading]` in x and y, where the deck heading is the deck track's piece at that event (`v.tracks[deck].pieces[v.tracks[deck].event_piece[index]]` mod 8) (tag `deck <i>`).
   - **A3 pins:** walk `map.expanded.ring` (port-line tracks clockwise from the north-west corner). For consecutive ring entries t1, t2 (no wrap) whose port ends are on the same box side: their port-end points differ along the side by exactly d, where d = 2 when both ends are in the same port (same `PlanarEnd::index`), else `2 * v.structure.side_gap[side]`; the sign follows the clockwise walk: N side +x, E side +y, S side -x, W side -y (tag `pin <side>`). A track's port end is its `from` end when `from.port`, else its `to` end; the point is `point[t].front()` or `.back()` accordingly.
   (Copies need no row: a copy leaves its separator parallel, so its offset is a cusp, A1.)
2. A new hidden TEST_CASE at the end of the file: `TEST_CASE("planar probe: closure with Slots' least exact answer on T1-T3 and LL2", "[junction][planar][planarshape][.probe]")`: for T1, T2, T3 (`PlanarTText(n)`) and LL2: `BuildPlanarShape(BuildPlanarStructure(topology, spec, 16))`; for each shaped variant run `ClosureLp(v, false)` and `ClosureLp(v, true)`. Print per hub one line `X2A <hub> shaped=<n> closes_free=<n> closes_exact=<n> lp_ms_max=<t>` and, for each variant whose exact LP fails, `X2A <hub> <tag> OPEN <why>` (the `why` already lists the conflicting row tags). No REQUIRE on the counts.

## Run
- Build: `cmake --build /home/pete/OpenTTD/.claude/ng1/build-rel --target openttd_test -j16 > /home/pete/OpenTTD/.claude/ng1/build-rel/x2a-build.log 2>&1`; grep for `error`.
- `cd /home/pete/OpenTTD/.claude/ng1/build-rel && timeout 900 ./openttd_test "planar probe: closure with Slots' least exact answer on T1-T3 and LL2" > x2a-run.txt 2>&1`; `grep X2A x2a-run.txt`.
- `/home/pete/OpenTTD/tools/junction/jt fast /home/pete/OpenTTD/.claude/ng1/build-rel '[planarshape]' 10`.

## Do not
- Change product code, the oracles, the other tests, or `ClosureLp`'s rows for `exact == false`.
- Add bundle or face rows (stage B), tolerances, or a REQUIRE on the counts; try to make an OPEN variant close.
- Run the wide gate or any other heavy job.

## Stop conditions
- `map.expanded.ring` is empty, or a ring entry's track has no port end: stop and report.
- A structure's span event is missing from its deck track's events.
- The run exceeds 900 s.

## Gates and commit
- `[planarshape]` fast green inside 10 s.
- One commit on wt-slots: `Add: Junction planar probe: closure with Slots' least exact answer (X2a)` ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report (short, this format)
- References read
- Commit: hash and subject
- The X2A lines verbatim (all; if more than 40 OPEN lines, the first 10 per hub and a count)
- `[planarshape]` result: cases, passed, time
- Stops, anything left undone
- State: `git status --porcelain` and `git log -1 --oneline`
When you have reported, stop.
