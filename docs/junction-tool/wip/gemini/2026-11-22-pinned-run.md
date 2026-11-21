# For Gemini: the pinned answer-key run on the expert's hub

## What we ran
- The expert's PSG 197, extracted from the save. Our model's points are pinned to the expert's actual coordinates: every line's start and end (node points at the switch tile's entry-edge midpoint, port ends on the box edge) and every crossing point. 324 points, all matched. Pins sit within a window of +-w half tiles, with one common shift; lengths are free.
- Rows as before: pieces (axis 2k half tiles, diagonal >= 1, flex pieces may be 0), nodes, crossings (crossing point a distance from piece ends), tunnel spans and end runs, ports, plus clearance (per convex cell with dummy edges, by sides). Elastic mode is an LP with slack per row family.

## Results
1. **Clearance off, LP:** window 2 needs slack 41, all small; window 8 needs 0.5 (one crossing). So the base rules agree with the expert's geometry as real numbers.
2. **Clearance off, integer program:** infeasible at windows 2, 4 and 8. The (LP) IIS names the pieces of four lines around crossings. So something integral fails that the LP allows: we suspect half-tile parity at crossings. A crossing point is an integer variable; the crossing parameter along each piece counts from either end, and axis pieces are whole tiles.
3. **Clearance on (cells), LP, window 8:** slack 233 over 144 rows, spread (clear 75, cross 51, port 41, end 30, node 17, land 13), none over 8. The worst cells contain dummy edges.
4. **Pairwise exact separation** (no dummies; per wall pair one binary per axis side; about 38,700 binaries): infeasible at window 2, but the base is infeasible there too. Undecided at 120 s for windows 4 and 8.

## Questions
1. Where does half-tile parity bite at a crossing in OpenTTD terms? Our model has a straight separator on an axis and the crosser at 45 or 90 degrees, a tunnel or bridge. What are the parity facts of the crossing point (tile centre? edge midpoint?) for an axis x axis crossing, and for an axis x diagonal one? And what must the two pieces' lengths from the crossing satisfy?
2. To separate "dummy cells too strict" from "spacing too strict", is there a cheaper decisive test than integer pairwise (38k binaries)? For example: pairwise only inside the faces whose cells stretch, with cells elsewhere; or an LP relaxation of pairwise with the binaries fixed from the expert's actual geometry (we know which side each wall pair is on in the save, so the binaries are not unknowns).
3. If the dummy choice turns out to be the over-strict part, is one binary per dummy (over its candidate landing walls) a sound fix, given convexity? Or is there a better linear way to let a dummy's landing wall follow the lengths?
