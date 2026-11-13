# HH1: unrelated pieces may share a tile only as Upper|Lower or Left|Right (no X|Y diamond)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes. Run `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-hh1 <head>` with the head given in your launch prompt. Builds: `build` (Debug) and `build-rel` (Release). Ignore docs/junction-tool/wip/. Never use git stash.
- Review finding F5 (docs/junction-tool/REVIEW-opus-since-move.md):
  - The occupancy family in placement/junction_placement.cpp (`PlaceTrackGraph`, occupancy phase; in the old file it was around T:2272-2281) makes two unrelated pieces on one tile illegal only if both reach a common tile edge.
  - That admits X|Y, an at-grade diamond crossing with no node, which D182 and D217 forbid ("never share a tile" except the parallel diagonal pairs).
  - Render merges the bits silently (placement/junction_render.cpp).

## Read first
src/junction/CLAUDE.md, placement/CLAUDE.md, src/tests/junction/CLAUDE.md, CODINGSTYLE.md (naming and doc comments), review F5, and the occupancy check in junction_placement.cpp (find it by the common-edge test).

## Tests (red first)
1. New test in src/tests/junction/junction_track_geometry.cpp (a new file, added to src/tests/junction/CMakeLists.txt), with tags `[junction][geometry]`. It is named "unrelated pieces share a tile only as a parallel diagonal pair" and asserts `JunctionTracksMayShareTile`:
   - true for (TRACK_BIT_UPPER, TRACK_BIT_LOWER) and (TRACK_BIT_LEFT, TRACK_BIT_RIGHT), in both argument orders;
   - false for (TRACK_BIT_X, TRACK_BIT_Y), (TRACK_BIT_X, TRACK_BIT_UPPER), (TRACK_BIT_UPPER, TRACK_BIT_LEFT) and (TRACK_BIT_X, TRACK_BIT_X).

   Red step: first add the function declaration and a definition that implements TODAY's rule, exactly as the occupancy check computes it (true unless both reach a common tile edge). Run the test and record the failure on the X|Y case. Then change it to the new rule.
2. Add a `[geometry]` row to docs/junction-tool/TESTS.md with a 1 s budget, following the existing rows.

## Changes
- `bool JunctionTracksMayShareTile(TrackBits a, TrackBits b)` in src/junction/junction_track_geometry.{h,cpp}, with a doc comment that cites D182 and D217. It returns true only when `(a | b)` is exactly TRACK_BIT_HORZ or TRACK_BIT_VERT and `(a & b) == TRACK_BIT_NONE`.
- The occupancy check in junction_placement.cpp calls it in place of its common-edge test for UNRELATED pieces. Related pieces (graph-related, the `related` test) keep their current treatment.
- placement/CLAUDE.md: add or replace the occupancy fact with: "Two unrelated pieces share a tile only as Upper|Lower or Left|Right (D182, D217; JunctionTracksMayShareTile)."

## Do not
Change render, the related test or any other family. Do not change the hashes on purpose.

## Stop conditions
- The occupancy check is not a simple per-tile-pair test that one call can replace.
- `jt identity` hashes or planned counts change. In that case, report which test and which values before and after, and do NOT update expected values. The orchestrator then decides.
- Any [realterrainsweep] max goes over 100 ms.

## Gates
- Before the change, run `tools/junction/jt gate build-rel` and `tools/junction/jt identity build-rel` from the worktree root. Expect 489/3041662 for the gate and 4/255 for identity.
- After the change:
  - Debug and Release build, with no new warnings.
  - gate passes, with 1 case added.
  - identity is unchanged, or you stop as above.
  - `tools/junction/jt fast build '[geometry]'` and `tools/junction/jt fast build '[topology]'` pass.
- One commit: `Fix: Junction placement refuses an at-grade X|Y diamond between unrelated pieces`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
- References read.
- Commit hash and subject.
- Red evidence: the failure message.
- Functions added.
- Before and after counts.
- Stops.
- `git status --porcelain` and `git log -1`.

When you have reported, stop.
