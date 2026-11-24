# BRIEF d449-p1: which S-bends a legal drawing uses, and which rules explain them (D449 proof, step 1)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 75 minutes. Solves: one 15 s stage A export per hub (T1, T2, T3 if it shapes), CP-SAT at most 120 s per hub (8 workers). No other solves.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng2`, branch `wt-c2`, head `<HEAD>`. Build Release `build-rel` (`cmake --build build-rel --target openttd_test -j4`). Never touch ng1 or any openttd process. Never use git stash. OR-Tools venv: `/tmp/claude-1000/-home-pete-OpenTTD/06bbdccc-e14a-42a9-b25a-da0995d90c4b/scratchpad/ortools-venv/bin/python`.
- D449 (DECISIONS): a bend pre-program decides the S-bends before Slots sizes lengths. Evidence so far: `docs/junction-tool/wip/HANDOVER-2026-11-25.md` §7 (V's 197 recon), `wip/probes/shape/recon-197-steps*.md` (read their summaries only), `wip/probes/slots/flank-lock.md` (CP-SAT sections). HiGHS sizes a FIXED S-bend set in 0.1-1.5 s; it cannot choose one (T2: nothing in 15 s; CP-SAT: 1.9 s).
- Tools: `tools/junction/planar_cpsat.py` (`--values=`, `--lengths=`: piece lengths per track then piece, in variable units), the probe's `PLANAR_STAGEA_EXPORT`, `PLANAR_DUMP_TRACK`.

## 2. Task (analysis; one small probe knob allowed)
1. For T1, T2 (and T3 with `PLANAR_SLOTS_HUB=T3` if the probe supports it; if not, say so): export stage A with the probe's defaults (T2/T3: `PLANAR_STAGEA_COORD=128`), solve with CP-SAT `--lengths=`, and map lengths back to (track, piece, heading, flex middle or flank). If mapping needs it, add ONE env knob to the probe that prints, per track, its pieces with heading and kind (flex middle / flank / fixed) and stretch/node context (`PLANAR_DUMP_FLEX=1`), committed as `Add: Junction: probe knob, flex layout dump`.
2. Table per hub: every flex insert, used (length > 0) or not, with: track, kind of track (Line/Copy/Row), stretch position (node-adjacent: which node, split/merge, straight leg or diverging leg, sibling side; or mid-stretch), stretch has a diagonal piece (yes/no), insert direction relative to the sibling or the neighbouring lane (toward / away / n/a), size in lanes.
3. Rules from the 197 recon, each a predicate on that table: (R1) a diverging leg steps toward its sibling after a split; (R2) before a merge, away from it; (R3) a prio joiner turns in at the node (the D451 cusp insert); (R4) a step on a stretch with a diagonal piece is length-coupled (optional); (R0) every other insert is 0. For each hub: how many used inserts each rule predicts, how many used inserts no rule predicts (list them), how many rule-predicted inserts CP-SAT left at 0 (list them).
4. Is CP-SAT's choice unique? Rerun CP-SAT once per hub with the objective (`--objective`) and compare the used set (same / differs where). Only if time allows.

## 3. Output
- `docs/junction-tool/wip/probes/d449/p1.md`: the tables (compact), per-hub counts per rule, the unexplained lists. At most 120 lines. Commit it: `Doc: Junction: D449 P1, S-bends a legal drawing uses vs the recon rules`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## 4. Do not
- No rule implementation, no change to the drawing program, defaults or Shape. No interpretation beyond the counts: the orchestrator designs from them.

## 5. Report
References read; commits; per hub: inserts total/used, predicted by R1..R4, unexplained used, predicted-but-unused, uniqueness; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
