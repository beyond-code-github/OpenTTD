# BRIEF domain-4: rule A narrowed to S-bends toward the sibling leg

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 45 minutes wall. No rerun loops. Never run git stash.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- D429 (rule A) as built (`PlanarNodeLeadRows`, planar_draw.cpp near line 2330) makes every node end whose first flex pair starts at the end travel one game piece first. The X1 suite (`docs/junction-tool/wip/probes/domain/encoding-x1-span_lead.txt`, main checkout) shows 18 legal polylines it rejects: the straight leg stepping AWAY from its sibling at the node. The rule only needs to stop an S-bend TOWARD a sibling leg before the separation point.

## 2. Read first
- `src/junction/planar/planar_draw.h` lines 40-61 and the function at planar_draw.cpp ~2330.
- `src/junction/planar/planar_structure.h` lines 25-33 (`PlanarStructureSite`: `node` indexes `v.structure.map.expanded.nodes`, `split`, `straight` (a track, or UINT32_MAX at a fork), `side` (+1 right of travel, -1 left, 0 at a fork): where the other leg leaves or arrives).
- The test "planar draw: node lead rows on X1" in `src/tests/junction/planar/planar_domain_encoding.cpp`.

## 3. The rule (replace the body and doc comment of `PlanarNodeLeadRows`; same signature, same tags)
For each track t (skip: no pieces, no flex, and the D412 in-port line as now):
- **Start row, only when t is a leg of a split at its start** (`from` is a node n, and that node has a site with `split` true): its sibling side `sib` = `site.side` when `t == site.straight`, `-site.side` otherwise; at a fork (`straight == UINT32_MAX` or `side == 0`) or for a node with no site, `sib = 0` (both sides count).
  - The candidate inserts are t's flex middles m in ascending order, taken while every piece before m is a flank or insert of a flex pair (i.e. m == 1, then m == 3 when 2 is the flank between, and so on: stop at the first flex middle whose preceding piece is not itself a flex middle plus one). Simplest exact form: walk m over `flex` ascending while `m == 1 + 2 * k` for k = 0, 1, ....
  - An insert m steps to side `d = sign(pieces[m] - pieces[m - 1])` (+1 right of travel, lifted headings).
  - The first insert m in that walk with `sib == 0 || d == sib` gets the row: p = (t, 0), q = (t, m), s = STEP[pieces[0] mod 8], least 2. No such insert: no row.
- **End row, only when t is a leg of a merge at its end** (`to` is a node with a site whose `split` is false): the mirror. Walk flex middles descending while `m == n - 2 - 2k`; `d = sign(pieces[m] - pieces[m + 1])` taken as the side the step moves to when walked toward the node, i.e. `d = sign(pieces[m] - pieces[m + 1])`; `sib` as above; row p = (t, m + 1), q = (t, n), s = STEP[pieces[n - 1] mod 8], least 2.
- Trunks (a split's in line, a merge's out line) get no row.
- Doc comment: state the rule in these words, that it narrows D429's first cut (outward S-bends at a node are legal: X1 suite, 18 polylines), and the D412 exemption.

## 4. Tests
- Update "planar draw: node lead rows on X1": with X1's sites, list the rows the new rule gives (expect starts of A1, C1, B1, D1, each against its first insert toward its sibling; for each, assert q's point index and the insert's side against the sibling side you read from the site; write the expected indices from the sites in a comment). The same-polyline geometry from domain-3 (A1 piece 0 = 12, B1 piece 0 = 6) still has every row holding. Add: the polyline of the 18 too-strict cases' first example (A1 pieces 2 = 0, 3 = 2, 4 = 2, 5 = 0, 6 = 4 on top of the base, run moved to the front as needed) has A1's row holding. Red: the old body fails the new expectations; record the message.

## 5. Gates and output
- `tools/junction/jt fast build '[planardomain]' 5`; `tools/junction/jt gate build-rel` (one rerun allowed if only the LL² joint case fails).
- Release: `plain+lead` and `span+lead` as in domain-3c (overwrite their two files), timeout 120.
- Update the node lead bullet in `src/junction/planar/CLAUDE.md`: "a leg's end at a split (or merge) whose first S-bend there steps toward its sibling leg travels one game piece before it; outward S-bends and trunks are free".
- One commit: `Change: Junction: node lead rows only for S-bends toward the sibling leg (D429, D428)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 6. Stop conditions
- A node of X1 has no site, or a site's `straight` is not one of the node's legs.
- Anything not covered.

## 7. Report (short)
Commit; red evidence; the rows on X1; the two sets' lines (polylines: both accept, both reject, too strict, too loose, undecided) and their group keys with counts; suites; state. When you have reported, stop.
