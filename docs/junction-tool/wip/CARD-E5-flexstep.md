# CARD E5-FS: a flex step needs no dummy (orchestrator, 2026-11-22)

## Context
- On Shape's 197, 830 of the 878 dummies sit at flex bends (wip/probes/e5land/NOTES.md). A flex pair (h, h±1, h, D397) has one reflex bend on each side. H12b resolves each one with a dummy, and each dummy's named landing pins lengths, even when the flex is 0.
- With flex off there are 48 dummies, but 197 can't be drawn (it needs steps).
- A (choosing landings with binaries) would attack the symptom. Parked, after Pete's go on the Gemini exchange (2026-11-22).

## The rule (what changes)
1. **Shape (H12b and H12):** cells are refined on the face walk with every flex pair taken as size 0. A flex bend is no corner, and only the other reflex corners get dummies. On 197 that is about 48 dummies.
2. **Drawing program / Slots, clearance per cell:** a cell's walls are grouped into **sides**. A side is a maximal run of walls whose headings are equal once each flex pair's insert is removed, so the run is a, insert, b, ... (also the left and right pairs of one stretch), plus today's straight-corner joining. Each side has its *lines*: the lines of its h-heading walls (a, b, ...), **not** the insert's line.
3. **Rows:** each vertex of a side keeps its rung spacing from **every line of every other side**, except the two sides meeting at that vertex (as today). No row targets an insert's line. No row goes from a side's vertices to its own lines.

