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

## Layout program (`planar_layout`, DESIGN-planar §2a)
- The layout is one integer program through `JunctionMilp` in half-tile units: per line a chain of vertices (start, one spare bend plus the route's least turn count per stretch, each crossing, end), per segment a heading in 8 and a length; a node is one vertex shared by its lines, a crossing one vertex shared by its two lines.
- A line's segments keep to the short turn arc of its routes plus one heading beyond each end.
- A bend vertex with no curve starts a zero-length segment unless the next vertex is an event; a zero-length segment keeps the heading before it; segments at a node, port or crossing vertex have length.
- Ports: the port line's end segment has the port heading and at least 4 units; a port's lines are adjacent, the two ports of a side 2 (gap + 1) apart (ml_gap on W and E, branch_gap on S).
- A split's left leg turns left or not at all, its right leg right or not at all, and they differ; a merge's left leg turns left into the trunk, its right leg right; each leg has at least 2 units before its next event.
- A merge where one leg carries a prio route: that leg is straight into the merge, axis, for n_prio + 1 tiles; the element's row tiles, on the side opposite the joiner, hold no other track.
- A crossing's over line is axis at C, the under line not parallel; the over line has 3 + (under diagonal) units each side and 2 more on the approach side (the axis piece `RasterizeJunctionRoute`'s B starts from) unless its deck merges from the crossing before; a prio line is never over (doubling is not built).
- Consecutive over crossings of one line with no curve between share one deck within the longest middle; otherwise a curve lies between.
- A non-prio route runs at least 2 TL units from its last split or port to each merge.
- Clashes (`JunctionTracksMayShareTile`, touching across a tile edge, heads exclusive, deck middles over ground allowed), curve gaps (`JunctionCurveGaps` on the solved hands), prio element room and terrain facts are added lazily as separations or floors, each holding while the headings it was found at hold, and the program is solved again until none is found.
- `PLANAR_DEBUG` set in the environment prints the lazy loop's rounds.

## Back end (`planar_plan`, DESIGN-planar §3, §4)
- The flat seed is cached per topology text and the spec's rule fields, at box bound 64 tiles, then 128.
- On terrain the program runs again with terrain facts lazy, warm-started from the seed; the hover fixes the seed's binaries.
- Each route path renders to one `JunctionTrackRoute` through `RasterizeJunctionRoute` (F, L, R per piece, B<middle> per deck), tagged edge = line, copy 0; the signal pass reads a `JunctionSignalNetwork` filled natively (main = the route holds prio, group = its letter).
- `PlanarCheckerTopology` gives the checker the ports as lane groups in slot order and every route path as a lane path with its route's prio flag (D301).
- `PlanarLL2Text` is the PSG 227 pattern at two lanes; its 18 lane paths equal the composed hub's (`ComposeJunctionHub` on the width-2 T).
- `PlanarHubText` maps the window's hub type: one lane each way to the T, two with any pairing to LL², two otherwise to the two-lane T.
