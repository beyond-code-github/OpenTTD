# BRIEF hub-prio-rows: the signal pass places the prio on the drawn hub's own rows

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 90 minutes. Probe runs: one T1 at the end (15 s stage A).

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Builds: Release `build-rel` (the rig server and the engineer's client run `build-rel/openttd`: never start, stop or signal them; build only the `openttd_test` target, never the `openttd` target, so the client and server binaries stay in step). `JT_JOBS=4`, cmake `-j4`. Never touch ng2.
- Recon: `docs/junction-tool/wip/RECON-hub-prio-rows.md` (read it whole). Goal: a signalled T1 in the rig. Today T1's render is unsignalled: "the prio element at 13,1 does not fit", because the hub's rows are not given to the signal pass and its search invents a row from the main line.
- Rulings: D349 (prio rows are real tracks of the bundle), D353 (beside a diagonal piece the row is on the same tile, opposite half), D452 (the hub is the contract; Signals reads it). The checker's prio facts are in `src/junction/CLAUDE.md` (Rule checker): coverage may bend; shape rules apply to the joins alone.

## 2. Read first
- `CODINGSTYLE.md` (C++); `src/junction/CLAUDE.md` Signals and Rule checker prio bullets; `src/junction/planar/CLAUDE.md` Drawn hub section.
- `junction_signals.h` (network struct), `junction_signals.cpp` :40-60, :100-200, :340-515; `planar_plan.cpp` `RenderPlanarDrawnHub`.
- The probe's hunks: `git diff bc47f12f73 dc79e8c83f -- src/junction/signals src/junction/junction_rules.cpp src/junction/planar/planar_plan.cpp src/junction/planar/planar_components.h` (to file, read with offset/limit).

## 3. Changes
1. `JunctionSignalNetwork::prio_rows` (`std::vector<std::vector<JunctionPiece>>`, doc comment: each prio row's pieces, upstream first, its last piece beside its merge tile; D349, D452).
2. `RenderPlanarDrawnHub`: per hub track of kind Row, its domain-raster pieces as `JunctionPiece`s (the same construction as the path pieces) pushed to `out.network.prio_rows`; a Row piece on a bridge is a why. Rows are not added to routes, layout, node or port tiles (the signal pass draws them).
3. `junction_signals.cpp`: port the probe's given-row search (`given`, `row_index`, `row_of`, `plain`, `row_free`, `candidate`, the leaf drawing of `given[i]`, the axis touch point beside a diagonal, row signals facing against the row's trackdir, `placed_merges`, the failure "the given prio row does not reach its merge at X,Y") WITHOUT: any getenv/PRIODBG code, the probe-only `CheckSignals` filter, and the `CheckPrio` skip. `CheckPrio` and `CheckPrioReach` both judge every leaf. When a merge has a given row, skip the straight pattern for it (it is deferred to the search).
4. `junction_rules.cpp`: port the probe's opposite-half allowance in `CheckPrio` beside a diagonal piece (D353), it goes in the signals commit.
5. Docs: `planar_plan.h` doc of `RenderPlanarDrawnHub`, `src/junction/planar/CLAUDE.md` (rows now reach the signal pass), `src/junction/CLAUDE.md` Signals bullet (a given row is used before the search invents one), SPEC-slots-lengths §4.1 "Rows are not rendered ..." sentence -> "Rows reach the signal pass as `prio_rows`; it places joins and signals on them".

## 4. Tests, outermost first
- **Outer (red first):** `[junction][planardomain]` "a drawn hub's prio is placed on its own row (T1 with curves)": the fixtures `t1-final-curves.drawing.txt` + `t1-final-curves-stagea.drawing.txt` (as the render pin builds the hub) -> render -> `SignalJunctionTrackRoutes` (test spec): why empty; the layout has the row's tiles; then `CheckJunctionRules` on the result with `PlanarCheckerTopology`'s ports/lane paths (as `[planarbuild]` or the PR-era tests do): no prio violation. Record red (today: "the prio element at 13,1 does not fit").
- **Inner:** one hand case per kind the probe handled, in `src/tests/junction/signals/` (or the existing signals test file): a straight given row beside an axis main line (the search places it like the straight pattern would), and a row beside a diagonal piece (opposite half, D353) accepted by `CheckPrio`. Red first for each.
- If `CheckPrio` refuses a leaf the probe accepted only through its skip: STOP, report the rule and the leaf. Do not bypass.

## 5. Gates (D453: shared code, signals and the rule checker)
- `JT_JOBS=4 tools/junction/jt gate build-rel` and `JT_JOBS=4 tools/junction/jt identity build-rel` (junction_signals.cpp is on the identity list): every pinned number unchanged (v1 paths pass no rows, so nothing may move). A moved identity number is a stop.
- `JT_JOBS=4 tools/junction/jt fast build-rel '[planardomain]'`.

## 6. Probe and rig file
- One T1 run: `PLANAR_SLOTS_HUB=T1 PLANAR_FLANK=1 PLANAR_NODE_FLANK=1 PLANAR_GRID_COND=1 PLANAR_STAGEA_NODEGAPS=1 PLANAR_STAGEB_CURVES=1 JUNCTION_MILP_INCUMBENT=1 PLANAR_RIG_OUT=/home/pete/OpenTTD/.claude/ng1/docs/junction-tool/wip/rig-2026-11-25/T1-v2-signals.json`. Report the `rig:` line (signals count) and commit the JSON.

## 7. Commits
- `Feature: Junction: the signal pass places the prio on a given row (D349, D353)` (signals + rules + network field + inner tests), `Add: Junction: the drawn hub gives its prio rows to the signal pass` (render + outer test + docs), `Doc: Junction: T1 v2 rig layout with signals`. End each with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## 8. Report
References read; commits; red texts; gate and identity results (counts, any moved number); the T1 `rig:` line; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.

## Run 2 (2026-11-25, after run 1's stop; run 1's work is the WIP commit on wt-e1; start from it, no history rewrite)
- **Ruling D454 (engineer): the row as drawn.** `prio_rows` carries the hub row's FULL piece list (its upstream join off the main line, the beside run, its join on the merge tile). The signal pass:
  1. matches a row to a merge by the row having a piece on that merge's tile (not "last piece beside the merge tile");
  2. draws the row's pieces as given (they are track, not a choice), including both end joins;
  3. searches only the signal elements along it: signal pairs (row signal facing against the row's trackdir, main-line signal) and extra touch points (joins) between the row's two end joins, fewest first, at most 20000 leaves, as today's search orders them; the row's end joins count as elements already placed;
  4. judges every leaf with `CheckPrio` and `CheckPrioReach` (no skip). If every leaf fails, report the first leaf's `CheckPrio`/`CheckPrioReach` violations in the why (one line), so the stop names the rule.
- `row_index` (the row piece beside a main-line piece) must handle the row's half pieces at its ends: beside a main-line piece, the row piece on the side tile (axis main line) or the same tile's opposite half (diagonal, D353); end-join pieces have no beside main-line piece and are skipped.
- Stop conditions added: the row's beside run is shorter or longer than the coverage the checker demands (D384: exact length) and `CheckPrio` refuses for that reason: STOP and report the row length vs coverage; do not trim or extend the row.
- Inner tests: one hand case with a full given row (upstream join, beside run on an axis main line, merge-tile join) that the pass signals and the checker accepts; one beside a diagonal main line (D353). Red first.
- Everything else (gates incl. wide gate + identity, docs, probe, commits) as parts 4-8. Note: the X2 exactness failure seen in run 1's gate is fixed on feature (7219cb3847, merged into this head): it must pass now.
- **Clarification (engineer: "prios are signalled differently to real track"):** the row is never signalled by the ordinary phases (block, drag, bay, fill, split/merge); it is no route and no ordinary signal lands on its pieces. Its only signals are the prio element's own (the types and facings the existing prio code uses), judged by CheckPrio + CheckPrioReach. The outer test asserts every signal on a row tile came from the prio placement.

## Run 3 (2026-11-25, after run 2's stop: hub routes run through rows; WIP 7731757140 on wt-e1, start from it)
- **Ruling (orchestrator, D452's contract, §4.1 "Rows carry no route"):** `PlanarDrawnHubOf` drops every route path that runs on a track of kind Row; `CheckPlanarDrawnHub` adds the promise "no route path runs on a Row track" (one line naming the route and row when broken). Then rows are only in `prio_rows`, never in `render.routes`/`network.routes`, and no ordinary phase walks them.
- Inner test for the oracle (hand-edited T1 hub with a path through `prio:E1`: one line). Red first.
- Then rerun the outer test. Expected next wall (report, do not chase): the row's beside run vs the coverage `n_prio` (14 on the test spec; the row spans about 4 main-line steps): if `CheckPrio`/`CheckPrioReach` refuse for coverage, STOP with the why and the numbers (row beside length, coverage tiles). D384: a prio is exactly its coverage long; the drawing program does not size rows to coverage yet: that is an upstream step, not this one.
- The assertion "every signal on a row tile came from the prio placement": check it through the returned tokens (`Reason::Prio`), as run 2 found.
- Commit attribution: end every commit with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>` exactly (the session's attribution line), whatever model you are.
- Gates, docs, probe, commits as parts 5-8 once the outer test passes; if it stops at coverage: commit the hub ruling + oracle test (`Fix: Junction: the drawn hub's routes never run on a prio row`) and leave the signals work as WIP.

## Run 4 (2026-11-25, D455 supersedes D454: a prio row is never drivable; start from wt-e1's 13b7203171 and the WIP below it)
- **Ruling D455:** `prio_rows` carries each Row track's DRAWN beside run only: the hub's row points without the beside opening/closing steps `PlanarDomainInputOf` adds (`PlanarBesideOpening`/`Closing`); rasterise those points. The signal pass draws EVERY join, both ends included, as the chevron `CheckPrio` checks (a streamlined pair of half pieces pointing against travel, the row's straight piece through the join's row tile except on the merge tile), exactly as v1's probe leaf did; it matches a row to its merge by the row's last piece beside the merge tile (the probe's `given` rule, which fits D455). Run 2's "row as drawn with its end joins" logic is replaced.
- **Fixtures:** use the coverage-sized T1 drawings from ng2 (`tools/junction/topologies/t1-final-coverage.drawing.txt` and `t1-final-coverage-stagea.drawing.txt`, merged into this head by the orchestrator) for the outer test, so the row is exactly its coverage long (D384). If they are not in the head, STOP.
- Finding to report, not fix: the domain input still adds the opening/closing steps for rows, so the domain judges the row as a connected (drivable) track; name where (file:line) so a later step can align the domain with D455.
- Everything else as parts 4-8 and runs 2-3 (inner tests, the prio-signal assertion via `Reason::Prio`, wide gate + identity, docs, probe with `PLANAR_COVERAGE=1` added to the knobs, commits with the Opus 5.5 attribution line).
