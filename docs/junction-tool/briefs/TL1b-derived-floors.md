# TL1b: every placement floor derived from the rules, scaling with train length and speed (DESIGN-G1 §14.7 T0; RECON-train-length)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git checkout -B wt-tl1b <HEAD>` (given at launch). Never use git stash.
- RECON-train-length.md (read it whole): 28 of 56 valid cells (T and LL_RR, tl 3-8 x reference speed 100/120/160/196, prio_tiles from SizeJunctionPrio) violate rule 3 (CurveSpeed) or rule 7 (WaitingRoom). Seven spots, classes A-G. Five derived floors (bits 1, 2, 4+32, 8, 16 of its experiment) clear all 56 cells, with the test-cell boxes (T 41x14, LL_RR 62x25) unchanged.
- The experiment patch is /home/pete/OpenTTD/docs/junction-tool/wip/tl1a-experiment.patch (read it whole; it is scratch: a global switch, iostream, debug prints). You implement its bits 1, 2, 4, 8, 16, 32 as permanent code, always on. Bit 64 is not part of this brief.
- The engineer's ruling: the train-length maths belong in the graph's placement; every floor comes from the rules (memory: no hedged padding, derive from the rules or prove with the checker).

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); TESTS.md (`[topology]`, `[topoprojsweep]` rows); RECON-train-length.md; the patch.
- src/junction_topology.cpp PlaceJunctionTrackGraph (the in chain ~:1280-1300, the out chain ~:1365-1385, the element pins ~:1045-1060, the generic chain runs ~:1420-1440, the Y rows ~:1130-1145); src/junction_chain.cpp JunctionCurveGaps.
- src/tests/junction_topology.cpp: SignalledOf, "the LL_RR seed is clean", "LL_RR's uncut w2 bridge is too long at nmax 9" (:588), "LL_RR with an L_R sideline: placement" (:705).

## Tests (record each red, run against HEAD before your code)
1. **Outer, new tag `[topologysweep]` "topology: the seeds are clean at every train length and speed"**: for T (JunctionTTopology) and LL_RR (JunctionLLRRSidelineTopology), tl 3..8, v in {100, 120, 160, 196}: spec = MakeTestJunctionSpec() with tl, train.max_speed = reference_speed = v, prio_tiles = 0 then SizeJunctionPrio(spec).prio_tiles (exactly the recon's method); skip a cell whose prio_tiles is 0 (INFO it; the recon has 4: tl 7-8 at v 196). Seed pipeline as SignalledOf; REQUIRE placement succeeds; CHECK CheckJunctionRules(layout, spec, topology) is empty (INFO each violation with the cell). Red: 28 violating cells (record the count of failing cells from the output, not from the recon).
2. **`[topology]` "topology: the seeds keep their test-spec size"**: at the test spec, T placement size 41x14, LL_RR 62x25. Green before and after (a guard, not a red).
3. **`[topology]` "topology: curve spans in the seed routes"**: at the test spec, on LL_RR's seed routes, for each curve element (Corner, SBend) tagged on a route: the units between its two curves equal the formula the floors use (Corner: h; S-bend of m: 2m), and a run of d tiles between two elements puts their neighbouring curves 2(d-1) units apart. If any measured value differs from the formula, STOP and report the measured values (this is the recon's UNEXPLAINED item: the B-second term may be stricter than the checker).
- Every existing test stays green unedited, with exactly two allowed exceptions (see Changes 6).

## Changes (src/junction_topology.cpp, tests, TESTS.md, the circuit file)
Let `g3 = std::min(2 * tl - 1, 3 * cl)` beside g1, g2 (one definition, comment: the three-apart pair of JunctionCurveGaps).
1. **(A) out-fan, bit 1:** after `k_out`'s current formula: `k_out = std::max(k_out, 1 + (g3 - 2 - 2 * m_out + 1) / 2);` Comment: (c0, c3), the joiner S-bend's first curve to the fan's last, is 2 + 2 (k - 1) + 2 m_out units.
2. **(D) in-edge d_min, bit 2:** as the patch: `d_min = max(d_min, 1 + (g2 - h + 1) / 2, 1 + (g3 - 2 * m_in - h + 1) / 2)`, unconditional, with its comment.
3. **(B, C, F) generic run, bits 4 + 32:** replace `const int dist = 2;` by the patch's derived run: a file-static helper `static int JunctionRunBetween(...)` (name it so; parameters: the two elements' kinds, hands, m, and h, tl, cl) returning `max(2, same hand last/first ? 1 + ceil(g1/2), 1 + ceil((g2 - span_A)/2), 1 + ceil((g2 - span_B)/2))`, 2 when either is not a curve element; across a from-node (first run, i == 0) the max over the predecessor edges' last curve element, as the patch does. No print.
4. **(E) bit 8:** the S-bend ending on a row split node (not a merge) is no longer pinned to `stop` (the patch's condition, without the switch); its edge's last run takes JunctionRunBetween against the successor edges' first curve element, as the patch does.
5. **(G) bit 16:** the Y floor: for every path from a main slot to a non-main slot, `node_pt[first merge after its first split] - node_pt[that split] >= tl + 1` (Y phase only), as the patch's walk. Comment: the waiting room 2 dy - 1 >= 2 tl units.
6. **The two tests the recon says move (bit 4), and only these:** "LL_RR with an L_R sideline: placement" and "LL_RR's uncut w2 bridge is too long at nmax 9". Before editing either, record its old and new numbers and, for each changed number, the floor term that now binds (which pair, which elements). Then: update the placement test's numbers to the new ones; for the nmax test keep its premise (the uncut w2 bridge is too long) by changing only nmax to the largest value at which it still fails, and rename it accordingly. If either change needs anything else, STOP.
7. Regenerate tools/ottd/layouts/circuit-llrr-TL4.json (Release: `JUNCTION_CIRCUIT=$PWD/tools/ottd/layouts/circuit-llrr-TL4.json build-rel/openttd_test "[.circuitllrr]"`) and commit it.
8. TESTS.md: a `[topologysweep]` row (gate only, not per step; its Debug and Release time; budget 10 s), `[topology]` row: add "TL1b" and the count.

## Do not
- No global, no iostream, no debug output, no switch: the floors are always on.
- Do not add bit 64's term, or any floor the recon did not measure.
- No change to the checker, JunctionCurveGaps, SizeJunctionPrio, the structure/cut, or junction_nodes.

## Stop conditions
- Test 3's formulas differ from the measurements; any cell of test 1 still violates with all five floors (report cell and violation); test 2's sizes change; an existing test other than the two named fails; the circuit test fails; anything not covered.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `tools/junction/jt fast build '[topoproj]' 5`; test 1 in Release (`build-rel/openttd_test "[topologysweep]"`, output to a file); `tools/junction/jt gate build-rel` (report the count).
- One commit: `Change: Junction placement floors derived from the curve and waiting-room rules at every train length and speed`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus: test 1's failing-cell count before; per class A-G the cells it was failing in before and 0 after; the two moved tests' old/new numbers with the binding term; boxes at tl 8 v160 (T, LL_RR) before/after; `[topologysweep]` Release time. When you have reported, stop.
