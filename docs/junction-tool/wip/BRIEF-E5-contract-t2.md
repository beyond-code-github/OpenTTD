# BRIEF: E5 why T2's contracted final is infeasible (read-out only)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: about 45 minutes in total. Each solve has a 15 s cap. Run the hidden test once. No rerun loops, no tuning.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (branch wt-e1), head HEAD_HASH, clean. Release build `build-rel`.
- Context: `PlanarDrawPipelineOptions::final_contract` (5921c5c826, brief docs/junction-tool/wip/BRIEF-E5-contract.md) removes held flex inserts and makes committed ones steps before the integer final. On T2 (first variant, `ShapeFirst(PlanarTText(2), 1)` or the any-hub test's v0) the contracted final is Infeasible, core `piece F2 2`, `sep 10 5 10`, `sep 10 5 11`; without contraction it is Optimal. Suspicion to test, not assume: a pair the zero-contraction skip exempted (walls that touch once every may-be-zero piece is contracted) is written once F2's flex pieces are merged.

## 2. Read first (once each)
- `git show 5921c5c826 -- src/junction/planar/planar_draw.cpp src/junction/planar/planar_draw.h`.
- planar_draw.cpp: the pairwise block's skip rules (adjacent, span, same piece, zero contraction) and how `sep_margins`/`margin_tags` and `skip_pairs` work.

## 3. Changes
### 3.1 Production (measurement aids only)
- `PlanarDrawPipelineResult`: `PlanarShapeVariant final_variant; ///< With final_contract: the contracted variant the final ran on.` and `std::vector<std::vector<std::array<int, 2>>> final_sides; ///< With final_contract: the mapped reference the final read its sides from.` Set both in the contract path.
### 3.2 One hidden test "planar draw: T2 contracted final read-out", tag `[.][planarcert]`
1. T2 v0 pipeline with `final_contract = true`, `final_seconds = 5`. WARN the final status and conflict.
2. F2 before and after: the original variant's F2 `pieces`, `flex`, `event_piece` and its lengths in `last_round.lengths`; the contracted variant's F2 `pieces`, `steps`, `event_piece`; which F2 flex middles were erased and which kept.
3. Face 10 of the contracted variant (`PlanarShapeFaceWalls(final_variant)`): every wall `track#piece hH` with its two points in `final_sides`; for pairs (5,10) and (5,11): every candidate's margin on `final_sides` (`margin_tags` on a `CheckPlanarDrawable` with the final's options as the pipeline builds them, `sides_from = &final_sides`).
4. The same walls in the original variant: find the original face whose walls contain the same tracks around F2's piece (match by track and the pieces the merged piece came from), list its walls, and for the corresponding wall pairs say whether the uncontracted final wrote a `sep` row for them or skipped them, and with which skip reason (if the code can tell; otherwise compare the `sep` tags present in the uncontracted final's `sep_margins` with all candidate tags).
5. Three contracted finals with `skip_pairs`: {sep 10 5 10}, {sep 10 5 11}, both. WARN each status.

## 4. Do not
- No change to any default or behaviour; no fix.

## 5. Stop conditions
- The original face cannot be matched without new production code: list both faces' walls and stop step 4 there.
- A gate fails for a reason you did not cause and cannot isolate in one rerun.

## 6. Gates and measurement
- `tools/junction/jt fast build-rel '[planardraw]' 5`.
- `build-rel/openttd_test "planar draw: T2 contracted final read-out" > build-rel/contract-t2.log 2>&1`; copy to `docs/junction-tool/wip/probes/e5land/contract-t2-<short head hash before your commit>.log`, same commit.
- One commit: `Add: Junction: T2 contracted final read-out (E5)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. No merge or push.

## 7. Report (short, fixed format)
- Commits; files changed; members added. Suite result.
- Steps 1-5 lines verbatim (wraps joined). Stops and anything left undone. State.

When you have reported, stop.
