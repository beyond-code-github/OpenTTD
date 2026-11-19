# PROBE-FL (commit ce089585477f6081a036f734561327a78feef6f9 on probe-fl, from probe-pd)

Changes (PLANAR_LATE letters, planar_components.cpp): r = prio fray in-lane rise lower bound 0; c = core of a prio fray's straight leg (site_of(c,true) with straight==c and prio; no extra data needed) gets per lane AddSeg(Turn(h,-1),0) then AddSeg(h,0) after the run, diagonal lengths equal (tag "late climb"); g = fray gap rows Ge. No model refusal of the zero-length diagonal.

1. T3 o1t3f1s0 pinned (area size track): none 1796.25 81x46 1283.15 (9.9 s); r identical; g identical; c 1813.25 81x46 1282.64 (7.6 s); rc, rcg identical to c. Last slack line none/r/g: face 47 slack 21.5; c/rc/rcg: face 38 slack 27. (The T3 test itself reports 1 failed assertion on all runs including no env.)
   Best by track: c (rc/rcg tie): track -0.5 tiles, area +17.
3. End to end with c, pinned and unpinned: both MAPPED T3 variant o1t3f1s0 area 1813.25 80x45. Violations (same both): 7 "prio block needs exactly one join", 11 "prio track on the ML is not a streamlined join", 11 "prio track touches a used track", 2 "exactly one prio signal between consecutive segments".
4. pv (32 variants, budget 15 s):
   T3 c: 28 built, 4 fail (base infeasible); best o1t3f1s0 1813.25 81x46; max 10.5 s. Base: 24 built, 4 clash-not-separable + 4 base infeasible; best o1t3f1s0 1796.25; max 15.2 s.
   T2 c: 28 built, 4 fail (base infeasible); best o1t3f1s0 884.25 71x31; max 3.3 s. Base: 28 built; best o1t3f1s0 932.25 75x31; max 5.4 s.
   Per variant areas: files fl-pv-{T3,T2}[-base].txt in scratchpad. Summary: T3 c wins 2 variants base failed, wins o0t1f1/o0t3f1 by about 90-100, loses o1t1f1/o1t3f1/o0 f0 by 20-40. T2 c better on most f1 variants (o1t1f1s0 897 vs 968, best 884 vs 932).
5. Route tiles (copy 0, EF+E/F lines, clipped to x >= E1 exit x - 36 as the window; alignment approximate): E1 37 (base 46), E2 41 (46), E3 40 (44), F1 53 (53), F2 45 (49), F3 38 (45). Unclipped lines (distinct copy-0 pieces): EF1 43 EF2 38 EF3 33, E1 35 E2 43 E3 46, F1 51 F2 49 F3 47.
