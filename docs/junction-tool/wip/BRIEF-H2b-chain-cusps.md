# BRIEF H2b: a beside track's parent chain; the closure probe opens cusps

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch wt-shape, head HEAD_HASH. Builds: build (Debug), build-rel (Release), current. Logs: /tmp/claude-1000/-home-pete-OpenTTD/315c4ccd-306d-453c-be89-5dda9a8e71c5/scratchpad/h2b/.
- Implements: docs/junction-tool/wip/SPEC-shape.md §2.1 "Mirrored crossings" and §4 H8 (a beside track equals its parent over its range; D349: a row follows its lane through bends), and §5 Q1 (closure "once Slots opens the cusps"). Follows H2 (7b0e4255d5) and its diagnosis: (1) on T3 a row runs past its first parent part, so the parent must be the chain of its lane's parts; (2) the closure LP put legs that meet a node at the same heading on one point, which only Slots may separate.

## Read first (once each, with offset/limit)
1. src/junction/planar/planar_shape_layer.cpp: the oracle's `Parent`, `MirrorOf`, H4 mirror check, H8; the program's beside and mirror rows.
2. src/tests/junction/planar/planar_shape_layer.cpp: `ClosureLp` and the probe.
3. src/junction/planar/planar_regions.h lines 89-112.

## Commit A: the parent chain
- **Definition** (one anonymous-namespace function used by the oracle and the program, replacing `Parent`): for a Copy or Row track b, its chain is: start at the straight leg of b's start node's site; then repeatedly, at the current track's end node, the out track whose `part_of` equals b's `line`; stop at the track whose end node is b's end node. If the walk fails (no such out track, or it reaches a port first), the oracle reports "H8: no parent chain for b" and the program returns false with that why.
- **Correspondence:** b's k-th crossing mirrors the chain's k-th crossing (the chain's events concatenated in order), with the same crosser; b's start point ↔ the chain's first point; b's end point ↔ the chain's last point; b's k-th event point ↔ the chain's k-th event point.
- **Program:** replace the beside rows with: H[b] at each of its points = H at the corresponding chain point; and, for each stretch of b, a binary u: every chain stretch lying between the two corresponding chain points has turn >= -M(1-u) and turn <= M u (M = 64), so the chain turns one way across b's stretch, as b does. Mirror rows unchanged otherwise (crosser on one heading across both crossings, the angle shared).
- **Oracle H8:** b.pieces equals the chain's pieces concatenated (at each chain node the next track's first piece is the previous track's last, counted once); b.event_piece[k] equals the index, in that concatenation, of the chain's k-th event's piece; the crosser's piece is equal at both crossings (unchanged). H4's mirror check uses the new correspondence.
- **Tests:** red first: a test "t3 first variant shapes" (`BuildPlanarStructure(PlanarTText(3), MakeTestJunctionSpec(), 1)`, variant 0): `ShapePlanarStructureVariant` returns true and the oracle returns empty; record its failure before the change ("crossing N is in no structure and mirrors no crossing"). Existing tests stay green. Tag `[planarshape]` if it runs under 1 s in Debug, else `[planarshape][slow]`.
- Commit: `Fix: Junction planar Shape beside tracks follow their parent chain (H2b)`; update the "Shape layer" bullets of src/junction/planar/CLAUDE.md (the chain replaces "the straight leg of its split node" wherever the beside rule or mirrors are described).

## Commit B: the closure probe opens cusps (test file only)
- In `ClosureLp`: at every node, a **parallel leg** is a leg that is not the site's straight leg and whose heading at the node equals the trunk heading mod 8 (at a split, its first piece ≡ the in track's last; at a merge, its last piece ≡ the out track's first); a fork's two legs are both parallel legs when equal to the trunk. A parallel leg's point at the node = node point + w * σ * STEP[(h + 2) mod 8] (both coordinates), with w >= 0 a new continuous variable per (node, leg), h the trunk heading mod 8, σ the leg's side as the program's node rule reads it (Side against the straight leg, else the site's side; at a fork against the other leg). Every other leg keeps the node point. This stands in for the S-bend Slots will make there; nothing else changes.
- Add one sentence to the probe's comment: the offset is the cusp opening Slots owns (SPEC-shape §3 hand-on (1)).
- Run the probe in Release and copy its lines into the report. For each hub with any closure-infeasible variant, also write the first such variant's infeasible row tags to <hub>-iis.txt in the log dir.
- Commit: `Add: Junction planar Shape closure probe opens cusps (H2b)` (no CLAUDE.md change: the probe is a test).

## Do not
- Do not change Structure, the map, or any rule of the program other than the beside rows.
- Do not relax any row to make a variant shape or close; an infeasible result is the answer.

## Stop conditions
- The t3 test still fails after commit A for a reason other than the chain (give the why or the oracle messages).
- A chain is ambiguous (two out tracks with b's line at one node).
- The build fails outside your files.

## Gates (both commits)
- Release build (zero warnings in your files); `tools/junction/jt fast build '[planarshape]' 5`; `tools/junction/jt gate build-rel` within budget (it stood at 27.0 s of 30 s: report the time; do not hide it).
- Trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>` on both commits.

## Report
- References read; commits (hash, subject); red evidence; functions added or removed (names only); probe lines; gate results and times; stops; state (`git status --porcelain`, `git log -2 --oneline`).

When you have reported, stop.
