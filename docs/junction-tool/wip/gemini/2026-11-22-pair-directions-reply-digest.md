# Gemini on pairwise directions: digest (orchestrator, 2026-11-22)

- **Q1:** Nöllenburg & Wolff (IEEE TVCG 17(5):626-641, 2011, §3.3) separate non-incident face-sharing edges with binaries over the 8 half-planes, pruned by the face's cyclic order (our topological side). Follow-ups (Bast, Brosi, Storandt, SIGSPATIAL 2018 / ACM TSAS 2020) prune further and add lazy cuts, or leave MIP. None decides directions combinatorially in general. (References to verify.)
- **Q2:** exact only for monotone walks: if every edge on the face walk from wall i to wall j projects non-negatively onto i's inward normal, j lies inside i's line in every drawing (no binary needed). Across a reflex corner it depends on lengths. This is our D405 monotone test applied to pairs. Its 60-80% guess is to be measured.
- **Q3:** lazy cuts are exact but conflict with D381 (no lazy loop); fixing from a relaxed drawing (our `sides_from` fixed point) is the standard alternative.
- Action: measure the monotone-walk rule on V's real drawing (pairs fixed, agreement with V, pairs left), running in ng1.
