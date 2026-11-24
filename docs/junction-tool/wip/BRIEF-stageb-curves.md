# BRIEF stageb-curves: curve spacing and bays in stage B and the polish (probe), T1 for the rig

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 45 minutes. Probe runs: 2 on T1 (without and with the knob), 15 s per solve.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Build Release `build-rel` (the rig server and the engineer's client run `build-rel/openttd`: never start, stop or touch them; rebuilding is fine). Load limit `JT_JOBS=4`, cmake `-j4`. Never touch ng2.
- Why (engineer, looking at T1 in the rig: "it's using CL1 right?"): the probe's fixture sets `options.bays = options.curves = false` (planar_domain_encoding.cpp:152, a domain-suite default), so no stage draws curve spacing. The spec at 160 km/h derives `rules.curve_same` 5 half tiles (D67, `DeriveCurveSpacing`). The drawing program has the rows (D420: `curves`, `bays`); stage B and the polish work on a fixed contracted shape (`plain`, steps cleared, so every bend is a real bend), where they are exact.

## 2. Read first
- `src/junction/planar/CLAUDE.md`, Drawing program bullet "Bays and curve spacing (D420)".
- `planar_domain_encoding.cpp`: `StageBOptions`, the probe's polish options (`sp`), the `PLANAR_RIG_OUT` hook.

## 3. Changes
- Env `PLANAR_STAGEB_CURVES` in the probe: when set, stage B's options and the polish's options get `curves = true` and `bays = true` (applied in the probe after `StageBOptions` returns, so the contraction pins, which call `StageBOptions`, are unchanged). The stage B and polish lines print `curves+bays` when on.
- No other change.

## 4. Probe runs (Release, T1)
Both with `PLANAR_SLOTS_HUB=T1 PLANAR_FLANK=1 PLANAR_NODE_FLANK=1 PLANAR_GRID_COND=1 PLANAR_STAGEA_NODEGAPS=1 JUNCTION_MILP_INCUMBENT=1`, once without and once with `PLANAR_STAGEB_CURVES=1`, the second with `PLANAR_RIG_OUT=/home/pete/OpenTTD/.claude/ng1/docs/junction-tool/wip/rig-2026-11-25/T1-v2-curves.json`. Report per run: stage A status/time, stage B rounds and status, polish length, judge and domain lines, the `rig:` line; with curves, the number of `curve` rows and any `curve`/`bay` row in a conflict. If stage B is Infeasible with curves, report the conflict families; do not chase.

## 5. Gates and commit
- `JT_JOBS=4 tools/junction/jt fast build-rel '[contraction]' 5` (unchanged).
- Commit: `Add: Junction: probe knob, curve spacing and bays in stage B and the polish`, and the JSON if written (`Doc: Junction: T1 v2 rig layout with curve spacing`). End each with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## 6. Report
References read; commits; the per-run lines of part 4; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
