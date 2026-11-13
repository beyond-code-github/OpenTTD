# ARR1: the track arrangement as data (REVIEW-joint-loop Q6 step 1, D254, D255)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
Placement will take its arrangement (which track lies west of which on a row, which band event lies north of which, which piece of a crosser passes under a row bridge) from the order sweep and the programs instead of rediscovering it by a loop (D254). This step only builds that arrangement as data and proves the existing seeds obey it. Placement does not read it yet; nothing in placement, the joint model or the pipeline changes.

The definition is D255 in `docs/junction-tool/DECISIONS.md`; read it first. A recon (ARR0) already measured it at three seeds with a throwaway probe: `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/arr0/arr0_test.cpp`. Reuse its evaluation logic (how it finds an edge's columns on a stack y from the seed's rendered, tagged pieces, how it takes an event's y, how it finds the piece between a bridge copy's heads) in the test helper; do not copy its probe definitions beyond what this brief states.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`: `git -C /home/pete/OpenTTD/.claude/ng1 checkout -B wt-arr1 <HEAD given at launch>`. `build` and `build-rel` exist (HiGHS configured); delete stale `.pch` files if the build complains. Leave untracked files alone.
- Output files go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/arr1/`.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md` (C++ part);
- `docs/junction-tool/DECISIONS.md`: D254, D255 (the last two rows);
- `src/junction/topology/junction_topology.h` lines 115-270 (`JunctionTrackRowItem`, `JunctionTrackBandEvent`, `JunctionTrackOrder`, `Step`, `JunctionTrackPrograms`, `ProgramJunctionTrackGraph`);
- the ARR0 probe above;
- `src/tests/junction/topology/junction_topology.cpp` around line 2610-2660 (tests that take a seed with `MakeJunctionGraphSeed`);
- `src/junction/topology/CLAUDE.md`.

## Changes
In `src/junction/topology/junction_topology.h` / `.cpp`, after the programs:

```cpp
/** Which piece of a crosser lies between a doubled bridge copy's heads on that copy's head row (D255). */
enum class JunctionUnderPiece : uint8_t {
	RunBefore, ///< The run that ends at the Under token.
	RunAfter, ///< The run that starts at the Under token.
	PreviousBendTail, ///< The tail of the nearest bend token (Fan, Corner, SBend) before the Under token.
	NextBendHead, ///< The head of the nearest bend token after the Under token.
};

/** A crosser's Under token under one copy of a row edge's doubled bridge, with the pieces that may pass under it. */
struct JunctionArrangementUnder {
	uint32_t edge; ///< The crosser.
	uint32_t token; ///< Index of its Under token in the edge's program.
	uint32_t bridge_edge; ///< The row edge overhead (the token's index).
	uint8_t copy; ///< The bridge copy (the token's copy).
	std::vector<JunctionUnderPiece> pieces; ///< The candidates that exist in the program, in enum order (a bend candidate exists only when such a bend token exists).
};

/** The arrangement the order and programs fix (D255): placement's static rows read it. */
struct JunctionTrackArrangement {
	struct RowPair { uint32_t row; JunctionTrackRowItem west; JunctionTrackRowItem east; };
	struct BandPair { uint32_t band; uint32_t south; uint32_t north; }; ///< Indexes into order.bands[band]: north lies strictly north of south.
	std::vector<RowPair> row_pairs; ///< Per row, each consecutive pair of order.rows[row].
	std::vector<BandPair> band_pairs; ///< Per band, each pair of events where the later has a higher layer and touches an item of the earlier.
	std::vector<JunctionArrangementUnder> unders; ///< Per Under token of a doubled bridge (both copies), in edge then token order.
};

JunctionTrackArrangement ArrangeJunctionTrackGraph(const JunctionTrackGraph &graph, const JunctionTrackOrder &order, const JunctionTrackPrograms &programs);
```

- "Touches an item": two events share an edge; a node event counts every edge incident to the node; a crossing event counts its two edges a and b.
- "Doubled bridge Under token": an `Under` step whose index edge carries a doubled bridge, i.e. the tokens emitted by the `doubled_bridge(x)` branch of the band programs (they come in pairs, copies 0 and 1). If the programs do not let you tell a doubled bridge's Under tokens from a single bridge's without new data, STOP and report.
- Doc comment on the function in the style of its neighbours, citing D255.
- `src/junction/topology/CLAUDE.md`: one standalone fact, no D-ids or references: the arrangement is built from the order and the programs: each row's consecutive items west to east, each band's touching events of a higher layer strictly north, and per Under token of a doubled bridge the pieces that may pass under it (the runs either side, the previous bend's tail, the next bend's head).

Nothing else changes.

## Tests (outside-in, red first; record each red message)
1. **"arrangement: the seeds obey the arrangement"** in `src/tests/junction/topology/junction_topology.cpp`, tag `[junction][topology]` for T and LL_RR x L_R. If those two together take over 2 s in Debug, tag it `[junction][topologysweep]` instead and say so. A second case with the same body for L_R x LL_RR (it seeds through the joint loop, about 16 s), tag `[junction][topologysweep][.sweep]`.
   - Take the seed with `MakeJunctionGraphSeed` and build the arrangement from its graph, order and programs.
   - REQUIRE the arrangement is not empty (row_pairs and unders non-empty).
   - Row pair held: on every stack y of the row where both items have tiles (an edge by its pieces' columns, a node by its tile), the west item's west end ≤ the east item's west end and its east end ≤ the east item's east end.
   - Band pair held: the north event's y < the south event's y (a node event by its tile's y, a crossing event by the y of its Under/Above token's span, as ARR0).
   - Under held: the crosser's tile strictly between copy `copy`'s heads, on that copy's head row, belongs to a piece in `pieces`.
   - CHECK every fact held; on a break, FAIL_CHECK naming the fact (kind, row/band, items or edge and token). INFO the counts (row pairs, band pairs, unders) per hub.
   - Red: with `ArrangeJunctionTrackGraph` returning `{}`, the REQUIRE fails. Record it.
2. **"arrangement: one Under fact per doubled bridge Under token"** `[junction][topology]`: on T and LL_RR x L_R, `unders.size()` equals the number of Under steps of doubled bridges counted from the programs, and every `pieces` is non-empty. On T, CHECK `unders.size() == 4` (ARR0). Red with the stub.

## Do not
- Change placement, the joint model, the pipeline, or any existing test.
- Add arrangement facts for ARR0's "band edge on a row node" or "port stub on another row" pieces (D255: step 2 rules them).
- Use `pkill`, a broad `kill`, or any form of `git stash`.

## Stop conditions
- A fact breaks at a seed: report every broken fact and stop (that contradicts ARR0 or D255).
- Identity or an existing test changes.
- Anything not covered.

## Gates
- `tools/junction/jt fast build '[topology]' 5`;
- `tools/junction/jt gate build-rel`: all pass, report the time;
- `tools/junction/jt sweep build-rel`: all pass within its budget;
- `tools/junction/jt identity build-rel`.

Give every `jt` call a Bash timeout of at least 400000 ms.

One commit on wt-arr1: `Add: Junction track arrangement from the order and programs`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
- references read;
- the commit;
- red evidence per test;
- types and functions added;
- per hub: row pairs, band pairs, unders, all held;
- gate, sweep and identity counts and times;
- stops;
- `git status --porcelain` and `git log -1`.

When you have reported, stop.
