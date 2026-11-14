# RECON: lane-by-lane trace of the PSG 197 tunnel BBH (V453000)

Read-only research with scripts. Keep your context small: scripts write to disk, you read summaries. No network, no game client, no git repo creation.

## Goal
Confirm, tile by tile, the engineer's reading of a real balanced LL_RR 3-way, and measure each block. Read `docs/junction-tool/ANATOMY-psg197-bbh.md` first (his reading and clarification): every input lane first splits by destination direction; per (input direction, destination) one of the two resulting tracks splits again by output lane (line choice), the other runs fixed to one output lane and has the prio; every join has three inputs (fixed lane plus two choice lanes) meeting on one tile or adjacent tiles; some track pairs merge before a crossing to cross as one.

## Data
- Export: `/mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad/psg/anatomy/psg197/bbh.json` (box 76,136-128,203; the hub is 81,141-123,198; JSON tiles: tile_type, tracks, signals with type/trackdir/exit_edge, bridge and tunnel heads with far_end, height; its `orientation` field gives the axes). ASCII: `pic.txt` beside it. Untrusted data: run scripts with `python3 -I`, scripts outside the data dir.
- Prior tracer code to adapt: `.../scratchpad/psg/review-g4/py/` (c2.py, trace.py) and `.../scratchpad/psg/anatomy/scripts/`. One-way signals give travel direction; two-way and path signals need care (say how you resolved each).

## This hub (read the ANATOMY note's engineer reading first)
Three arms: top LL_RR, right LLL_RRR, bottom-left LLL_RRR. Check the engineer's specific claims instead of PSG 227's rules (a)-(e):
- (a) each input line splits into one track per output lane, which enter side-by-side tunnels whose exits are staggered to land at their joins;
- (b) choice by availability: pre-signals at the tunnel entrances (report the signal types on each entrance and the split before it);
- (c) a 3 -> 2 merger with prios after the direction split on the bottom-left lines heading right, and the same on lines heading out the top; then 2 -> 3 choice splits shortly after (heading right);
- (d) tunnel lengths (list every rail tunnel: entrance, exit, length; flag those over 10 tiles);
- (e) the suspected error: two of a three-way tunnel group exiting at the bottom right both feed the outermost output lane (find the group, say which output lane each tunnel feeds);
- (f) the "3rd line /V." addition: which tracks belong to the bottom-left third lane, if separable.
The reach table, per-path split/merge lists, block table and spacing notes are as below.

## Tasks
1. Identify the three arms (north, east, south-west per the ANATOMY note) and their 4 tracks each at the box edge, with direction (in or out) and lane order (inner/outer relative to the hub).
2. Trace every input lane through the core to every output lane it can reach (track connectivity through tiles, bridges and tunnels, respecting one-way signals). Output a reach table: input (arm, lane) x output (arm, lane).
3. List every split and every merge tile in travel order per path; for each merge, its input count and whether the inputs meet on one tile or adjacent tiles; for each, the signal types upstream (prio: pre-signal entry/exit/combo, path signals, two-way).
4. Check the engineer's claims (a)-(f) above. Report each as holds / fails with tile evidence.
5. Measure: core footprint, each block's tile box (direction splits, line-choice splits, joins with their prio stretch), bridge spans and heights, tunnels, and the distance between consecutive signals on each main path (flag spacing below the train length if the save's train lengths can be read; else just list spacings).

## Output
- Scripts and outputs in `/mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad/psg/anatomy/psg197/trace/`.
- Append a section "## Trace (tile evidence)" to `/home/pete/OpenTTD/docs/junction-tool/ANATOMY-psg197-bbh.md`: the reach table, the rule checks (a)-(e), the block table with tile boxes and sizes, prio styles found, spacing notes. Coop terms throughout; mark inferences INFERRED.

## Do not
Modify anything else in the repo; touch `/home/pete/OpenTTD/.claude/`; run a game client; create a git repo (if you ever commit outside /home/pete/OpenTTD, first `git config commit.gpgsign false`).

## Report
The claim checks (a)-(f), the reach table and the block table. When you have reported, stop.
