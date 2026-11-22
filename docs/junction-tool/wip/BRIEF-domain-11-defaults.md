# BRIEF domain-11: the suite-cleared rules on by default

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 45 minutes wall. No hub runs beyond the gate. No rerun loops. Never run git stash.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- D428: a rule lands as a default when the domain suite is clean. On X1 it is exact (0 too strict, 0 too loose) with `node_lead` (D429, D430), `end_node` and `cross_piece` (STATUS, 2026-11-23). D431 records the audit.

## 2. Changes
- `PlanarDrawOptions`: `node_lead`, `end_node`, `cross_piece` default `true`; their comments say "on by default (D428: X1 domain suite exact)" and keep the toggle (false is the old behaviour).
- `PlanarDrawPipelineOptions`: add the same three (default true) and pass them into every stage's `PlanarDrawOptions`, as `strict_span` is passed today (planar_draw.cpp: every `CheckPlanarDrawable(v, ..., {...})` call in `DrawPlanarPipeline` and the final options).
- The harness (`planar_domain_encoding.cpp`): set the three explicitly from the `PLANAR_DOMAIN_RULES` suffixes (absent suffix = false), so the report runs mean what they did.
- `src/junction/planar/CLAUDE.md`: the three bullets say on by default; D431 one line under the drawing program section: "The E5 pipeline is a measurement tool (D431): Slots is built from suite-cleared rules, sides by construction and whole tiles."

## 3. Tests and gates
- `tools/junction/jt fast build '[planardomain]' 5` and `tools/junction/jt fast build '[planar]' 15` (Debug; report the time).
- `tools/junction/jt gate build-rel`.
- Release: `span+lead+endnode+crosspiece` once (overwrite its report): it must still be exact.

## 4. Stop conditions
- Any gate test that passed before now fails: report its name and failure; do not change the test or the rule.
- Anything not covered.

## 5. Output
One commit: `Change: Junction: node lead, end runs off switch tiles and crossing pieces with length on by default (D428, D431)`, Co-Authored-By Claude Sonnet 5.5. Report: commit, suites with times, the report's totals, stops, state. When you have reported, stop.
