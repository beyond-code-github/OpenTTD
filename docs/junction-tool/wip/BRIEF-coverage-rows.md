# BRIEF coverage-rows: every prio row is exactly its coverage long in the drawing program (D384)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 75 minutes. Probe runs: at most 2 on T1 (15 s stage A each).

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng2`, branch `wt-c2`, head `<HEAD>`. Build Release `build-rel`. `JT_JOBS=4`, cmake `-j4`. Never touch ng1 (another agent; the rig runs its binary). Never use git stash.
- Rulings: D384 (a prio row is exactly its coverage long along its path; the pair lane and row runs exactly the coverage between the row's split and merge; scope "Slots (coverage)"), D349, DESIGN-sss-v2 §3.3 ("D384's prio length exact" is a Slots row). `src/junction/planar/CLAUDE.md` Drawing program: "Not yet in it: ... coverage".
- Evidence: T1's drawn row spans about 4 main-line steps against the test spec's coverage of 14 tiles (`n_prio` in the signal pass), so the prio cannot be signalled.

## 2. Read first
- `src/junction/planar/CLAUDE.md` Drawing program bullets (corridor family, node gaps, bays/curves: the pattern for an option and its tag).
- `planar_structure.cpp` `PlanarRuleFieldsOf` (how `rules.coverage` is computed and its unit), `planar_structure.h` `PlanarRuleFields`.
- How the checker measures coverage: `CheckPrioReach` / the prio length in `src/junction/junction_rules.cpp` and `JunctionPrioTiles` (junction_spec / train model): in tiles along the main line, how a diagonal piece counts.
- `planar_draw.cpp`: the per-piece length variables (`length[t][i]`, coefficient `AXIS_LEAST` for axis), the bays/curves rows (how travel along a path is summed), the corridor family (rows and their parent chain).

## 3. Design (decided, except the unit check below)
- `PlanarDrawOptions::coverage` (bool, default false, probe; doc: D384). With it, per track of kind Row: the row's travel along its path (sum of its pieces' travel, the same travel measure the bay rows use) `== rules.coverage` (tag `coverage <row>`). One equality row per row track. No other change.
- **Unit check first:** confirm that `rules.coverage`, the bay rows' travel measure, and the checker's coverage (`n_prio` tiles along the main line, a diagonal piece counted as the checker counts it) agree for an axis row and for a row with a diagonal piece. If they do not agree, STOP and report the three definitions with file:line; do not convert.
- The probe reads env `PLANAR_COVERAGE` into stage A, stage B and the polish (all three: the contraction pins need stage A to carry the same rows), and prints the number of `coverage` rows in the stage A line.

## 4. Tests
- Inner (`[junction][planardraw]`, red first): on a hand variant or the T1 variant with build_only + program_out: with `coverage` there is one `coverage <row>` row per Row track with bound `rules.coverage`; `CheckPlanarProgramOnPoints` on a drawing whose row is shorter breaks it, and on one whose row is exactly that long holds it (fixed points, no solve). Record red.
- Probe (Release, T1): `PLANAR_SLOTS_HUB=T1 PLANAR_FLANK=1 PLANAR_NODE_FLANK=1 PLANAR_GRID_COND=1 PLANAR_STAGEA_NODEGAPS=1 PLANAR_STAGEB_CURVES=1 PLANAR_COVERAGE=1 JUNCTION_MILP_INCUMBENT=1 PLANAR_FINAL_SAVE=<scratchpad>/t1-cov.txt PLANAR_STAGEA_SAVE=<scratchpad>/t1-cov-a.txt`. Report stage A status/time, stage B, polish, judge, domain, each row's travel vs `rules.coverage`. If stage A or B is Infeasible, report the conflict families (with `PLANAR_STAGEA_IIS` if that is the existing knob) and stop; do not chase.
- If the probe passes judge and domain: commit both saved files as `tools/junction/topologies/t1-final-coverage.drawing.txt` and `t1-final-coverage-stagea.drawing.txt` (the next step signals them).

## 5. Do not
- No change to defaults, the corridor family, stage A's other rows, or the signal pass. Source edits with Edit/Write only.

## 6. Gates and commit (D453)
- `JT_JOBS=4 tools/junction/jt fast build-rel '[planardraw]' 30`, `... '[planardomain]'`, `... '[planardomainexact]' 30`, `... '[contraction]' 5` (all unchanged except the new test).
- Commit: `Add: Junction: prio rows exactly their coverage long in the drawing program (probe, D384)`, plus `Doc: Junction: T1 drawings with coverage rows` for the fixtures. `src/junction/planar/CLAUDE.md`: one bullet for `coverage`, and remove "coverage" from "Not yet in it". End each with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## 7. Report
References read; the unit check (three definitions, agree or stop); commits; red text; probe lines and row travel vs coverage; suites; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
