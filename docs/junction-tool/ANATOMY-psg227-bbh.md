# Anatomy: PSG 227 LLRR "Simple" BBH (mfb), a balanced LL_RR 3-way

Source: `PSG_227.sav`, centre tile 367,339 (engineer). Export `OD/psg/anatomy/psg227/bbh.json` (box 327,299-407,379), ASCII `OD/psg/anatomy/psg227/pic.txt` (OD = `/mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad`). Junctionary BBH list: LLRR "Simple", PSG227, mfb.

## Shape (orchestrator, from the export)
- Three LL_RR arms: north (diagonal approach from the NW, then Y-axis at x 352-358), east (X-axis, y 342-345), south-west (Y-axis at x 362-372 from about y 365, turning west along y 360-365). An unrelated single line runs down the box's east edge.
- Woven core about x 356-382, y 320-350: about 26 x 30 tiles. Box counts: 526 plain track tiles, 415 signal tiles, 38 bridge tiles, 2 tunnel tiles, 1 station tile.
- Bridges in short runs of four, stacked around rows 333-346, x 360-375; heavy diagonals in the arm approaches and fans.

## The engineer's reading (2026-11-14, in his words, grouped)
1. **One choice lane per input line.** "Out of each output choice an input line has, *one* of them gets a prio but the other does not." For the north lines, "the outer line there has a choice but the inner line does not, it connects only to the outgoing line"; for the east lines "it's actually the innermost one that gets a choice". So each input line has one lane that may take either lane of a destination and one lane fixed to one destination lane.
2. **Prio goes to the lane without a choice.** "They've given priority to the lines without a choice, and left the ones with choice to yield in both cases." For trains exiting to the south-west, "one path yields to trains arriving from the top, the other to trains arriving from the right."
3. **Three-into-one joins.** "Any one join node only ever has three lines coming in to it. The points where the three lines merge towards the ports either all convene on a single tile, or are on adjacent tiles such that there is no space for a signal between them - effectively the same thing."
4. **Routes of the north lanes.** "The outermost line takes a very simple path - splits to the outside, then splits again later and then crosses over to the bottom left of the core where it's quieter and space is easier to make. The inner line splits to the inside, and then immediately swings right under the bridges that carry the traffic straight ahead." He suspects "a number of TL violations across the hub".
5. **A choice split that looks like a doubled bridge.** Near the top of the core: "it's a choice split - and the leftmost bridge comes down for a signal then rises again to allow the line from the other direction to pass under it. So the single line choices split, one crosses the mainline and climbs again to let the other line under, which then joins right up with the other choice lane before the merged line then meets the prio line on the topmost edge."
6. **Prio styles.** "About three different prio approaches in the hub, some involving path signals (I think this allows prio over a doubled bridge) and some involving two way signals."
7. **Diagonal crosses at doubled-bridge ends.** "Diagonal crosses that allow the end of a doubled bridge to take either direction - so this combines a bridge end where the two bridges converge but then allow an immediate split": for the south-west inputs a choice of outgoing direction, for the north-east inputs "a choice of line immediately after the main core crossing ends".
8. **Terrain.** Built around a bay: the direct route to the top of the hub would cross the water, so the line hugs the shore "and then almost double back on itself to get in to position for the left turn down through the core out to the bottom left. It's actually this turn that helps with the compactness because it allows the inside line with the choice to bridge over the diagonal of the turn and keep its crossing short."

## The engineer's clarification (2026-11-14)
- "Every line from every direction initially splits into two - this represents the choice of direction outbound. Then for each of the two lines from an incoming direction to a given output direction, *one* of these then splits again and allows choice of output line for that direction (the direction having already been chosen by the first split)."
- So per input direction: each of its two lanes splits by destination (direction choice); per (input direction, destination) two tracks result, one per input lane; one of them splits again by output lane (line choice), the other runs fixed to one output lane, with the prio.
- "The two post-direction-split lines incoming from the bottom left heading out to the right, join with no prio *before* the core and pass under the bridge as one, whereas the equivalent lines incoming from the right heading out to the top pass over the core and then join." A pair of tracks to one destination may merge before a crossing to cross as one track.

## What this means for our model (orchestrator, to confirm)
- **Balance is per line, not per lane pair.** Each input line reaches both lanes of each destination through its one choice lane; the other lane is fixed. That is fewer paths than our pairing "any" (every lane to every lane) and more than "keep". A candidate pairing mode: one choice lane per input line per destination.
- **Joins are three-way:** the fixed (prio) lane plus two choice lanes meet on one tile or adjacent tiles. Our nodes are two-legged and cascade; this is a different node shape.
- **Prio goes to the fixed lane;** choice lanes yield. Prio is not one 23-tile element per main merge (D119/D159); several prio styles exist (path signals over a doubled bridge, two-way signals).
- **Bridge ends double as splits** (diagonal cross right after two bridges converge). Our model separates bridge heads from nodes.
- **Real hubs accept spacing that our checker may reject** (suspected TL violations): a candidate over-prescription to measure with the corpus oracle.
- **Terrain shapes the hub**, and a turn can be used to shorten a crossing.

