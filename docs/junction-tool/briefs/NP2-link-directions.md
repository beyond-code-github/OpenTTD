# NP2: directions per free link, not per path (projection of the G1 seed, step 2)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (persistent; Debug `build`, Release `build-rel`). First: `git checkout -B wt-np2 <HEAD>` (given at launch). Never use git stash.
- NP1 (merged) traces the node input by a topology. LL_RR's template now fails with "path P moves both ways along y": an LL_RR lane fans north off its port row, then runs south to the S port. DESIGN-N3 §6 "Monotone paths" made every path monotone so one sign per path (JunctionNodePath::dir, dir_y) could turn unit gaps into differences. The unit formula only needs monotone motion between the two points of each gap (units change between p and q = 2 d_x Δx + 2 d_y Δy - 2 Σm over the free links between p and q). So: a sign per free link, and each gap takes its signs from the free links between its two points.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); docs/junction-tool/TESTS.md (`[topology]`, `[nodes]` rows); DESIGN-N3.md §6 (the "Monotone paths" and "Units" bullets).
- src/junction_nodes.h :20-80 (JunctionNodeLink, JunctionNodeLateGap, JunctionNodePath).
- src/junction_nodes.cpp: ReadPoints' direction code (:225-260, `plus`/`minus`); NodeSpan (:46); AddPathDiffs from `const int d = node_path.dir;` (~:382) through rule 3's prio row (~:460); the bridge rule's `const int d = across == Axis::X ? ...dir : ...dir_y;` (~:606); the late gaps in AddJunctionNodeLateConstraints (~:749-760); `link_direction` in the search (~:1222); DiffJunctionNodeTemplates' links, paths, late_gaps entries (~:1855-1880).
- src/tests/junction_nodes.cpp "the template comparator finds every member" (~:1425); src/tests/junction_topology.cpp NP1's test "the LL_RR seed traces into the node template".

