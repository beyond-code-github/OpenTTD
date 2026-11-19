# PROBE-UD (throwaway), commit af1c9ac0c0ec91b045ad10f80160c6a2f4790902 on probe-ud

Change: planar_paths.cpp. With PLANAR_UNDERDIAG set, an under crossing skips the right-angle block and first takes the first piece i >= at with odd (diagonal) heading that suits; else the old loop.

1. No env, o1t3f1s0: 932.25 75x31 track 735.505 (program shape wins over default 956.25 69x34, with PLANAR_SHAPE=default).
2. PLANAR_UNDERDIAG=1: 932.25 75x31 track 735.505 (1851 ms vs 3473 ms; one solve per variant, the program shape now equals the default shape). Last slack 536.5 rungs 198. UNDERDIAG+SHAPE=default: 932.25 75x31, identical. Default shape alone: 956.25 69x34 track 691.848, slack 902.5. So the knob lifts the default shape from 956.25 to 932.25, equal to what the program shape already gave; no composer refusal; no area gain over today's best.
3. DRY with knob on T2: 32/32 variants built; dry line shows no headings.
4. pv (base -> UD). T2: 28 built/4 failed both ("base infeasible"); best o1t3f1s0 932.25 both; per variant areas mostly differ (some smaller, e.g. o0t0f0s0 1558->1424, o0t3f1s0 976->948, others larger, e.g. o0t0f1s1 1080->1116); 7 equal.
   T3: base 24 built/8 failed (4 base infeasible, 4 a clash is not separable), best o1t3f1s0 1796.25 81x46; UD 20 built/12 failed (4 base infeasible, 8 clash not separable), best o1t3f1s0 1976.25 95x45. T3 worse everywhere it built except o0t2f0s0 (2787->2741); 4 variants (o0t1f1s0/s1, o0t3f1s0/s1) become failures.
   Files: scratchpad ud-pv-T2[-base].txt, ud-pv-T3[-base].txt.
5. End to end: plan succeeds both hubs, no segfault on T3 with or without the knob. T2 knob: MAPPED 932.25 74x30, 10 violations, same as base (3 touches a used track, 3 not streamlined join, 3 needs exactly one join, 1 prio signal count). T3 knob: MAPPED 1976.25 94x44, 32 violations (11 touches, 11 not streamlined, 7 exactly one join, 2 prio signal count, 1 farthest prio signal Exit); base: 1796.25 80x45, 27 violations (11,11,3,2).
