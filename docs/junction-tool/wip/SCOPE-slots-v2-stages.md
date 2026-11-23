# SCOPE: Slots v2 as stages (for 2026-11-24)

## Where we are (2026-11-23, end of day)
- The domain suite certifies the rows (D428, D436): ten configurations exact, V's 197 accepted at its points bar 5 rebuild artefacts; behaviour judged through `RowsJudge`; `SlotsOptions` is the one rule set the judge and Slots share (lazy-2).
- Shape hands over V's 197 with offers = 3 and D443 (shape-5); the D443 crossing/copy bypass is fixed and pinned (78702a4d18).
- T1 is drawn by Slots from Shape's variant, no seed, no pins, lazy keep-apart (1-5 of ~300 pairs get rows), and the judge and the domain accept it (lazy-2).
- T2 is NOT drawn: not because it is infeasible (lazy-5: stage A's LP with every keep-apart row has slack 0), but because (1) the integer stage is slow even with a few hundred side binaries, and (2) stage A's S-bend choice can give a contracted writing that breaks the node-turn rules (F6/F8), so stage B gets an illegal writing.
- Keep-apart sides cannot be fixed upfront for most pairs (sides-1..5, cells-1/2): the layout fixes the half-plane, not the axis that carries the spacing. Safe pair rules (walk_strict, facing) decide ~30% on 197.

## The shape (close to the frozen E5 pipeline, D418/D422/D427, rebuilt on certified parts)
1. **Stage A, continuous (LP), lazy keep-apart:** Shape's variant with its offers; no grid, continuous lengths; keep-apart rows only for pairs that come too close, each added with its side read from the drawing that broke it (lazy-4's `fixed_pairs`), freed if a round is infeasible. Output: a drawing that fixes which offers are used (> 0) and, for every pair with rows, its side.
2. **Contraction that keeps the turn rules by construction:** unused offers removed (D432), but where removing a flank would turn more than one eighth at a node (F6) or give two sibling legs one heading (F8), the flank stays. The contracted variant is checked with the judge's turn functions before stage B; a failure is a bug in this step, not a solve problem.
3. **Stage B, integer, contracted, grid on:** `SlotsOptions` with the references read from stage A's drawing (`port_zero_from`, `node_gap_from`, `along_from`); stage A's sides fixed for its pairs; lazy for any pair still too close (side from the breaking drawing; freed on conflict).
4. **Check:** the judge (`DrawingProgramJudge(DEFAULT_RULES)`) and the domain on every result; a refusal is reported, never returned as a drawing.
5. **Later, optional: one-axis compaction** (wip/NOTE-one-axis-compaction.md) on stage B's drawing: alternating x/y LPs, no binaries, legal at every step.

## Steps for the morning (each one agent, test-first)
0. Cheap checks on T2's integer stage before restructuring: a tight coordinate bound for every big-M (from stage A's drawing), a zero objective (feasibility only), stage A's drawing as a MIP start; one solve each, 15 s cap. (Hypotheses: big-M size, then integer parity, then objective.)
1. Stage A as an LP lazy loop (continuous lengths, no grid): T1 and T2; rounds, pairs, ms; the drawing's chosen offers.
2. Contraction with the turn rules: the rule (keep the flank when dropping it breaks F6/F8; F3 within a track as `ContractedVariant` already does), a unit test per rule (red first), applied to stage A's T2 drawing.
3. Stage B integer from stage A (fixed sides, lazy): T1 regression, T2; judge and domain verdicts.
4. Then T3 and 197 from Shape's variant, each with the 15 s seed budget (D297) in view.

## Open questions (bring to Pete when they arise, not before)
- If stage B's integer solve is still slow with fixed sides: where the time goes (grid parity, node gaps, the remaining free pairs).
- If stage A's side guesses are wrong often on T3/197: whether to fall back to binaries for those pairs, or to one-axis compaction from a generous first drawing.
- The remaining Shape item: V's 197 needs offers = 3 and D443 (shape-5); flex pairs 197: 1980 (only the LP stage sees them all).

## Gemini's review (2026-11-23 evening), digested
Adopted:
- Step 0 order as planned: tighten every big-M from stage A's box and use a zero objective on T2's 51-pair instance first; if still slow, fix the binaries from stage A to separate branching from the parity lattice.
- Per-pair big-M from bounds (not one global M): implement with the step-0 check.
- Lazy rows: a pair's side is never flipped; a pair whose fixed side conflicts is promoted to binaries, so the set only grows (no cycling). lazy-4's freeing already does this; keep it monotone.
- Stage B: fix sides only for pairs with comfortable stage-A margin (>= spacing + 2 half tiles); give binaries to the tight ones; MIP start from stage A's drawing; on infeasibility, one bounded retry releasing all of stage A's active pairs.
- Compactness: a short warm-started polish (length objective, MIP start = stage B's legal drawing, 2-3 s) instead of one-axis compaction.
Not adopted (with evidence):
- "Non-parallel walls in one face have their sides fixed by the face walk, so 1 binary each": contradicted by sides-1 (`inward` restriction fails on 197 410 times, X1 268) and cells-2 (even inside convex cells the two-way inward choice fails 16-39 times per fixture). It holds only where the walk is monotone (walk_strict), which we already use.
- Indicator constraints: `AddGenConstrIndicator` is Gurobi's API; our backend is HiGHS (src/junction/placement/junction_milp.cpp), which has none; per-pair tight M is the equivalent we can do.
- 2-binary (log) encoding of the 4 candidates: fewer binaries but usually a weaker relaxation than one-hot; a measurable option only if step 0 leaves binaries as the bottleneck.
To Pete: one-axis compaction parked (Gemini: diagonals and N/S pieces freeze with y, little freedom left; the polish pass is simpler). Reversible.
