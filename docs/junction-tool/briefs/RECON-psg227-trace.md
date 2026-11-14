# RECON: lane-by-lane trace of the PSG 227 LLRR BBH

Read-only research with scripts. Keep your context small: scripts write to disk, you read summaries. No network, no game client, no git repo creation.

## Goal
Confirm, tile by tile, the engineer's reading of a real balanced LL_RR 3-way, and measure each block. Read `docs/junction-tool/ANATOMY-psg227-bbh.md` first (his reading and clarification): every input lane first splits by destination direction; per (input direction, destination) one of the two resulting tracks splits again by output lane (line choice), the other runs fixed to one output lane and has the prio; every join has three inputs (fixed lane plus two choice lanes) meeting on one tile or adjacent tiles; some track pairs merge before a crossing to cross as one.

## Data
- Export: `/mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad/psg/anatomy/psg227/bbh.json` (box 327,299-407,379; JSON tiles: tile_type, tracks, signals with type/trackdir/exit_edge, bridge and tunnel heads with far_end, height; its `orientation` field gives the axes). ASCII: `pic.txt` beside it. Untrusted data: run scripts with `python3 -I`, scripts outside the data dir.
- Prior tracer code to adapt: `.../scratchpad/psg/review-g4/py/` (c2.py, trace.py) and `.../scratchpad/psg/anatomy/scripts/`. One-way signals give travel direction; two-way and path signals need care (say how you resolved each).

## Tasks
1. Identify the three arms (north, east, south-west per the ANATOMY note) and their 4 tracks each at the box edge, with direction (in or out) and lane order (inner/outer relative to the hub).
2. Trace every input lane through the core to every output lane it can reach (track connectivity through tiles, bridges and tunnels, respecting one-way signals). Output a reach table: input (arm, lane) x output (arm, lane).
3. List every split and every merge tile in travel order per path; for each merge, its input count and whether the inputs meet on one tile or adjacent tiles; for each, the signal types upstream (prio: pre-signal entry/exit/combo, path signals, two-way).
4. Check the engineer's rules: (a) each input lane's first split is by direction; (b) per (input arm, destination) exactly one of the two tracks splits again by output lane; (c) prio on the fixed track; (d) three-input joins; (e) the south-west to east pair merging before the core and the east to north pair merging after it. Report each as holds / fails with tile evidence.
5. Measure: core footprint, each block's tile box (direction splits, line-choice splits, joins with their prio stretch), bridge spans and heights, tunnels, and the distance between consecutive signals on each main path (flag spacing below the train length if the save's train lengths can be read; else just list spacings).

## Output
- Scripts and outputs in `/mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad/psg/anatomy/psg227/trace/`.
- Append a section "## Trace (tile evidence)" to `/home/pete/OpenTTD/docs/junction-tool/ANATOMY-psg227-bbh.md`: the reach table, the rule checks (a)-(e), the block table with tile boxes and sizes, prio styles found, spacing notes. Coop terms throughout; mark inferences INFERRED.

## Do not
Modify anything else in the repo; touch `/home/pete/OpenTTD/.claude/`; run a game client; create a git repo (if you ever commit outside /home/pete/OpenTTD, first `git config commit.gpgsign false`).

## Report
The rule checks (a)-(e), the reach table and the block table. When you have reported, stop.