## Tests (record each red)
1. **Outer (edit NP1's test; `[topology]`): "topology: the LL_RR seed builds a node template"** (rename it): REQUIRE tpl.error is empty (INFO it). Red: the "moves both ways along y" error (record it).
2. **`[topology]` "topology: link directions match the T's path directions"**: on the traced flat T (`BuildJunctionNodeTemplate(layout, spec)` of the T seed from SignalledOf), for every path and every consecutive point pair that is a free non-diagonal link, link.dir == (link.axis == Axis::X ? path.dir : path.dir_y); and for every late gap, gap.dir == its path's dir and gap.dir_y == its path's dir_y. Red: the members do not exist; write the test first with the new members and record the compile failure as the red for this guard test only.
3. The comparator test (junction_nodes.cpp): add FIELD(dir) to "links" and FIELD(dir), FIELD(dir_y) to "late_gaps" (the only allowed edit of an existing test besides test 1).
- Every other existing `[nodes]`, `[nodespeed]`, `[topology]` test stays green and unedited. The T's template must not change except the new members.

## Changes
1. **Members.**
   - JunctionNodeLink: `int8_t dir; ///< A free X or Y link: +1 when travel grows its axis coordinate, -1 when it shrinks it; 0 for a rigid link or a free diagonal (whose signs are its JunctionNodeDiagonal dx, dy).` (after `axis`; update the aggregate initialisers).
   - JunctionNodeLateGap: `int8_t dir; ///< The x sign of the free links between p and q.` and `int8_t dir_y; ///< The y sign of the free links between p and q.` (after `u`).
   - JunctionNodePath::dir and dir_y keep their meaning for monotone paths; change their comments to "...; 0 when it has none or moves both ways (then only per-link and per-gap signs apply)".
   - NodeSpan: `int8_t dir; ///< As JunctionNodeLink::dir.`
2. **ReadPoints.** When a free X or Y link is made, set its dir from the flat pieces (sign of dx for X, dy for Y; the existing code already computes them) and the span's dir likewise. Remove the two "moves both ways" Fail lines; path.dir = plus && !minus ? 1 : (minus && !plus ? -1 : 0), the same for dir_y.
3. **The gap rule (add_gap in AddPathDiffs).** In its loop over the free links between p and q also collect signs: a free diagonal contributes x sign diag.dx and y sign diag.dy; a free X link x sign link->dir; a free Y link y sign link->dir. If two x signs or two y signs differ: `this->Fail(fmt::format("path {} points {},{}: moves both ways along {}", pi, p, q, "x" or "y"))` and return. d_x, d_y = the collected signs (0 when none). Use d_y in place of the path's d_y in the Y difference; push the late gap with d_x and d_y.
4. **Rule 1 (free links):** `link->axis == Axis::X ? d : d_y` -> `link->dir`.
5. **Prio row (rule 3):** the prio row's dir (today `static_cast<int8_t>(d)`) = the x step of the merge piece's exit direction (`TileIndexDiffCByDiagDir(TrackdirToExitdir(trackdir)).x`); if it is 0, Fail("path {}: prio merge not on an X piece", pi).
6. **Bridge rule (~:606):** `const int d = span.dir;`.
7. **Late gaps (AddJunctionNodeLateConstraints):** d_x, d_y from gap.dir, gap.dir_y.
8. **Search `link_direction`:** return `link.dir` (drop the path scan).
9. **Comparator:** links compare dir; late_gaps compare dir and dir_y.
10. Delete the now unused locals `d`, `d_y` in AddPathDiffs only if nothing else in the loop reads them.
11. DESIGN-N3.md §6, the "Monotone paths" bullet: replace with "**Signs per link** (NP2): each free link has its own sign along its axis (a free diagonal its dx, dy); a gap between points p and q takes the signs of the free links between them, which must agree (template error otherwise). A path may move both ways (an LL_RR lane fans north, then runs south)."

## Do not
- Do not change the units formula, any rule's constants, the solve phases, the render, or the anchor. Do not touch NodeAnchorShift, heights or signals.

## Stop conditions
- An existing test fails; test 1 after the change fails with another template error: report the error text, the path's from_slot/to_slot and the points' tiles, and stop (do not fix the next error).
- Anything not covered.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `tools/junction/jt fast build '[nodes]' 5`; `tools/junction/jt gate build-rel` (report the count; NP1's count before).
- One commit: `Change: Junction node template signs per free link, paths may move both ways`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. TESTS.md `[topology]` row: add "NP2" and the new count.

## Report
The standard report, plus LL_RR template counts (points, links, classes, diagonals, late gaps). When you have reported, stop.

## Amendment 1 (orchestrator, after stop 1 and stop 2)
Derivation (units of the links between p and q, δ = change from flat): ΔU = Σ_Xfree 2 s_l (δx_to - δx_from) + Σ_Yfree 2 s_l (δy_to - δy_from) + Σ_diag 2 m_d. Points joined by a rigid or Y-free link share an X class; across a diagonal δx changes by dx_d m_d. So if the free X links agree on one sign d_x: ΔU = 2 d_x (δx_q - δx_p) - 2 d_x Σ_diag dx_d m_d + Σ_Yfree 2 s_l (δy_to - δy_from) + 2 Σ_diag m_d. After the Y snap every term but δx_q - δx_p is known, so a late gap needs only the free X links to agree; the diagonals and the free Y links may have any signs. For a monotone stretch this equals today's formula (d_x dx_d = 1, d_y dy_d = 1), so the T does not change.
Changes to the brief:
- **JunctionNodeLateGap**: `int8_t dir; ///< The x sign of the free X links between p and q.` and, in place of dir_y, `std::vector<uint32_t> y_links; ///< The free Y links between p and q (indices into links).` (after `diagonals`). Comparator: dir and y_links. Comparator test: FIELD(dir), VEC(y_links).
- **add_gap**: with an X run between p and q (late): only the free X links' signs must agree (else Fail "moves both ways along x"); push {path, p, q, g, u, diagonals, dir = d_x, y_links}. Without an X run (a Y difference now): the free Y links' signs and the diagonals' dy must agree (else Fail "moves both ways along y"), as in the brief.
- **Late constraint**: with m_d = NodeDiagonalSteps(...) and the Y shifts known: `yfree = Σ_{l in y_links} 2 * links[l].dir * (shift_y(points[l.to].ycls) - shift_y(points[l.from].ycls))`, `k = d_x * (q.tile.x - p.tile.x) + CeilHalf(g - u - yfree - 2 Σ m_d + 2 d_x Σ dx_d m_d)` where shift_y(c) = values[c] - tpl.flat[c]. Keep the existing NodeAddAtLeast call shape by d_x's sign.
- **Test 2**: for every late gap, gap.dir == its path's dir (drop the dir_y check).
- The keep_slot use you found (`span.axis == X ? d : d_y` -> `span.dir`) is accepted.
