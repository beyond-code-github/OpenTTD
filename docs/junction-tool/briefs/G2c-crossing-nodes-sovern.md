# G2c: crossing nodes with windows, and SoverN projected (D172, DESIGN-O4 "G1", crossing nodes)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Principle
The map says A->S and N->A both cross B (node X_B) and cross each other (node X3, D171). Each node's tile envelope is applied as a window to every edge that passes it, relative to the crossers already placed (D172). Projection realises X3 both ways on the same map and keeps the better score. Each projection is built once and checked once.

## State
- Worktree `/home/pete/OpenTTD/.claude/g2c`, branch `wt-g2c`, head given at launch (feature/junction-tool; G1, G2a and G2b commit 1 `6b6f3a4a48` merged: `JunctionTMapFirstHeight`, `first_height` seam). Create `build` configured like `/home/pete/OpenTTD/build` (Debug); build `openttd_test` only.
- Starting point: apply `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/g2b/g2b_commit2_wip.patch` (`git apply`, it applies cleanly). It is the stopped G2b commit 2: NoverS and SoverN per height, `crossing`, `projections`, `only_crossing` seam, the `route_line` lambda on `FindJunctionLineRoute`, the column-xI bridge-only cost for A->S (X3), the 20000 expansion cap, and the envelope `[xI - D, xI + D]` on A->S that this brief REPLACES.
- Design: `docs/junction-tool/DESIGN-O4.md` section "G1", paragraph "Crossing nodes and their windows"; D171, D172 in `docs/junction-tool/DECISIONS.md`; the earlier brief `docs/junction-tool/briefs/G2b-third-crossing-in-projection.md` (its tests 2 to 4 and amendments 2 and 3 still hold except where this brief changes them).

## Test speed
`[junctionmap]` under 5 s (Debug). Develop against it only; the wider gate once at the end.

## Read first (once each)
`CODINGSTYLE.md`; the patch; after applying it, `ConstructJunctionTMap` (whole) and `src/tests/junction_map.cpp`; `src/junction_line_router.h` (`blocked`, `resource_cost`).

## The change: X_B's window (SoverN only)
- Remove the patch's `[xI - D, xI + D]` envelope on A->S (the resource_cost part for W and b rows). Keep D's computation and its comment (D = nmax - 1 - 2 * k_head, k_head = 1), and keep the column-xI bridge-only cost (X3) and the expansion cap.
- A->S is X_B's first crosser: no window. After it routes, collect the x of its pieces on the W row and the b row (bridge spans included, as the crossing set does): lo, hi. If hi - lo > D, log "A->S crossing wider than the doubled bridge" for that height and try the next height.
- N->A: its line request's `blocked` gains every tile of the W row and the b row with x < hi - D or x > lo + D (within the frame). Write a comment stating the window rule and quoting D172.
- NoverS is unchanged (old router, column fence).

## Tests (red first)
1. Test 1 (rule-clean, both specs) stays green.
2. G2b test 2 ("both crossing realisations are projected") as written.
3. G2b test 3 ("SoverN puts A->S's bridge over the N column") as written: SoverN must now build a layout for both specs. Report whether it is clean per spec, and its violations (rule, tile, message) if not: information, not a stop.
4. "SoverN's crossings fit one doubled bridge": for both specs with `only_crossing = "SoverN"`, every piece of A->S and N->A on the W and b rows lies strictly between the lanes' bridge heads (as G1 test 4 does for NoverS; reuse its check).
5. BENCH lines per realisation with the phase split (the patch has them).
Red evidence: test 4 and test 3 fail before your change (the patch alone builds no SoverN layout).

## Do not
No anchor on xI or any other window than the one above; no window on A->S; no change to NoverS, skeleton, finish, signal pass, checker, router, `GenerateJunction`, `junction_organic.*`, savegame, network or command code. No other realisations, no sweep. No `std::to_string`/`std::printf` in tests; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
- Test 1 stops being clean.
- SoverN builds no layout at any height for either spec: stop, commit nothing, report SoverN's per-height log with lo, hi, the window and which edge failed (and expansions used).
- `[junctionmap]` over 5 s.

## Gates
0 warnings; `[junctionmap]` green under 5 s; once at the end `"[junction]~[slow]~[mapexport]"` green (report time; `map_export JSON of a hand-built area` fails before this step). One commit: `Change: Junction T map's third crossing decided in projection`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence; members added; suite results with times; BENCH lines; SoverN clean or its violations per spec; which realisation was kept per spec and both scores; stops; `git status --porcelain`, `git log -1`.

When you have reported, stop.
