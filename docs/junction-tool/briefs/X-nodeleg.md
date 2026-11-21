# Brief X-nl (v2): the node piece belongs to the trunk side, not to the legs

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng2`, branch `wt-x`, head `{HEAD}`. Python only, `python3 -I`.
- A previous run (v1 of this brief) stopped correctly. Its findings:
  - `shape_segs` (extract.py ~531) prepends the node piece's own segment to any line whose first piece has one predecessor node piece. So both legs of a split start with the node piece's heading.
  - The node piece is the piece whose exit edge feeds two pieces: the trunk's LAST piece. E.g. node 78,173 holds only `Y`; the switch is the next tile, 78,172, where `Y` and `Lower` share the entry edge.
  - The model's node is a point at the switch tile's entry edge midpoint (agreed in the Gemini review, D412). The straight leg's first piece is its own bit on the switch tile (heading of `Y`), and the diverging leg's first piece is its diagonal bit there (`Lower`, one eighth off).
  - So prepending the node piece to the legs puts the trunk's last piece at the start of every leg. That is why Shape's checker reports H3 "leg at the trunk heading" for all 60 split legs.

## 2. Read first
- `extract.py`: `shape()`, `shape_segs`, `crossings.segs_of` (does it prepend the same way? the crossing piece indexes must stay consistent with shape.txt), `Sh.lens`, and where node pieces are excluded from the trunk (`offnode`).
- `out/psg197/shape.txt`, `lengths.txt`, `steps.txt` and `report.txt`.

## 3. The rule (generic)
- A split node piece is the last piece of its trunk line. A merge node piece is the first piece of its out line. In both cases the node point is the edge the node piece shares with the legs.
- Legs never include the node piece: a split leg starts with its own first piece (on the switch tile), and a merge leg ends with its own last piece.
- Implement it where the node piece is attached today (`shape_segs`, and `segs_of` if it does the same): attach it to the trunk side instead of the legs. Then every geometry consumer (shape, lengths, crossing piece indexes, steps) sees the same piece lists.
- A node piece between two nodes (an empty line, or a node piece that is both a merge's out piece and a split's trunk piece): it belongs to the line it sits on. If its line is an empty line with a heading from 00f492f289, keep that. If the ownership is ambiguous, stop and report the node and tile.

## 4. Checks
- psg197 (one-off script, not committed): among non-straight split legs, how many start at their node's trunk heading, before (31 of 31) and after (expected 0; list any that remain). Among straight legs, before (29) and after (all should still start at the trunk heading).
- `compare.py psg197`, before (shape identical 97 of 99, steps 94, both 93; projection differs D12e, D12g; steps differ C23, D12e, EF1, EF2, F3c) and after, with every difference class change.
- psg186 runs.

## 5. Do not
- Change node statements, straight legs, lane directions, the 90-degree rule, the empty-line headings or `hubs.tsv`.

## 6. Commit
- `Fix: Junction corpus: the node piece belongs to the trunk side`; regenerate and commit `out/psg197` and `out/psg186`; ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Do not commit `__pycache__`. README: one sentence.

## 7. Report (short, the full content)
- References read; commit; the counts before/after; compare.py before/after; psg186
- Stops; state (`git status --porcelain`, `git log -1 --format='%h %s'`)

When you have reported, stop.
