# Brief X-ns: the straight leg when the trunk or a leg is itself a node piece

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng2`, branch `wt-x`, head `{HEAD}`. Python only, `python3 -I`.
- Finding (X8 rerun, 2026-11-22): the C++ loader refuses the extractor's 197 structure with "node with trunk L101 has no straight". `report.txt` lists two undetermined straight legs: the 104,179 and 105,179 splits, both "(trunk/leg is a node piece)". They are two splits one tile apart (`node L84 -> L101, L68`, then `node L101 -> L69, L70`, where L101 is the single node piece between them).
- In `extract.py` `structure()` (~lines 457-490), a node whose trunk or a leg is a node piece (no line id, `-1 in lgs` or `None in trs`) is marked undetermined before any heading is compared.

## 2. Read first
- `extract.py` lines ~400-500 (`structure()`, `offnode`, `hd0`, `head`, `SEG`, `LID`) and how `head(piece, ...)` gives a piece's heading in its travel direction.
- `out/psg197/report.txt` and `structure.txt` (the `straight` lines and the `# straight L101: undetermined` comment).

## 3. The rule (generic)
- The heading to compare is that of the piece adjacent to the node piece on the trunk side (the last trunk piece before a split, the first after a merge) and on each leg side, whatever that piece is: an off-node piece of a line, or another node piece. A node piece's heading is its own heading in its travel direction.
- The straight leg is the leg whose adjacent piece keeps the trunk's heading, as today.
- Only the source of the adjacent piece changes: when it is a node piece, use that node piece directly instead of marking the node undetermined. When a leg is a node piece, its leg line for the `straight` statement is the line that node piece belongs to (the line id the topology gives it, e.g. L101). If a node piece has no line id at all in the topology, stop and report.
- Keep the undetermined outcomes for the other reasons (no geometry, no trunk, several legs keep the heading).

## 4. Checks
- psg197: `undetermined straight legs: none` (or list what remains, with reasons). `compare.py psg197` straights matched before and after (59 of 61 before), plus any other difference class change.
- psg186: the undetermined count before and after.
- The C++ check (merged by the orchestrator later), not yours to run.

## 5. Do not
- Change lane directions, the 90-degree rule, the cycle guard, or `hubs.tsv`.

## 6. Commit
- `Fix: Junction corpus: straight legs next to node pieces`; regenerate and commit `out/psg197` and `out/psg186`; ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Do not commit `__pycache__`.

## 7. Report (short, the full content)
- References read; commit; the two 197 nodes' straight legs now; compare.py straights before/after and any other change; psg186 before/after
- Stops; state (`git status --porcelain`, `git log -1 --format='%h %s'`)

When you have reported, stop.
