# Probe 2e: binding conflict of LL2 o49t0f1s0 base with the merge-step rule

Method: the 2e WIP plus ll2-fan-iis.patch (patch -p1; git apply refused) plus env knobs (INNER, OPTEXIT, IIS, ZDUMP, DUMPS; all dropped again). Core = LP-IIS of the failing base solve, deletion-filtered (relax, minimal, "core alone LP-infeasible = true"). x grows east, y south, half tiles.

## Finding: the rule's inner-merge step 0 is the defect, not a required-vs-optional difference
1. VERIFIED: with the rule as written (inner merges 0; EO1/WO1/SO1 +1; EO2/WO2/SO2 -1 via PlanarMergeExitStep, which the brief's test demands for EO2 side -1) the base is infeasible with a 16-row core:
   - L824 (A2a#0, A2b#0 are two lanes of one bundle run, >= 2 apart): A2a#0.end.x - A2b#0.end.x >= 2;
   - A2b#2 is a route diagonal of lane A2b, required, L >= 2 (L681), moving it 2+ toward A2a's side;
   - both lanes then go straight (zero-x pieces #1,#3,#4, AFj1#0-#2, AFj2#0-#2) into the two inner merges AFj1/AFj2;
   - L1014 (Join's `Eq(... , 2*sigma)` on the two out lines of one n-lane join): AFj1#2.end.x - AFj2#2.end.x = 2.
   So the outs would be A2a0-A2b0+L(A2b#2) >= 4 apart but must be exactly 2. The two pieces that can absorb that are AFj1's exit step (and the A2a/A2b optional pieces #0, which the core already bounds). With AFj1 step 0 there is no slack.
2. VERIFIED: the launch head's own solve of o49t0f1s0 (OPTEXIT=1: every merge's step optional Turn(h,+1), same rows as launch) picks, in the base solve and in the last round: AFj1#1=4, CBj1#1=2, EDj1#1=2 (on), AFj2/CBj2/EDj2 = 0 (off); EO1/WO1/SO1#1 = 10 (on), EO2/WO2/SO2 = 0 (off). So claim (a) holds for the 1-lane merges only; the x2 inner merges have NO step at launch.
3. VERIFIED: forcing the j2 steps on (the brief-agent's (b): inner merges all +1) is infeasible with another core (CBj2 line: L1310 bundle offset B1/B2 = 2 y; CBj2#6 is the joiner diagonal of the next node, required L >= 2 and exactly consumes that gap; CBj2#1 (exit step, Turn(h,+1), L >= 2, same direction) cannot also fit. Identical core when OPTEXIT=2 forces the optional switches on (z lower bound 1), so required == optional-on). So (b) was infeasible because it forced j2 on, not because required differs from optional.
4. VERIFIED: step +1 on the j1 inner merges only (EDj1, CBj1, AFj1), 0 on j2, with the rule's EO/WO/SO steps: base is FEASIBLE (then lazy rounds hit the budget at round 13, 8 clashes; not examined).

## Required vs optional-on rows (VERIFIED by reading, planar_model.h AddSeg ~352-376, Join ~980)
Optional piece: L in {0} or [lo, OPTMAX=16] via z (rows L >= z*lo, L <= z*16); required: L >= lo, no upper bound. Same direction, same cost, same parity/curve/separation rows (those are emitted per Seg, indexed by position in the strand's segs, which is unchanged: the piece is #1 in both forms when present). Required is strictly looser than optional-on, so no row differs adversely. The only index-sensitive code, Join's ordering row (`st[k]`, `st[k+1]`), is gated on exit_step != 0 and uses the same st indices.
Not the cause: piece order, a.z/b.z handling.

## Consequence for 2e
The gap rule of brief §3 (step only where a ring neighbour on the same box side lies on the joiner's side) must also decide the inner one-lane merges. Evidence: j1 inner merges need the step (their out lines are the bundle's lane-offset absorber), j2 must not have it. Candidate: INFERRED, an inner merge's out line is neighbour-adjacent via the n-lane join's Eq pairing (the other out lane of the same join), i.e. the rule's "neighbour on the same side" should look at the sibling out lane of the paired merge, not only the ring. Not tested.
Also INFERRED: EO2/WO2/SO2 get a LEFT step (-1) under the rule; the launch solves keep them at 0 (they are off); in the probe, with those left steps the base is feasible.
