# lazy-5: why T2 is undrawable with every keep-apart row (diagnosis)

Hidden test `[.][lazydiag]` ("planar slots: T2 stage A elastic keep-apart diagnosis (lazy-5)", planar_domain_encoding.cpp), `build-rel`, one solve at a time, 15 s cap each. T2 offers 1, D443 off, flex placed on the unflexed variant; stage A options as the lazy test (`SlotsOptions(X1Fixture(v), DEFAULT_RULES)`, `node_gaps` off), `keep_apart_out`, no `only_pairs_on` (every pair has rows: 7780 binaries). Elastic: `elastic = true`, `elastic_families = {"sep"}` (every other row hard; `grid` is ignored under elastic, by design).

| run | status | ms | total slack | stretched `sep` rows |
|---|---|---|---|---|
| elastic, continuous (LP) | Optimal | 633 | 0 | 0 |
| elastic, `elastic_integer` | Error (15 s cap hit) | 18230 | - | - |
| control: hard rows, grid on | Error (cap) | 20585 | - | - |
| control: hard rows, grid off | Error (cap) | 23722 | - | - |

## Findings

1. LP total slack is 0: no keep-apart row needs stretching in the relaxation, so no single pair is too close for Shape's layout (step 2's case: the problem, if any, is in the side choices' combination).
2. The integer run produced no result in the cap (Error, not Infeasible), so there are no stretched pairs to class (step 3 has nothing to act on). The hard all-rows control with the lazy test's own options is also Error in 20.6 s (grid on) and 23.7 s (grid off): stage A with all rows is slow, not proved Infeasible.
3. The premise in lazy-4 is not what that note measured. The "4128 binaries, Infeasible" figure is the all-rows control of STAGE B (contracted, node gaps on), not stage A. The stage A Infeasible rounds of lazy-4 (round 8 with 67 fixed pairs, empty conflict) are rounds with sides FIXED from the previous round's points (D444 trial), so they are infeasible by the fixed sides, not by the rows. Stage A with every row and free sides has never been shown Infeasible; here it is unresolved in 15 s.
4. The LP drawing is not integer or on the grid; the domain refuses it (82 breaks: [grid] 29, [node] 50, [turn] 2, [connection] 1), which says nothing about keep-apart closeness (no stretch to place).

## Open question

Which model is meant: stage B's (contracted, where the all-rows control IS proved Infeasible, 4128 binaries)? Elastic there needs a contracted variant from a stage A drawing, which this brief does not name.
