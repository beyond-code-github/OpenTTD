# Probe C: prio crossings open (PLANAR_PRIOOPEN=1), ng1 e4b50580e1

Patch: probe-prio-open.patch (planar_paths.cpp: open set + encoding; planar_components.cpp: program-only solve when the default shape fails). Confirmed: CountMappedBits takes open from PlanarOpenCrossings; dflt.over is filled before any BuildPlanarPaths failure.

Bits: T2 and T3 have 3 channel crossings (B,D bit0; B,F bit1; D,F bit2). Today only D,F is open (1 bit); with the env bit0 and bit1 are prio/non-prio, bit2 equal-prio. LL2 tag and variant count are unchanged (no prio/non-prio crossing opened).

Full mapped solves (base -> env):
- T2: o1t3f1s0 862.25 (94x29), 32 solved, 2.8 s -> o4t3f1s0 862.25 (94x29), 29 solved, 8.1 s. Same hub (o4 = D,F flipped, no prio/non-prio bit set).
- T3: o1t3f1s0 1933.25 (138x39), 32 solved, 4.1 s -> o0t1f1s1 1941.25 (129x45), 24 solved, 7.5 s. Winner differs and is 8 larger (search path, bits 0/1 clear again; the winner is not a prio/non-prio flip).
- LL2: o17t0f1s0 1347.25 (87x59), 48 solved, 92 s -> identical, 48 solved, 93 s.

One prio/non-prio bit set, other fields zero (PLANAR_ONLY, all solve, no failure text):
- T3 o1t0f0s0 (B,D under) 2814.25 (115x63); o2t0f0s0 (B,F under) 2747.25 (122x58).
- T2 o1t0f0s0 1458.25 (79x47); o2t0f0s0 1489.25 (84x46).
(Zero-fields tags, so not comparable to the winners; they show a solve exists, no default/program/composer failure appeared.)

End-to-end (T3, env): mapped-dump plan + signals + CheckJunctionRules: 0 violations, 1941.25, 128x44.

Reading: opening prio crossings gave no area gain on T2, T3 or LL2 at channel level; T3 got worse (local search lands elsewhere with more bits) and T2 costs about 3x the time. No prio-under variant reaches the winner area.