## Not yet done
- A lane-by-lane trace through the export to confirm 1-3 and 7, with tile boxes for each join, choice split and prio.
- Signal types per tile (path, two-way) at the prios.

## Trace (tile evidence)

Recon 2026-11-14, from the export plus the save's piece graph. Scripts and raw outputs: `OD/psg/anatomy/psg227/trace/` (`reach.txt`, `blocks_raw.txt`, `per_lane_blocks.txt`, `per_lane_events.txt`, `events.txt`, `paths_all.txt`, `signals_bridges.txt`; entry point `a1.py`..`a9.py` over `core.py`/`net.py`). Marked INFERRED where the data does not state it.

**Method.** Box 335,311-392,368 (cuts each arm cleanly: 4 lanes per arm, 2 in and 2 out; excludes the station tile and the unrelated east line). A piece graph with directions from the one-way signals. 11 pieces carry signals in both directions (two-way: 357,334y 357,335y 361,329r 362,332y 363,332y 366,356l 371,350y 371,358u 372,336r 372,338l 374,338r) and 6 tiles are diagonal crosses where the piece directions cannot be fixed by propagation (367-368,346 and 373,345-346 with 368,347 and 374,346); those pieces were left traversable both ways. The reach table is identical under the strict one-way model, so none of this changes the result. No path-type two-way signals exist; the only path signals are two `path_oneway`. The graph has small cycles round the two-way pieces (e.g. 361-362,329-331 and 373-376,338-342); paths below are shortest paths. A block below = a signal-free cluster (pieces joined with no signal between them).

**Arms and lanes (task 1).** Names follow the picture (`pic.txt`: x to the right, y down). Lane id = edge coordinate; "inner" = adjacent to the other direction's pair, "outer" = the edge lane. The lanes are LL_RR at each edge.

| Arm | Edge | Out lanes (outer, inner) | In lanes (inner, outer) |
|---|---|---|---|
| N (top) | y=311 | N350, N353 | N354, N355 |
| E (right) | x=392 | E343, E342 (E343 is inner) | E344, E345 |
| SW (bottom-left) | x=335 | SW362, SW363 (SW362 inner) | SW361, SW360 (SW361 inner) |

Fixed lane (reaches one lane per destination): N354, E344, SW360. Choice lane (reaches both lanes of each destination): N355, E345, SW361. So the choice lane is the outer in-lane at N and E, the inner in-lane at SW. Engineer's inner/outer: N matches ("outer has the choice"); E does not by this edge-order definition (he said the innermost has it; the choice lane E345 is the outer one at x=392); SW unstated. INFERRED: his "inner" at E is measured differently or at the core, not at the box edge; ask him.

**Reach table (task 2).** 18 of 24 pairs; the 6 unreached are same-arm pairs.

| in \ out | N350 | N353 | E342 | E343 | SW362 | SW363 |
|---|---|---|---|---|---|---|
| N354 (fixed) | | | x | | x | |
| N355 (choice) | | | x | x | x | x |
| E344 (fixed) | | x | | | | x |
| E345 (choice) | x | x | | | x | x |
| SW360 (fixed) | x | | | x | | |
| SW361 (choice) | x | x | x | x | | |

Every out lane has exactly 3 source lanes, one fixed and two choice: N350 = SW360, SW361, E345; N353 = E344, SW361, E345; E342 = N354, N355, SW361; E343 = SW360, N355, SW361; SW362 = N354, N355, E345; SW363 = E344, N355, E345.

