# PROBE-CM: face slack as a linear compactness term (throwaway), commit c56fbf0179 on probe-cm (from probe-pr 623cf0d8e7)

Changes: `obj_fixed` in planar_model.h (added to the cost in Solve, never cleared); `RungExpr`/`RungCon` split in planar_components.cpp; with PLANAR_FACEW=w every ladder rung adds w*coef per term of RungExpr to obj_fixed; under PLANAR_TIMES a `slack S rungs N` line after the final solve (`slack - rungs 0` without ladder). Type used: `Ex` (the file's expression type).

## 1-2. T2 o1t3f1s0 single variant (slack in tiles)
| w | area | size | track | slack | rungs | ms |
|---|------|------|-------|-------|-------|----|
| none | 932.25 | 75x31 | 735.505 | 536.5 | 198 | 3460 |
| 1 | 932.25 | 75x31 | 751.505 | 498.5 | 196 | 5013 |
| 3 | 932.25 | 75x31 | 751.505 | 498.5 | 196 | 4628 |
| 10 | 956.25 | 69x34 | 697.848 | 480.5 | 196 | 4496 |
| 30 | 960.25 | 69x34 | 698.676 | 463 | 197 | 4155 |
No failures. Slack falls only ~14% at w=30 while area rises 28 tiles.

## 3. pv run T2 (32 variants, 28 built, 4 failed in all three: o0t3f0s*, o1t3f0s*)
Best o1t3f1s0: none 932.25 (75x31), w=3 932.25 (75x31), w=10 956.25 (69x34). w=3 equals none for every variant. w=10 differs from none on:
o0t0f1s0 1079.25->1083.25, o0t0f1s1 1080.25->1098.25, o0t2f1s0 1026.25->1030.25, o0t2f1s1 1027.25->1045.25, o1t0f1s0 1036.25->1056.25, o1t1f0s0 1234.25->1244.25, o1t1f0s1 1241.25->1251.25, o1t2f1s0 983.25->1003.25, o1t3f1s0 932.25->956.25. Other variants identical. Full table (none, w3, w10): scratchpad cm-pv-table.txt.

## 4. End to end, PLANAR_FACEW=10
- T2: plan succeeded; MAPPED T2 variant o1t3f1s0 area 956.25 size 68x33; 14 VIOLATIONs, all rule 9: prio block needs exactly one join 4, prio track on the ML is not a streamlined join 4, prio track touches a used track 4, farthest prio signal must be Exit 1, exactly one prio signal between consecutive segments 1.
- T3: SIGSEGV at planar_plan.cpp:238 (PlanPlanarJunction), no MAPPED line. The same segfault occurs with no PLANAR_FACEW (baseline), so it is not caused by the probe (possibly the known CPU instability or an existing bug; not investigated).
