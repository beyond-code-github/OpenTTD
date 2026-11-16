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

## Channels (`planar_channels`, D305, D306, D327)
- `BuildPlanarChannels` groups a topology's lines into channels: lines with the same in ports reaching them, the same out ports reached, the same depth from the in ports and the same crossed channels in order (refined until stable); a line may end inside its channel at a node of its own while the others carry on.
- A channel's lines are ordered left to right across travel: port lines by the port's order, other lines by their leftmost in leg's order, then their leg position.
- A crossing whose two lines start or end at the same node group belongs to that group (a fray's or join's own crossings); every other crossing belongs to the pair of channels, and a channel's crossings follow its first line's event order.
- The T at one, two and three lanes gives twelve channels (EF, AB, CD, ED, CB, AF and the six movements A to F) and three whole channel crossings (B x D, B x F, D x F); in LL², E1 and E2 share a channel and the crossing choice legs are channels of their own.

- A site is the splits leaving one channel or the merges entering one channel (grouped by channel, not by node statement): a fray when its legs reach different out ports, a lane split when the same; a join when its legs come from different in ports, a lane merge when the same.
- A site's straight channel is the one carrying a prio route when exactly one does, else the one turning least between its port headings; a tie (or two prio channels, as in LL²'s 227 exits) leaves it open as a variant. The side is the other channel's turn direction (+1 right). On the T this gives E and B straight with F leaving right and A left, D joining from the right and C from the left, and ties at the S fray and S join (the old `fray_s` and `join_s` variants).
## Channel paths (`planar_paths`, D327, D328)
- At a channel crossing a prio channel is over a non-prio one (the main line stays straight, D328); between two prio or two non-prio channels the over channel is a choice (`PlanarOpenCrossings`, one bit each in `BuildPlanarPaths`).
- A channel turns the short way from its single in port's heading to its single out port's (falling back to the one it has), 45 degrees a piece; its crossings sit in its event order on the first piece at or after the last that suits (axis when over, not parallel to the over piece when under), with a back-and-forth detour only when none suits; over channels are placed first, and a cycle of overs is refused.
- An empty crossing triangle (three channels crossing pairwise, each with its two crossings adjacent) can sit either way round; `PlanarTriangles` finds them and `FlipPlanarTriangles` swaps each flipped triangle's crossings on its three channels; the mapped solve tries both (a variant bit each). The T has one (B, D, F); the hand compositions use the flipped one.
- On the T: B is over D and F, D x F is the one open crossing; B and E are straight, F E-SE-S, D N-NE-E, C N-NW-W, A W-SW-S; the topology's event order puts D's B crossing before its F crossing (F x D between the main lines), unlike probe 9's hand composition (F x D south of B).

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
- The flat seed is cached per topology text and the spec's rule fields. It solves the component layout (`SolvePlanarComponents`) for topologies with a composition (T2, LL², `seed.components`, `bound` 0) and the §2a program otherwise, at box bound 64 tiles, then 128; a topology with no composition behaves as before.
- A component plan renders the cached programs through `RenderPlanarRoutePrograms` (flat placement even on terrain, levels solved after as for any plan); the hover reuses them.
- `RenderPlanarRoutePrograms`: each program through `RasterizeJunctionRoute` (turn +1 is the rasteriser's L, a deck is B<middle>), tags edge = the path line and copy = the program's copy, `JunctionTrackRoute` index = the running index of (route, path), signal routes with arrivals, switches, merge nodes and the programs' copy switches sorted by piece, node tiles from the programs (a split's the first piece of its out leg, a merge's the last piece of its in leg), port tiles the first (in) or last (out) piece of a program on that line, layout size from the tracks' bounding box.
- On terrain the program runs again with terrain facts lazy, warm-started from the seed; the hover fixes the seed's binaries.
- Each route path renders to one `JunctionTrackRoute` through `RasterizeJunctionRoute` (F, L, R per piece, B<middle> per deck), tagged edge = line, copy 0; the signal pass reads a `JunctionSignalNetwork` filled natively (main = the route holds prio, group = its letter).
- `PlanarCheckerTopology` gives the checker the ports as lane groups in slot order and every route path as a lane path with its route's prio flag (D301).
- `PlanarLL2Text` is the PSG 227 pattern at two lanes; its 18 lane paths equal the composed hub's (`ComposeJunctionHub` on the width-2 T).
- `PlanarHubText` maps the window's hub type: one lane each way to the T, two with any pairing to LL², two otherwise to the two-lane T.

## Component solver (`planar_components`, DESIGN-components §6)
- `SolvePlanarComponents` has a hand composition per topology name, `T2` (`PlanarTText(2)`) and `LL2` (`PlanarLL2Text()`); every route line of the composition must exist in the topology's lines; the one-lane T is refused (it keeps the §2a layout), and any other topology goes to `SolvePlanarMapped`.
- `SolvePlanarMapped` composes from the channel graph alone (D327, `MappedComposer`): probe 9's n-lane fray, join, doubled core and bundles, placed by the sites (straight channel, side, prio) and paths (headings, crossing pieces, over channel). Its variants are every choice of the open crossings and the tie sites (at most 6 bits). A straight prio channel arrives on the join-entry lines from its join's far lane and, when it is over crossings, doubles each lane once over them all (far lane copying outward, the others away from it); deck ends sit on whole pieces; a non-prio route has a whole-train bay (2 TL + 4) from its last split or port to each merge. Lane splits and lane merges (LL²'s 227 block) are refused for now.
- Mapped T3 passes the checker at 2062 tiles (122x43), where probe 9's hand composition had 1949. Its triangle flipped, with right-angle crossings under prio channels (`PLANAR_PERP`, an experiment), the variant o1t3f1 reproduces the hand composition exactly (1949.25, 129x42) when solved alone (`PLANAR_ONLY`); among all variants at once it misses its 15 s budget. mapped T2 is 1003, where the hand composition had 935. The mapped one-lane T breaks the bay and adjacent-split rules (a known gap, hidden test).
- Four variants per composition (LL²: S fray side x diagonal west fray; T2: S fray side x S join side), each built and solved on its own thread; the least area inside the outline wins, ties by variant order.
- The model translates `wip/planar-probe8.py`: strands of segments in half-tile units, optional segments (binary z, length 0 or in [lo, hi]) between pieces of one heading (D321), decks, crossings, prio rows and doubled-lane copies; curve rows follow whole routes and lapse with the optional curves they span.
- Each solve is a fresh `JunctionMilp` with every variable integer and parity auxiliaries, warm-started from the model's last solution, with a 5 s time limit (`JunctionMilp::SetTimeLimit`); a solve that reaches it counts as infeasible, and a variant gets a 15 s budget for its lazy clash loop.
- Clashes are separated lazily (one pair per round, first of four sorted directions that stays feasible); the area inside the outline is then minimised by sequential linearisation around the clash-free solution.
- The output is one `PlanarRouteProgram` per route path and copy choice: pieces (tile, heading 0 N .. 7 NW, Ground/Head/Middle, line index in the path) and copy switches; routes and paths are matched to the topology by route name and the comma-joined line names.
- On the test spec the solver finds LL² at 800 tiles (variant `-1F1w1`, 64x47) and T2 at 935 tiles (`+1F1w-1`, 89x33).
