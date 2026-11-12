# R7b: the Shift edit, redone: Shift item adjacent to the edge's row, parent's cut and choices (REVIEW-opus-since-terrain-placement Q3, top change 2; R7 brief)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-r7b <HEAD>` (given at launch). The branch wt-r7 holds the old WIP 52e60a4224: read it (`git -C /home/pete/OpenTTD/.claude/nodes show 52e60a4224`) and reuse what this brief keeps; never merge or check it out. Never use git stash. Another agent works in /home/pete/OpenTTD/.claude/nodes2: never touch it. Use absolute paths.
- Read first: docs/junction-tool/briefs/R7-shift-edit.md (the edit, types, tests, refusals and the Note are all still in force except where this brief overrides them); docs/junction-tool/REVIEW-opus-since-terrain-placement.md Q3 (the R7 failure) and Q2 "Shift: candidate or structural?"; CODINGSTYLE.md (C++ part).
- Since R7: L1 (e4e5b75913) made occupancy legality graph-based on the settled placement (no whitelist; empty runs are occupants); ES2 (407604f7f8) added the "box" bound and the side-0 clearance contradiction; the `[topologysweep]` edit sweep (every placement edit fails with a reason or is checker-clean) is green.

## Overrides of R7 (from the review)
1. **Adjacent item.** The Shift item is inserted in the stack immediately next to the edge's own row item on the Shift's side (a row gap at that boundary): every item beyond it on that side moves one row out, and their fans/S-bends grow through the existing relations. "Beyond the row's side tracks" (WIP W:4120-4129) is wrong. The Shift S-bends span exactly one row (from the edge's row to the Shift item's row); no `ge(1, ..., 1)` relaxation of the S-bend's row span (WIP W:2365).
2. **Parent's cut and choices.** ApplyJunctionShifts does not call CutJunctionTrackBridges and does not re-derive m: it keeps the parent seed's cut, `placement.choices` (m, corner, port_offset) and maps them to the edited programs (existing tokens keep their values; the two new Shift tokens get 0 in m/corner, being horizontal S-bends sized by rows); it places with the fixed-choices overload. If a fixed-choices placement cannot take the new stack item without re-deriving something, STOP and report what.
3. **Refusal on prio coverage** is the coverage stretch (within even_prio + 1 of a joiner merge, as R7 says), not per row (WIP W:~4108).
4. **Doubled rows.** On a doubled row the Shift's side may not cross the row's other copy or lane row: refuse with `why` ("Shift would cross <edge>") when the adjacent position on that side is held by an item of the same row's other copy/lane; the other side may still be eligible.

## Tests (record each red; R7's three tests, with these changes)
1. Outer `[topoproj]` R7 test 1 as written (house mid-row on LL_RR builds by a Shift; report the side used and both sides' outcomes).
2. `[topology]` "topology: a Shift on flat ground": every eligible row edge of T and LL_RR, both sides: each is either refused with a reason (R7 refusals and override 4) or placed AND checker-clean: no WARN, no "clean >= 1 of 2" (WIP WT:2885-2890 is wrong). Report counts eligible/refused/clean per topology.
3. `[topology]` R7 test 3 (refusals), plus override 4's refusal.
4. ApplyJunctionShifts + placement Release time per edit (mean/max over test 2); report it (no limit; the review estimated 60-70 ms for the WIP because of the re-cut).
- Every existing test and sweep green with identical numbers (diff build-rel outputs of `[topologysweep]`, `[topologystretch]`, `[placeoracle]`, `[realterrainsweep]`, `[topoprojsweep]` against the launch HEAD's, both captured to files); circuit byte-identical.

## Stop conditions
- Any Shift on flat ground places but is not checker-clean: report the edge, side, rule, tile and the occupants of the S-bends' rows over their columns (the review's diagnostic); do not relax anything.
- Any existing number changes; anything not covered.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `'[topoproj]' 5`; the sweeps above; `tools/junction/jt gate build-rel` (report the count and time).
- One commit: `Add: Junction Shift edit: an S-bend pair on a row edge with its own adjacent stack item`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus test 1's house, side, box before/after, test 2's counts and test 4's times. When you have reported, stop.
