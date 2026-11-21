# For Gemini: solver choice, lazy constraints and licensing

## Context
- We are building an OpenTTD fork (OpenTTD is GPL v2) with a rail-hub layout solver. It links HiGHS (MIT) as its MIP solver.
- Our keep-apart model is pairwise separation: per pair of walls in a face, one of up to 4 separating directions. On our test hub that is about 1,500 pairs and 6,000 binaries (big-M rows).
  - With all directions as binaries, HiGHS does not solve it in 120 s.
  - With directions fixed (taken from the expert's drawing) it is an LP and fits.
- **Measured in HiGHS 1.16.0:** `kCallbackMipDefineLazyConstraints` is a placeholder. It is accepted, never invoked, and offers no way to add a row from a callback.
  - Lazy constraints in HiGHS appear to be an open draft, PR #1636 "Add lazy constraints".
- We would like exact keep-apart without building an outer re-solve loop. Our alternative is fixing directions from a bounded number of earlier drawings.

## Questions
1. **Licensing.**
   - Is OpenTTD GPL v2 only, or "v2 or later"?
   - Is Apache-2.0 code (SCIP since 9.0, OR-Tools CP-SAT) compatible with linking into a GPLv2 program? Our understanding: Apache-2.0 is compatible with GPLv3 but not with GPLv2.
   - Which open-source MIP or CP solvers with working lazy constraints, or native disjunctions, are GPLv2-compatible? Candidates: CBC / COIN-OR (EPL-2.0: does it designate GPL as a secondary licence?), lp_solve (LGPL-2.1), GLPK (GPLv3+, so not v2-only compatible?).
   - Short, with references (licence texts or the FSF compatibility list).
2. **HiGHS roadmap:** is there any public timeline or recent progress for lazy constraints (PR #1636, or the HiGHS docs and release notes)? Say if you do not know.
3. **Modelling:** is a pairwise-direction disjunction of this size (about 1.5k disjunctive pairs over continuous coordinates and integer lengths) typically easy for a CP-SAT-style solver with native disjunctions or interval constraints, and hard for a big-M MIP? Or is it hard for both? Is it worth a standalone CP-SAT measurement to find out whether the problem itself is hard or just hard for big-M? Guesses marked as guesses.
