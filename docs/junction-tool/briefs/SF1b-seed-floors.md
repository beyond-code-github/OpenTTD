# SF1b: seed floors from the rules: out-fan gap derived, doubled lane split next to its bridge (D215; RECON-seed-floors)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git checkout -B wt-sf1b <HEAD>` (given at launch). Never use git stash.
- RECON-seed-floors.md (read it): (1) the out-fan's JUNCTION_SHIFT_BACK_TILES = 4 (junction_topology.cpp :1368) is exactly the curve-gap minimum on the joiner's route (merge S-bend c0,c1 then fan c2,c3: (c0,c2) >= g2, same-hand (c1,c2) >= g1); k = 3 breaks CurveSpeed on every out row. (2) LL_RR's doubled leg e8 (heading +x) has its lane split 6 tiles before its bridge heads because :1303 and :1305 are lower bounds only; making them exact keeps the same cut, the same boxes (T 41x14, LL_RR 62x25) and a clean checker.

## Read first
- CODINGSTYLE.md (C++ part); TESTS.md (`[topology]`, `[topoproj]` rows); RECON-seed-floors.md; src/junction_topology.cpp :970-990 (constants), :1275-1375 (in chain with g1/g2 at ~:1283, doubled legs, out chain); src/junction_chain.cpp JunctionCurveGaps.
- src/tests/junction_topology.cpp: the placement tests ("the T's placement", "LL_RR with an L_R sideline: placement", "LL_RR series bridges"), "the LL_RR seed is clean".

## Tests (record each red)
1. **Outer `[topology]` "topology: a doubled leg's lane split sits next to its bridge" (D215)**: on LL_RR's placement, for every doubled edge, the copy-0 first near head (rp.nh0[0], i.e. placement.bridges of the edge's first structure bridge, copy 0 start head) is exactly 2 tiles along heading after the lane split (the copy-1 SBend's first tile, placement.copies[e][t].first). Red: e8 is 8, not 2 (record).
2. **`[topology]` "topology: the out-fan gap is the curve-gap minimum"**: with a spec whose tl or cl gives g2 = 9 (e.g. tl 5, cl 6 if the spec allows; choose any spec values that MakeTestJunctionSpec can take and that change ceil(g2/2)), every out fan starts ceil(g2/2) (or the same-hand value, by the formula) tiles after its joiner merge, and the seed is checker clean. Red: the constant 4 (record).
- The T placement test and every existing test stay green unedited (the recon shows no change at the test spec).

## Changes (junction_topology.cpp only)
1. Out chain: replace `JUNCTION_SHIFT_BACK_TILES` in `along(heading, rp.fof, njm, ..., true)` by `k_out = std::max((g2 + 1) / 2, same_hand ? 1 + (g1 + 1) / 2 : 2)` where g1, g2 are the in block's (move their computation up if they are scoped inside it), and same_hand = the hand of the joiner merge's closing curve (c1, the joiner route's last curve before the merge) equals the hand of the fan's first curve (c2) (derive the hands as the in block does for c0/c2: the fan turns toward the port row: +1 when it lies right of the heading). Comment: two lines citing JunctionCurveGaps on the joiner's route and RECON-seed-floors §1. If the hand of c1 is not available from the structure/programs without new data, STOP and report. Remove JUNCTION_SHIFT_BACK_TILES from junction_topology.cpp if nothing else uses it.
2. Doubled legs: make `along(heading, rp.nh0[0], rp.lane_split, 2, ...)` (:1303) and `along(heading, rp.nh1[0], rp.sb1_last, 1, ...)` (:1305) exact (`true`); update the comment above (:1300) to say the split sits next to its bridge (D215) while the run from the exit split to the lane split keeps the slack.
3. Regenerate tools/ottd/layouts/circuit-llrr-TL4.json (Release: `JUNCTION_CIRCUIT=$PWD/tools/ottd/layouts/circuit-llrr-TL4.json build-rel/openttd_test "[.circuitllrr]"`) and commit it.

## Stop conditions
- Any existing test fails (placement numbers included); c1's hand unavailable; the circuit test fails; anything not covered.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `tools/junction/jt fast build '[topoproj]' 5`; `tools/junction/jt gate build-rel` (403 before).
- One commit: `Change: Junction seed floors from the rules: out-fan curve gap, lane split next to its bridge (D215)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. TESTS.md `[topology]` row: "SF1b" and the count.

## Report
The standard report, plus e8's lane split before/after and the test-2 spec values. When you have reported, stop.
