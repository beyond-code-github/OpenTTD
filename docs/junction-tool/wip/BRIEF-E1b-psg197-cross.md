# BRIEF E1b: PSG 197's crossings in travel order, as `cross` statements (data only)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Repo /home/pete/OpenTTD, branch feature/junction-tool, head HEAD_HASH (check with `git log -1`). Work in the main checkout; this step touches docs only.
- Plan: docs/junction-tool/PLAN-sss-v2.md E1; card docs/junction-tool/wip/CARD-E1-crossings.md; rulings D389 and D390 (docs/junction-tool/DECISIONS.md, last two rows).
- Budget: 45 minutes in total. No C++ build, no game, no test suite.

## Read first (once each, with offset/limit or grep)
- docs/junction-tool/DECISIONS.md: rows D389, D390 only (`grep -n '^| D389\|^| D390'`).
- docs/junction-tool/wip/CARD-E1-crossings.md (short).
- docs/junction-tool/tools/junction/topologies/psg197.txt (78 lines): the topology the statements go into.
- docs/junction-tool/wip/topologies/psg197-decisions.md: the tunnel table and the "Line index" table (each line's routes, from tile, to tile in travel order, tunnels in series). Do not read the 112-row crossing table; use the resolved file below instead.
- docs/junction-tool/wip/probes/e1/psg197-crossings-resolved.md: the 122 crossing pairs (rows `| L | M | tunnel |`; the first 300 lines are a copy of the decisions file, the crossing rows are what counts), and wip/probes/e1/tunnel-tunnel.txt (the 14 tunnel-under-tunnel crossings with their shared tile).
- The trace, OD = /mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad/psg/anatomy/psg197/trace/: `core.py` (`build()` gives the piece graph: pieces `pc = (tile, piece)`, successors `N.S[pc]`), `net.py` only as far as `core.build` needs. Run trace code with `python3 -I`, importing it by `importlib` as core.py does. Do not modify any file under OD.

## What to produce
1. **Script** docs/junction-tool/wip/probes/e1/order.py (python3, standard library plus the trace modules). It:
   a. walks every line of psg197.txt that appears in the 122 pairs, from its "from" tile to its "to" tile (Line index), in travel order, over the piece graph, giving the line's tile sequence. A tunnel the line runs through contributes its middle tiles in order between its two heads (tunnel table: entrance, exit). Port ends: the walk ends at the box edge cell named in the Line index.
   b. for each of the 122 pairs (L, M), finds the tiles in both L's and M's sequences. The crossing's position on L is the index on L of the first shared tile in L's order; likewise on M.
   c. per line, orders its crossings by position; writes the result.
   d. checks, and prints one line per breach: every one of the 122 pairs has at least one shared tile; no line's walk is ambiguous (two different walks between its ends, or none); the pair set it emits equals the 122 exactly; each pair appears in both its lines' lists; no two crossings of one line share a position (a tie). It also prints per pair the number of shared tiles when more than 1 (information only).
2. **Statements** appended to docs/junction-tool/tools/junction/topologies/psg197.txt after the `prio` line: a comment line `# Crossings (D389): authored from the save, each line's crossings in travel order (wip/probes/e1/order.py).`, then one `cross L: M1, M2, ...` per line that has crossings, lines in the order of the Line index table. Change nothing else in psg197.txt.
3. **Output file** docs/junction-tool/wip/probes/e1/order-out.txt: the script's full output (per line: its crossings with tile and position; then the check lines).

## Do not
- Do not add, drop or change a crossing pair; the 122 pairs are fixed (D390 (1) already applied). A pair with no shared tile is a stop, not a fix.
- Do not change the node, port or prio statements of psg197.txt, or rename any line.
- Do not write C++, run the game, or edit any file outside the three named above.

## Stop conditions
- Any check in 1d fails (report the breaches verbatim, commit nothing except the script and output so far as a WIP commit `Doc: Junction E1b WIP: ...`).
- A line's walk cannot be found or is ambiguous and the tunnel table does not settle it.
- The 45-minute budget runs out.

## Gate and commit
- Run `python3 -I docs/junction-tool/wip/probes/e1/order.py > docs/junction-tool/wip/probes/e1/order-out.txt 2>&1`; it must report zero breaches.
- Rerun the parity probe on the unchanged pair file: `python3 docs/junction-tool/wip/probes/e1/parity.py docs/junction-tool/tools/junction/topologies/psg197.txt docs/junction-tool/wip/probes/e1/psg197-crossings-resolved.md` must still print `bad 0` (it ignores `cross` lines; this proves the statements did not break the file's node parsing). If parity.py fails on the new `cross` lines, make it skip lines starting with `cross` (this one-line change is allowed) and say so.
- One commit: `Doc: Junction E1b: PSG 197 crossings in travel order (cross statements, D389)`, ending with the line `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Do not push.

## Report (short, this format)
- References read
- Commits: hash and subject
- Numbers: lines walked, cross statements written, pairs with more than one shared tile, any tie
- Checks: each 1d check and its result; parity result
- Stops, and anything left undone, with the reason
- State: `git status --porcelain` and `git log -1 --oneline`

When you have reported, stop.
