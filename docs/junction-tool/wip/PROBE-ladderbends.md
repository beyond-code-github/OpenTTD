# Probe: free bends in the ladder solve (PLANAR_LADDERBEND=1 bundle+join slots, =2 bundle slots), clash loop only in the rough
Worktree .claude/ng1 at 72c03f4. Patch: wip/probe-ladderbends.patch (spike-freebend + probe-noclash + base_i rung map + slot parity rows lapsing with the slot binary: x-2k in [p,p+1] and x-2k+b <= p+1, slot pieces only). Logs: scratchpad ladderbends/r2-{T2,T3,LL2}-{1,2}/, today = full-*-0/. Ladder passes detect-only, up to 6, last legal ladder solution stands else the rough; rough keeps its clash loop. A ladder pass at the 5 s limit counts as infeasible (not a proof).
Rung map by (strand, base_i): mode 1 drops 207 (T2), 348 (T3), 2642 (LL2) rungs (exit-step ones), mode 2 drops 0.
Today = the laddered area of the unmodified binary; "worse" includes variants whose ladder failed and the rough stands (rough is slightly above today's laddered).
## Per hub (variants solved today: T2 28, T3 27, LL2 189)
| hub/mode | ladder final legal (converged) | ladder infeasible | area better / same / worse vs today | best area (today) | ms median/max (today) |
| T2 m1 | 0 | 28 (pass 0: 4, pass 1 then failed: 24) | 0 / 24 / 4 (+4) | 906.25 (906.25) | 6709 / 9128 (106 / 852) |
| T2 m2 | 26 (22) | 4 | 16 (best -75) / 4 / 8 (worst +81) | 905.25 o1t2f1s0 (906.25) | 29886 / 44933 |
| T3 m1 | 0 | 27 | 0 / 27 / 0 | 1941.25 (1941.25) | 8563 / 10146 (181 / 592) |
| T3 m2 | 23 (18) | 11 | 11 (best -112) / 5 / 11 (worst +131) | 1954.25 o0t1f1s0 (1941.25) | 22807 / 48546 |
| LL2 m1 | 0 | 189 (all, ~5.4 s = limit) | 0 / 173 / 16 (+183) | 1654.25 (1654.25) | 7980 / 12431 (4198 / 8760) |
| LL2 m2 | 0 | 189 (all, ~5.4 s = limit) | 0 / 173 / 16 (+183) | 1654.25 (1654.25) | 7858 / 11571 |
Slots taken: only mode 2 on T2/T3: 23 T2 variants use slots (31 bends in all), 20 T3 variants (22). The best T2 variant (905.25) took 1 bend, 6 passes (not a fixed point at 6 passes in 21 of 26 T2 variants). Best T3 variant of the mode (1954.25) took 2.
Passes histogram, mode 2 (ladder solves, 0..6): T2 {0:1, 1:2, 3:1, 4:2, 5:1, 6:21}; T3 {0:3, 1:1, 2:7, 3:5, 4:2, 5:3, 6:6}. Mode 1: T2 {0:4, 1:24}, T3 {0:11, 1:16}, LL2 {0:189}.
## Reading
- Parity lapse fixed the zero-slot infeasibility seen earlier. Mode 2 (bundle slots) ends legal on 26/28 (T2), 23/27 (T3) variants, takes bends on 23 and 20, and beats today's area on 16 and 11 variants (up to -75, -112) but is worse on 8 and 11 (up to +81, +131); the best per hub is 905.25 (-1 tile on T2) and 1954.25 on T3 (today 1941.25, no gain).
- Mode 1 (join slots as well): every ladder pass infeasible on all three hubs (T2/T3 quickly after pass 0 or 1, LL2 at the 5 s limit); join slots stay broken, as in the spike.
- LL2: every ladder solve hits the 5 s limit in both modes; nothing to compare.
- Cost: T2 m2 median 30 s (today 0.1 s), T3 23 s: two orders of magnitude over today; most of it ladder solves at 2-9 s each, up to 6 passes.
- "Passes 6" in 21/26 T2 legal variants means the re-read loop did not settle; rungs move every pass.
