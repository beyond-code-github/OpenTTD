# Plan: organic junctions on any terrain (draft for the engineer)

## Goal
Place a junction on any terrain; it slopes, bridges, tunnels and (when cheaper) terraforms as needed, in the cheapest, most organic way (engineer, 2026-11-09; D78, D95). One hover generation under 100 ms (D151). Keep it plain: the search space is small (a T or 3-way is 4-6 movements in about 40 x 20 tiles with 3-4 heights).

## What the examples show (RECON-organic.md)
- The main lines stay level; the branches change height around them.
- A crossing is a per-crossing choice: tunnel under (most common, often a row of portals at 1-tile lane pitch, staggered 1-2 tiles), bridge over (short, doubled), or around.
- On flat land a 2-3 tile mound is raised in front of each portal; otherwise the land is left alone.
- Along a flow: split, optional shift, crossing, merge, linked by straight runs of 3-10 tiles. No fixed template: the same few pieces in different orientations and mixes.
- Game rules (checked in code): bridges never cross other bridges; tunnels cross when their depths differ; per tile, surface track + one tunnel per depth + at most one bridge span.

## What we have and keep
- The line router (LP4-LP7): multi-track lines, terrain at one plane, bridges for all tracks, waypoints, frozen prefix. With one track it is a single-track router.
- The checker (all rules), the signal pass, prio timing and sizing (D137), the prio and merge windows (D108, D148, D148a), pricing and the build sequencer, the ghost and the two-tier hover (library + refine).

## What goes
- The fixed junction shape (W-row crossing set, one bridge section, lane styles 0 and 1, the sweep over heights/slacks/margins). Its rules knowledge survives as the windows where a movement may leave or join a line.

## The model
- **Given:** the main lines (the lines being joined) and the terrain snapshot.
- **Per branch movement:** route it with the one terrain-aware router, from its split window to its merge window. The router's state is tile, heading and height; its moves are track at the current height (flat or on a foundation), a slope up or down (an axis piece on an inclined slope tile), a bridge (existing B<n>), and a tunnel (from a portal slope tile straight on to the next tile at the portal's height, priced by the game's formula). Earlier routes occupy their tiles at their own level only, so passing under or over them is just a move with a price.
- **Windows:** where a movement may split off or join is set by the coop rules as today (prio coverage, no split inside a coverage, signal spacing, straight run before a merge).
- **Then:** the signal pass and the checker on the whole layout. On failure, the next movement order or the next window position. Keep the cheapest that passes (cost: track, slopes, foundations, clearing, bridges, tunnels, terraform; then footprint).
- **Terraform:** first as one priced move, a mound in front of a portal (raise the 2-3 tiles a portal needs); levelling plateaus later if needed (D95: a priced fallback, never forced).

## Steps (each test-first, one agent each)
| Step | What | Gate |
|---|---|---|
| O1 | Heights in the line router: state gains z; slope up/down on inclined slopes; usable-at-z per tile; per-level occupancy (surface, bridge). Synthetic terrain tests: a hill crossed by slopes, a valley by a bridge. (Was LP8.) | Painter and junction both use it |
| O2 | Tunnels in the router: portal on a matching slope, straight on to the exit at the same z, depth-aware occupancy (tunnels cross at different depths only), game price formula. Tests: a ridge tunnelled, two tunnels crossing at different depths, a same-depth crossing refused. | |
| O3 | Portal mound as a priced terraform move. Test: flat land, a crossing goes under via a mound when cheaper than a bridge. | |
| O4 | Junction generator on the router: main lines given, each branch movement routed between its windows, movement orders tried, signal pass + checker, cheapest wins. Outer test: the T on the test specs is rule-clean on flat ground, on a hill and across a river; the TL6 narrow-branch case builds. | Replaces the template and both styles |
| O5 | Timing gate and hover: one generation under 100 ms on the test specs and the NUTS rig; the GUI's refine tier uses it; library kept only if still needed. | 100 ms |
| O6 | Rig check on a hilly map, announced with the join command. | Engineer's eye |

The painter continues on the same router: after O1-O3, LP9 (corridor terrain snapshot) onwards. Stations are unaffected.

## Questions for the engineer
1. Is the order right: router heights and tunnels first (O1-O3), then the junction on top (O4)?
2. Terraform in the first cut: only portal mounds (O3), or also levelling and cuttings?
3. Does a junction movement route as a single track, or as its lane bundle (for example both lanes of a doubled branch together, as the line router does)?
4. Anything from today's junction (the S2 solver, the library) you want kept regardless?
