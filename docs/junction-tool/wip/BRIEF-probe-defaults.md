# BRIEF probe-defaults: the v1 rule set is the Slots probe's default (audit AUDIT-v1-v2-parity.md)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 60 minutes. Probe runs: one T1, one T2 (15 s stage A each).

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng2`, branch `wt-c2`, head `<HEAD>`. Build Release `build-rel`; `cmake --build build-rel --target openttd_test -j4` before jt. `JT_JOBS=4`. Never touch ng1 or any openttd process. Never use git stash.
- Why (engineer: "How many of these do we actually need?"): rules that exist but are opt-in env knobs in the `[slotsstep2]` probe get forgotten. Needed and built: curves + bays (stage B and polish), coverage (D384, all stages), grid_conditional + stage A node gaps, flank_turn, node_flank, node_cusp (D448, D451).

## 2. Changes
- In the probe (`planar_domain_encoding.cpp`, the stage A options block and `StageBToDomain`): those rules ON by default. Each existing env knob that turned one on is replaced by an env knob that turns it OFF, named `PLANAR_NO_<RULE>` (`PLANAR_NO_CURVES` covers curves+bays, `PLANAR_NO_COVERAGE`, `PLANAR_NO_GRID_COND`, `PLANAR_NO_STAGEA_NODEGAPS`, `PLANAR_NO_FLANK`, `PLANAR_NO_NODE_FLANK`, `PLANAR_NO_NODE_CUSP`). Print one line at the probe start listing the rules on.
- The contraction/stage B pins keep exactly the options their fixtures were drawn with: `StageBOptions` (and the pins' stage A where used) take the rule set explicitly (a small struct or bool parameters local to the test file, your choice of the smallest form; name it in the report), and each pin passes the set its fixture's header or commit names. No pin result may change.
- `PlanarDrawOptions` defaults in `planar_draw.h` are NOT changed (that is a production decision, D435's pattern, for later).
- Docs: `docs/junction-tool/wip/HANDOVER-2026-11-25.md` probe-knob list -> a short note that the knobs are now `PLANAR_NO_*` (point at this brief).

## 3. Tests and runs
- `JT_JOBS=4 tools/junction/jt fast build-rel '[contraction]' 5`, `... '[planardraw]' 30`, `... '[planardomain]'`, `... '[planardomainexact]' 30`: every result unchanged.
- Probe T1 with no env at all except `JUNCTION_MILP_INCUMBENT=1`: report the rules-on line, stage A status/time, stage B, polish, judge, domain, `rig:` (with `PLANAR_RIG_OUT` to the scratchpad). It must match today's signalled T1 run's judge/domain (accept, 0 breaks).
- Probe T2 likewise (plus `PLANAR_STAGEA_COORD=128`): report the same lines; T2 stage B Infeasible is the known limit: report, do not chase.

## 4. Commit
- `Codechange: Junction: the Slots probe draws with the v1 rule set by default`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## 5. Report
References read; commit; the rule-set parameter's form; suites; T1 and T2 probe lines; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
