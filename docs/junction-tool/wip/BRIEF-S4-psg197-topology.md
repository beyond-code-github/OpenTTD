# BRIEF S4: PSG 197 BBH01 (V453000, "Tunnel only") as a planar topology plus its Structure decisions (transcription, Sonnet)

You execute this brief and nothing else. You write exactly the three files named under Output; you change no code and run no game. If something is not covered, or you would have to choose between two readings of the hub, record both in the gaps file and carry on with the one the trace's tile evidence supports better; stop only on the stop conditions. Keep your context small: read the trace files with offset/limit and grep; do not dump bbh.json.

## Why
Corpus growth (ROADMAP §17 row; D270, D362): a hub = its topology + its Structure decisions (separator and kind per crossing group, copies) + cross-sections. Recreating a real hub from its own decisions is the completeness test for every layer. The engineer asked for V453000's PSG 197 tunnel hub first, and warned we may need to look beyond the topology to represent it: so the gaps file matters as much as the topology.

## Read first (once)
- The notation: /home/pete/OpenTTD/src/junction/planar/planar_topology.h lines 60-92 (format comment), /home/pete/OpenTTD/src/junction/planar/CLAUDE.md section "Topology (`planar_topology`)", and two examples: `PlanarLL2Text()` in /home/pete/OpenTTD/src/junction/planar/planar_plan.cpp (lines 438-480; PSG 227's pattern at two lanes: choice routes, `name=R1+R2` lines, nodes, prio) and /home/pete/OpenTTD/docs/junction-tool/wip/topologies/LL3a.txt. Crossings are DERIVED from the ring and routes; you never write them.
- The hub: /home/pete/OpenTTD/docs/junction-tool/ANATOMY-psg197-bbh.md (whole: the engineer's reading, the comparison with 227, the trace and its tables).
- The trace data, OD = /mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad/psg/anatomy/psg197/trace/: `reach.txt` (which in lane reaches which out lane), `paths_all.txt` and `per_lane_events.txt` (each lane's path: switches, tunnels, joins in order), `joins.txt`, `tunnels2.txt`/`tunnels3.txt` (50 tunnels, entrance signals), `block_table.md`. The picture: OD/../pic.txt.

## Method
1. **Frame.** Rotate/reflect the hub into the canonical frame only if it makes the notation simpler; otherwise keep the save's sides (N, E, S arms). State which in the topology's comment.
2. **Ports.** Per arm, one out port and one in port with their lanes left to right across travel (anatomy table "Arms and lanes"), headings into/out of the box.
3. **Routes.** A route per (in arm, out arm) movement and lane path, named letter + lane as the examples; a lane path that can take several out lanes (a choice, e.g. by tunnel availability) is one route with several paths, as `PlanarLL2Text`'s choice routes. Use `reach.txt` for which pairs exist (37 of 42).
4. **Lines and nodes.** Walk each in lane's paths (`per_lane_events.txt`) and write every switch as a `node` statement: a 1 -> 2 split or 2 -> 1 merge (a 3 -> 2 merger is a sequence of 2 -> 1 merges; write the order the tiles show). A line carries the routes of every path through it.
5. **Prio.** `prio` lists the routes that hold priority at their merges (the anatomy says where prios are; `tunnels3.txt` and the signals tell pre-signal types).
6. **Structure decisions (sidecar).** For every pair of lines that cross in the hub (read from the tile evidence: a tunnel under another line, a bridge, or a level crossing): which line is the separator (the line IN the tunnel or ON the bridge), the kind, the tunnel's length in middle tiles, which parallel tunnels form one group (side by side, same crossers), and whether a line is doubled (copy) over it.

## Output
- /home/pete/OpenTTD/docs/junction-tool/tools/junction/topologies/psg197.txt: the topology in the notation (a `#` comment header: source, frame, and anything simplified).
- /home/pete/OpenTTD/docs/junction-tool/wip/topologies/psg197-decisions.md: the sidecar table (crossing line pair, separator, kind, length, group, copy, tile of the entrance) and a count summary.
- /home/pete/OpenTTD/docs/junction-tool/wip/topologies/psg197-gaps.md: every feature of the hub the topology + decisions cannot express or that you had to simplify, each with the tile evidence and what the model would need (e.g. choice by tunnel availability; lanes 3 -> 2 -> 3; tunnels of different lengths landing at their joins; two tunnels feeding one lane; levels where tunnels pass under each other). Also any reading you were unsure of, with both readings.

## Do not
- Change code, the anatomy doc, or the trace files; run the game; fetch anything.
- Invent a switch, lane or crossing the trace does not show; if the trace is missing something, put it in the gaps file.

## Stop conditions
- The trace files are missing or unreadable: stop and report.
- More than a third of the in lanes cannot be followed to an out lane from the trace: stop and report which.

## Report (short)
- The three file paths; counts (ports, lines, routes and paths, nodes, prio routes; sidecar rows by kind); the gaps file's headings; anything not done. When you have reported, stop.
