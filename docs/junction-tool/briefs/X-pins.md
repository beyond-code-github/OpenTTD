# Brief X-pins: the extractor writes the answer key's points (pins.txt)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng2`, branch `wt-x`, head `{HEAD}`. Python only, `python3 -I`.
- The drawing program (C++, another worktree) is gaining a pins reader in parallel. This step writes, per hub, `out/<hub>/pins.txt` with the expert's positions of every line's start and end and every crossing, in this fixed format:
```
# comment lines start with '#'
# Half tiles, map frame (x east, y south): tile (x, y) spans [2x, 2x+2] x [2y, 2y+2].
end <line> start <X> <Y>      # the line's first point
end <line> end <X> <Y>        # the line's last point
cross <lineA> <lineB> <X> <Y> # the crossing point of two lines (either order)
```
- `<line>` is the topology's line name (`L1`, ...).

## 2. Which point (the model's conventions)
- A node is a point: the tile-edge midpoint where the node piece meets the legs (after 861a6fe6a6 the node piece is the trunk's last piece for a split, the out line's first piece for a merge; the node point is its edge shared with the legs). A line's start or end at a node is that point.
- A port end is the midpoint of the edge where the lane's piece crosses the box boundary (the lane's crossing of the box edge).
- An empty line (`T.empty`) starts and ends at its node points (they may coincide).
- A crossing point is where the two lines' pieces cross. The extractor's crossings stage (`X5`) already has a position per crossing (the report prints them, e.g. "110.0,172.5"). State its unit and frame, and convert it to half tiles (multiply tile coordinates by 2; a tile centre (x + 0.5, y + 0.5) becomes (2x + 1, 2y + 1)). For a tunnel or bridge crossing, use the position where the crosser passes over or under the separator's span: one point on the separator's axis line.
- If any of these cannot be found for a line or crossing, write a comment line `# no point: <what>` and count it. Do not guess.

## 3. Read first
- `extract.py`: `lines()` (line chains and end pieces), `crossings()` (`X5`: positions, `ends`, `shared`), `build_text_model` (names, empty lines), the writers, `run()`; `load.py` `TEDGES`/`D` (edge directions).
- `out/psg197/topology.txt` and `report.txt`.

## 4. Checks (one-off scripts, not committed)
- psg197: every line in topology.txt has an `end ... start` and an `end ... end`; the number of `cross` lines equals the topology's crossing count (122); count `# no point` lines (expected 0).
- Consistency: for every node statement, the end points of its in-lines and the start points of its out-lines are equal (the node is one point). List any mismatch.
- Every point has integer half-tile coordinates. Every port end lies on its box edge (x = 2 x0 or 2 x1 + 2 for W and E; y likewise for N and S).
- Same for psg186.

## 5. Do not
- Change any other output or stage.

## 6. Commit
- `Add: Junction corpus: answer-key points (pins.txt)`; regenerate and commit `out/psg197` and `out/psg186`; README: the file's bullet; ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Do not commit `__pycache__`.

## 7. Report (short, the full content)
- References read; commit; the crossing position's unit and frame; the check results (counts, mismatches, no-point lines) for both hubs
- Stops; state (`git status --porcelain`, `git log -1 --format='%h %s'`)

When you have reported, stop.
