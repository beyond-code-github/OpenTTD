# Gemini on solvers and licensing: digest (orchestrator, 2026-11-22)

- **Licensing (verify against the licence texts before relying on it):** OpenTTD is GPLv2-only. Apache-2.0 (SCIP 8+, OR-Tools CP-SAT) is GPLv3-compatible, not GPLv2. CBC (EPL-2.0) has no GPL secondary licence designated: incompatible. GLPK (GPLv3+): incompatible. lp_solve (LGPL-2.1): compatible but far weaker than HiGHS. HiGHS (MIT) stays the shipped solver.
- **HiGHS lazy constraints:** on the developers' wish list behind parallel MIP; an outside collaborator is starting; no release target (HiGHS discussion #3079, per Gemini). Do not wait for it.
- **CP-SAT vs big-M (a guess):** native disjunctions with clause learning are likely much stronger than big-M (whose LP relaxation sets direction binaries to 1/2 and gives no guidance); an offline CP-SAT benchmark of 197's pairs would tell whether the problem is inherently hard. Measurement only, never linked.
- Recommends amending D381 for (1) (bounded sides_from re-picks), optional offline CP-SAT benchmark on the side.
