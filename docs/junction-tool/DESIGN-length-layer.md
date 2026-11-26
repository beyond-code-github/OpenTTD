# DESIGN: the length layer, tile routing and group compaction (draft 2026-11-26, D462)

Status: draft for the engineer. Read with FACTS.md (M8-M13, M11, T4a, C1) and D462. This replaces the drawing program's solve (stage A/B, choosers, joint CP). It does not replace Structure, the track map, the domain or the drawn hub.

## 1. The problem (D462)
Given one Structure variant, produce its most compact legal drawing, or near it, fast. Legal means the domain (tile rules) **and** curve speed (G3) **and** the coop path rules (L1-L4). Compact means the least area inside the outline (K1).

## 2. Why tiles (M11)
Every rule we have is a tile rule or a rule along a path of tiles, and each is cheap to check exactly. The row-based solve had to translate them into inequalities over continuous points, and every gap in that translation cost weeks (M10). Here the layout **is** tiles: a rule is checked where it lives, by the code that already judges the game (the domain, `JunctionMinCurveSpeed`). Nothing needs an encoding, so there is nothing to keep exact.

## 3. Inputs (kept from today)
- The track map: tracks, nodes, each track's events in order, crossings, structures (separator, crossers, allowed kinds, limit), copies, prio rows, sides per stretch.
- The headings the rules force: port headings, each separator on an axis across its span, node legs one eighth off (D404), crossing angles within their rules. **Not** Shape's free choice between a diagonal and an axis run with an S-bend (T4a): that is decided here.
- Rule fields: tl, cl, bay, coverage, node_leg, sd, deck and tunnel limits, port gaps.

## 4. The model
- A **layout** is the drawn hub on tiles: per track its pieces (tile, trackdir), nodes on switch tiles, structures as head, middles, head on an axis, ports on box edges.
- **Groups** that move as one (M11):
  - a **bundle**: lanes running together, their copies (equal, one tile apart, B5), their prio rows (one lane beside, coverage long, L2); like a PCB differential pair;
  - a **structure**: its deck and every crosser's crossing tile under it (B1, B4).
- **Legality** is one function on a layout: domain plus curve speed plus coop path rules. It is the only judge. Every state the algorithm keeps is legal.

## 5. The algorithm (VLSI symbolic layout, then compaction)
1. **A legal sparse start.** Lay the topology out with generous spacing: structures and nodes far apart, tracks routed between them on tiles by a state-space search (state: tile edge, heading, and the bends inside the train-length window, so curve speed holds by construction; G3). Fixed-size elements (bridges, prio rows, bays) keep their size; only the free track between them stretches. Phase 1 needs no quality, only legality (M8 showed choices from a stretched drawing are usable).
2. **Compaction by legal moves.** Repeat until no move shrinks the area:
   - **cut shift**: everything on one side of a cut through free track moves one tile toward the other; each track the cut crosses loses a tile there;
   - **local rewrites**: shorten a run, slide a bend, turn a diagonal into an axis run with an S-bend or back (T4a), drop an S-bend that no longer does anything;
   - **group moves**: a bundle or a structure moves as one.

   Each move is applied and judged by the legality function; a move that breaks anything is undone.
3. **Hand-off:** the drawn hub (D452) to Heights and Signals, unchanged.

## 6. Known risks (to probe in this order, riskiest first within the layer)
1. **Compaction quality.** Does a greedy sequence of legal moves reach B-level density, or do coupled tracks lock (M11)? D445 parked one-axis compaction in the row solve because fixing one axis freezes diagonals. Here a cut that crosses a diagonal needs a rewrite at the crossing (a diagonal cannot lose one tile of x alone), so cut shifts and rewrites must combine. This is the first thing to test.
2. **Speed.** Number of moves times the cost of a legality check. The domain on T2 runs in milliseconds; incremental checks (only the tiles a move touched) may be needed.
3. **The sparse start for any topology:** placing structures and nodes so every track routes.

## 7. First probe (risk 1)
Use T2's existing legal drawing (`t2-p6cf`, 68 x 50.5) as the sparse start. It is legal, stretched, and on variant 0. Build only the legality function on a tile layout and two moves (cut shift in x and y, with the diagonal rewrite at the cut). Run greedy compaction. Measure the area against 69 x 48.5 (rows, bends fixed) and the time. If cut shifts lock immediately, record where and why before adding moves.

## 8. Open (for the engineer)
- Does the outline area (K1) stay the objective, or the box first?
- May compaction change a node's position along its trunk freely (it moves bays and coverage with it)?
