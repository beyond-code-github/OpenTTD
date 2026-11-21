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
