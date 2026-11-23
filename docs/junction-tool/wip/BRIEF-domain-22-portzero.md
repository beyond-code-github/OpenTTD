# BRIEF domain-22: a D412 port line is 0 or an ordinary line (D434, F5)

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 75 minutes wall. Probe flag only: no default changes, so no wide gate. One Release report run per fixture. Never run git stash. Do not touch branch wip-defaults.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. First `git merge --ff-only feature/junction-tool` (it only adds docs). Commit there; do not merge back.
- Read first: `/home/pete/OpenTTD/docs/junction-tool/DECISIONS.md` rows D412, D424, D432, D433, **D434** (this step); `CODINGSTYLE.md`; `src/junction/planar/planar_draw.cpp` lines 560-670 (the D412 `port_stretch` code, the flex-group rows, grid seeds, node gaps); `planar_draw.h` lines 155-170 (option pattern: `node_gaps`, `node_gap_from`); `src/tests/junction/planar/planar_domain_encoding.cpp` lines 1025-1200 (`RunEncoding`: suffix parsing, `readouts`, `accepts`) and the test "a node-to-out-port line keeps its least lengths (F2)" (~line 1598), which is the pattern for your tests.
- F5 (in `docs/junction-tool/wip/probes/domain/encoding-x1-span_lead_endnode_crosspiece_grid.txt`, line ~72): on X1 the rows accept the polyline with base geometry plus `AC1.2=2 AC1.3=1 AC1.4=0` (contracted AC1 = [1x1, 2x2, 3x1]; its east run is off the centre line). D412 frees every piece of an in-port line with no event (`port_stretch`), so no least length, no flex row and no grid seed reaches it.

## 2. The rule (D434), behind a flag
In `PlanarDrawOptions` (planar_draw.h, next to `node_gap_from`), add exactly:
- `bool port_zero = false;` doc: `///< A probe (D434): a D412 port line (in-port, no event, to a node) is 0 or an ordinary line, the branch from port_zero_from (tag `port zero`).`
- `const std::vector<std::vector<double>> *port_zero_from = nullptr;` doc: `///< With port_zero: a drawing's lengths (as PlanarDrawResult::lengths); a D412 line whose travel there is below 1 half tile is held at 0 (its pieces free), every other is an ordinary line. Null: every one ordinary.`

In planar_draw.cpp, where `port_stretch` is computed (~line 567), make it `bool` (not const) and, when `options.port_zero` is on and `port_stretch` is true, set it from the reference: sum `(*options.port_zero_from)[t]` (when the pointer is non-null and `t` is in range); `port_stretch = sum < 1`; with no reference, `port_stretch = false`. Everything below that already reads `port_stretch` (free pieces, the skipped flex rows, the grid's run_positive) then follows unchanged. After the flex-group rows loop (~line 592), add: when `options.port_zero && port_stretch`, a row over all `length[t]` terms (variable, coefficient), bounds `-INF .. 0`, tag `fmt::format("port zero {}", map.tracks[t].name)`. Mirror the node-gaps `leg zero` row exactly.

Do not change the D412 skip in the lead-rows function (~line 2594); note in the report whether an ordinary-form D412 line now lacks lead rows it would otherwise get (a finding, not a fix).

## 3. Tests (outermost first; red by assertion before the code: add the two option fields first with no behaviour, run, record the failure, then write the behaviour)
In planar_domain_encoding.cpp, tag `[junction][planar][planardomain]`:
1. "planar draw: a D412 port line off its centre line is rejected under port_zero (F5, D434)": X1 base plus `g["AC1"]` pieces 2=2, 3=1, 4=0 (check the Geometry key form against the F2 test); contracted variant, steps cleared, options as the F2 test plus `grid = true`, `port_zero = true`, `port_zero_from` = per track one entry, its travel from `cv->points` (sum over consecutive points of max(|dx|, |dy|)). Assert `CheckPlanarProgramOnPoints(...).broken` holds a row starting with `integer`. Also assert that with `port_zero = false` the same points break nothing (that is F5 today).
2. "planar draw: port_zero holds a zero D412 line at 0 and frees an ordinary one never (D434)": on X1's base contracted variant, build the program twice with `port_zero` on: (a) `port_zero_from` giving AC1 travel 0: the program has a row tagged `port zero AC1` and every AC1 length variable has lower bound 0; (b) travel 4: no `port zero` row, and AC1's length variables have lower bound >= 1. Read the variable bounds from `PlanarProgram::lower` by the length variables of AC1 (find how the F2-style read-out names them; if variables cannot be told apart by track, stop and report).

## 4. The suite
- `RunEncoding`: a new suffix `+portzero`, stripped FIRST (it is written last: `span+lead+endnode+crosspiece+grid+portzero`). When on, `readouts` takes the `Contracted` (not just its variant) and sets `o.port_zero = p.port_zero = true` and both `port_zero_from` to that polyline's per-track travel from its points (as test 1). Update the three call sites. Nothing else in `RunEncoding` changes.
- Release (`build-rel`), once per fixture X1, X2, M1, K1, L1, Y1, rule set `span+lead+endnode+crosspiece+grid+portzero`, new report files `encoding-<fixture>-span_lead_endnode_crosspiece_grid_portzero.txt`, written under the WORKTREE's `docs/junction-tool/wip/probes/domain/` (never the main checkout) and committed. Find how earlier reports were run from the env names in the test file (`PLANAR_DOMAIN_REPORT[_X2|_M1|_K1|_Y1|_L1]`, `PLANAR_DOMAIN_RULES`); timeout 300 s each.

## 5. Gates and commit
- `tools/junction/jt fast build '[planardomain]' 5` and `tools/junction/jt fast build '[planardraw]' 30` (Debug): green, or name any failure you did not cause.
- One commit: `Add: Junction: a D412 port line is 0 or an ordinary line, behind port_zero (D434)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 6. Stop conditions
Test 1 with `port_zero = false` breaks something (F5 premise false); a fixture has no D412 line and the report changes anyway; any change to a default; anything not covered.

## 7. Report (short)
References read; commit; red evidence per test; members added; per fixture: too strict / too loose counts vs the `..._grid.txt` report, every non-zero group's key and first example; the lead-rows note; gate results; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
