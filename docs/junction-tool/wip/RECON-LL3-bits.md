# Recon: does LL3 (LL3a) have a variant space that widens LL2's?

Measured at 20f98229e0 (wt-planar, build-rel), by a throwaway debug test (patch: scratchpad/dbg-bits.patch; branch deleted). Topology text: wip/topologies/LL3a.txt (lane-paired, one-lane channels; the text SPIKE-LL3 used). LL2 text: PlanarLL2Text() (planar_plan.cpp:438).

## 1. LL2 bits (6 open + 3 ties + 1 triangle + 3 steps = 13, 8192 variants)
Open crossings (bit i = i-th; bit set = the higher-index channel is over):
- 0: F{F1,F2} x B{B1,B2}   (all three bundles are "prio" channels: F2, B2, D2 are prio lines, so every bundle pair is open)
- 1: F x D{D1,D2}
- 2: B x D
- 3: E2b x D1a   (lane-split sub-channels)
- 4: C2b x B1a
- 5: A2b x F1a
Ties: 3 (the CD fan site; AFj nodes A2a/F1a join and A2b/F1b join). Triangle: 1 (crossings F x B, B x D, F x D). Steps: {B1,B2} toward -1; E2b toward -1; B1a toward -1.

## 2. LL3a bits (11 open + 0 ties + 1 triangle + 5 steps = 17, 131072 variants)
36 line crossings, all between one-lane channels (24 channels). Open (both or neither channel prio; prio set E1-3,B1-3,A1,F2,A3):
bit0 F1xA2, bit1 F1xD1, bit2 F1xD2, bit3 F1xD3, bit4 F2xB1, bit5 F2xB2, bit6 F2xA3, bit7 F2xB3, bit8 F3xD1, bit9 F3xD2, bit10 F3xD3.
The other 25 crossings are fixed (prio over non-prio): all 9 BxD, all 3 F2xD, F1xB1..3, F3xB1..3, F1xA3, E2xD1, E3xD1, E3xD2, C2xB1, C3xB1, C3xB2.
Ties 0 (every merge node has a straight leg). Triangle: 1, F3,B3,D3 (crossings F3xB3, B3xD3, F3xD3). Steps: E2, E3, B1, B2, B3 (all toward -1).

## 3. Mapping under widening
No, not bit for bit. LL3a is not LL2 with each channel tripled: it has one-lane channels (each lane fray splits on its own, lane-paired D325), and a different prio set (E1-3, B1-3, A1, F2, A3 vs LL2's E1,F2,A1,B2,C1,D2), so which crossings are open differs.
- LL2 bit0 F x B -> LL3a bits 4,5,7 (F2xB1..3). Only F2 is a prio lane, so F1xB*, F3xB* are fixed (B over): 3 open of 9. LL2 bit0 clear (F over B) has no uniform image.
- LL2 bit1 F x D -> bits 1,2,3,8,9,10 (F1xD*, F3xD*): 6 open of 9; F2xD* fixed. One LL2 bit, six LL3 bits.
- LL2 bit2 B x D -> none: all 9 BxD fixed (B prio, D non-prio). LL2 variants with bit2 set have no image.
- LL2 bit3 E2b x D1a -> none: E2xD1, E3xD1, E3xD2 fixed (E prio).
- LL2 bit4 C2b x B1a -> none: C2xB1, C3xB1, C3xB2 fixed (B prio). The fixed direction is B over = LL2 bit4 SET; clear has no image.
- LL2 bit5 A2b x F1a -> bit0 F1xA2, orientation inverted (LL3 bit0 = NOT LL2 bit5, because F1 is the lower channel here, A2b was the lower there).
- LL3-only bit: bit6 F2xA3 (no LL2 counterpart).
- Ties: LL2 has 3, LL3a 0 (no counterpart; LL2's t bits cannot be lifted).
- Triangle: 1:1 (F,B,D triangle of LL2 <-> F3,B3,D3 only; the lane-1/2 triples are not triangles).
- Steps: LL2 {B1,B2} -> LL3a B1,B2,B3 (1:3, would have to agree); LL2 E2b -> E2,E3 (1:2); LL2 B1a -> none.
So several LL3 bits per LL2 bit (bit0 x3, bit1 x6, steps x3, x2), bits with no image either way (LL3 bit6; LL2 bits 2,3,4 and ties, B1a step), and bits that are forced in LL3 but free in LL2.

## 4. Face-map side check on widened images (planar_paths.cpp:184, CheckPlanarShape)
Whole sweep of LL3a over x triangle flips (4096 over/flip combos, shape check only): 2048 pass, 2048 fail. The failures are exactly "B3 over F2 required": bit7 is set in every passing combo, free otherwise. (Reported culprit is B1, B2 or B3 depending on check order; B3 is the real one.) Same sweep on LL2: 112 of 128 pass; the 16 failures are path-building failures ("no piece of B1/F1 suits its crossing"), no side failures.
Widened images (t dropped; LL2 s1 -> s28, s2 -> s3, s3 -> s31; f1 -> f1):
- LL2 o0 (and all 12 baseline tags o0t0-2f0s0-3, which share o0) -> o1 (and o65 for bit6=1); with s0/28/3/31: side check FAILS ("B1 crosses F2 from the wrong side"), all 7 tried.
- LL2 o17t0f1s0 -> o177t0f1s0 (o241 for bit6=1); o49t0f1s0 -> o176t0f1s0 (o240 untested): side check PASSES. Also o176/o177 f0, o177t0f1s28, o176t0f1s28 pass the side check.
- Dry reason of the passing ones: "a turning channel over a crossing with doubled lanes is composed for two full-share lanes only" (known composer limit, planar_components.cpp:1257). Nothing else fails.
SPIKE-LL3's o0 is rejected because bit7 (F2xB3) must be set (B3 over F2) and o0 has every over bit clear (F2 over B3).
Consequence: LL2's best (bit0 set) lifts through the side check; the 12 baseline variants (bit0 clear) do not.

## 5. Conclusion
LL3a's variant space is not a widening of LL2's: 11 open vs 6, 0 ties vs 3, 5 steps vs 3, only the triangle maps 1:1. Open-ness changes with granularity (one-lane channels, per-lane prio), 4 of LL2's 6 open bits have no free LL3 bit (3 of them forced by prio, 1 split across lanes), and the face map forces F2xB3 in LL3a while LL2 leaves F x B free.
Lifting LL2's winner works as a seed only through an explicit lane-level image: with LL2's bits fixed the images fix bits 0-5,7-10 and steps, leaving LL3 bit6 (F2xA3) free = 2 variants (if lane-paired bits must agree, as above); if lane bits need not agree, the mapped bits are still determined by the winner's direction, so the same one free bit remains plus whatever the widening rule leaves open.
A skeleton approach needs (a) a rule per LL2 bit giving its LL3 lane crossings and direction, (b) a decision for LL2 variants whose image is forced in LL3 (bits 2,3,4: LL3 forces the LL2 default for bits 2,3, and bit4 SET), and (c) bit7 treated as forced.
