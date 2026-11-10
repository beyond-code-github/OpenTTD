# NW1c: finish NW1 on top of CS3

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/nw1`, branch `wt-nw1`, with a WIP commit (358d0f46b8) of NW1 (`docs/junction-tool/briefs/NW1-shift-back-window.md`, read it whole): the shift-back retry over d = 0..tl, and `route_line`'s approach/continuation lookups chained across rasters that meet at a node, precomputed per request. Its `build` and `build-rel` exist.
- First merge `feature/junction-tool` (head given at launch; CS3 merged: CS1/CS2's AdvanceState seeding removed, boundary curves judged by rule 3 through `boundary_ok`/`JunctionMinCurveSpeed`) into `wt-nw1`; resolve conflicts keeping both: the chained lookups now feed `approach`/`continuation`, which CS3's `BoundaryOk` uses.
- Why it stopped before: chained CS2 (router pair rule) refused the flat T's N->B goals that rule 3 accepts; CS3 fixed the rule. Tests only through jt with bracketed tags.

## Steps
1. Rebuild; run `[junctionmap]` (Debug) and `[projection]`, `[projectionedges]` (Release). Expected: the flat identity and "the T map reports the hub offset" pass again (N->B routes at d = 0 on flat ground), and the times are back near TESTS.md's. If not, stop and report which test and the trace line of the edge that changed.
2. Run "a height change inside the junction's W end moves the shift-back": expected clean at the flat height. If unclean, stop and report the violation and the edges as before.
3. Finish NW1's brief: jt runs (all tags in its Gates plus `[approach]`, `[continuation]`, `[curvespeed]`), golden trace unchanged, gate, and squash the WIP and your changes into one commit on top of the merge: `Change: Junction N->B's failure moves the shift-back west; curve state chains across edges`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Do not
As NW1's brief. No other change.

## Report (short)
Commit; test outcomes (steps 1 and 2) with times; jt lines; stops; `git status --porcelain`, `git log -3 --oneline`.

When you have reported, stop.
