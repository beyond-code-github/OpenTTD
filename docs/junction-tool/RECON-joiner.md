# Recon: the endpoint joiner (prior art)

Web research, 2026-11-08. wiki.openttdcoop.org was unreachable (ECONNREFUSED), and tt-forums is behind Anubis. Coop facts therefore come from search excerpts: likely, but verify when the wiki is reachable. Pages to read then: Mainline, Merging_Tracks, Basic_Junctions, Priorities, Signals.

## Coop practice

- **ML** (2–5+ tracks): the top priority.
  - **LL_RR:** the track count per direction, with the gap between them (in practice wider than 1 tile, for expansion). Model a line as {N_left, N_right, gap}, which matches our notation.
  - **Signals:** every other tile. One long gap (for example 6 tiles) bottlenecks the line.
  - **Bridges and tunnels:** double ML bridges and tunnels, and keep lines in sync through the doubling. A common error is a track exit inside a doubled section.
- **SL and SLH:** never join a sideline or a station straight to the ML; go through a sideline hub. Joiners yield to the ML (prios).
  - The waiting area must hold a whole train.
  - One excerpt says prio coverage is about 9 tiles (unverified).
- **Shift Mainline:** research-grade; not a default.
- **Junction criteria** (OpenTTD wiki, Junctionary/Junction Criteria, confirmed):
  - split-before-merge;
  - natural turns (left-hand exits);
  - sharpest turn (double-bend spacing);
  - direct paths;
  - symmetry;
  - breakdown impact;
  - area, hills, bridges.
- **Not found:** a documented rule for the minimum distance between junctions along an ML, or for where a branch may leave. Define our own: join points aligned to the signal grid, and at least one train of signalled straight track between junctions.

## Tools and UI patterns

| Tool | What it does | State |
|---|---|---|
| Vanilla Autorail | drag only, no pathfinding | built in |
| RoutePlanner patch (0.7, YAPF) | ctrl-click Autorail, then two tiles for start and end; hill-avoidance slider; no bridges | old |
| Rail/Road Planner patch (2019–20, A*, Factorio-style) | click-drag from start to goal, ghost preview, Shift to cancel, one drag per leg | stalled, unmerged |

JGRPP and CityMania: no auto-route tool found.

**Patterns to copy:**
- an endpoint is a tile plus a heading, snapped to the end of existing track;
- a ghost preview;
- the cost shown before building;
- one action per leg;
- a modifier key to cancel.

## AI rail pathfinder (AyStar library, confirmed)

| Parameter | Default |
|---|---|
| tile | 100 |
| diagonal_tile | 70 |
| turn | 50 |
| slope | 100 |
| bridge_per_tile | 150 |
| tunnel_per_tile | 120 |
| coast | 20 |
| max_bridge_length | 6 |
| max_tunnel_length | 6 |
| max_cost | 2e9 |

- **Endpoints:** sources are pairs of `[start_tile, tile_before]` and goals are pairs of `[last_tile, tile_after]`, many-to-many. Our `JunctionRouteRequest` starts and goals already have this shape.
- **Tuning:** forum users raise bridge and tunnel costs to about 200–300 to stop needless bridges.

**Worth adopting:**
- a turn penalty of about half a tile (this also fixes our generator's wiggly ties, from the D1 note);
- a slope penalty of about one tile;
- a bridge premium;
- the coop extras: curvature spacing (we have it), and penalties for running beside or crossing foreign track.
