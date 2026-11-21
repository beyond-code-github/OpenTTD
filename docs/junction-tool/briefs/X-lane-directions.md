# Brief X-dir: lane directions from each lane's first signal (corpus extractor)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng2`, branch `wt-x`, head `{HEAD}`. Work and commit only there. Python only; run everything as `python3 -I`.
- Problem: `load.py` gives every candidate lane the save's direction through one sign per connected component (`build_dirs`: a union-find with parity over all connected pieces; `signs`: a majority vote of the signals inside the box; `box_lanes.exit_dir`). In a woven hub one component spans the whole hub, so lanes flip in groups. On PSG 197 it disagrees with the engineer's pinned lanes on E167 and N91 (report line "lanes whose direction in hubs.tsv differs from the save's own signal direction"); on Brainmelter (psg186, no pinned lanes) the per-side counts are wrong.
- Answer keys:
  - psg197: the `lanes` column of `hubs.tsv` (16 lanes, pinned by the engineer).
  - psg186: the engineer's counts per box edge, in the extractor's side names: N (y min edge): 4 in, 3 out; W (x min edge): 2 in, 4 out; E (x max edge): 3 in, 3 out; S: none. The extractor finds 8 candidate lanes on W, where the engineer counts 6.

## 2. Read first (once each, with offset/limit)
- `tools/junction/corpus/README.md`.
- `tools/junction/corpus/load.py`: the class with `build_dirs`, `signs`, `across`, `pedges`, `heads`, `sw`, `bh` (lines ~60-145), and `box_lanes` (~146-223).
- `tools/junction/corpus/extract.py`: `choose_lanes` (~59-79) and where the report writes the lane notes.

## 3. The rule (implement exactly)
In `box_lanes`, a lane's direction comes from the first signal on its own track, walking inward from the box edge:
1. Start at the lane's piece `pc` at the box edge, entered through its box-edge `e`. The piece's other edge is the inward exit.
2. If the save has signals on this piece (`self.votes[pc]`, the set of `exit_edge`s):
   - exactly one exit edge, equal to the inward exit: `in`;
   - exactly one, equal to the edge it was entered by: `out`;
   - more than one: `bidir`.
   Stop there.
3. Otherwise step across the inward exit with `across(pc, exit)`. Continue only when it gives exactly one piece and that tile is neither a switch tile (`sw`) nor a bridge head (`bh`, `heads`); the next piece is entered through `e2`. At most 64 steps.
4. If the walk stops without a signal, use the old component vote (`exit_dir`) and mark the lane's source as `vote`; else the source is `signal`.
- Keep a `src` key on the lane dict (`signal` or `vote`). The report's lane note lists each lane's source after its direction (e.g. `N84:out(signal)`).
- Keep `build_dirs` and `signs` (the fallback uses them).

## 4. Checks (run in this order; record before/after)
1. Before the change: run psg197 and psg186 extraction and compare (README "Run" line; psg186 likewise, without compare). Save the reports' lane lines to `/tmp/claude-1000/xdir-before.txt`.
2. After:
   - psg197: the "differs from the save's own signal direction" note must be gone (all 16 pinned lanes agree with the derived ones). `compare.py psg197` must show no new difference class compared to before.
   - psg186: the per-side in/out counts of the derived lanes, printed by a short `python3 -I` one-off reading the new report (no new file in the repo). Compare them with the answer key above.
   - For W, list every candidate lane (pos, track, direction, source, and whether its component has a switch) so the engineer can see which 2 of the 8 are not hub arms.
- If psg197 still disagrees on any pinned lane, stop and report that lane's walk (tiles visited, where it stopped, which signal it read).

## 5. Do not
- Change `hubs.tsv`, the graph, ports, lines or any later stage; drop lanes; special-case a hub or a lane.

## 6. Commit
- Regenerate `out/psg197` and `out/psg186` and commit them with the code: `Change: Junction corpus: lane directions from each lane's first signal`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Update README's `hubs.tsv` bullet: blank lanes are derived from each lane's first signal walking inward, else the component vote.
- Remove `tools/junction/corpus/__pycache__/` if you created it; do not commit it.

## 7. Report (short)
- References read; commit
- psg197: pinned vs derived (agree / list), compare.py differences before and after
- psg186: per-side counts, derived vs answer key; the W candidate lane list; lanes decided by vote
- Stops; state (`git status --porcelain`, `git log -1 --format='%h %s'`)

When you have reported, stop.
