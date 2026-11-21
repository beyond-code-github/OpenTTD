# Brief X-nl: a leg's raw headings include its switch-tile piece

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng2`, branch `wt-x`, head `{HEAD}`. Python only, `python3 -I`.
- Finding (X8 rerun, 2026-11-22): the extractor's 197 loads in C++, but Shape's checker reports H3 "leg L2 at node 0 is at the trunk heading" at many nodes. The elastic drawing stretches node rows (181 of 324 slack).
  - Example: `node L3 -> L1=E1, L2=F1` with `shape L3: 0`, `shape L1: 0 1`, `shape L2: 0 1 2 1`. Both legs start at the trunk heading 0.
  - Cause: a line's pieces exclude the node piece (the switch tile). A diverging leg's 45-degree turn happens on the switch tile (the node piece's own track bit toward that leg), so the leg's first off-node piece is often back at the trunk heading.
  - The model (D404, `src/junction/planar/CLAUDE.md` "Shape layer": a node's non-straight leg starts one eighth off the trunk toward its own side) treats a node as a point. The turn belongs to the leg's first piece.

## 2. Read first
- `extract.py`: `shape()` (~523 onwards: `shape_segs`, the raw heading list per line, the projection to monotone stretches and the steps, `Sh.lens`); the node-piece heading helper in `structure()` (e3f6d414df); `S6.nodes` (each node piece's kind, trunk, legs, straight leg).
- `out/psg197/shape.txt`, `steps.txt`, `lengths.txt`, `report.txt`.

## 3. The rule (generic)
- For every split node piece and each of its legs that is a line with track: prepend to the leg's raw heading list the node piece's own heading on that leg's side (the travel heading of the switch tile's track bit that leads into that leg), with its length (a diagonal 1, an axis 2, as the other raw pieces), unless the leg's first raw heading already equals it.
- For a merge node piece, mirror it: append the node piece's heading on that leg's side to the leg's raw list.
- The straight leg's side bit has the trunk heading, so it adds nothing new (equal to its first heading); apply the same code to it and let the equality skip it.
- Then the existing projection runs unchanged. A raw list that becomes non-monotone (diverge, then straighten) gets its step recorded in `steps.txt` as for any other stretch.
- Empty lines (`T.empty`) keep their one heading (00f492f289).
- If a leg's side bit of the node piece cannot be identified (more than one bit, or none), stop and report the node and tile.

## 4. Checks
- Before/after counts on psg197 (one-off script, not committed): legs whose first projected heading equals their node's trunk heading, split into straight legs and the others. After the change, among non-straight legs: count and list any that still do.
- `compare.py psg197`: shape identical projections (97 of 99 before), steps, and any other difference class, before and after.
- psg186 runs.

## 5. Do not
- Change the node statements, straight-leg rules, lane directions, the 90-degree rule, or the empty-line headings.

## 6. Commit
- `Fix: Junction corpus: a leg's raw headings include its switch-tile piece`; regenerate and commit `out/psg197` and `out/psg186`; ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Do not commit `__pycache__`. README: one sentence.

## 7. Report (short, the full content)
- References read; commit; the counts before/after; compare.py before/after; psg186
- Stops; state (`git status --porcelain`, `git log -1 --format='%h %s'`)

When you have reported, stop.