## Why it is planar with clearance (proof sketch, to be written out in full in SPEC-shape §8)
- Take a side S with h-lines at offsets o_1, ..., o_k across h (the a and b of each pair, shifted by the flex sizes). Its *strip* is the band between its innermost and outermost lines, and every point of S (inserts included) lies in it. The *inner line* of S is the line nearest the cell's interior.
- **Rows give separation.** Every vertex of another side T keeps the spacing from every h-line of S, so in particular from S's inner line, on the interior side. T is the polyline through its vertices, all inside that half-plane, so T as a whole stays at least that spacing inside S's inner line. S lies wholly on the outer side of its inner line (in its strip). So S and T are separated across h by at least the spacing, and every point of the insert is covered too, because the insert lies in the strip.
- **Turning gives convexity.** With flexes at 0 the cell's sides have fixed headings that turn one way and close (H12 on the refined walk), so the inner lines bound a convex polygon. Every side lies outside it (in its strip), and every other side's vertices lie inside its half-planes. The sides therefore form a closed chain with no two non-adjacent sides closer than the spacing.
- **Adjacent sides** share only their corner vertex. Each neighbour's far vertices keep rows against the other's lines, as today; the corner itself is exempt (k, next).
- **End caps (Gemini's check, 2026-11-22):** an outward step must not push its strip past an ADJACENT side's line. That is held by rows, because the step's inner vertices (insert/a and insert/b) are vertices of its own side and keep rows against every line of every other side, the two adjacent sides included. Only the shared corner vertex is exempt, and only against the two sides it touches. The written proof must state this, and the inner test must include a step whose b would cross the next side's line, which must be refused.
- **Units:** an insert's points lie between a's and b's lines, so the distance across h is what the rows hold. For a diagonal insert next to an axis side, the tile rule's spacing is measured on the h-normal (RungSpacing(h, own)), the same quantity today's rows use. No extra row is needed (to be checked in the written proof, D334 tile rule).
- **Gemini's version (2026-11-22)** sweeps the un-flexed wall by s·u(h±1) and subtracts max(0, -u·n)·s per opposite wall. It is the same idea, written per opposite wall. Ours needs no s term, because rows against both a's and b's lines cover the shifted wall exactly.

## Cost
- **Conservative in one way:** another track can't enter the pocket that an outward step leaves (the space between a's line and b's line on the pocket side). A toggle candidate if a hub ever needs it.
- **No binaries.** Rows are about the same count as today's per cell; dummies drop about 18×.

## Tests (outside-in, for the brief)
- **Outer:** Shape's 197 draws with clearance. Today it is infeasible; the honest expectation is that the remaining landing failures among the 48 non-flex dummies may still block it. Measure per face, then whole.
- **Inner:** a hand stretch with one flex pair beside an opposite wall. With the flex at size s, a track one lane from b's line is accepted and a track inside a's line is refused. No dummy at the flex bends; the cell count equals that with flex removed.

## Prior art
- Tamassia's refinement handles reflex corners in general.
- "Zero-size steps are no corners" is our octilinear analogue of keeping bends that cancel (a bend pair) out of the orthogonal representation's face structure (Bridgeman et al. 2000, turn-regularity: a face whose turn sequence cancels locally needs no refinement there).

## Decision for Pete
- **Recommendation:** adopt (amends D385 (1) and H12b: flex bends are not refined; side groups in the clearance rows). A parked, kept as the fallback for the remaining non-flex dummies.
- **Reversible:** yes. Behind a flag in Shape (H12b skips flex bends) and in the drawing program (side groups).

## Progress (2026-11-22)
- Proof written: SPEC-shape §8.8 (Lemma 1 strip, Lemma 2 tile separation across a grid line, theorem steps 1-5, end caps step 3). Units check: no extra row for diagonal inserts beside axis sides or axis inserts beside diagonal sides (Lemma 2; the diagonal case uses the tile rule's opposite corners).
- Added while proving: D408 (vertices keep >= 0 from their cell's dummy lines). Open, pre-existing: no spacing across a dummy (§8.8 Open).
- Steps: FS-a Shape (H12/H12b on the reduced walk, flag), FS-b drawing program (side rows, flag), then 197 per face and whole.
- FS-a DONE f46aeba9fe (197 dummies 878 -> 48; T2/T3/LL2 flex-bend dummies 7956 -> 0). FS-b DONE 12c4538238 (`PlanarCellClearRows`, `flex_sides`; gate 726/726).
- 197 by sides (FS-b measurement): faces alone 139 feasible, 9 infeasible (7, 8, 11-17), 14 undecided at 15 s (4, 6, 46, 52, 64, 71, 72, 75, 84, 99, 124, 141, 147, 153); whole Infeasible in 2.2 s (clear 275, piece 141, cross 31, node 16, end 12, land 10). The reported IIS tags (`piece` pairs) are the LP relaxation's and not meaningful.
- Diagnosis (temp test, reverted): faces 7, 8, 11, 12, 13 have no dummies and draw with landings only; the side rows make them infeasible. Faces 7 and 11 are slivers between two lanes of one out port (EO1/EO2, CD1/CD2; pinned 2 apart at the box), closed at the far end by a lane that crosses or merges between them (F12b x B2c then BF12b; D2). The closing side's inner vertices must keep 2 from one lane's lines and 1-2 from the other's inside a strip 2 wide, which no lengths give; and the room V makes there is the lanes stepping apart (a fan), i.e. the outward step's pocket that D407 closes (the stated cost). These are bundle-interior faces (lanes of one channel or port with their node group), which v2 gives no cells (D388: lanes as offset curves): the corridor family, not free faces. Faces 13-17 (EF1-3, SO1-3) follow the same naming pattern; unconfirmed one by one.
- Next: FS-c (zero objective on the 14 undecided faces, running); then the corridor family for bundle-interior faces (design in the orchestrator, outside-in).
- FS-c DONE 2ac8dafd6f (`PlanarDrawOptions::tidy`): with no objective 12 of the 14 undecided faces draw (1-2 s, face 6 6 s); 71 and 141 still undecided at 15 s. So on 197 per face: 151 draw, 9 infeasible (bundle-interior, above), 2 undecided. Whole: Infeasible in 2.2 s either way. Default kept true (tidy point for tests); per-face measurements use tidy = false.
- FS-d DONE da69ef5286 (`pocket`, default false; SPEC-shape §8.9). NEGATIVE: with the pocket rule on every side, 197 faces alone go to 119 feasible, 33 infeasible, 10 undecided (from 151/9/2); whole Infeasible in 3.3 s (piece 352, clear 175, along 138, cross 76, node 30, end 30). The target faces 7, 8, 11-17 stay infeasible (7, 8, 11, 13 now name `box`). So the pocket is not the only thing blocking them, and the blanket along rows break 34 faces that drew. Flag stays off. Next per D409: the elastic mode (FS-e) to see what actually stretches on faces 7 and 11 and on the whole hub, before any further rule.
