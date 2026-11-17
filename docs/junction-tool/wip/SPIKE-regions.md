# Spike: regions from the topology vs the lazy clash sides (2026-11-17)

Code: `spike-regions.patch` (print hooks in SolveLazy1/SepOptions, test registration) and `spike-regions-test.cpp` (hidden test `[.regions]`, hub from env `REGIONS_HUB=T2|LL2`). Raw output was in the scratchpad `regions/` folder.

## 1. The planar map (verified by the run)
- Built from `topology.crossings`, `events`, `ring`, nodes and ports as the brief says; faces traced with the face on the left of each dart.
- Rotations needed no tuning: the first choice (port end = previous ring edge, next ring edge, line; crossing = a's in, b's end on a's left, a's out, b's end on a's right, with a's left arc = clockwise from a's entry to exit along the first path through a) passed validation on both hubs.
- T2: V=42 E=72 F=32 (V-E+F=2), one outer face (the 12 box edges). LL2: V=54 E=90 F=38 (V-E+F=2), one outer face.
- Every face is a simple walk; the outer face is exactly the ring walked clockwise.
- Faces are real regions: e.g. T2 face 3 = CB1, CD1, C1 stub plus ring edge; the 20 interior faces in the middle are the quads/triangles between crossing lines.

## 2. Lazy separations (verified by the run, final chosen variant)
Collected from the lazy rounds that produced the chosen layout (T2 o0t2f1s1, area 906.25; LL2 o49t0f1s0, area 1654.25 as printed by the solver). Only clash separations are logged, not gap/region rows from the composer. A separation is per pair of segments, so several rows can be one line pair.

| hub | faces | V/E/F | seps | same region agree | contradict | not adjacent | self/copy |
|---|---|---|---|---|---|---|---|
| T2 | 32 | 42/72/32 | 4 | 0 | 0 | 4 | 0 |
| LL2 | 38 | 54/90/38 | 7 | 5 | 0 | 2 | 0 |

## 3. Detail
- T2 not adjacent (all 4): C1 (2 segments) and C2 (2 segments) against F2 segment 4, solver kept the C side to F2's right (f=(1,-1)). C and F2 share no face: C is in the top-right corner, F2 runs from the bottom through the middle. This is a geometric clash of lines that are far apart in the topology, not neighbours; the lazy loop fixed a side between them that the box shape (not a region) forced.
- LL2 agree (5): A1|A2a face 25, C1|C2a face 30 (two rows), D1b|D2 face 15, F1b|F2 face 21. In all five the face implies p on q's left and the solver kept p on q's left.
- LL2 not adjacent (2): D1a|E1 (segments 2 and 5, solver kept D1a on E1's right). D1a and E1 share no face.
- Contradictions: none. Ambiguous (lines sharing faces on both sides of q): none. Self/copy: none (copies D1a/D1b etc. are separate lines in LL2).

## 4. What it says
- Verified: where lazy-separated lines are neighbours, the face side agrees with the solver in 5 of 5 cases; no contradictions on either hub.
- Verified: the map is consistent (Euler, one outer face) on both hubs, so the topology does carry the regions.
- Not verified, inferred: the 6 non-adjacent separations (all of T2's) are not covered by "neighbour regions" alone. The side between non-neighbours is probably implied transitively by a chain of faces (e.g. through the face graph, C-F2 is a path of crossing faces), or it is a real extra choice. Not tested here.
- Caveats: the match from strand segment to a line stretch is by line only (any common face, any stretch); segment heading of q is read as q's travel direction (no sign contradictions, so consistent); only one successful lazy result per hub, not a sweep; the sample is small (11 separations total).
- Side note: the LL2 area printed (1654.25) is above the test limit of 807 given in the area-limit test; the spike used the same mapped call and the hidden test did not check it.
