# Spike: free bends everywhere (PLANAR_FREEBEND), at 05e0159b09, s0 variants only (verified = measured)
Patch: wip/spike-freebend.patch. Curve rows lapse with the optional slots (verified: no "adjacent optional" failure; built and solved). Slots built as briefed, plus Eq rows making the lanes' diagonals of one slot equal length (added). Binaries per variant: T2 0 -> 30, LL2 12 -> 78 (verified).
Baselines (s0, 16 T / 52 LL2 variants): T2 14 solve, best 917.25 o0t2f1s0, median 85 ms, max 244. T3 14 solve, best 1945.25 o1t3f1s0, med 184, max 336. LL2 49 solve, best 1654.25 o49t0f1s0, med 2.2 s, max 3.2 s.
## Result: full brief setting (join + bundle slots), FREEBEND=16 and 128: nothing solves
- T2, T3: 0/16 each. With sides: "infeasible with topology sides" (IIS: the side rule on a segment next to a bend, port-order rows and a lazy separation of lanes ED1/ED2). Rerun with the sides rule off (PLANAR_NOSIDES, my addition): 0/16, "a clash is not separable" (all 14 baseline-solving variants), 20-30 s per hub.
- LL2: 0/52, "base infeasible" before any clash round, ~5.5 s each = the 5 s solve limit, not a proof (no IIS core). Same with join slots off. Inferred: the 78-binary base model does not find a first solution in 5 s.
## Split by where the slots are (T2/T3 sides off, FREEBEND=16; verified)
- Join slots only: 0/16 on T2 and T3, "a clash is not separable" (fast, 4-9 s). The exit lanes' fixed 2 spacing plus equal slots leave the lazy separations nothing to move. Cause not diagnosed further.
- Bundle slots only: T2 15/16 solve, T3 8/16; 15-19 s per variant (the 15 s clash-loop budget, i.e. not converged), best T2 950.25 (baseline 917.25), 6 variants better and 7 worse than the no-bend baseline by >0.5%, best T3 1976.25 (baseline 1945.25). With sides on: LL2 0/52 too.
- Big-M: hi 128 vs 16 on bundle-only T2: 16/16 solve, same times (15.2 s median vs 17.0 s), best 954.25 vs 950.25. So the bound is not what slows it; the extra binaries and lazy clashes are (inferred; not tested at hi 4).
## Not produced
- LL2 pv diff against 2e2/before.txt: every FB=16 variant fails, so nothing to compare. Best-variant slot listing: no LL2 variant solves. PLANAR_IIS core for LL2: none printed (time limit, not proof).
## Reading
Optional bends everywhere are two orders of magnitude slower on T2 and, where they solve, no better than the no-bend baseline (the solver stops at the budget on a worse point); join slots break feasibility through the lazy separations. Not a drop-in; a bend would need to be added where a rule says it is needed (as today) or the model changed so the clash loop does not fight the free slots.
