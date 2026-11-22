# BRIEF: E5 contract unused flex before the integer final (measurement behind an option; D427 step 1)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: about 75 minutes in total. Each integer final has a 15 s cap. Run each measurement once. No rerun loops, no tuning.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (branch wt-e1), head HEAD_HASH, clean. Release build `/home/pete/OpenTTD/.claude/ng1/build-rel`.
- Context (D427, card docs/junction-tool/wip/CARD-E5-bundle-ends.md last section): the pipeline's LP rounds draw PSG 197 (slack 0) but its integer final finds nothing in 5 s off the grid or 15 s on the grid. 197 drew as an integer drawing in 6.4 s when V's S-bends were fixed bends with no optional flex (PAPER-notes C8). In the final today, flex inserts the commit held at 0 stay in the model as zero-length pieces and walls (about 28.5k keep-apart pairs instead of about 1.5k). This step removes them before the final and measures.

## 2. Read first (once each)
- `CODINGSTYLE.md`, C++ part.
- `src/junction/planar/planar_draw.cpp`: `DrawPlanarPipeline` from the commit stage to the final (`out.committed`, `last`, `last_input`, `final_options` ~line 2058, `dump_to`); how `curve_flex_from`/`curve_flex_above` commit or hold an insert; how `steps` (`PlanarShapeTrack::steps`) is drawn (an insert with at least its least length, flanks may be 0).
- `src/junction/planar/planar_shape_layer.h`: `PlanarShapeTrack` (`pieces`, `event_piece`, `flex`, `steps`), `RefinePlanarShapeCells`.
- `src/tests/junction/planar/planar_draw.cpp`: `ApplyStepsAsBends` (~2081), the same erase you will do, and the any-hub test ("planar draw: the simplest drawing on every hub", K3 and the `PLANAR_ANYHUB_GRID` hook).

## 3. Changes
### 3.1 `PlanarDrawPipelineOptions::final_contract` (production, off by default)
- `bool final_contract = false; ///< A measurement (D427): before the integer final, every flex insert the commit held at 0 is removed with its trailing flank (one straight piece remains), every committed insert becomes a step (PlanarShapeTrack::steps, a fixed bend pair), and the final runs on that variant with the last drawing's points and lengths mapped onto it.`
- When set, before the final:
  1. Copy the variant (`vc`). For each track, for each flex middle m (descending): if the insert is committed (its length in `out.committed` at or above `commit_above`, the same test the commit stage uses), keep pieces m - 1, m, m + 1 and record m; otherwise erase pieces m and m + 1 (as `ApplyStepsAsBends` does) and shift `event_piece` entries above m by 2. Then set `steps` to the recorded middles, renumbered after the erasures (ascending), and clear `flex`.
  2. `RefinePlanarShapeCells(vc, why)`; if it fails, the final is not run: `out.final.status = "contract: " + why`.
  3. Map per track the reference points and lengths onto `vc`: for an erased pair at middle m, drop points m + 1 and m + 2 (the insert's two end points; the merged piece m - 1 then ends at the old point m + 2... see note) and set the merged piece's length to the sum of the three old lengths (m - 1, m, m + 1). Note: piece p runs from point p to point p + 1; erasing pieces m and m + 1 removes points m + 1 and m + 2, and the old piece m - 1 now ends where old piece m + 1 ended. This applies to `last_input` (for `sides_from`) and to `last.lengths` (for `node_gap_from` and `start_lengths`).
  4. Final options as today, with: no `curve_flex_from` (there are no flex inserts left; committed bends are steps), `sides_from` = the mapped `last_input`, `node_gap_from` and (when `final_start`) `start_lengths` = the mapped lengths, `coord_max` = 2 x the largest coordinate in the mapped `last_input`, rounded up to a multiple of 16 (at least 64).
  5. `PlanarDrawPipelineResult` gains `uint32_t final_pairs = 0;` (doc: the final's keep-apart pairs, from its result's `pairs`), set in both paths; with `final_contract` also `uint32_t contracted = 0;` (erased flex pairs) and `uint32_t contract_steps = 0;` (committed inserts kept as steps).
- With `final_contract` false nothing changes.

### 3.2 Test hook
- In the any-hub test's K3 block: `po.final_contract = std::getenv("PLANAR_ANYHUB_CONTRACT") != nullptr;` and add to the K3 WARN: `pairs <final_pairs> contracted <contracted> steps <contract_steps>`.

### 3.3 Docs
- `src/junction/planar/CLAUDE.md`: one bullet for `final_contract` (a measurement, off by default).

## 4. Do not
- No change with the option off. No merging of other collinear pieces (only the erased flex pairs). No change to the rounds, the commit, the prune or any default.

## 5. Stop conditions
- `out.committed`'s indexing cannot tell committed from held inserts per flex middle: report what it holds.
- The mapped `last_input` does not match `vc`'s piece counts (the `sides_from` check refuses): report one track's before/after counts.
- A gate fails for a reason you did not cause and cannot isolate in one rerun.

## 6. Gates and measurement
- `cd /home/pete/OpenTTD/.claude/ng1 && tools/junction/jt fast build-rel '[planardraw]' 5`; `tools/junction/jt gate build-rel` (30 s). Known flakes: "planar components: T2 mapped lands in its box at the anchor", "joint: a port fan's size follows the rows"; rerun alone and report.
- Four runs of `build-rel/openttd_test "planar draw: the simplest drawing on every hub"`, each to its own log in build-rel: (a) no env; (b) `PLANAR_ANYHUB_CONTRACT=1`; (c) `PLANAR_ANYHUB_GRID=1`; (d) both. Copy the four logs to `docs/junction-tool/wip/probes/e5land/contract-{a,b,c,d}-<short head hash before your commit>.log`, same commit.
- One commit: `Add: Junction: contract unused flex before the integer final, a measurement (E5, D427)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. No merge or push.

## 7. Report (short, fixed format)
- Commits: hash and subject; files changed; members added.
- Suite results (count and time per gate).
- A table: hub (T1, T2, 197; T3 and LL2 for completeness) x run (a-d): final status, ms, pairs, contracted, steps (join Catch's 80-column wraps first).
- Stops and anything left undone. State: `git status --porcelain` and `git log -1 --oneline`.

When you have reported, stop.
