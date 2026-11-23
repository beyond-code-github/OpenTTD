# F9 diagnosis: keep-apart too strict next to a portal-only tunnel end (X2, domain-50)

Head 2d46f41f88 on wt-e1. Diagnosis only; the probe (a test case printing the A1/B1 pairs, the domain raster and an end-run-2 counterfactual, plus a one-line env-var widening of the `sanctioned` lambda) is removed; nothing but this file is committed.

## Read
planar_draw.cpp pair loop (1451-1650), `span_piece` (1310), `sanctioned` (D427 probe, ~1516), the X2 fixture, `ContractedVariant`, `DrawingProgramJudge`; D376, D381, D414, D437, D438; `git show ee19dcd355`.

## 1. The failing pairs
All 23 too-strict polylines (33 geometries) share the prefix `B1: N2, NE3, E2, ...` with A1 = `E16` (one contracted piece). They are two groups only by the tail and by which of the box/port rows also break (the port and box rows break because the polyline moves B1's far end; those are the off-interface class and are not the cause). The same four pairs fail in every one of them, with A1 = piece 0, B1 pieces as contracted:

| pair (face i j) | A1 wall | B1 wall | kept by | candidate slacks (worst, half tiles) |
|---|---|---|---|---|
| sep 3 2 4 (and mirror sep 5 2 4) | piece 0, E, whole separator | piece 2, E2 | strict span | -2, -2, -8, -14 |
| sep 3 2 5 (and mirror sep 5 1 4) | piece 0, E | piece 3, NE (5 or 1 long) | strict span | -1, -6 (or -2), -14, -6 |

Group 1 (`B1.11=4`): `B1: 0*2,1*3,2*2,1*5,0*4`, A1 (4,11)->(20,11); B1 points (11,16) (11,14) (14,11) (16,11) (21,6) (21,2).
Group 2 (`B1.11=0`): `B1: 0*2,1*3,2*2,1*1,0*4`, A1 (4,5)->(20,5); B1 points (11,10) (11,8) (14,5) (16,5) (17,4) (17,0).

B1's crossing piece in the contracted variant is piece 1 (NE3): `ContractedVariant` takes the first same-heading piece that meets A1's piece in one point, and NE3 ends exactly on A1's line at (14,y) (D414: crossing at a piece end). The pairs that fail are B1's two pieces after the crossing piece. Every other A1/B1 pair is `adjacent`, holds, or is the crossing piece's own (held by the crossing).

## 2. The tiles (group 1, raster of the full points; tile columns x=2..10; A1 is row y5)

```
        x=2  x=3  x=4  x=5  x=6   x=7   x=8   x=9  x=10
y1      .    .    .    .    .     .     .     .    b11
y2      .    .    .    .    .     .     .     .    b10
y3      .    .    .    .    .     .     .     b8   b9
y4      .    .    .    .    .     .     b6    b7   .
y5      g    g    g    Pt   m+b3  m+b4  m+b5  Pt   .
y6      .    .    .    b1   b2    .     .     .    .
y7      .    .    .    b0   .     .     .     .    .
```
g = A1 ground track, Pt = portal (tunnel end piece, first/last), m = tunnel middle, bN = B1 raster piece N.
(A1 raster pieces 0..7 are tiles x=2..9 on row 5; domain structure first 3, last 7, ends 1 -> portals are tiles x=5 and x=9, middles x=6,7,8. B1 raster: b0 (5,7) N, b1 (5,6) NE, b2 (6,6) NE, b3 (6,5) NE, b4 (7,5) E, b5 (8,5) NE, b6 (8,4) NE, b7 (9,4), b8 (9,3), b9 (10,3), ...)

- B1 piece 1 (NE3) ends over tile (6,5), the first middle tile (this is the crossing piece).
- B1 piece 2 (E2) is raster b4: it runs ALONG A1's line over middle tile (7,5): the crosser's heading equals the separator's (the D438 shape, tunnel middle only).
- B1 piece 3 (NE) starts over middle tile (8,5) (b5) and leaves A1's row at (17,10), tile (8,4); it never touches the portal tile (9,5).
- The gap between the walls is 0 (E2 lies on A1's line) and 0 at the crossing point; the keep-apart candidates want 2 and 1 half tiles (slack -2, -1).

So the whole of B1's occupancy of A1's row (tiles x=6..8) is over the middles, and the portal tile x=9 and the portal at x=5 are clear.

## 3. Why the domain accepts, and what changed under end run 2
Domain: a deck/tunnel middle tile is free ground above (the missing-crossing and head rules only forbid a non-crosser, or a head/end piece, on the tile); a listed crosser may occupy any middle tile with any shape; the end pieces (here 1 portal at each end) must be exclusive. B1 occupies exactly the three middle tiles x=6..8; portals x=5 and x=9 are free. `CheckPlanarDomain`: legal, no break.

The rows: the keep-apart read-out does not read the end run at all (`end_run` appears in planar_draw.cpp only at 1206, the `end <s> node` rows; the pair loop never reads it), so it is identical before and after D437. The strict-span skip (`sanctioned`) exempts the separator's span piece only against the crossing piece of its crosser (the event piece of the structure's crossing on both walls); B1's other pieces over the same middles get rows. That is too strict whenever the crosser bends at the crossing (D414) and the bend pieces lie over the middles. No skip ever covered these pieces.

Before D437 these polylines were `both reject`, not accepted: with end run 2 the tunnel needs two pieces (portal and ramp) outside the last middle tile; B1 reaches tile x=8 and A1 ends at x=9, so `PlanarDomainInputOf` reports `no room for its end runs`, the structure is dropped, and the domain then refuses (connection breaks at (14,y) and (16,y), tiles (7,y) and (8,y)). Counterfactual run in the probe at head: end_run 2 forced on the two examples gives exactly that (`domain legal 0`, notes `structure 0 (A1): no room for its end runs`). D437 moved the domain's boundary one tile; the rows' too-strict keep-apart, hidden before, became visible. The brief's guess (a span skip reaching the neighbouring piece under end run 2) is not what happened; the keep-apart is end-run independent.

## 4. The general rule and its reach
Rule (one sentence): a crosser's pieces are exempt from keep-apart against the span piece of the structure they cross wherever the crosser lies over the structure's middles, that is wherever the crosser (and the pieces between it and the crossing) stays at least the end run (one piece, 2 half tiles along the separator's axis) short of the separator span piece's ends, because ground above a tunnel's or under a bridge's middles is free and the portal/head tile is exclusive.

Measured pieces of the rule:
- Part A only (exempt EVERY piece of the crossing track against the span piece, no clearance part; probe, env var in `sanctioned`): X2 too strict 23 -> 0, too loose 0; the 23 polylines become off interface (their port/box rows still break). All other nine `[planardomainexact]` fixtures stay exact (0 strict, 0 loose, same counts). But X2 gains 23 polylines (33 geometries) in `loose off interface` (domain refuses, full rows refuse by port/box only, the reduced rows accept): these are the same family with E4 instead of E2 (`B1: 0*2,1*3,2*4,1*1,0*4` and tails 1*3, 1*5, 0*2 1*2 ...), where B1's NE piece after the E run sits on the portal tile x=9 (domain: no room for the portal, `connection` break). So Part A alone is too loose; the clearance (Part B) is needed: in half-tile coordinates the crosser points within one half tile of the separator's line must keep x between a0 + 2 and a1 - 2 (group 1: a0=4, a1=20, B1 maxes at x=17 <= 18; E4: (19,y-1) > 18). Part B was checked on the examples only (E2 holds, E4 breaks), not as rows.
- Reach, from the base read-outs (`ReadOut(true)`, pairs newly skipped by Part A, i.e. a span wall against a non-crossing piece of its crosser): X1 16, X2 36, L1 26, Y1 16, BR1 16, TT1 32, TS2 68, M1 0, K1 0, P1 0 (no structure). They are all pairs where the crosser track has pieces besides its crossing piece over the span (flex pieces, S-bend inserts, bend pieces). Their polylines stay exact under Part A (the enumerations rarely put a crosser piece along the separator; X2 is the only fixture whose crosser bends at the crossing, so the leak above shows only there). The risk: any fixture with a crosser bending near a portal needs Part B or it leaks the same way; this should be checked when a second such fixture exists (TS2's crossers are straight at the crossing).
- Relation to D438 (crosser along a tunnel): piece 2 (E2) is exactly that shape on a middle tile (heading equal to the separator's). Part A covers D438's keep-apart part for the pieces over the middles; D438 still needs Shape's crossing angle 0 and the span width. Bridges are included by the sentence (a crosser under a deck middle is free too); the domain's `under` rule is the same for both, but D438's tunnels-only ruling concerns the along-run, so the rule's along-run case should stay tunnel-only (a bridge deck's crosser along it is the ground track beneath the deck and the domain accepts it only if it is a middle).

## Stops
None. No rule change made; the probe and its env var are removed (`git diff` clean before this file).
