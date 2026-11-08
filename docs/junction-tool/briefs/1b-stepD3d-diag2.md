# Brief 1b step D3d-diag2: why TL3 (ml_gap 1, branch_gap 0) gives no layout

You execute this brief and nothing else. This is a diagnosis: add temporary prints, run, then revert everything. Commit nothing. If something is not covered, stop and report. Keep your context small: send output to files and report only findings.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`.
- **Previous diagnosis** (`docs/junction-tool/briefs/1b-stepD3d-diag.md`, findings in `STATUS.md` queue item "D3d"):
  - TL6 was fixed by widening the router's search box (3f2bfedd09).
  - TL3 still gives 0 candidates. Its pass-1 failures are E2S ×40, N2E ×12 and N2W ×8; its pass-2 failures are N2E ×64.
  - Hypothesis for h ≥ 14: in pass 2, N→E is trapped by E→S in the under-span corridor (x26..31), since rows 2–3 hold the doubled lanes and there are no crossable tiles.
  - Separate open question: at h12, TL3 fails pass-1 N2E, where TL4 builds an n10 zone.
- **Scratchpad:** `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/d3d2/`
- **Never run `tools/ottd` commands:** a rig run is live.

## 2. Read first

- `src/junction_generator.cpp`, `BuildJunctionCandidate`: the windows and fences, the pass-1 order, the zone and C, the lane doubling, and pass 2.
- `src/tests/junction_design.cpp`, `[.gen]`: env `JUNCTION_GEN_TL`, `JUNCTION_GEN_G`, `JUNCTION_GEN_B` and `JUNCTION_GEN_SKIP`.

## 3. Steps

1. **Build** `build-rel/openttd_test`. Run `[.gen]` with TL=3, G=1, B=0 and `JUNCTION_GEN_SKIP=1`, and confirm "built 0 candidates".
2. **h12, TL3 versus TL4.** Print the partial layout ASCII (`RenderJunctionAscii`) just before the failing pass-1 N2E route, and the N2E request: starts, goals, bounds, fences, windows. Do the same at TL4 h12, where it succeeds. What differs? Look at curve_spacing, s_bend_window = 2·TL, and the E→S route shape.
3. **h14, TL3, pass 2.** Print the partial layout before the failing N2E route, plus the request. Confirm or refute the corridor hypothesis by listing the tiles N→E would need, and which piece occupies each.
4. **Try, as experiments only (temporary code, reverted at the end),** each of the following alone, and report the candidate count each gives at TL3 and TL4. Use TL4 to catch regressions: it should keep ≥ 4 clean.
   - (a) In pass 2, re-route E→S after N→E instead of before, if the code structure allows it in under 15 lines.
   - (b) Give pass 2 the same crossable tiles as pass 1, if any are computed.
   - (c) Raise the minimum height for TL3. Report what the first clean height would be, if there is one up to ml_gap+20.
5. **Revert everything** (`git checkout src/`) and confirm `git status --porcelain` is empty.

## 4. Report

- the root cause for each failure mode, with evidence: ASCII up to about 25 lines each, and the request numbers
- the experiment results table: variant × TL3 count × TL4 count
- your recommended fix, in 3 sentences, labelled as a recommendation
- `git status --porcelain` (empty) and `git log -1`

When you have reported, stop.
