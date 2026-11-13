# CF1: correct the CLAUDE.md facts the review found wrong

You execute this brief and nothing else. Edit only the CLAUDE.md files under src/junction/ and src/tests/junction/. Read-only otherwise. Keep your context small. Never use git stash.

## State
- Repo /home/pete/OpenTTD, main checkout, branch feature/junction-tool, at the head given in your launch prompt. Ignore docs/junction-tool/wip/.
- docs/junction-tool/REVIEW-opus-since-move.md §Q5 lists items (a)-(h): facts that are wrong against the code or the rulings. Read §Q5 and the findings it cites (F1, F3).
- D225 says a CLAUDE.md holds current, decided facts only. SP2 has since moved code: placement/junction_placement.cpp, placement/junction_render.cpp, signals/, junction_pipeline.cpp. The review's "T:" line numbers refer to the old junction_topology.cpp, so find the code by name.

## Do, item by item (confirm each against the code before editing)
- (a) Root: replace "no stage reads a rendered layout as its input (D201, D213)" with: "Stages read the graph, never a rendered layout (D201, D213); the level solve is the one exception today: it reads the rendered, signalled layout (junction_pipeline.cpp)."
- (b) placement: delete the "a ramp is a tile, not a graph token, and a height failure is not a graph edit" line.
- (c) placement: restate the foundation fact from rail_cmd.cpp's GetRailFoundation, which is around lines 360-380. Read it and state exactly when a lone diagonal piece gets a levelled, halftile or special foundation.
- (d) game: restate the estimate fact to say exactly which commands are test-run and which are priced from the plan (planned tiles and terraform), from junction_layout.cpp around lines 350-371 and 534-539.
- (e) topology: replace the "only build, pricing, JSON, ASCII and the rig script branch on `tunnel`" list with the full list of places that branch on `tunnel` today. Grep `\.tunnel` under src/junction.
- (f) topology: restate the bridge-straddle fact so it says a band edge carries at most one bridge (F3). Keep any part that is true for other edges.
- (g) Root: restate the checker fact with D193's condition, as D193 states it.
- (h) legacy: replace "an unreachable non-projection branch" with the fact that the window's library job calls `SizeJunctionPrio` and `BuildJunctionLibrary` live on every spec change (tool/junction_gui.cpp, around EnsureLibrary).
- Keep the format: present tense, ids in brackets, no dates, no "why".

## Commit
One commit: `Doc: Junction CLAUDE.md corrections from the review`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
For each item: the old line and the new line, one line each. Then the commit hash and `git status --porcelain`. When you have reported, stop.
