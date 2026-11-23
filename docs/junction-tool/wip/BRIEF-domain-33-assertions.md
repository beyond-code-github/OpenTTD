# BRIEF domain-33: the suite asserts what it found (regression guards)

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 75 minutes wall. Test code only. Never run git stash. Do not touch branch wip-defaults.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- The domain suite's headline result (every configuration exact: 0 too strict, 0 too loose) lives only in hidden report tests (`[.][planardomainenc]`), which the gate never runs; and five findings have no pinned case. A unit-test suite must fail when a rule regresses (engineer, 2026-11-23: "small and targeted - useful for regressions").
- Read first: `docs/junction-tool/TESTS.md` (tags and budgets); `tools/junction/jt`; in `src/tests/junction/planar/planar_domain_encoding.cpp`: `RunEncoding` (~1117), `Tally` (~624), each fixture and its hidden report test, and the existing pinned tests named "(F2)", "(F5, D434)", "(F6)" as the pattern (build a Geometry on a fixture, `ContractedVariant`, read-out, `CheckPlanarProgramOnPoints` / `accepts`-style verdict, plus the domain verdict).

## 2. Exactness asserted
1. `RunEncoding` gains a last parameter `std::array<uint32_t, 7> *totals = nullptr`: when set, it receives the per-class polyline counts summed over all configurations (no other behaviour change; it may still write the report when `report_path` is non-null; allow `report_path == nullptr` to skip writing if it does not already).
2. One test per fixture X1, X2, M1, K1, L1, Y1, P1, BR1: `"planar domain: <F> is exact under the default rule set"`, tag `[junction][planar][planardomainexact]`, rule set `span+lead+endnode+crosspiece+grid+portzero`, no report written; `CHECK(totals[2] == 0)` (too strict) and `CHECK(totals[3] == 0)` (too loose), with an INFO naming the fixture and both counts. Same configurations as its hidden report test (share the config list; do not copy it).
3. Measure each in Release (`build-rel`) and Debug (`build`). Add the tag and its budget to TESTS.md: if the eight together take under 10 s in Release, tag stays as is (it runs in the gate); report the gate's new wall time (`tools/junction/jt gate build-rel`). If the gate exceeds its 30 s budget, STOP and report the eight timings (do not retag on your own).

## 3. Pinned cases (each two-sided: the case and its nearest counterpart; red evidence not required where the property already holds, but each must FAIL if its rule is switched off: show that once per test by flipping the option or suffix in a scratch run, then revert)
1. **D430 (outward S-bend at a node is legal):** on X1, the straight leg stepping AWAY from its sibling immediately at its split: rows accept, domain accepts. Counterpart: the same step TOWARD the sibling with no lead: rows reject, domain rejects (F1 already covers part; reuse its geometry if it fits, say so).
2. **D433 (zero-length trident):** K1 with BC1 = 0: rows accept, domain accepts. Counterpart: BC1 = 1 half tile (below node_leg): rows reject, domain rejects.
3. **D434 (zero port line):** a fixture with a D412 line (X1's AC1 or K1's ABC1) at total length 0 with its node on the box edge (the trident-like case; if no fixture can express it because the port end and node coincide with a moved start, stop and report): rows accept under `port_zero`; counterpart AC1 of total length 1 half tile: rows reject.
4. **Bridge end run (BR1):** a crosser whose separator end run is exactly 1 piece before the bridge head: accepted on BR1, while the same geometry with the structure a tunnel (X1-shaped with `tunnel`) is rejected (end run 2).
5. **Copy offset (Y1):** the copy A1#9 moved one lane further from its parent over its whole length: rows reject (corridor); counterpart: the base: rows accept.
Name each test after its rule, e.g. `"planar domain: an outward S-bend at a node is legal (D430)"`, tag `[junction][planar][planardomain]`.

## 4. Gates and commit
- `tools/junction/jt fast build '[planardomain]' 5` (Debug), `tools/junction/jt gate build-rel` (once).
- One commit: `Test: Junction: the domain suite asserts exactness and pins its findings (D428)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 5. Report (short)
References read; commit; per exactness test its Release and Debug time; the gate's wall time; per pinned test: the geometry used and the switched-off evidence (which option, what failed); stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
