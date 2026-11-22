# BRIEF domain-9: a structure's end run stays off a node's switch tile

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 45 minutes wall. No rerun loops. Never run git stash.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- The X1 suite finds 2 too-loose polylines (`[head]`): B1 crosses A1 close to the split at A1's start, and the tunnel's end run (portal and ramp, 2 tiles) would sit on the split's switch tile (tile (2,2): AC1, C1, A1). The `end` rows (planar_draw.cpp, "Spans" block, ~line 972) measure the end run from the start of the separator's span piece, and that piece may start at the node point when the pieces before it are 0. The game cannot put a portal or ramp on a switch tile (RECON rule 3 and 6; pin 7).
- Rule: the travel along the separator from a node at its start to its first crossing in the structure is at least the end run plus one tile (the switch tile); mirrored at its end.

## 2. Read first
- planar_draw.cpp: the Spans block (~930-980: `u`, `least_run`, `len`, the `end` rows), the `length[t][k]` Length type (variable, coefficient), and how the separator's track start/end are found (`map.expanded.lines[t].from/to`, `.port`).
- The harness file (rule sets, `PLANAR_DOMAIN_RULES`).

## 3. Changes
- `PlanarDrawOptions::end_node` (bool, default false, a probe; D428): when set, per structure with `least_run > 0` whose separator starts at a node (not a port): one row `sum over pieces k < piece of length[sep][k] + u.front() >= 2 * least_run + 2`, tag `end <s> node`; whose separator ends at a node: `sum over pieces k > piece of length[sep][k] + (length[sep][piece] - u.back()) >= 2 * least_run + 2`, same tag. Lengths as the program's Length terms (variable times coefficient).
- Harness: the suffix `+endnode` in `PLANAR_DOMAIN_RULES` sets `end_node` on the read-outs (combinable: `span+lead+endnode`).
- `src/junction/planar/CLAUDE.md`, drawing program section: one bullet for `end_node` (the rule, probe, off by default).

## 4. Tests
- Fast (harness file): "planar draw: end runs stay off a node's switch tile": X1 with `end_node`: the program check (`CheckPlanarProgramOnPoints`) on the first too-loose example's witness representation (from `encoding-x1-span_lead.txt`, the `[head]` group) breaks `end 0 node`; on the base it holds. Red: without the option neither row exists.

## 5. Gates and output
- `tools/junction/jt fast build '[planardomain]' 5`; `tools/junction/jt gate build-rel`.
- Release `span+lead+endnode` and `plain+lead+endnode` (new files `encoding-x1-span_lead_endnode.txt`, `...plain_lead_endnode.txt`), timeout 300.
- One commit: `Add: Junction: end runs stay off a node's switch tile (probe, D428)`, Co-Authored-By Claude Sonnet 5.5.

## 6. Report (short)
Commit; red evidence; per set the totals (other shape, both accept, both reject, too strict, too loose, undecided) and group keys with counts and first examples; suites; state. When you have reported, stop.
