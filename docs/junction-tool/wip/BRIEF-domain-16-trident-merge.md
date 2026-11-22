# BRIEF domain-16: the trident's zero-length track; a focused merge configuration

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 60 minutes wall. Test code only (no gate). One Release measurement each for K1 and M1, rule set `span+lead+endnode+crosspiece`. Record findings; fix no rule. Never run git stash. Do not touch branch wip-defaults.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- domain-15 (6a82db386e): K1's trident case (BC1 length 0) stops the read-out ("track BC1 has no piece"); M1's enumeration is 95% off grid, gives 2 both-accept polylines and fires no lead row, so the merge's lead END rows are still unexercised.
- D433 (DECISIONS.md): a node-to-node track with no event may have total length 0; its contracted writing keeps one piece of length 0 (its first Shape heading).

## 2. Changes (harness `src/tests/junction/planar/planar_domain_encoding.cpp`)
1. `ContractedVariant`: a track whose polyline has no run (all points equal) keeps exactly one piece, the variant's first piece heading for that track, length 0 (two equal points); `steps` empty for it. Everything else as now.
2. K1: restore BC1 in {0, 2, 4, 6} in K1a (domain-15 brief).
3. M1, replace M1a and M1b by two focused configurations on the S-bend just before the merge (contracted judging means only real geometry counts, so vary real geometry, on the grid):
   - **M1c (B1):** B1 arrives at the merge point (16,5) on heading NE. Vary the S-bend formed by B1's last three pieces before the node, read from B1's pieces after flex: its NE flank before the insert in {1,2,3}, the insert (heading N or E as the flex pair gives, axis: {0,2,4}), its NE flank after the insert in {0,1,2}; B1's first N run absorbs the rest so B1 still starts at (13,12) and ends at (16,5) (compute it; skip a geometry whose N run would be negative or off the grid).
   - **M1d (A1):** the same for A1's last S-bend before the node (A1 arrives on heading E; inserts NE or SE; axis flanks {0,2,4}, diagonal insert {1,2}), A1's first run absorbing the rest so it still spans (4,5) to (16,5).
   If "the S-bend just before the merge" does not exist in a track's pieces after flex, stop and report its pieces and flex middles.
4. The report prints, per configuration, the number of lead rows over the judged variants (it must be above 0 for M1c and M1d; if 0, say so: that is a finding).

## 3. Gates and output
- `tools/junction/jt fast build '[planardomain]' 5` (time under 1 s Debug).
- Release once each: K1 and M1 (overwrite / write `encoding-k1-...txt`, `encoding-m1-...txt`), timeout 300.
- One commit: `Change: Junction: the trident's zero-length track and a focused merge in the domain suite (D433, D428)`, Co-Authored-By Claude Sonnet 5.5.

## 4. Report (short)
Commit; per configuration: classes (other shape, both accept, both reject, too strict, too loose), off-grid count, lead rows, group keys with counts and first examples; wall times; state. When you have reported, stop.
