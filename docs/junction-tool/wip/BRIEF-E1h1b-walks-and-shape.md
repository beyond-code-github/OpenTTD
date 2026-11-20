# BRIEF E1h-1b: one walk per line by exclusive pieces; re-derive the affected crossings; PSG 197's shape (data only)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Repo /home/pete/OpenTTD, branch feature/junction-tool, head HEAD_HASH (check `git log -1`). Main checkout, data only: no build, no tests, one Python process at a time. Budget 45 minutes.
- E1h-1 (brief docs/junction-tool/wip/BRIEF-E1h1-psg197-shape-file.md; read it, its conventions, outputs and findings stand) stopped: the crossing points of F1c and D12f were computed on their first walks, which run over a sibling's track (E1c's, D12e's); their second walks do not contain those points. Its scripts are in /tmp/claude-1000/-home-pete-OpenTTD/48e4a4c3-7120-4e24-9896-a0f4d5c22c1b/scratchpad/ (shape_first.py, shape_second.py and their outputs): start from them.

## Ruling (orchestrator): the walk of a line
A physical piece belongs to one line. For every line with more than one walk (E1h-1 found D12e, D12f, D12g, F1c), use the walk none of whose pieces (tile, piece) lies on any other line's walk, the node tiles at its two ends excepted. Exactly one such walk: use it. None or several: stop and report the line and its walks. Every other line keeps its single walk.

## Step 1: re-derive the crossings that touch a re-walked line
For every pair of the 121 (psg197.txt's `cross` statements) with a line whose walk changed: recompute its crossing point on the two lines' displaced polylines (marks.py's method), the mark (marks.py's rule), and the pair's position on both lines. Then rebuild the `cross` order of every line that has such a pair (positions along the line, as order.py). Write the result as docs/junction-tool/wip/probes/e1/cross-rewalked.txt: the full set of 53 `cross` lines (unchanged lines byte-identical), and list in the report every line whose order or marks changed (old and new).
- A pair with no intersection on the new walks, or with two: stop and report it.
- Do NOT edit psg197.txt (the orchestrator applies the file after the C++ planarity check).

## Step 2: the shape file
As E1h-1's Output 1 and 2, on the walks of the ruling above and the crossing points of step 1; the `at` lines follow cross-rewalked.txt's order. Outputs: tools/junction/topologies/psg197.shape.txt, docs/junction-tool/wip/probes/e1/shape.py, shape-out.txt (with E1h-1's findings list).

## Commit
`Doc: Junction E1h-1b: PSG 197 walks by exclusive pieces, re-derived crossings, shape from the save`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`; only cross-rewalked.txt, shape.py, shape-out.txt, psg197.shape.txt. Do not push.

## Report (short)
References read; commit; the walk chosen per ambiguous line (and why the other was excluded: which line's pieces it shares); the changed `cross` lines (old -> new); counts and findings of the shape file; stops; `git status --porcelain`, `git log -1 --oneline`. When you have reported, stop.

## Amendment (orchestrator, 2026-11-20, after the first E1h-1b stop: D12g only node tiles; D12e/D12f mutually ambiguous)
Replaces the ruling's per-line test:
- Pieces on node tiles (any node's tile, not only the line's own ends) are never counted as shared.
- Choose the walks of all ambiguous lines (D12e, D12f, D12g, F1c) jointly: over every combination (one walk per ambiguous line), count the pieces each chosen walk shares with any other line's walk (single-walk lines and the other chosen walks), node-tile pieces excepted; take the combination with the least total. A unique minimum: use it. A tie: stop and report the tied combinations and their counts. Report the count of the chosen combination and the next best.
- A line whose walk has no piece off node tiles (D12g): its shape is one piece, the heading of the chord from its start node tile's centre to its end node tile's centre; it has no crossings.
Steps 1 and 2, the commit and the report stand. Start from your scratchpad excl.py.
