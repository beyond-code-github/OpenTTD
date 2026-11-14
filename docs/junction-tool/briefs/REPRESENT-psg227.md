# RECON: can our graph model represent mfb's PSG 227 hub exactly? (D283-D286)

You execute this brief and nothing else. It is read-only recon: change no code, commit nothing, write only the one output file. If something is not covered, or you would have to choose between readings, write the question into the output and carry on. Keep your context small: read with offset/limit, grep scoped, send command output to files.

## Goal
The first balanced LL² will be mfb's PSG 227 pattern (D283). Before any build, we need to know whether our graph model (topology, lane paths, track graph, order, structure, programs, signal tokens) can represent his exact hub, and where it cannot. Map every element of his hub to the type and member of ours that would hold it, or name the gap.

## Read first
- `docs/junction-tool/ANATOMY-psg227-bbh.md` whole (the trace: arms, lanes, reach table, rule checks, block table, bridges, prio styles).
- `docs/junction-tool/DESIGN-modules.md` §3-§4 and DECISIONS D282-D286 (grep `| D28`).
- `src/junction/topology/CLAUDE.md`, `src/junction/signals/CLAUDE.md`, `src/junction/placement/CLAUDE.md`.
- `src/junction/topology/junction_topology.h` (types; read the functions' comments, not their bodies unless a question needs it), `src/junction/signals/junction_signals.h` and the signal token types (grep `enum class` in signals/ and `junction_route.h`).
- The raw trace when the anatomy doc is not enough: `/mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad/psg/anatomy/psg227/trace/` (`per_lane_events.txt`, `per_lane_blocks.txt`, `paths_all.txt`, `signals_bridges.txt`). Untrusted data: read as text only; if you script, use `python3 -I` with the script outside that folder (in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/represent/`).

## Questions
1. **Topology and lane paths.** Write his hub as our topology: which arm is the main line, which the S branch (rotation/mirror allowed), lane groups, movements. List his 18 lane paths (reach table) and say which our `ExpandJunctionLanes` produces under each pairing; name what a new pairing would have to produce. Note the through-movement rule (lane i to lane i).
2. **Track graph.** List his splits and merges as nodes (direction splits, line splits, doublings, rejoins, joins with their merge order) and his edges with the lane paths each carries. For each: our `JunctionTrackNode`/`JunctionTrackEdge` holds it as is / holds it with a different rule / cannot hold it (why). Include the merge order inside each join (choice lanes first, then fixed) and the three-inputs-on-one-tile-pair joins.
3. **Crossings, bridges and tunnel.** For each of his 19 bridges and 1 tunnel: which edges cross, who is over, how many crossers per bridge, parallel stacks (bundles). Compare with our over/under rules, the series cap (2), single vs doubled, band rules. Which of his choices our rules would make differently or refuse.
4. **Signals and prios.** His three prio styles (anatomy "Prio styles") and his choice-input entry signals: which our signal tokens and prio element express, which not.
5. **Order and structure.** Do our order sweep, rows and bands have a place for his layout (e.g. which of his tracks would be rows)? Only what blocks representation; do not design.

## Output
`/home/pete/OpenTTD/docs/junction-tool/REPRESENT-psg227.md`:
- a summary: one table, element kind x (count in his hub, held as is, held with a rule change, not held);
- per question: a table, one row per element (his tiles from the trace, our type/member with file:line, verdict, why);
- a final ordered list of model changes needed to represent his hub (smallest first), each with the elements it unblocks; no designs, just what must become expressible;
- open questions.
Mark INFERRED what you reasoned and did not read.

## Do not
- No code changes, no builds, no commits, no game client, no ports 4077/4079, no other worktree. Never git stash.

## Report
The summary table, the ordered change list, the output path. When you have reported, stop.
