# T1: placement on JunctionPositions with reasons; the hidden choices become fixed per-state values (DESIGN-G1 §14.2, §14.4, §14.7 T1; D216)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git checkout -B wt-t1 <HEAD>` (given at launch). Never use git stash.
- TL1b merged: every placement floor is derived; `[topologysweep]` runs T and LL_RR over tl 3-8 x 4 speeds, all clean.
- Today PlaceJunctionTrackGraph (src/junction_topology.cpp ~:932-1650) collects difference constraints `cons` (`ge`, `eq`, `along`, `run`) per phase and solves them with a Bellman-Ford lambda `solve` (~:1117). Three values are found outside the relations by a loop (~:1552-1606): each vertical S-bend's m (`free_m`, re-read from the X solve until it settles), the port-row offset `best_o` (a minimax over fans), and each `joiner_div` Corner's size (read from the Y rows, ~:1153 and ~:1459). D216: within a search state these are fixed values, changed only by an edit; and an infeasible solve must name the relations and graph elements that caused it.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); TESTS.md (`[positions]`, `[topology]`, `[topologysweep]` rows); DESIGN-G1.md §14.2, §14.4.
- src/junction_positions.{h,cpp} whole; src/tests/junction_positions.cpp.
- src/junction_topology.h JunctionTrackPlacement and PlaceJunctionTrackGraph; src/junction_topology.cpp PlaceJunctionTrackGraph (the Elem struct and chain build ~:1040-1100, the solve lambdas ~:1104-1145, refresh ~:1148, the build's Corner/SBend relations ~:1450-1480, the rounds ~:1552-1610), CutJunctionTrackBridges' call of it.

## Tests (record each red; run against stubs before the real code)
1. **Outer, `[topologysweep]` "topology: placement from its own choices equals the seed"** (append to the TL1b sweep's cells or a copy of its loop): per cell, p = the cut's placement (CutJunctionTrackBridges); q = PlaceJunctionTrackGraph(same inputs with the cut structure, p.choices); REQUIRE q succeeds and q equals p in every field except `why` (a file-static `SamePlacement` helper in the test file comparing fields; INFO the first difference). Red: the overload stub returns an empty placement.
2. **`[topology]` the same at the test spec for T and LL_RR** (fast copy of test 1).
3. **`[topology]` "topology: a contradictory choice is explained"**: LL_RR at the test spec (if LL_RR has no joiner_div Corner, the T; if neither, STOP): take the first joiner_div Corner in (edge, token) order, set its `choices.corner` to (its rule floor k) - 1, solve with the overload: REQUIRE it fails; `why` non-empty; every entry has a non-null rule; some entry carries that Corner's edge; the error text contains the chain. Red: stub.
4. **`[positions]` "positions: an infeasible propagation explains itself"**: 4 variables in [0, 100]; AddDifference(1, 0, 3, reason 7), AddDifference(2, 1, 4, reason 8), RestrictRange(0, 5, 5, reason 6), then RestrictRange(2, 0, 10, reason 9) returns false; Explain() == {8, 7, 6, 9} (lo chain of the failed variable from it to its root, then its up chain). Also Explain(2, true) on a feasible copy (without the last restrict) == {8, 7, 6}. Red: Explain returns empty.
- Every existing test stays green unedited (positions, topology, topoproj, nodes, the circuit). tools/ottd/layouts/circuit-llrr-TL4.json regenerated in Release must be byte-identical (`git diff --quiet` on it); if not, STOP.

## Changes
1. **junction_positions.h/.cpp** (existing callers unchanged by default arguments):
```cpp
static constexpr uint32_t NO_REASON = UINT32_MAX;
bool AddDifference(size_t a, size_t b, int k, uint32_t reason = NO_REASON); ///< x_a - x_b >= k.
bool RestrictRange(size_t v, int min_value, int max_value, uint32_t reason = NO_REASON);
std::vector<uint32_t> Explain(size_t v, bool lo) const; ///< The reasons that set v's lo (or up), from v back to a root (a range, a domain, the initial bound); stops at a variable met twice.
std::vector<uint32_t> Explain() const; ///< After a call returned false: Explain(failed, true) then Explain(failed, false).
```
   Per variable a cause for lo and for up: `struct Cause { uint32_t reason = NO_REASON; size_t from = SIZE_MAX; };` (private). A difference d (a - b >= k) raising lo_a sets {d.reason, b}; lowering up_b sets {d.reason, a}; a range sets {reason, SIZE_MAX}; a domain or avoid sets {NO_REASON, SIZE_MAX}. The walk continues at `from`'s lo (for a lo chain) or `from`'s up (for an up chain), skipping NO_REASON entries in the output. Lo/Up trail entries also store the old cause so Undo restores it. `size_t failed` = the variable whose lo passed its up in the last failing call. Difference gains `uint32_t reason`.
2. **junction_topology.h:**
```cpp
/** Why a placement relation exists (DESIGN-G1 §14.4): the rule and the graph elements it serves. */
struct JunctionPlacementReason {
	const char *rule = nullptr; ///< A literal naming the floor or block, as the code's comment there names it (e.g. "out fan k_out", "chain run", "waiting room").
	uint32_t edge = UINT32_MAX;
	uint32_t node = UINT32_MAX;
	int32_t row = -1; ///< The row chain's index, -1 when none.
};

/** The values placement does not derive from its relations (DESIGN-G1 §14.2): fixed within a search state, changed only by an edit. */
struct JunctionPlacementChoices {
	std::vector<std::vector<int>> m; ///< Per edge, per token of JunctionTrackPrograms::edges: a vertical S-bend's size, 0 for other tokens.
	std::vector<std::vector<int>> corner; ///< Per edge, per token: a joiner_div Corner's row span, 0 for other tokens.
	int port_offset = 0; ///< The port rows' offset.
	bool operator==(const JunctionPlacementChoices &) const = default;
};
```
   JunctionTrackPlacement gains `JunctionPlacementChoices choices; ///< The choices this placement was solved with.` and `std::vector<JunctionPlacementReason> why; ///< On failure: the explanation chain of the contradiction.`
   New overload: `JunctionTrackPlacement PlaceJunctionTrackGraph(..., const JunctionPlacementChoices &choices, std::string *error);` (same parameters, choices before error). Doc: one Y solve and one X solve with the choices fixed (DESIGN-G1 §14.2).
3. **junction_topology.cpp:**
   - The body moves into a file-static helper taking `const JunctionPlacementChoices *fixed` (nullptr: today's rounds). Both public functions call it.
   - Elem gains `int token = -1;` (its index in programs.edges[e]). If a free_m S-bend or joiner_div Corner occurs anywhere but an edge's own chain (e.g. on a copy), STOP.
   - `cons` entries gain a reason index into a local `std::vector<JunctionPlacementReason> reasons`; a local `JunctionPlacementReason why_now` is set at the top of every block that adds relations and at each derived floor (with the loop's edge, node or row), and `ge` records it (assert `why_now.rule != nullptr`).
   - `solve` becomes: a JunctionPositions(pt_count, 0, 4095); each cons as AddDifference(v, u, k, reason index); RestrictRange(0, 0, 0, the "origin" reason); on success val[i] = Lo(i); on failure fill `why` from Explain() (map indices to reasons) and error = "the placement rules contradict each other: " + the chain, each as `rule (edge e, node n, row r)` with absent parts left out, joined by " <- ". Bellman-Ford is removed.
   - With `fixed`: no rounds. Y: free_m S-bends use choices.m; each joiner_div Corner adds `eq(1, el.first, el.last, choices.corner)` beside its existing `ge`; port rows = choices.port_offset + notation; fan_m from them; X: each free_m S-bend's `ge(0, ...)` becomes `eq` with choices.m (same orientation), joiner_div kp = choices.corner. Then the same output code as today.
   - Without `fixed`: today's rounds unchanged; at the end fill `choices` from the final el.m, vy[first] - vy[last] of each joiner_div Corner, best_o.
4. TESTS.md rows: `[positions]` +1 (T1), `[topology]` "T1" and the count, `[topologysweep]` time.

## Do not
- No change to any relation, floor, the cut, the render, signals, junction_nodes, or the generator's use of JunctionPositions.
- No facts, no incremental re-solve, no search in this step.

## Stop conditions
- Any existing placement number changes (incl. the circuit file); a test 1 cell differs; no joiner_div Corner on T or LL_RR; free_m/joiner_div outside an edge chain; a reason-less relation the assert finds in a block you cannot name from its comment; anything not covered.

## Gates
- `tools/junction/jt fast build '[positions]' 5`; `tools/junction/jt fast build '[topology]' 5`; `tools/junction/jt fast build '[topoproj]' 5`; `build-rel/openttd_test "[topologysweep]"` (to a file); `tools/junction/jt gate build-rel` (412 before; report the count).
- One commit: `Change: Junction placement solved on JunctionPositions with reasons, choices fixed per state`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus: Release time of CutJunctionTrackBridges (whole cut search) for LL_RR at the test spec and at tl 6 v196, before and after; the number of relations per phase at the test spec for LL_RR; and, as a scratch measurement not committed: on "LL_RR with an L_R sideline: placement", Explain(v, true) for the X_e1 and X_e2 points (the chains of reasons that put them at their columns). When you have reported, stop.

## Amendment 1 (orchestrator, after the stop: the bridge-hug pick reads the previous X solve)
1. **The hug pick is a fourth choice.** `JunctionPlacementChoices` gains `std::vector<std::array<int, 4>> hug; ///< Per doubled-leg bridge, in (row, k) order of the crosser block: low point, low dx, high point, high dx of the crosser tiles the copy-0 heads hug (D80, D118; point ids of this structure's placement, valid only with the same structure).` With `fixed`, the crosser block takes its pick from `hug` (no read of vx); the crossers' `ge` relations are unchanged. Without `fixed`, the rounds pick as today and the final round's picks fill `hug`. (Later, T1b's packed bridge groups replace the pick; not now.)
2. Restart from your saved patch (/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/t1-partial.patch) and the positions part in the worktree; run every gate and the byte-identical circuit check; complete the report items.
3. Test 4's red: run it once against an `Explain` returning empty and record it.
4. Accepted: reusing `Elem::tok`; `why_now` pushed lazily and deduplicated, provided every relation's reason is the block or floor it was added in.
