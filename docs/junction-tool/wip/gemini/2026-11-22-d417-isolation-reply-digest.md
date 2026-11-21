# Gemini on the D417 isolation: digest (orchestrator, 2026-11-22)

- Agrees: (2) out (the strict rule is fragile at steps with free flanks; the binary remainder misses the 15 s gate); (3) unsound.
- Flags the clear leak: convex faces keep cell clearance rows in pairwise mode even with clear_rows off; line clearance is stricter than segment separation where a vertex does not project onto the wall. Action: `pairwise_all_faces` probe (running).
- Recommends amending D381 for (4) solver-native lazy separation (exact; typically 20-80 of 1449 pairs active), with (1) the sides_from fixed point as warm start and fallback. Orchestrator: (1) first unless HiGHS really supports lazy constraints; HiGHS declares kCallbackMipDefineLazyConstraints but JunctionMilp has no callbacks: 30 min spike running outside the repo.
