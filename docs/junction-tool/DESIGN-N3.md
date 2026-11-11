# DESIGN-N3: node template, render, structure search (D182-D184)

Status: orchestrator design, 2026-11-11. Builds on DESIGN-nodes rev 3 (§3, §4a, §4b, §9), FLAT-T-CHAINS.md, N0a/N0b/N1/N2 (merged).

## 1. Geometry of the template (from the flat T, D184)
- Coordinates: the flat layout's (after the crop). Rows = y, the junction's long axis = x.
- **Paths**: the checker's traced paths (MakeJunctionCheckContext(...).paths, as ExtractJunctionChains reads them). A **run** = consecutive pieces of one path with the same direction.
- **Points**: the pieces the model positions: the first and last piece of every run on every path, every switch piece (SwitchEventsOf), the first and last on_bridge piece of every bridge on a path. A point is identified by (tile, trackdir); paths that share a piece share the point.
- **Links**: consecutive points on one path. A link is **free** iff both points are on the same row, the path travels along X between them (trackdirs of the X axis), and no piece from the first to the last is on_bridge. Every other link is **rigid** (column runs, diagonals, curves between adjacent pieces, bridge spans).
- **Classes**: union-find over points across rigid links. Each class is one variable of JunctionPositions; a point's x = value(class) + offset (offset from the flat). Every piece of a rigid run belongs to its class with an offset too (needed for crossings).
- **Direction**: on one path all free links travel the same way in x (assert; true for the T). So between two points p, q of a path, units(q) - units(p) = flat units + 2 * dir * ((x_q - x_p) - flat(x_q - x_p)). Every unit rule becomes a difference constraint between two classes (or a constant check when both are one class).

## 2. Constraints (the template)
1. Free link p->q: dir * (x_q - x_p) >= 1.
2. Curve gaps (N0a): per path, JunctionCurveGaps(hands, tl, curve_speed, reference) over the path's curves; each gap g between curve points ci, cj -> the unit formula of §1.
3. Prio coverage (N0b's rule, JunctionChainViolations): for every prio merge m on a main-line path, every curve c before m: units(m) - units(c) >= 2p; every split s before m: units(m) - units(s) >= 2p + 1.
4. Crossings: for every bridge (heads h1, h2 on one path, h1 first in travel) and every piece of another path whose tile is strictly between the heads in the flat: that piece's x strictly between the heads' x (two constraints, >= 1 each). The piece is on a rigid run (assert).
5. Anchor: the class of the last piece of the EIn->SOut path (the S port, fixed by the frame, D178) is fixed to its flat value.
Not in the template yet, found by the oracle (CheckJunctionRules) in tests: signal slots, collisions between unrelated tracks, D172's span windows beyond rule 4. Added only when a test shows one (simplicity rule).

## 3. Render
For values of every class: every point at (value + offset, flat y); every rigid run's pieces translated with their class; every free link filled with axis pieces along its row; bridges translated with their heads' class; E/W port end runs extended so the crop of FinishJunctionCandidate (content +- 2) gives the box. Then NormaliseJunctionLayout, PlaceJunctionSignals, NormaliseJunctionLayout (as FinishJunctionCandidate does). Round trip: render(flat values) == the flat layout.

## 4. Structure (N3b/N3c, designed after N3a measures the template)
- A structure = a set of **shifts**: (free link, side +-1). A shift is an S-bend pattern (two opposite 45-degree curves with a one-row diagonal) inside that link; every point after it on the path takes dy += side until the next shift on that path. Rigid diagonal links whose ends get different dy change their x offset by one per row (45 degrees) and their units by 2 per row; a free link or bridge whose ends get different dy is invalid.
- Facts (N2) per structure: a free link on row r covering column c that is not Straight, or a break between c and c+1 (StraightRun false): JunctionPositions::AddAvoid with an auxiliary var for x_b - 1 for breaks. A rigid piece whose tile fails its fact (Straight/Switch/Half): RestrictDomain on its class.
- Search: best-first by cost (added curves, then tiles), start = flat structure; on infeasibility the failing fact's link gets the repair children: a shift to row r-1 or r+1 before the fact (D182: may stay shifted; another fact or rule later adds the shift back). Bound: at most rows-available shifts per line. Collision of rendered tracks = this structure fails (measured; becomes a constraint if it recurs).
- Outer tests (N3c): the house at (xI-1, 0), the unbuildable hub tile, the W-end incline: each builds, CheckJunctionRules clean, no router call.
- N3c scope note: this step realises a shift as a detour within one free link (shift out and back before the next point). Staying shifted past a node (D182) is a later repair kind, added only when a measured case needs it.
