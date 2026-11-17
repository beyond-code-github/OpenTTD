# Analysis: what the one-lane merge exit-step rule (05e0159b09) is missing

Method: the old build (every one-lane merge exit an optional `Turn(h,+1)`) was run on the 201 LL2 tags with a 60 s limit under 8-way load; 56 variants finished and give 672 (variant, merge) old choices. The rule inputs (joiner side with ties resolved, step) were printed from HEAD per merge. Candidate rules were then tested by forcing steps in HEAD (a temporary `PLANAR_FORCE` knob) and solving all 189 variants. Scratch: `.../scratchpad/merge-ana/` (rows.json, dry.txt, force.*.txt, res.*.raw).

## 1. Where the rule disagrees with the old solver (554 of 672 agree)
| out line | old step | rule step | count |
|---|---|---|---|
| EO2 (joiner from left) | 1 | 0 | 55 of 56 (0 only in o49t0f1s0) |
| WO2 (joiner from left) | 1 | 0 | 49 of 56 (0 in the 7 variants with B1>F1, F1>D1, B1>D1, i.e. o1 and o49) |
| SO2 (joiner from left) | 1 | 0 | 4 of 56 (SO2 is 0 in the other 52) |
| AFj1 inner, own joiner left, AFj2's joiner right (ties t2/t3) | 0 | 1 | 10 |
Everything else (EO1 WO1 SO1, EDj1/2, CBj1/2, AFj2, AFj1 otherwise) matches 100%.

## 2. What the data says
- The pinned gap after an exit pair is not closed by the pair's second line alone. EO2 steps +1 toward its ring successor AB2 (an in line on the same box side) in every variant except o49: the pinned gap EO2|AB2 is not covered by any fray fan step (the gap list holds only EF1 and CD1 fans, away from WO1 and SO1), so the out line must close it up itself, whatever side its joiner is on. The rule's "joiner arrives from the right" condition therefore misses EO2 (its joiner is always on the left).
- Cost of that step in HEAD (forced, area): o0t4f0s0 2656 -> 2579 (EO2=1 alone), o1t0f0s0 2054 -> 2022, o3t0f0s0 2819 -> 2769; with WO2=1 too 2567 / 2029 / 2692. On o49t0f1s0 the same step costs +28 (1654 -> 1682): there the old solver kept it off. So EO2 and WO2 are real area-driven choices that flip with the crossing orientation; only o49 and o1 show the "off" side.
- What separates "off" from "on" is the over/under choice, not ties, flips or steps bits: EO2 is off only when crossings 5 and 6 (C2b/B1a, A2b/F1a) are both flipped (o49: B1a>C2b, F1a>A2b); WO2 is off when those are flipped or when B is over F over D with B over D (the cycle-free order, o1). Ties (t bits) only move AFj1/AFj2 sides; they do not change EO2/WO2/SO2 once the over choice is fixed (o3t0..t7: EO2=1 WO2=1 SO2=0 in all 21).
- AFj1 (inner lane merge): old step is 0 exactly when its joiner arrives from the left AND the other merge of the site has its joiner from the right (both joiners outside the pair, nothing between the lanes to close up). Sides (-1,-1), (+1,+1), (+1,-1) all stepped. Hit 56 of 56.
- SO2 stays unexplained: 1 in o1t0, o1t2, o1t6, o5t6; 0 in the rest. Forcing SO2=1 changes area by only -5 to +12 on o0t4/o1t0/o3t0 (area-neutral to slightly bad), so these look like solver ties, not rules.

## 3. Candidate rule and hit rate
Port exit onto an out line whose ring successor is an in line on the same side (EO2, WO2, SO2) gets +1 as well, unless (a) EO2/WO2: the crossing orientation of that variant is "flipped" (o49 type), or (b) WO2: B over F, F over D, B over D. SO2 = 0. Inner merges: +1 when another merge of the site has its out line to the right, except when own joiner is left and the other's joiner is right.
| variant | picks matching old (672) | LL2 areas vs before.txt (189 variants) |
|---|---|---|
| HEAD rule | 554 (82.5%) | median +2.8%, 182 worse |
| EO2=1 always | - | median +0.87%, 99 worse >0.5%, best 1682 (loses the o49 optimum) |
| plus (a),(b) incl. AFj1 refinement, B2 | 668 (99.4%) | median -1.24%, 134 better, 20 worse >0.5% (max +4.3%), best 1654.25 kept |
Misses: 4 SO2 picks only (solver ties above). The 20 variants still worse are all o5/o3/o1 tie variants (e.g. o5t2f0s1 +4.3%) where the old build found other combinations; they may be old-solver noise or SO2=1 cases, not tested.

## 4. Caveats
- Only 56 of 189 variants had old choices (the old build was too slow for the rest under load); the over-orientation test (a),(b) is fitted on o49 plus o1, so it is evidence, not proof. The principled core is the EO2/WO2 close-up on a gap no fray fan covers; the orientation dependence is the part to derive from the model (probably which neighbouring in-line's channel is crossed over the gap) before coding it.
- Tie-driven: on non-o49 variants "EO2=1" alone is already 20-30 area units better than the rule.
