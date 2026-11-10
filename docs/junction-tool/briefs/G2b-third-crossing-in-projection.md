# G2b: the third crossing decided in projection (D171), best of two projections

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Principle
The map says only that A->S and N->A cross (D171). Projection realises the crossing both ways on the same map and keeps the better score (engineer: "if maps are cheap to produce then we can absolutely try a few different ones to see what projects best"). Each projection is built once and checked once. No other alternatives, no parameter sweep.

## State
- Worktree `/home/pete/OpenTTD/.claude/g2b`, branch `wt-g2b`, head given at launch (feature with G2a merged: the T map's four routed edges use `FindJunctionLineRoute`, phase timings in `JunctionTMapResult`).
- Design: main checkout `docs/junction-tool/DESIGN-O4.md` revision 5 and section "G1"; D171 in `DECISIONS.md`.

## Test speed
`[junctionmap]` under 5 s (Debug). Develop against it only; the wider gate once at the end.

## Read first (once each)
`CODINGSTYLE.md`; `ConstructJunctionTMap` (whole) and its tests in `src/tests/junction_map.cpp`; `src/junction_line_router.h` (`resource_cost`, `JunctionResourceLevel`, `existing`, `blocked`).

## The two realisations
- **NoverS** (today's): N->A goes over A->S. A->S keeps today's column fence (it may not use column xI above row g+3, nor ys-1, ys-2); N->A bridges over A->S where A->S crosses column xI.
- **SoverN** (new): A->S goes over N->A. A->S routes first as today but crosses column xI only on a bridge span: drop the column fence for A->S and give its request a `resource_cost` that returns `100 * spec.prices.rail_piece * 100` (prohibitive) for any `Surface` or `Head` piece on column xI at rows g+2 .. ys-1, and 0 otherwise (BridgeSpan pieces over those tiles are free). N->A then routes from `start_n` straight up the column under A->S's span to its merge window (unchanged window rule). If the line router does not allow surface track under an existing bridge span of the layout, stop and report.
- Everything after (crossing set, skeleton, lanes, B->S, N->B, finish, check) is unchanged and runs per realisation.

## Changes
- `JunctionTMapResult` gains `std::string crossing; ///< "NoverS" or "SoverN": the realisation kept.` and `std::string projections; ///< One line per realisation: name, clean, occupied, slivers, micros.`
- `ConstructJunctionTMap`: per height, project NoverS then SoverN (each: route, skeleton, lanes, finish, check once); keep the clean one with the lower `ScoreJunctionLayout`; ties keep NoverS. If neither is clean at a height where both routed, that is the result (unclean) for the stop condition below; if neither routed, try the next height as today.
- `layouts_built` counts both; test 2's assertion becomes `layouts_built <= 2` per routed height.

## Tests (red first)
1. Test 1 (rule-clean, both specs) stays green.
2. "both crossing realisations are projected": for the test spec, `projections` has two lines, NoverS and SoverN, and `crossing` is the one with the lower score among the clean ones.
3. "SoverN puts A->S's bridge over the N column": project with a test seam that keeps only SoverN (add `std::optional<std::string> only_crossing` as the last parameter of `ConstructJunctionTMap`, default unset): the layout has a bridge whose span covers a column-xI tile, carried by A->S (its heads are off column xI), and no bridge of N->A over A->S. Report whether SoverN is clean for each spec; if it is not, report its violations (rule, tile, message) — that is information, not a stop, as long as test 1 stays clean via NoverS.
4. BENCH lines per realisation with the phase split.

## Do not
No other realisations (main line over/under stays as today), no sweep, no changes to skeleton, finish, signal pass, checker, `GenerateJunction`, `junction_organic.*`, savegame, network or command code. No `std::to_string`/`std::printf` in tests; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
- Test 1 stops being clean.
- The line router cannot lay N->A under A->S's span.
- `[junctionmap]` over 5 s.

## Gates
0 warnings; `[junctionmap]` green under 5 s; once at the end `"[junction]~[slow]~[mapexport]"` green (time). One commit: `Change: Junction T map's third crossing decided in projection`, with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence; members added; suite results with times; BENCH lines; SoverN clean or its violations; stops; `git status --porcelain`, `git log -1`.

When you have reported, stop.

## Amendment 1: the first height from the map (folded in; orchestrator, from G2a's logs)
G2a measured (old router, Debug): heights 6..11 all fail on A->S and cost most of the route time (test spec: route 353 ms over 8 heights; the built height costs 4 ms). For NoverS the map gives a lower bound, derived from the rules G1 already applies:
- A->S must cross column xI between rows 0 and ys (its split is at x <= xI-2, its goal at xO > xI on row ys entered from above); the column fence forbids rows 0..g+3 and ys-1, ys-2, so it crosses at y_c >= g+4.
- N->A starts at (xI, ys), its first piece on ys-1 carries the signal and its near head is on ys-2 (D113, D3d); its bridge over y_c needs a middle tile at y_c, so ys-2 >= y_c+1, giving ys >= g+7.
- ys = H-3 (D115a stub), so H >= g+10.
Add `int JunctionTMapFirstHeight(const JunctionSpec &spec)` returning `spec.ml_gap + 10`, with this derivation in its comment; NoverS starts its heights there. SoverN has no derived bound in this step: it starts at the old `first` (ml_gap + 5).
Test: "the first height is a lower bound": for both specs, with a test seam that starts NoverS at `JunctionTMapFirstHeight(spec) - 1` (add `std::optional<int> first_height` as the last parameter of `ConstructJunctionTMap`), that height's log line is a failure (A->S), so the bound skips only heights that cannot build. Report heights_tried and micros before and after.

## Amendment 2: which router (correction to "State")
G2a did NOT switch the T map to the line router (per success it is 7-8x slower in Debug; the switch is deferred). The T map's edges still use the old router. The line router now supports start/goal on identical existing track, splitting off and merging onto existing track with a curve (commits 8f341d8475, 5a54a047ac).
- NoverS: unchanged, old router.
- SoverN: A->S and N->A are routed with `FindJunctionLineRoute`, because only it has `resource_cost` for "only on a bridge span over column xI". Take the `route_line` lambda from `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/g2/part2_tmap.patch` (it builds the line request from G1's old request: windows, fences as `blocked`, base settings; the W row is not laid and Head on W-row tiles is prohibitive), and add the column-xI resource_cost on top for A->S. B->S and N->B stay on the old router.

## Amendment 3 (orchestrator, after the first G2b stop)
Split into two commits.
1. **Commit 1, first height only** (lands regardless): `JunctionTMapFirstHeight`, the `first_height` seam and its test, NoverS only (no SoverN code yet). Subject `Change: Junction T map starts at the height its rules allow`. Measured in the stopped run: test 461 -> 215 ms, pinned 228 -> 81 ms Debug.
2. **Commit 2, SoverN with its crossing envelope.** The stop showed the missing map constraint (D171: a crossing's envelope): both branch crossings with the main line B must fit under ONE doubled bridge, so A->S's crossing of B's rows must lie within the span the skeleton can bridge. In NoverS the column fence did this implicitly. For SoverN, N->A crosses B's rows on column xI; give A->S's line request, on top of the column-xI cost, a prohibitive `resource_cost` for any Surface or Head piece on the W row or the b row with x outside [xI - D, xI + D], where D = nmax - 1 - 2 * k_head (nmax = the largest n with spec.bridge_span_ok(n), as G1 computes; k_head = the skeleton input's k_head, 1). State this derivation in a comment.
   - SoverN must also fail cheaply when it cannot build: cap each SoverN line request at `max_expansions = 20000`.
   - If SoverN still builds no layout at any height for either spec, keep commit 1, drop commit 2, stop and report SoverN's per-height log (as before). Otherwise tests 2 and 3 as written; `[junctionmap]` under 5 s.
