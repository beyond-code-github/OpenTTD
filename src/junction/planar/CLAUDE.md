# Junction tool: planar/ (the planar line/route model, DESIGN-planar)

- The planar model stands beside the old pipeline and shares none of its graph, order, structure, programs or placement concepts.

## Topology (`planar_topology`)
- A topology is text in the engineer's notation: `port <side> <in|out> <heading>: lines` (lines left to right across travel; a side's ports in reading order, N and S west to east, W and E north to south), `node in -> out`, `prio routes`.
- A line name is capital letters, a lane number and an optional lower-case suffix; `CB1` carries routes C1 and B1; `name=R1+R2` names its routes explicitly.
- A node statement is a group of 1 -> 2 splits (fewer in than out) or 2 -> 1 merges (more in than out); a split's legs are the out lines whose routes lie in its in line's, in authored order; a merge's legs are the in lines whose routes lie in its out line's.
- A route is a lane path (D301); its paths are every walk over lines carrying it from its one entry port line to an exit port line; a choice route has several.
- The ring is the port lines clockwise from the north-west corner: each port's lines enter in reading order when right of travel points along reading order, else reversed.
- Two paths of different routes that share no line must cross iff their ends interleave on the ring; paths sharing a line never cross.
- A crossing is a pair of lines; it crosses every path through one with every path through the other, so a line pair is admissible only when all those pairs must cross. Port lines (a port at either end) never cross.
- Crossings are chosen greedily: admissible pairs covering the most path pairs first, then by line index, each path pair covered once; a must-cross pair left uncovered refuses the topology.
- Event order: each line's crossings in the order a straight chord of its first path meets the other lines' chords, ports on a circle at uneven angles and each port's lines packed together, so bundles cross whole (D306) and the orders agree.
- The T is authored by `PlanarTText(lanes)`: the engineer's drawing turned half a turn into the canonical frame (main W-E, branch S): A E->S, B E->W, C S->W, D S->E, E W->E, F W->S; B and E hold prio.
- At one lane the T has exactly three crossings (B×D, B×F, D×F); at two lanes 18: the 12 core crossings on single-route lines and one fray crossing at each of the six rings.
