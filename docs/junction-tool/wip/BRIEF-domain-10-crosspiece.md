# BRIEF domain-10: a crossing sits on a piece with length

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 45 minutes wall. No rerun loops. Never run git stash.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- The last too-loose X1 polyline under `span+lead+endnode` (`encoding-x1-span_lead_endnode.txt`, `[head]`): B1's crossing piece (piece 4, heading N) has length 0 in the witness, so B1 actually crosses A1 at the vertex between two NW diagonal pieces, on a tile edge, occupying two of A1's tiles; the rows read Shape's axis crossing (one tile), so the tunnel's end run reaches the split's switch tile. A crossing must sit on a piece that exists. D414 (a crosser may cross at its piece's END, a bend at the crossing, as V does at L40) stays: the piece still has length.

## 2. Changes
- `PlanarDrawOptions::cross_piece` (bool, default false, a probe; D428): per crossing c and side, the piece the crossing sits on has length >= 1 half tile (diagonal) or >= 2 (axis): one row `length >= least` on that piece's Length (variable times coefficient), tag `cross <c> piece`. Write it next to the `cross` rows (planar_draw.cpp ~741).
- Harness: suffix `+crosspiece` sets it (combinable with `+lead`, `+endnode`).
- `src/junction/planar/CLAUDE.md`: one bullet (probe, the rule, why: a zero-length crossing piece crosses at another angle).

## 3. Test (fast, harness file): "planar draw: a crossing sits on a piece with length"
The witness of that too-loose polyline (copy its piece lengths from the report) breaks `cross 0 piece` with the option and not without; the base holds. Red: without the option no such row.

## 4. Gates and output
- `tools/junction/jt fast build '[planardomain]' 5`; `tools/junction/jt gate build-rel` (if one unrelated case crashes once, rerun it alone and the gate once; report both).
- Release `span+lead+endnode+crosspiece` and `span+lead+crosspiece` (new report files, `+` as `_`), timeout 300.
- One commit: `Add: Junction: a crossing sits on a piece with length (probe, D428)`, Co-Authored-By Claude Sonnet 5.5.

## 5. Report (short)
Commit; red evidence; per set the totals (other shape, both accept, both reject, too strict, too loose, undecided), group keys with counts and first examples; suites; state. When you have reported, stop.
