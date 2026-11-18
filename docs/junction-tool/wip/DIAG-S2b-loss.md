# Diagnosis: why S2b loses T2's 862.25 and T3's 1933.25

Cause: the angle cost (SHAPE_CROSSING_ANGLE_COST = 1), not the side rule. The side rule alone loses nothing.

1. Program shape T2 o1t3f1s0 / o1t2f1s0, before -> after S2b (headings identical except as listed; d = (Hu-Ho) mod 8):
   - Only crossing X1 (F1 under B1) changes: Hu 3 -> 4, d 5 -> 6, i.e. a 45 degree crossing (|a|=3) -> right angle (|a|=2). Same side (-1 = d outside 1..3) before and after: an ANGLE change, not a side change.
   - X0 (D1 under B1, d=3, CrossingFrom +1) and X2 (D1 under F1, d=5, CrossingFrom -1) are unchanged and already agree with CrossingFrom; so is X1's old d=5 (side -1). The 862.25 shape never broke a side.
   - Shape change is one heading in F1/crossing piece; program-only area 862.25 (94x29) / 866.25 (94x32) -> 954.25 (93x32) / 957.25 (93x32+3). Default shape is 918.25 / 921.25 and wins after S2b (default mode), so default-mode 918.25/921.25 is just the default shape.
2. SHAPE_CROSSING_ANGLE_COST = 0 (side rule only), program mode: T2 o1t3f1s0 862.25, o1t2f1s0 866.25, T3 o1t3f1s0 1933.25: identical to before S2b. The whole T3 program table with cost 0 is identical to before S2b (0 changed). So the side rule is harmless on these variants; the 45 -> 90 degree preference costs +92 / +91 (T2) and +88 (T3) in program-mode area; the right-angle X1 gives a wider layout (cause not examined).
3. No crossing is broken by CrossingFrom in the 862.25 shape: X0/X1/X2 all have shape side == CrossingFrom (+1,-1,-1; all line crossings of each pair agree). So there is no CrossingFrom-wrong / side-not-fixed case to check on the layout; not examined further.
4. T3 newly failing (program mode, pv table): o0t2f0s1 "infeasible with topology sides;" with cost 1 only. With cost 0 and before S2b it solves (2702.25, 122x56). It is infeasible with the default shape too (the baseline default-mode table lists it failing), so in default mode nothing newly fails; it only appears so in program mode. The right-angle preference moves that variant's program shape to one the lazy loop cannot satisfy topology sides on (not a base infeasibility, so no IIS). Not diagnosed to the crossing level.
Also: T3 program-mode table under cost 1 changes 20 variants (mostly o1*f1* worse by 87-90 tiles, o0*f0* much better, e.g. o0t0f0s0 2990 -> 2698); best program area 1933.25 -> 2016.25.
Implication: S2b's side rule can stay; the angle cost as an objective term (weight 1) is what loses; dropping the angle term (cost 0) restores the program-mode results; it is Pete's call whether right angles are worth the area.
Method: temporary DIAG dump and cost edit, reverted; tree `git diff` byte-identical to S2b-wip.patch.
