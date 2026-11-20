# BRIEF E1b2: PSG 197 crossing order by real geometry (follows E1b's stop)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Repo /home/pete/OpenTTD, branch feature/junction-tool, head HEAD_HASH (check with `git log -1`). Main checkout; docs only. Leave the untracked docs/junction-tool/wip/BRIEF-E1c-cross-parser.md alone (not yours; do not add it to a commit).
- E1b (docs/junction-tool/wip/BRIEF-E1b-psg197-cross.md, read it once: its "What to produce", "Do not", gate and report format all still apply) stopped with 4 ambiguous walks (D12d, D12e, D12f, F1c) and 20 position ties. Its script is docs/junction-tool/wip/probes/e1/order.py (150 lines) and output order-out.txt. This step changes order.py by the two rulings below and finishes E1b's deliverables.
- Budget: 45 minutes in total.

## Ruling 1: positions are real intersection points, not tile indices
- Coordinates: OpenTTD map frame, tile (x, y) covers [x, x+1] x [y, y+1]; its edge midpoints are NE (x, y+0.5), SW (x+1, y+0.5), NW (x+0.5, y), SE (x+0.5, y+1).
- Each piece of a line's walk is one segment between two edge midpoints: X: NE-SW; Y: NW-SE; UPPER: NE-NW; LOWER: SW-SE; LEFT: SW-NW; RIGHT: NE-SE. Check these names against what `N.W.pname(pc)` returns (net.py, core.py's `pl`) and stop if a piece name in the walks is not one of the six.
- A tunnel the line runs through is one straight segment from the centre of its entrance tile to the centre of its exit tile (tile centre (x+0.5, y+0.5)), replacing any tile-by-tile middle expansion.
- A line is the polyline of its segments in travel order; its arc length parameter s runs from 0 at its start.
- The crossing of a pair (L, M) is where the two polylines intersect. Its position on L is s_L at that point, on M s_M. Before intersecting, displace each line sideways by `1e-6 * (k + 1)` map units to its right of travel, k being the line's row number in the Line index table (0-based); this breaks triple points (three lines through one point, for example A1c, C23c and F2b at the centre of 95,166) the same way on all three lines.
- If a pair's displaced polylines intersect more than once, the crossing is the first intersection along L, for L the line earlier in the Line index table; report every such pair with its count (information; not a stop unless the count is even, which is a stop: an even count means the lines touch and part without crossing).
- If a pair's displaced polylines do not intersect at all, that is a stop (report the pair, both lines' segments near their closest point).

## Ruling 2: ambiguous walks
- For a line with several walks between its ends, compute the crossing order of that line, and of every partner line, under every walk. If all walks give the same order on every line, use any of them (the first found) and report "ambiguous, order-invariant" with the walk count. If they differ, stop and report the line, the walks (as piece lists) and the two orders.
- A walk may not visit the same tile twice (removes the doubling-back walks E1b reported).

## Checks (replace E1b's 1d tie check; keep its other checks)
- No two crossings of one line at equal s (after displacement): any is a stop.
- The emitted order of each line's crossings, plus the per-pair tie-free positions, is printed as before.
- Consistency: each crossing point is computed once per pair and its s_L and s_M are read from that same point, so the orders on all lines come from one drawing. Assert it: the point recomputed from L's segment at s_L and from M's segment at s_M agree within 1e-9. Any mismatch is a stop.

## Deliverables (E1b's, unchanged)
- order.py updated; order-out.txt regenerated; `cross` statements appended to docs/junction-tool/wip/topologies/psg197.txt exactly as E1b's brief says; parity probe rerun (`bad 0`).
- One commit: `Doc: Junction E1b: PSG 197 crossings in travel order by geometry (cross statements, D389)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Do not push.

## Report: E1b's format, plus: pairs with more than one intersection (count each), ambiguous lines and whether order-invariant.

When you have reported, stop.
