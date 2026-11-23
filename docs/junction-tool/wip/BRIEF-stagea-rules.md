# BRIEF stagea-rules: stage A keeps the grid parity of runs that may be zero, and node gaps

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 75 minutes. Probe runs: at most 2 per hub (T1, T2), 15 s per solve.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng2`, branch `wt-c2`, head `<HEAD>`. Build: Release `build-rel`. Load limit `JT_JOBS=4`, cmake `-j4`. Never touch ng1 (the rig runs its binary).
- Evidence: `docs/junction-tool/wip/probes/slots/contraction-diag.md` and the pin tests "stage A's contracted drawing satisfies stage B's rows (T1)/(T2)" (both `[!shouldfail]`). Two stage A rules are missing:
  1. **Grid parity of a run that may be zero.** `planar_draw.cpp` grid block (~:698-720) makes a point odd across only on an axis run known positive. A run that is a flank of free flex inserts is not known positive, so stage A leaves it at an even coordinate; after contraction it is a fixed piece and stage B's grid makes it odd: T1's F1 at x 14 and 16, and T2's `cross 6`, `cross 15`, `end 0`, `end 2`, `half`.
  2. **Node gaps.** Stage A sets `node_gaps = false`; T2's `leg prio:B2` is 1 half tile against `rules.node_leg` 2.

## 2. Read first (once each, offset/limit)
- `CODINGSTYLE.md` (C++ part); `src/junction/planar/CLAUDE.md` Drawing program bullets on grid and node gaps.
- `planar_draw.cpp`: the grid block (from `if (options.grid)` ~:698 to the end of the parity pass, including how a point is written `2 h + p` and the `gridded` set), and the node-gap rows (grep `"leg {}"`).
- `planar_draw.h`: `grid`, `node_gaps`, `node_gap_from`.
- `src/tests/junction/planar/planar_domain_encoding.cpp`: the `[slotsstep2]` probe's stage A options (`sa`), `PLANAR_STAGEA_SAVE`, and the two pin tests.

## 3. Design (decided)
- **`PlanarDrawOptions::grid_conditional`** (bool, default false, probe; doc comment: "stage A: an axis run that may be zero is odd across when it has length"). With `grid` and `grid_conditional`: for every axis run of a track that is NOT known positive (the runs the grid block skips today), one binary `z` (tag `grid run <track> <first piece>`) with `sum of the run's length terms (in half tiles) - coord_max * z <= 0`, and for each point of the run not already gridded, its across coordinate written as `2 h + q` with `q` integer in [0, 1] and `q - z >= 0` (tag `grid cond <track> <point>`). A point already gridded by a positive run keeps its row. Nothing else changes; without the option the program is byte-identical (check with `build_only` + `program_out`: same rows and variables on T1 and T2).
- **Stage A node gaps:** the probe reads env `PLANAR_STAGEA_NODEGAPS`: `sa.node_gaps = true` with `node_gap_from = nullptr` (every node-to-node track at least `rules.node_leg`). If T1 or T2 has a node-to-node track that must be zero (a trident, D433) this is infeasible: then report which tracks and STOP (do not add a reference).
- The probe reads env `PLANAR_GRID_COND` into `sa.grid_conditional`, and prints the binary count of `grid run` rows in the stage A line.

## 4. Tests, outermost first
- **Outer (new fixtures, expected green):** run the probe once per hub with `PLANAR_GRID_COND=1 PLANAR_STAGEA_NODEGAPS=1` plus today's stage A knobs (T1: `PLANAR_FLANK=1 PLANAR_NODE_FLANK=1 JUNCTION_MILP_INCUMBENT=1`; T2: the same plus `PLANAR_NODE_CUSP=1 PLANAR_STAGEA_COORD=128`) and `PLANAR_STAGEA_SAVE`, saving `tools/junction/topologies/t1-stagea-rules.drawing.txt` and `t2-stagea-rules.drawing.txt`. Add two pin cases on them ("... satisfies stage B's rows, stage A with grid parity and node gaps (T1)/(T2)"), `[junction][contraction]`, NOT shouldfail. They must pass. If either fails, mark it `[!shouldfail]`, list the broken tags with values (diagnosis style), and stop after the commit.
- The old fixtures' cases stay as they are (`[!shouldfail]`, the evidence of the loose rules).
- **Inner (new, `[junction][planardraw]`):** on a hand variant or the X1 fixture, a flex flank run with length on an even across coordinate: with `grid_conditional` the fixed-point check (`CheckPlanarProgramOnPoints`) breaks a `grid cond` row; with the run at length 0 it holds. Red first: write the test before the option exists in the program (the option declared, ignored), record the failure.
- Report per hub: stage A status and time with and without the two knobs (the "without" numbers from today's runs are in `docs/junction-tool/wip/probes/slots/flank-lock.md`; do not rerun them), the `grid run` binary count, and the probe's later lines (contracted turns, node turns, stage B, polish, judge, domain).

## 5. Do not
- No change to stage B, the contraction, any default or the old fixtures. No knobs beyond `PLANAR_GRID_COND` and `PLANAR_STAGEA_NODEGAPS`. Source edits with Edit/Write only (no sed, no Python).

## 6. Gates and commit (D453)
- `JT_JOBS=4 tools/junction/jt fast build-rel '[contraction]' 5`, `... '[planardraw]'`, `... '[planardomain]'` (unchanged results).
- Commits: `Add: Junction: stage A grid parity on runs that may be zero (probe)` (option + inner test), then `Add: Junction: pins on stage A drawings with grid parity and node gaps` (knobs, fixtures, outer cases). `src/junction/planar/CLAUDE.md`: one bullet for `grid_conditional`. End each with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## 7. Report
References read; commits; red text; per hub the numbers of part 4; pin results (green, or broken tags); suite results; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
