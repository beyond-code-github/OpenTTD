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