**Rule checks (task 4).**
- (a) Each input lane's first split is by direction: holds for 5 of 6, E345 holds with a doubling first. Direction splits: N354 at 362-364,328-329 (out: E342 | SW362); N355 at 362-363,324-326 (E pair | SW pair); E344 at 379-380,344 (N353 | SW363); SW360 at 356-357,339-340 (N350 | E343); SW361 at 363,354 (E pair | N pair). E345: the first split, 381-382,346, is a doubling (two parallel tracks that each still reach all four outs); each track then peels one destination at 374-379,345 (N-only | all) and 374-378,346 (SW-only | all), and the direction split completes at the diagonal cross 372-373,345-346 (4 tracks in, 2 out: 372,344r to N | 372,347u to SW). So the E345 direction split exists but spans three blocks over d11-d22.
- (b) Per (input arm, destination) exactly one of the two tracks splits again by output lane: holds for all 6, from the reach table (choice lane reaches 2, fixed lane 1) and the tiles. Line-choice splits: N355 to E at 369-370,330-331 (370,332y E342 | 370,332d E343); N355 to SW at the cross 367-368,346-347 (369,347r SW363 | 367,348y SW362), fed by two parallel 1x6 tracks at x 367 and 368, y 340-345; E345 to N at 361-362,340 (361,339y N353 | 361,339r N350, a path signal on 361,340l before it); E345 to SW at 370-372,347-348 (370,349y SW363 | 370,349u SW362); SW361 to E at 367-368,337 (369,337x E342 | 369,337r E343); SW361 to N at 359-361,342-343 (359,341y N353 | 359,341r N350). The fixed tracks run to one out lane each but are not plain: each shows a doubled stretch (splits to two parallel tracks that rejoin as a pair): N354 to SW (365,331 split, rejoin 365-366,346-347), E344 to N (363-364,340-341 split, rejoin 362-363,330-331 together with the third E344 input), SW360 to N (357,334 split, rejoin 357,335-336), SW360 to E (374-376,340-342 and 373-374,338-340). INFERRED: the doubling is the "prio over a doubled bridge" shape of reading 6.
- (c) Prio on the fixed track: holds at all 6 final joins, on the signal evidence (INFERRED: entry pre-signals mean yield). The fixed lane's input is gated by a plain block signal or a two-way combo; every choice input is gated by an entry pre-signal. Details under prio styles.
- (d) Three-input joins: holds at the lane level for all 6 (3 sources each, above). At the tile level 3 are single blocks with three inputs on one tile pair (N353 at 360,328-329; SW362 at 367,356-357; SW363 at 370,358-359). The other 3 are two stages of 2 inputs: the two choice lanes join first, then the merged track joins the fixed lane. E342: 377-378,338 then 378-379,339, one tile between (378,338, entry signal on it). E343: 375-376,343 then 377-378,342-343, one tile between (376,343, entry signal). N350: 358,330-331 then 356,328-329, with a block signal (358,330r) and an entry signal (357,329r) over the three tiles between, so not "no space for a signal": two signals fit. Partly holds.
- (e) FAILS for SW to E, holds for E to N. SW360 to E343 and SW361 to E342/E343 share no piece until 378,343x (the east end, after the core; the E-branches are 53 and 90 pieces, 15 shared, all at x 378-392). They do not join before the core and there is no "pass under the bridge as one" of the two: SW361's E342 branch crosses by the tunnel 371,338 to 375,338 and its E343 branch goes round by 370-376,341-343, while SW360's runs the two-way route 372,338l-374,339r. The only pair merge on that path is SW360's own two parallel tracks (373-374,338-340, both inputs SW360). E344 and E345 meet on the way to N353 at 360,328-329 and on the way to N350 (E345 only, with SW361) at 358,330-331, both north of the bridge stacks (y 328-331; bridges start at y 330-334): holds. If his "bottom left" is the E arm instead (screen view), E to SW also joins late: 367,356-357 and 370,358-359, south of the core, so it fails either way. INFERRED: ask him which pair he means.

**Block table (tasks 3 and 5).** Boxes are tile boxes (x0,y0-x1,y1, width x height) of the signal-free block; in/out = tracks entering/leaving across signals. Travel order = increasing distance from the in lane. Core footprint of all blocks: x 356-382, y 324-359 (27 x 36); bridges and tunnel x 358-379, y 330-351.

