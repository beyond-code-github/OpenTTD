# BRIEF contraction-pin: stage A's contracted drawing satisfies stage B's rows (unit test, red first)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 60 minutes. Two probe runs (T1, T2), 15 s per solve, once each. This step pins and diagnoses; it does NOT fix the broken rows.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Builds: Debug `build`, Release `build-rel`.
- Evidence: `docs/junction-tool/wip/probes/slots/flank-lock.md`, last section "Node cusp": T2 stage A (flank + node_flank + node_cusp, coord 128, HiGHS incumbent) contracts with every turn holding, but stage B's listing solve (no keep-apart rows) is Infeasible, Optimal once the [node] family is dropped. Stage A drew it, so the contraction or a stage B row disagrees with stage A's points. Memory rule: an infeasible layer is an upstream gap; pin it as a unit test, then fix upstream.
- The invariant pinned here: **every row of stage B's program holds on the contracted points of a stage A drawing** (fixed points, no solve).

## 2. Read first (once each, with offset/limit)
- `CODINGSTYLE.md` (C++ part); `src/tests/junction/CLAUDE.md` (D453 test scope).
- `src/junction/planar/planar_draw.h` lines 80-125 (`PlanarProgram`, `PlanarProgramCheck`, `CheckPlanarProgramOnPoints`) and the options `build_only`, `program_out`.
- `src/tests/junction/planar/planar_domain_encoding.cpp`: `ContractedVariant` (:1030), and the `[slotsstep2]` probe from its start (:6664) to the stage B listing solve (~:7025): stage A options, the Geometry from stage A's lengths, `fa.SetUp`, the contraction, `plain`, stage B options `sb` and `listing`.

## 3. Changes
1. **Save knob** in the `[slotsstep2]` probe: env `PLANAR_STAGEA_SAVE=<file>` writes, right after the Geometry `g` is built from stage A, one line per track: `track <name> <start x> <start y> <len0> <len1> ...` (the start point `a.points[t].front()` and `g[name]` in piece order). Nothing else.
2. **Shared stage B options:** move the probe's construction of `sb` (SlotsOptions, port_zero_from, node_gap_from, along_from, coord_max, tidy false) and of `listing` into one file-local function in planar_domain_encoding.cpp that both the probe and the new test call; the reference vectors it points at are owned by the caller (pass them in). The probe's behaviour is unchanged.
3. **Fixtures:** run the probe once for T1 and once for T2 with `PLANAR_STAGEA_SAVE` (T1: `PLANAR_FLANK=1 PLANAR_NODE_FLANK=1 JUNCTION_MILP_INCUMBENT=1`; T2: the same plus `PLANAR_NODE_CUSP=1 PLANAR_STAGEA_COORD=128`), Release, and commit the two files as `tools/junction/topologies/t1-stagea.drawing.txt` and `t2-stagea.drawing.txt` (the folder `JUNCTION_TOPOLOGY_DIR` points at). If T1's or T2's stage A gives no Optimal/incumbent, STOP and report.
4. **The test** (planar_domain_encoding.cpp, tags `[junction][planardraw][contraction]`): "planar slots: stage A's contracted drawing satisfies stage B's rows (T1, T2)". Per hub: build the variant exactly as the probe does (topology, structure, shape, flex stripped, `PlacePlanarShapeFlex(v, why, true, false, 1)`), load the fixture into the Geometry and starts, `fa.SetUp`, `ContractedVariant`, `plain` as the probe makes it; stage B options from the shared function with `build_only = true`, `program_out = true`; `CheckPlanarProgramOnPoints(result.program, cv->points)`. Assert `broken` is empty; on failure print every broken tag (`INFO`).
   - Red is expected on T2 (the [node] family) and possibly T1: record the broken tags. This test stays red at the end of the step: commit it tagged `[!shouldfail]` additionally, so the suite records it as an expected failure, and say so in the commit message. (If T1 passes and T2 fails, split into two TEST_CASEs and mark only T2 `[!shouldfail]`.)
5. **Diagnosis (report only):** for every broken tag on T2, the row as written (terms with track/piece names where the read-out gives them, bounds) and the values of its terms on the contracted points; then, for each, whether stage A's options had that row (stage A sets `node_gaps = false`; compare with stage B's options) or whether the contraction changed the points it reads. One line per broken tag; group by cause.

## 4. Do not
- No fix to any row, the contraction or any option default. No change to stage A. No new knob besides `PLANAR_STAGEA_SAVE`.
- Never touch the rig, game clients or other worktrees. No `git stash`. Source edits only with the Edit or Write tool.

## 5. Stop conditions
- No stage A drawing for T1 or T2; `ContractedVariant` returns nothing for a fixture; the moved option code changes the probe's output (compare its stage B listing line before and after on T2).

## 6. Gates and commit (D453)
- `tools/junction/jt fast build '[contraction]' 5`; `tools/junction/jt fast build '[planardraw]'` (if over budget before your change, say so with the time and continue).
- Commit: `Add: Junction: pin, stage A's contracted drawing satisfies stage B's rows (red on T2)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## 7. Report
References read; commits; red evidence (the broken tags for T1 and T2); the diagnosis lines grouped by cause; suite results; stops; `git status --porcelain` and `git log -1`. When you have reported, stop.
