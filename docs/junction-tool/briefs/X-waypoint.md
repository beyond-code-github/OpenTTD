# Brief X-wp: rail waypoints and pass-through station tiles are track

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng2`, branch `wt-x`, head `{HEAD}`. Python only, `python3 -I`.
- Finding: in Brainmelter (psg186), two in-lanes enter the box along x at y 226 and 227 and pass a rail waypoint at x = 172. The extractor ignores station tiles ("stations inside the box (ignored by the graph): 172,226 172,227"), so these lanes break at the waypoint. They show as an 8-piece candidate component with no switch, and as lanes W225/W226 outside the hub. The export's non-rail tile kinds include `('station', 'rail')` and `('station', 'rail_waypoint')`.
- Brainmelter's box is now 163,224,225,281 (`hubs.tsv`, the west edge moved from 165: a merge node sat on it).
- Answer key (the engineer, screen-anchored, mapped to box edges): the west edge (x min) has 2 in (the waypoint lanes, y about 225-227) and 4 out (y about 228-234); the north edge (y min) 4 in, 3 out; the east edge (x max) 3 in, 3 out; the south edge none.

## 2. Read first
- `load.py`: `load_tiles`, the `World` constructor (how rail tiles become `nodes` with piece names and edges), `heads`; how a tile of type `station` is skipped today.
- The export format of a station tile: print one `rail_waypoint` tile and one `station`/`rail` tile from psg186 (their keys), in a one-off script.

## 3. The rule
- A `station` tile whose kind is `rail` or `rail_waypoint` and which has a rail axis (X or Y, from its export keys) is one straight piece along that axis (piece name `X` or `Y`, the same edges as a plain rail tile). Its signals, if any, are read as for rail tiles.
- If a station tile's axis cannot be read from the export, stop and report its keys.
- The report's "stations inside the box" checkpoint then lists only station tiles that are not track (none expected).

## 4. Checks
- psg186 on the new box: the per-edge in/out counts against the answer key, and the candidate lanes per edge (position, direction, source, switch component), in the report and the reply.
- psg197: unchanged (`compare.py psg197` identical; 197 has no station in its box; say if it does).
- `cycle: none` on both.

## 5. Do not
- Change the 90-degree rule, lane-direction rule, node-piece rules, or `hubs.tsv`.

## 6. Commit
- `Fix: Junction corpus: rail waypoints and pass-through station tiles are track`; regenerate and commit `out/psg186` (and `out/psg197` if anything changed); ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Do not commit `__pycache__`.

## 7. Report (short, the full content)
- References read; commit; the station tile keys; psg186 counts vs the key and the lane list; psg197 unchanged or not
- Stops; state (`git status --porcelain`, `git log -1 --format='%h %s'`)

When you have reported, stop.
