# BRIEF FX1: frame experiment, stage 1 (the red count)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch probe-fx1 (made from probe-g3e f40d43c61c), head HEAD_HASH. Release build dir build-rel. Logs and outputs: /tmp/claude-1000/-home-pete-OpenTTD/83ff9380-9534-4138-8dc1-79a833146e49/scratchpad/fx1/. This is a THROWAWAY probe branch: commit there, never merge.
- Implements: docs/junction-tool/DESIGN-tracks.md §17.6 step 1 and §17.3 (Slots frames); wip/REVIEW-architecture-holistic.md §7 "the cheapest experiment". Stage 1 measures the red: with today's slots (one frame per channel), how many variants' FIRST solve (no clash rows) is infeasible. No frames are added in this stage.
- Time box: 3 hours. Past it, stop and report what you have.

## Read first (once each, with offset/limit)
1. CODINGSTYLE.md (skim).
2. src/junction/planar/planar_paths.cpp lines 400-540 (`BuildPlanarLineHeadings`: the base list at ~464, the split-leg drop rule at ~486-501, the merge-leg one at ~502-515).
3. src/junction/planar/planar_components.cpp: 2195-2220 (`SolveLazy1`, "base infeasible"), 2840-2975 (TrackComposer: headings, then the D359 port-step block at 2921-2970 with `BuildPlanarGapSteps` at ~2934 and candidate steps at ~2939-2942), 3085-3140 (slot bends, `PLANAR_SLOTBEND`), 3685-3695 (`PLANAR_COMPOSER`).
4. tools/junction/pv (usage, the `run` output line format, `--limit`, `--tags`).
5. docs/junction-tool/TESTS.md: the top section (jt, pv).

## Changes (two env switches, nothing else)
1. `PLANAR_NODELEGS=1`, in `BuildPlanarLineHeadings` (planar_paths.cpp): read once at function start (`const bool node_legs_only = std::getenv("PLANAR_NODELEGS") != nullptr;`). When set, both drop conditions (split side and merge side) are false, so a 45-degree node leg is always inserted (split side) or pushed (merge side) and no base piece is ever dropped. Unset: behaviour unchanged.
2. `PLANAR_NOPORTSTEPS=1`, in TrackComposer's D359 block (pc ~2921-2970): when set, no step from `BuildPlanarGapSteps` and no candidate step is put into `heads`/`cpiece`; the working copies equal `hd` and the crossing pieces unchanged. Unset: behaviour unchanged.
Each switch gets a one-line comment naming this probe ("FX1, DESIGN-tracks §17.6 step 1").

## Runs (one at a time, never two in parallel)
Common env for all runs: `PLANAR_COMPOSER=tracks PLANAR_SLOTBEND=1`, `PLANAR_FORKMASK` and `PLANAR_MERGEMASK` unset. For each hub H in T2, T3, LL2 (LL2: every built variant; get the list with `PLANAR_DRY=1` as pv documents, pass it with `--tags`):
- **R0 baseline:** common env only. `tools/junction/pv run build-rel H`; copy `build-rel/pv-H.txt` to the scratch dir as `R0-H.txt`.
- **R1 stage 1:** common env + `PLANAR_NODELEGS=1 PLANAR_NOPORTSTEPS=1`. Same, saved as `R1-H.txt`.
- **IIS sample:** for up to 3 R1 variants per hub whose reason is `base infeasible` (first three in tag order), run the single variant with `PLANAR_ONLY=<tag> PLANAR_IIS=1` via the same test pv uses, output to `iis-H-<tag>.txt`. From each, list the conflicting rows by name (no full dump in the report).

## Tally (write to docs/junction-tool/wip/PROBE-FX1.md, about 400 words)
Per hub and run: variants total; built; base infeasible; other failures (grouped by reason text); solved (best area and tag). Then per IIS sample: tag, the row names in the IIS, and one line on which lines, frames or sites those rows relate (channel, site, port, deck), as read from the row names. Mark each claim VERIFIED (from an output file, name it) or INFERRED. No recommendations.

## Do not
- Do not add frames, rows, or any change to slots, the length model, the lazy loop or `MappedComposer`.
- Do not change any switch's default behaviour, any test, or pv.
- Do not tune timeouts or limits to get a different count.

## Stop conditions
- The build fails for a reason outside your two edits.
- R0's counts look broken (every variant failing construction, not solve): report and stop.
- Time box reached.

## Gates
- Release build of `openttd_test` (log to file, grep errors and warnings in touched files).
- Commit on probe-fx1: `Probe: Junction FX1 node legs only and no port steps (PLANAR_NODELEGS, PLANAR_NOPORTSTEPS)` with the trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. PROBE-FX1.md is written in the main checkout /home/pete/OpenTTD/docs/junction-tool/wip/ and NOT committed by you.

## Report
- References read
- Commit: hash and subject
- Table: hub x run (R0, R1): total, built, base infeasible, other failures, solved, best
- IIS samples: tag and row names, one line each
- Stops, time used, anything left undone
- State: `git status --porcelain` and `git log -1 --oneline` in ng1

When you have reported, stop.
