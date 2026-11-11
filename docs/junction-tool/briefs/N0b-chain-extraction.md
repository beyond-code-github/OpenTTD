# N0b: the flat T read as chains of runs, curves and switches (D182, D183, DESIGN-nodes §4b)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Why
The node model describes each movement of a junction as a chain: runs (straight in one of 8 directions) separated by 45-degree curves, with switches (splits, merges) at known points. Before the position solve can generate layouts, the model must read today's flat T exactly and agree with the checker on it. This step adds the reader and the first constraint template (curve gaps from N0a, prio coverage), and checks the flat T satisfies them. Generation does not change.

## State
Worktree `/home/pete/OpenTTD/.claude/n0b`, branch `wt-n0b`, head given at launch (feature/junction-tool, has N0a's `src/junction_chain.*` and N1's `src/junction_positions.*`). Create `build` (Debug) and `build-rel` (Release) like the main ones (clang-18 with ccache); build `openttd_test` only. Tests only through `tools/junction/jt` with bracketed tags: `'[nodes]'` (budget 1 s Release, 5 s Debug) in both builds; `jt gate build-rel` once before the commit. Output files in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/n0b/`.

## Read first (once each, offset/limit)
`CODINGSTYLE.md`; DESIGN-nodes.md §4a, §4b; RECON-edge-rules.md §1 and §5; `src/junction_chain.h/.cpp` (N0a); in `src/junction_rules.h`: JunctionPath, JunctionCheckContext, MakeJunctionCheckContext; in `src/junction_rules_internal.h`: SwitchEvent (first = is a split), SwitchEventsOf, JunctionTMovements; in `src/junction_rules.cpp`: CheckJunctionRules (~1694, how the context is made: FindJunctionPorts + JunctionTMovements), PaddedPathDirections (~599, how directions per unit are made), CheckPrio's merge/joiner logic (~1080-1145: ML = EIn->EOut or WIn->WOut; joiner = another path's merge on the same tile with a different trackdir; same from/to = lane merge, no prio), UsedTracks (grep); `SharedFlatT` in `src/tests/junction_map.cpp` (~311: `ConstructJunctionTMap(MakeTestJunctionSpec(), nullptr, std::nullopt, true)`).

## Types and functions (add to `src/junction_chain.h/.cpp`)
```cpp
/** A 45-degree curve on a chain: where it is along the path, which way, and the tile of the piece after the direction change. */
struct JunctionChainCurve {
	uint16_t unit; ///< Units from the path's start to the first unit in the new direction (same position rule as PaddedPathDirections, without padding).
	JunctionCurveHand hand;
	TileIndexDiffC tile;
};
/** A switch on a chain. */
struct JunctionChainSwitch {
	uint16_t unit; ///< Units from the path's start to the switch tile's piece.
	bool split; ///< SwitchEvent::first.
	bool prio; ///< A merge on a main line whose joiner is another movement (CheckPrio's rule); false for splits and lane merges.
	TileIndexDiffC tile;
};
/** One movement of a layout as the node model reads it (DESIGN-nodes §4b). */
struct JunctionChain {
	JunctionPortRole from;
	JunctionPortRole to;
	uint16_t units; ///< The path's length in units.
	std::vector<JunctionChainCurve> curves; ///< In path order.
	std::vector<JunctionChainSwitch> switches; ///< In path order.
};
/** Read every traced path of a layout as a chain, in the checker's path order. */
std::vector<JunctionChain> ExtractJunctionChains(const JunctionLayout &layout, const JunctionSpec &spec);
/**
 * The node model's constraints on chains, checked on given chains (the template the position solve will use): curve gaps (JunctionCurveGaps, DESIGN-nodes 4a) per chain,
 * and for every prio merge on a main-line chain: no curve and no split in the 2 * prio_tiles units before the merge's unit (D108, G9c coverage).
 * @return One line per violated constraint ("chain <i> curves <a>,<b> gap <g> below <min>", "chain <i> merge at unit <u>: curve at unit <c> inside the prio coverage", "... split at unit <s> inside ..."); empty when all hold.
 */
std::vector<std::string> JunctionChainViolations(const std::vector<JunctionChain> &chains, const JunctionSpec &spec);
/** One line per chain for logs: "<from>-><to> units <n>: C<unit><L|R>@x,y ... S<unit>@x,y M<unit>[p]@x,y ...", curves and switches merged in unit order. */
std::string FormatJunctionChain(const JunctionChain &chain);
```
Hand: DirDifference(previous direction, new direction) Left45 -> Left, Right45 -> Right; any other non-Same difference (a 90-degree step) is not a curve of this model: put a line "90-degree step at unit u" into the result of JunctionChainViolations instead (the chain records nothing for it; add `uint16_t steps90 = 0;` to JunctionChain counting them, the only extra member allowed).
Main line roles and port enum names: as CheckPrio uses them.

## Tests, red first (`src/tests/junction_chain.cpp`, tags `[junction][nodes]`)
1. "chains of a hand-built S-bend": build a tiny JunctionLayout by hand (use the existing test helpers in src/tests that build layouts from routes, e.g. RasterizeJunctionRoute/StraightRoute as junction_map tests do; grep for a small example) with one path: 3 X tiles, a one-row S-bend, 3 X tiles. If you cannot make a traced path without the T's ports, instead test the internal step by calling ExtractJunctionChains on SharedFlatT only and skip this test: report that. Asserts: 2 curves, opposite hands, unit gap 2.
2. "the flat T reads as chains": flat = ConstructJunctionTMap(MakeTestJunctionSpec(), nullptr, std::nullopt, true) (static, built once); chains = ExtractJunctionChains(*flat.layout, spec). Asserts: chains.size() equals the number of traced paths; the main-line chains for A have 0 curves; exactly one prio merge on A's chain and one on B's (lane B, the b row); every chain's steps90 == 0; `WARN` FormatJunctionChain of every chain (so the report can quote them). Red: the stub returns an empty vector.
3. "the flat T meets the chain constraints": JunctionChainViolations(chains, spec) is empty (INFO the lines). Red: stub returns one line "stub".
4. "the chain constraints catch a short prio coverage": copy the chains, move the A chain's prio merge unit down by 2 * prio_tiles (or add a synthetic curve on A's chain 4 units before its merge): exactly the coverage line appears.

## Do not
No change to generation, the router, the checker, the skeleton, JunctionPositions or other tests. No `std::to_string`/`std::printf`; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
Test 3 has violations on the flat T (report every line and FormatJunctionChain of the chains involved: either the model or the reader is wrong, and that is the orchestrator's call); steps90 > 0 on the flat T; the number of A or B prio merges differs from one each; OVER BUDGET (the flat T costs ~30 ms Release; if Debug is over 5 s, report the time); anything not covered.

## Gates
0 warnings; `jt fast build '[nodes]' 5`, `jt fast build-rel '[nodes]' 1`, `jt gate build-rel`. Update the `[nodes]` row in TESTS.md with the new times. One commit on `wt-n0b`: `Feature: Junction chains read from a layout (D182)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence per test; types/functions/members added; the WARN lines of test 2 (the chains, verbatim); jt lines with times; stops; `git status --porcelain`, `git log -1`.

When you have reported, stop.