| Block | Box | Size | In/out | Lanes through it |
|---|---|---|---|---|
| Direction split N355 | 362-363,324-326 | 2x3 | 1/2 | N355 |
| Direction split N354 | 362-364,328-329 | 3x2 | 1/2 | N354 |
| Direction split E344 | 379-380,344 | 2x1 | 1/2 | E344 |
| Doubling E345 | 381-382,346 | 2x1 | 1/2 | E345 |
| Peel splits E345 | 374-379,345 and 374-378,346 | 6x1, 5x1 | 1/2 each | E345 |
| Direction cross E345 | 372-373,345-346 | 2x2 | 4/2 | E345 |
| Direction split SW360 | 356-357,339-340 | 2x2 | 1/2 | SW360 |
| Direction split SW361 | 363,354 | 1x1 | 1/2 | SW361 |
| Line split N355 to E | 369-370,330-331 | 2x2 | 1/2 | N355 |
| Line cross N355 to SW | 367-368,346-347 | 2x2 | 4/2 | N355 |
| Line split E345 to N | 361-362,340 | 2x1 | 1/2 | E345 |
| Line split E345 to SW | 370-372,347-348 | 3x2 | 1/2 | E345 |
| Line split SW361 to E | 367-368,337 | 2x1 | 1/2 | SW361 |
| Line split SW361 to N | 359-361,342-343 | 3x2 | 1/2 | SW361 |
| Doublings of fixed/choice tracks | 364-365,330; 366-367,331; 363-364,340-341; 374-375,340; 357,334 | 1x1 to 2x2 | 1/2 | N354, N355, E344, SW360 |
| Join N350 (2 stages) | 358,330-331 then 356,328-329 | 1x2 each | 2/1 each | SW361+E345, then +SW360 |
| Join N353 (3 inputs) | 360,328-329 | 1x2 | 3/1 | E344, SW361, E345 |
| Rejoin of E344 pair | 362-363,330-331 | 2x2 | 3/2 | E344 x3 |
| Join E342 (2 stages) | 377-378,338 then 378-379,339 | 2x1 each | 2/1 each | SW361+N355, then +N354 |
| Join E343 (2 stages) | 375-376,343 then 377-378,342-343 | 2x1, 2x2 | 2/1 each | N355+SW361, then +SW360 |
| Join SW362 (3 inputs) | 367,356-357 (after rejoin 365-366,346-347) | 1x2 | 3/1 | N354, E345, N355 |
| Join SW363 (3 inputs) | 370,358-359 | 1x2 | 3/1 | E344, E345, N355 |
| Rejoins of SW360 pair | 357,335-336; 373-374,338-340; 375-376,341-342 | 1x2 to 2x3 | 2/1 | SW360 |

Order on each path (e.g. N355 to SW362): direction split (d20), doubling (d31), line-choice cross (d47) after the parallel 1x6 tracks and the core crossing, then the 3-input join (d58). E345 to N350: doubling d11, peel splits d15, cross d20, line split d37, joins d49-55. SW361 to E: direction split d39, line split d54-73, joins d84-89. Full event lists per lane in `per_lane_events.txt` and `per_lane_blocks.txt`.

**Bridges and tunnels (task 5).** 19 bridges and 1 tunnel inside the box. Spans (tiles between heads): 12 of 3 tiles (x 358-363 at y 334-340, x 365-368 at y 334-344), 3 of 4 tiles (365,333-338; 366,333-338; 372,334-339), 4 of 2 tiles (359,330-333; 369,348-351; 375,346-378,346; 376,345-379,345). Tunnel 371,338 to 375,338, 3 tiles under. Head terrain heights 1-3 (the export has terrain height, not bridge deck height; deck height INFERRED not available). Two stacks of parallel bridges: x 360-363 (y 334-338) and x 365-368 (y 333-338 and again 340-344). Full list in `signals_bridges.txt`.

**Prio styles (task 3).** 261 block, 26 exit, 21 combo, 18 entry, 2 path_oneway signals in the box; 11 two-way pieces (combo or exit both ways).
1. Single three-input join with pre-signals: fixed input gated by a two-way combo (361,329r into N353, 366,356l into SW362, 371,358u into SW363), both choice inputs by entry signals. Three hubs' worth: N353, SW362, SW363.
2. Two-stage join with a pre-signal: the two choice lanes join with block/block (no prio between them), the merged track passes an entry signal, the fixed lane arrives on a plain block signal. N350, E342, E343.
3. Path and two-way: the only path signals are 361,340l (E345) and 363,340l (E344) at the E-to-N line split and doubling; two-way exit/exit pieces gate the E344 rejoin (362,332y, 363,332y) and the SW360 doubling (357,334-335), and a two-way combo/exit chain runs on the SW360 and N354 E-route (372,336r, 372,338l, 374,338r). INFERRED: these are the prio-over-doubled-bridge and two-way styles of his reading 6; the roles of the path signals are not proven by the graph.
So three styles, as he said.

**Spacing (task 5).** The export has no vehicles, so train lengths were not read. Shortest-path signal spacings (all 18 paths in `signals_bridges.txt`): most gaps are 2 tiles (55-70 percent), then 3 and 4; 1-tile spacing occurs 0-4 times per path (at bridge-head and diagonal pairs such as 353,314y-353,313y and 357,323x-356,323x, and 363,355y-363,354y just before the SW361 direction split); maxima 7-9 tiles over bridge and tunnel spans (9 on E344 to N353 and N355 to E343). Every main path carries 26-44 signals over 68-106 tiles.

**Findings for the model (INFERRED).** (1) Rules (b), (c), (d) hold at lane level; (a) holds with the E345 exception; (e) fails as worded for SW to E. (2) The "fixed" lane is not one track: all three fixed lanes double and rejoin on at least one destination, which is where the two-way and path signals sit. (3) Two of three join shapes are 2+2 cascades, not true 3-input blocks; the cascade puts an entry signal on the joint piece, one tile or one signal gap from the second join.
