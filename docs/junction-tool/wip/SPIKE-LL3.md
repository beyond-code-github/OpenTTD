# Spike LL3a through the solver (d2ee981bcc, throwaway worktree, openttd_test Release)

Result: parses and face map valid, but NO variant builds. Every one of 1298 sampled variants fails in the composer before any solve.

1. Parse: ok (36 lines, 18 routes). `PlanarLineSides(..).Why()` empty; 24 channels, 36 crossings.
2. Bits: open crossings 11, tie sites 0, triangles 1, candidate steps 5 -> 2^17 = 131072 variants.
   - The "too many variants" guard (planar_components.cpp:2848, limit open+ties+triangles+steps > 6; here 17) refuses SolvePlanarMapped outright; only PLANAR_ONLY/PLANAR_DRY bypass it. The guard is hard-coded, and the variant space is exponential in the bits (see below).
   - DRY, sequential sample stride 101 = 1298 variants (33 s), 0 built. Each variant fails with:
     - 1298/1298: "a turning channel over a crossing with doubled lanes is composed for two full-share lanes only" (M.Fail, planar_components.cpp:1257, build_bundle). Examples: o0t0f0s0, o0t0f1s13, o1000t0f0s17.
     - 648/1298 also (in the default-shape path check, which is what SolvePlanarMappedVariant reports): "channel of B{1,2,3} crosses channel of F2 from the wrong side" (planar_paths.cpp:184, CheckPlanarShape side test). B1 162, B2 163, B3 323. Examples: o0t0f0s0 (B1), o101t0f0s0, o1041t0f0s18.
   - Full-solve of o0t0f0s0 and o0t0f0s31 (PLANAR_ONLY, one at a time): ok=false, 34 ms, "channel of B1 crosses channel of F2 from the wrong side".
3. No variant built, so no solve, area, size, shape or checker run was possible (nothing to plan).
4. Failure classes
   - Doubled-lane refusal, planar_components.cpp:1257 (`full != 2 || ch.channels[c].lines.size() != 2`): HARD-CODED 2-lane limit (DESIGN-tracks §12 hit list class). Every channel here has 3 lines, so every bundle over a crossing hits it. Not a real infeasibility. Beyond it, other 2-assumptions are probably waiting (the straight-core "doubled and single lanes" refusal at ~1300, JoinEntry(n), the diagonal-piece refusal at ~1260); not reached, untested.
   - Wrong side, planar_paths.cpp:184: real over-choice rejection (the over bit of a crossing must match the face map side, `CrossingFrom` vs the heading-derived `shape_side`), so it prunes about half the over assignments; it is not a hard-coded limit. Surprise: the o0 default variant (all over bits 0) is itself rejected.
   - Variant explosion: even with the refusal lifted, 131072 variants (one thread each in SolvePlanarMapped) is not tractable; the over bits would need deciding from the face map (about half survive pruning), not enumerating.
