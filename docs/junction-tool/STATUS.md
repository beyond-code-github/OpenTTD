# Junction tool: status

## 2026-11-10 afternoon: first organic T (O4b1 merged)

- **O4b1** (e825bf1be9): six movements of a T negotiated on flat ground; splits/merges emerge (switch rule: same from-port entering by the same edge, or same to-port leaving by the same edge); bridge heads/portals exclusive (router level `Head`); no head on a port tile. MakeTestJunctionSpec, 44x16: 4 rounds (conflicts 6,6,1,0), 14 routes, occupied 146 / slivers 25 (old generator 190 / 53), 53-83 ms over p = 14/20/26. Checker: connectivity, geometry, bridges clean except "ML bridge is not doubled" (D80 move is O4b2).
- Router today (merged): O4h/O4p/O4n heuristic, D161 approximate pruning, D162 multi-track floor, resource_cost + min_units, flat grids. Benchmark single-track: turn 1.2 ms, hills 4 ms, crossing 7.8 ms.
- **Next: O4b2** (rule feedback, prios, signal pass, D80 doubled-bridge move, checker, throughput, score). Then O4c terrain, O5 timing.

## PARKED 2026-11-10 (engineer rebooting; no agents running)

**Resume here:**
1. Design: DESIGN-O4 revision 3 (D160): all six movements routed by the line router under negotiated congestion, splits/merges emerge from shared track, prios added after, waiting bays/room as router minimum lengths + per-round feedback.
2. Route cost (the 100 ms gate's base). Benchmark `FindJunctionLineRoute: route cost benchmark` ([.bench], run alone: `build-rel/openttd_test "FindJunctionLineRoute: route cost benchmark"`). Medians (ms): O4r before: straight 150, turn 105, obstacle 107, hills 108, no-route 151. After O4h (merged 5ae79dcddd): straight 0.14, straight_2 0.22, turn 31, turn_2 23, obstacle 39, hills 35, hills_obst_2 17, no-route 146.
3. **WIP branch `wip-o4h2` (095dcd9d7b, pushed, not merged):** Manhattan distance (track passes between tiles only through edges) + terrain once per tile for the whole line (O4h's per-track terrain could overcount shared diagonal tiles). Single track: turn 3.0, obstacle 18, hills 9 ms; two-track got slower (straight_2 27, turn_2 31) because terrain-once is loose for axis lines. **Fails** `[slow] the route is as cheap as an uninformed search`: informed finds CHEAPER routes than h=0 (251 vs 259, L_R/LL_RR), so key-based pruning in the line router is not exact (path-dependent state in the key/legality). Next: a brief to find why h=0 returns a costlier route (key vs path_used / D110 closing), then the per-expansion hot spots (string Key via fmt per push and pop, path_used rebuilt per expansion, `used = path_used` copied per successor, node copied per pop) and a fast fail for no-route.
4. Then O4n1 (negotiation hook), O4n2 (minimum lengths), O4s (prio joins off one row), O4b.
**Merged today:** O2 tunnels (49a46ffb1c), O4r benchmark, O4h heuristic. Audit (Fable, ~$1-3) findings folded into revision 3 (D160 notes 1-3 superseded).

## 2026-11-10: O4 redesigned (tokens + negotiation), work started

**Design:** DESIGN-O4.md revision 2 (D157 tokens and negotiated routing, D158 prio bundles routed, D159 prio-or-not by throughput). Revision 1's backtracking is replaced.
**Merged:** O2 tunnels in the router (49a46ffb1c; 391/391). Follow-ups: a prefix ending in `T<n>` isn't treated as after-bridge, and prefix tunnels keep z 0 (only matters if prefixes carry tunnels); JSON writes `"z"` only for tunnels with z != 0 (absent = 0, keeps the golden test).
**Audit (Fable, ~$1-3):** 13 findings; accepted: lift spans and gaps enumerated in layer 1, route memo + admissible bound, measure one route first, per-track-bit resources + hug reward, minimum lengths for waiting room/bays, D113 in router, D148a phase with bridge blocks, D159 decided before search, O4n in two steps. **Waiting on the engineer:** A straight branch trunk (p, L searched) as first cut; B main-line prios as fixed strips, bent prios deferred until the signal pass supports them; C crossing assignment enumerated up front, negotiation kept inside each case for packing.
**Next:** after the audit, O4m (anchor layer measurement, no routing) and O4n (router: per-resource extra cost + minimum length) briefs; then O4b. T1b still listed below.

## PARKED 2026-11-09 late night (engineer to bed; no agents running)

**Resume here, in order:**
1. **O4a merged** (see git log: "Feature: Junction line router may start from any of several pieces"; 385/385 suite).
2. **O2** tunnels in the router (briefs/O2-router-tunnels.md, ready; O4a and T1 merged: start here).
3. **O4b** search brief to write from DESIGN-O4.md (runs beside O2); **T1b** checker/ghost tunnel fixes (listed below).
4. Morning deliverable for the engineer: per-generation timings vs 100 ms, compactness vs today's T (190/53 slivers) and coop numbers, ASCII renders of the first organic layouts.
**Scratchpad** backed up to OneDrive `scratchpad-2026-11-09/` (ptest.sh, compact/metric.py, s2fm2). After a reboot: copy ptest.sh back into the new scratchpad and fix the briefs' ptest path.
**Engineer decisions tonight:** D151 (100 ms; style 0 removal on hold), D152-D156 (organic plan, ramps, tunnels, compactness = occupied + slivers with coop as benchmark not ceiling, main line lift by money and tried first). Stations: Station window check still pending.

## Snapshot (2026-11-09 night): DIRECTION CHANGE to organic junctions

**Resume here.** Plan: `PLAN-organic.md` (D152-D154). The engineer: place a junction on ANY terrain, slope/bridge/tunnel/terraform as needed, cheapest and most organic; nothing canned, the layout must emerge from rules + costs + search ("emerge from the maths"); keep it plain (small search space); hover target < 100 ms per generation (D151; I had mis-recorded it as < 1 s). Compactness (D153): surface tiles + sliver tiles (free tiles in no 2x2 free square); coop hubs leave 2-5% slivers, our current T 28%; enclosed pockets are normal in coop hubs (22-39%) and weigh little; coop is a benchmark, never a ceiling. Evidence: `RECON-organic.md` (13 pieces from 27 images; §6 game rules: bridges never cross, tunnels cross at different depths; §7 compactness numbers; script `scratchpad/compact/metric.py`).

**Merged tonight:** O1 heights in the line router (5634729668); C1 compactness measure (a51a783e13; Make T: 190 occupied, 34 enclosed, 53 slivers); T1 tunnels in layouts (1a166eb170: JunctionBridge.tunnel, build step via CmdBuildTunnel, PriceTunnel, JSON, ASCII `O`, rig GS; game side and GS compiled only, not run).
**Merged:** O1b (8c5bede0ca, D152b: track along an incline is always a ramp; start pieces and bridge heads on such tiles are unusable). **Running:** O4a `.claude/o4a` (briefs/O4a-router-starts.md: a set of start pieces). Next on the router: O2 (brief ready) once O4a merges; O4b search brief to write (can run beside O2).
**T1 follow-ups (T1b, before O4 uses tunnels):** `CheckBridges` (junction_rules.cpp ~476-540): for tunnels use max tunnel length instead of `bridge_span_ok`; the "tile shared with another bridge" check must not flag a tunnel against a bridge or a tunnel at another depth (needs O2's tunnel z); D80 doubling and head checks stay. Ghost draws tunnel portals as bridge heads (`JunctionGhost::Set`, `DrawBridgeHeadGhost`) and may mark them bad when no bridge type fits: needs a portal case.
**Router chain (same files, in order):** O4a (line router takes a set of starts; brief to write from DESIGN-O4.md) -> O2 (tunnels, brief ready: briefs/O2-router-tunnels.md; T1 now merged).
**O4 design:** DESIGN-O4.md (place tool kept; main lines fixed by cursor, routed straight on terrain; branch position searched; four movements routed between rule windows; crossing a main line three ways incl. lifting the ML onto a doubled bridge; backtracking with incremental checks, branch and bound on occupied tiles; known limit noted). Open for the engineer: penalise lifting the main line beyond money?
**Then:** O4b search (parallel with O2 once O4a lands), O3 portal mounds, O4c terrain, O4d behind GenerateJunction, O5 100 ms gate, O6 rig; attach mode later.
**Junction (old path, frozen):** S2f2/S2f2b merged (D148/D148a). Timings at b04736187e: clear-spot test 2.25 s (0.58 s without style 0). Style 0 removal on hold (only style 0 builds Pinned TL6 branch_gap 0). No more work on the template generator.
**Painter:** LP6 bridges (b04736187e, D149) and LP7 waypoints + frozen prefix (40c379395a, D150) merged. O1/O2 are the painter's heights/tunnels too. LP10 (line to build steps/signals/cost) and LP9 (corridor snapshot, after O1) next.
**Stations:** waiting on the engineer's Station window check (play build has it: `.claude/play/build-play/openttd -n 127.0.0.1:3989`), then ST6 (separate rig), ST7.
**Crash under load: hardware**, BIOS update deferred (below).
**Memory added:** hover-target, simplicity-feedback.

### Crash diagnosis (kept)
i7-14700KF, microcode 0x11D, Gigabyte Z790 AORUS ELITE AX BIOS FG (2023-09-12): Raptor Lake instability. Evidence: kernel log RIP mid-instruction at the same hot router branch in several dumps; faults on valid addresses; sanitizers clean. Plan when the engineer takes it on: Q-Flash FO from USB (check FO notes for an ME prerequisite), load defaults; then confirm microcode >= 0x12B (registry `HKLM:\HARDWARE\DESCRIPTION\System\CentralProcessor\0` "Update Revision") and re-run the load loop in `.claude/crash` (`ptest.sh build-rel/openttd_test "[junction]~[slow]" 24`, 6+ rounds). Dumps: `C:\Users\micro\AppData\Local\Temp\wsl-crashes`.


## Handover snapshot (2026-11-09, before compaction)

**Direction (D119-D131):** simulation-driven design; evaluator judges, rig is final judge. Ultimate goal D124 (human + LLM building at Buborough scale). Plan: `PLAN-sim-driven.md`.

**NEXT: S2 per `PLAN-S2.md`.** Merged 2026-11-09: S2d solver seam (719f7d7d77), S2a stubs D115a (07e9f79421), S2b D117 a+b with D117b (6ef853bf27); LP1 notation groups D142 (6844190bd0), LP4 line router + LP3 step (49d3eca94a; follow-up for LP5: heuristic not admissible when track 0 is a turn's outer track, re-push keeps routes valid but maybe not shortest; use one track's distance). S2e merged (e2915d7e39): test spec 211 tiles (42x16) -> 193 (41x14), D118 stagger, k_w 1, k_sb 4, 'B' after a half piece closes the S-bend; hover cases 1.4-3x faster. Crash under load DIAGNOSED 2026-11-09 as hardware, not code: kernel log shows RIP in the middle of a jne (0f 85 4a fa <ff> ff) at the same hot router branch in several dumps, plus faults on valid stack/heap addresses; CPU i7-14700KF (Raptor Lake instability), microcode 0x11D, Gigabyte Z790 AORUS ELITE AX BIOS FG (2023-09-12), i.e. before Intel's 0x129/0x12B/0x12F fixes. Engineer to update BIOS; re-run the load loop afterwards. Dumps: C:\Users\micro\AppData\Local\Temp\wsl-crashes (lldb-18 reads them). Previous notes: SIGSEGV/SIGILL in generator tests, ASan stack = corrupted priority_queue in Router::Run (junction_router.cpp:493) on a JunctionParallelFor worker; TSan/ASan otherwise clean; BestCostTable and MoveText read OK; seen at only 6 test jobs too (raised plane, 0.5 s). Next: ASan with -D_GLIBCXX_SANITIZE_VECTOR + detect_stack_use_after_return, fixed-load threaded vs max_threads=1. Worktree .claude/crash has the sanitizer builds. Then S2f (remove the sweep loops for style 1, side-by-side footprint/timing; style 0 decision: no clean style-0 candidate on either test spec), S2g (hover timing on the rig). Painter next: LP5 (terrain + obstacles + heuristic fix + D143 narrowing cost). D145 lane geometry measured (RECON-lane-geometry.md). Stations: waiting on the engineer's Station window check (6 points), then ST6. Follow-up: generator tests SIGSEGV only under parallel load (see crash hunt).

**S2 inputs:** with the engineer, from RECON-solver-vars.md. S2 acceptance criteria gathered today: D118 stagger (shifted lane's bridge head one tile closer to the split, one signal tile per leg; today both heads are forced to the same x); no padding beyond the rules (remove S24 `+1`, e_slack/w_slack/bridge_margin become constraints; probe found minimum accepted e_slack=2, w_slack=1); STUB=1 (D115); prio coverage as an input bound (slider instant, no library problem); hover well under 1 s (profile: 70-91% of refine time is A* routing of >=576 sweep candidates per spot, rules ~15% (CheckPrioReach/SettleSignals), prio timing negligible). Probe data: `scratchpad/probe2/` (TL5, D138, 196, prio 15: only the parallelogram style builds, symmetric style exceeds the 10-tile span; at prio 4 symmetric wins by 4 tiles). Engineer's in-game view (image 20): long N->E (~9) and N->W (~14) parallel runs are the strict 15-tile coverage (D108); the "4-tile gap" not reproduced on flat ground (likely the prio extra-track rows) — ask if it matters.
**Open question for the engineer (line painter LP1):** in LL_RR, are same-direction tracks adjacent, underscores only between directions? JunctionLineSpec has one uniform gap; LL_RR/LLL_RR (D135b) need per-direction groups, which extends LP2 geometry. Brief LP1 after the answer, in a hand-made worktree.

**Done today (all on fork/feature/junction-tool):** D137 prio sized from the picked train (17d271456d; D137a rulings): `SizeJunctionPrio` (flat, once per train; returns `JunctionPrioSizing{prio_tiles, error}` since UI1), `GenerateJunction` single pass rejecting ML-delaying layouts unless `prio_strict` false, `junction_export train=... [prio=N]`. D138 design train = Buborough's 2x Mastermind (Electric) + 6x 3rd gen Universal Wagon (Rail): 14194 hp, 1090 kN, 561 t loaded (280 empty), 196 km/h, TL5. Sized coverage: TL4 14, TL5 15 tiles (coop R27: whole acceleration room; merge-block-clear alone ~11). D139 slider "Prio length N of M" (590d9c200c, max = strict, min = TL; below max non-strict, library unused). TM2 (712fb10ad4): train model steps twice per game tick (Train::Tick calls TrainLocoHandler twice); tile results unchanged. Window crash fix (b0da82784c, key recursion). UI1 (a7687d19cc): D141 design speed = min(rail limit, train max) — NUTS rail is unlimited and 300 km/h gave no layout at all; honest sizing errors; window prio display fixes. Rig: R1 `start --nuts`, `train --design`; R2 `speed-trace`; `junction-here` passes the D138 train and `--prio N`. Slow tests tagged `[slow]` (fast tier 150 cases, 13 s via `scratchpad/ptest.sh`). ccache on build/build-rel/build-play (D140).
**Small follow-ups:** the bridge actually priced/built (`PriceBridge`, `GameJunctionStepPoster::Post`, GUI `ComputePlacement`) still uses the fastest-engine rule; pass the spec's reference speed (D141). Debug "map_export JSON of a hand-built area" aborts when run alone (test isolation, road types). "every candidate is structurally sound" segfaulted once under 24-way parallel load (passes alone) — watch. D119 trade-off evaluator (ML delay/h, branch joins/h per slider value) and rig comparison 7 vs 15 tiles still to do.

**Running state:** no agents. Main worktree clean at the commit after a7687d19cc (this STATUS). Worktrees: `.claude/play` (detached at a7687d19cc; build-play has the current binary), `.claude/prof` (read-only profiling, build-prof RelWithDebInfo), `.claude/tm2` removed. NUTS rig: `OTTD_RIG_DIR=$S/ottd-nuts/ OTTD_RIG_PORT=3987 OTTD_RIG_GAME_PORT=3989 OTTD_RIG_BIN=.claude/play/build-play/openttd`, save `ottd-nuts/save/nuts1.sav`; engineer's client joined at 127.0.0.1:3989 in company 1 (console numbering) with train 0 and a line at y=20. Old rig on 3977/3979 still up. Engineer's GUI clients always use his normal config (memory).

**Evaluator (src/junction_throughput.{h,cpp}):**
- S1a (dfc42f13f9): running times, block occupation, headways, bottleneck. Test junction about 10.9 trains/1000 ticks.
- S1b + S1b2 (3c28377ffc, 4b22a45432): queue spillback with hold points (path end, merge signals). Test junction: no ml_blocked; one informational conflict (EIn->EOut held at its merge signal blocks EIn->SOut in block 56). Test 2 lost its tl effect (only the path-end hold remains); restore with a merge-signal bay fixture later.
- S1c + S1c2 (173c444302, 20dea9ad31): prio timing on the real layout; joiner run once, held intervals per block, exact range of ML starts that are delayed; `margin_ticks` = coverage slack, `worst_start`.
- Next: decide the finding above; then S1d (YAPF lane/platform choice, from RECON-yapf.md; judges pathfinder traps D132), path signals in the evaluator (RECON-pbs.md: one hop per reservation, tile-by-tile release, platforms released together, reservation 0-19 ticks after reaching the tile before the signal: use 20 worst case, 10 expected), S1e rig calibration, then S2.

**Recons in docs:** RECON-yapf.md, RECON-pbs.md, RECON-solver-vars.md. Solver-vars gaps to fold into S2: STUB still 3 (D115 says 1, never implemented); both lanes' bridge heads forced to the same x, so D118's stagger is not expressible; bridge_margin/e_slack/w_slack are hedges to become constraints; "first clean wins" ordering.

**Pathfinder traps (D132):** engineer rarely uses them. Known type: the overflow tree (Buborough (71,917)): a multi-piece dead end keeps a YAPF node (ChoiceFollows) where a plain dead end is dropped (DeadEnd aborts), so when no path is found YAPF's best-intermediate-node fallback can send trains into the overflow. Rig test pending: tree vs plain dead end; export at `scratchpad/mapx/bub_overflow_71_917.json`.

**Map data (D128, MX1/MX2):** `map_export`, `map_export_station`, `map_hotspots` console commands, now in Release builds. Fledhattan matched savread on 845 tiles (only signal states differ: live recompute). PSG saves load single-player headless: `XDG_DATA_HOME=$S/mapx/psgrun/data XDG_CONFIG_HOME=$S/mapx/psgrun/cfg build-play/openttd -v null:ticks=5 -g <save>` with commands in `psgrun/data/openttd/scripts/game_start.scr` wrapped in `script <log>`; missing NewGRFs are disabled (engineer's Windows NewGRFs linked read-only). Top hotspots exported: `scratchpad/mapx/psg186_{1,2,3}.json`, `psg219_{1,2,3}.json` (+ `_hotspots.txt`). Next: measure the hubs in those windows.

**Line painter (D112 revised by D133-D136):** click waypoints, ghost routed between consecutive points, terrain-following, nothing built until Confirm, free undo. Waypoint fixes position only; adding one re-routes the last two segments; the router only produces buildable track, failures shown as red tiles; slopes + bridges first; snap to own track ends; notation and gap shared with the junction tool; inner-track curve rule; bridges over own track/roads, clear trees/rocks, never houses; own rail-toolbar button; worker-thread routing with stale drop; no new network command; diagonal spacing 3 strips, 2 allowed when cheaper (priced). LP2 geometry done (f36e9cb6ad); next LP1 notation, LP4 router.

**Stations:** v2 per D120/D121/D122/D127/D129. References REF-stations-*.md. Test case: Chinningpool vs Chinningpool with exit bays (needs path signals + S1d in the evaluator).

**Hubs:** RULES-hubs.md; D123 (MSH), D125 (upgrades), D126 (multi-lane up to LLLL_RRRR).

**Rig:** server and the engineer's client run from `.claude/play/build-play/openttd`; `scratchpad/rig_coop.sh`. The binary was relinked by MX1/MX2 (Release, map commands included); takes effect on restart.

## Where things stand (2026-11-07)

- **Branch:** `feature/junction-tool`, from master at 938fd8fbff. Nothing is committed yet. The whole `docs/junction-tool/` folder is untracked and waits for the engineer's OK to commit.
- **Toolchain:**
  - cmake 3.22, ninja, and clang/clangd 18.1.8 (`clangd` points to clangd-18)
  - g++-13 from the ubuntu-toolchain-r PPA. Clang uses GCC 13's libstdc++; GCC 11's lacked constexpr `std::vector`.
- **Baseline (step 0, done):**
  - Configure: `CC=clang-18 CXX=clang++-18 cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`
  - Builds: `openttd_test` and `openttd` both build with 0 errors and 0 warnings.
  - Tests: `./build/openttd_test` passes 95 test cases (2178 assertions).
- **Decisions:** D1–D39 in `DECISIONS.md`.
- **Plans:**
  - `PLAN-1a.md`: placement, ghost, price, rotate and mirror, build. **Full detail, ready for review.**
  - `PLAN-1b.md`: the T-junction generator. Outline with 6 open items.
  - `PLAN-1c.md`: train physics, prio length and traffic-aware bridges. Outline with 6 open items, including D39.
  - `PLAN-endpoint-draft.md`: the old single-track link plan, kept for the later endpoint milestone.
- **Research files:**
  - `RECON-placement.md`: commands, ghost drawing, bridges, picker windows, settings
  - `RECON-junctionary.md`: wiki and coop terminology
  - `RULES-coop.md`: 65 rules with sources; R47 is superseded by D35
  - `IMAGE-INDEX.md`: 370 image references with context, 279 downloaded
- **Tools in `tools/`:**
  - `fetch_junctionary.py`: OpenTTD wiki Junctionary mirror
  - `fetch_coop_archive.py`: coop wiki uploads from the Internet Archive
  - `fetch_abr.py`: V453000's Advanced Building Revue
  - `index_images.py`: rebuilds `IMAGE-INDEX.md`
  - The downloads live in the session scratchpad, so re-run the tools in a new session.

## 1a progress

| Step | Commit | Result |
|---|---|---|
| 1 API skeleton + outer test | cbfc914136 | Accepted. Outer test red by design (0 == 2000). 96 cases: 95 pass. |
| 2 fixture + build steps | 8e83d9eadd | Accepted. 100 cases: 99 pass (outer still red by design). Agent skipped reading CODINGSTYLE.md; code fine. |
| 3+4 estimate, transforms | 48ae954dbb, 343901b842 | Accepted. Agent stopped correctly on a brief error (bridge-order test passed against stub); test corrected. 113 cases all pass; outer test green. |
| 5 game pricing (worktree) | de4ff005ce → cherry-picked be2c2d3861 | Accepted. Copied bridge_gui formula incl. clearing cost. Merged suite: 117 cases all pass, 0 warnings. |
| 6 drawing seams (worktree step6) | 7861e4d733, 50875f9647, 34afa33084, fbf58e3832 (cherry-picked) | Accepted after 2 justified stops (HasReservedTracks equivalence; bridge helpers + BRIDGE_Z_START). Upstream diff: rail_cmd.cpp +83/−36, viewport.cpp +27, tunnelbridge_cmd.cpp 6 `static` removals. Merged suite 117 cases all pass, 0 warnings. **Engineer in-game check pending** (existing rail/signals/bridges unchanged). |
| 7 ghost store + viewport hook | c7f1532b43, 06a0853f03, 17325e3b48 | Accepted after 1 justified stop (early returns in DrawTileSelection → hook at its call site). Added unnamed file-static MarkJunctionGhostDirty (accepted: avoids duplication). Invariant noted: bridge start is the northern head (transforms keep it; 1b generator must too). 121 cases all pass. viewport.cpp +6/−2. |
| 8 window, hover, tooltip | 940d683d5e, bd9bc335be | Accepted after 2 justified stops (_cur_railtype file-local → passed into ShowBuildJunctionWindow; ini_key test → `{}`). Agent quoted a CODINGSTYLE rule. 123 cases all pass. Upstream: rail_gui.cpp +16/−1, window_type.h +6, rail_widget.h +1, english.txt +12. Known: window keeps its rail type until reopened. **Engineer in-game check running.** |
| 8b opaque ghost, bridge by train speed | 114cc62d51 | Accepted, no stops. From engineer feedback (D44–D46). 123 cases all pass. **Engineer check pending.** |
| 9 build sequencer, callback, retry/cancel | ce6ad158b7, 865b323039, 49ed107692 | Accepted, no stops. Brief corrected the plan: Post must not be called from a callback, so a JunctionSequencer posts one step per tick and CcJunctionStep only reports results. Retry checks money > 0 only (brief was muddled; literal reading accepted). 129 cases all pass. Upstream: network_command.cpp +3/−1. **1a code complete; engineer's full manual checklist pending.** |
| 9b cost line in window | f46837de90 | Accepted, no stops. 129 cases all pass. Finding: STR_JUNCTION_STATUS_* lines in english.txt are one column misaligned (from step 9) — fold into next touch of that block. |
| 9c size cost line | 3ea03cc1ba | Accepted, no stops. UpdateWidgetSize reserves 15 digits (house pattern); english.txt alignment fixed. 129 cases all pass. |
| 9d clearing counted once under bridges | 12dc02c7c6 | Accepted. Deviations (minor): wrote real TestClear instead of stub before red (red used test stub, so still valid); skipped CODINGSTYLE; doc comment got an awkward `@note 6. …` line — reword on next touch of EstimateJunction. 131 cases all pass. |
| 9e shift-click estimate | d89c2dccb8 | Accepted after 1 stop (ShowErrorMessage needs an lvalue CommandCost). 131 cases all pass. |
| 9f button redraw + safeguards | 88556cf56d, 89196b3e6a | Done inline by orchestrator (D52). Found safeguards.h included after code in junction_gui.cpp (from step 7); moved. All junction files checked. 131 cases all pass. |
| 9g opaque invalid ghost, pinned footprint | 1dabd4e496 | Accepted. SetTileSelectSize(0,0) hides the cursor rect (no upstream edit). Agent re-pins after the failure-branch Set (sensible). 133 cases all pass. |
| 9h blue for pieces still to build | (see git log) | Inline (D52/D54). 133 cases all pass. |
| 9i generic blocked message | a2de219ef4 | Inline (D55). |
| 9j/9k outline order | 975e945eaf + next | Inline (D56): ghost sprites → selection → ghost outlines; make_square_red only when unaffordable. viewport.cpp hook now 3 lines. 133 cases all pass. |
| 9l crash fix | 80a0a4ea0e | Inline. 9g's SetTileSelectSize(0,0) tripped `assert(x_size >= 0)` in SetSelectionTilesDirty (engineer hit it in game). Now 1x1. Lesson for briefs: when relying on an upstream function's behaviour, the agent must also read that function's callees/asserts, not just its body. Refinement parked: fully hide the cursor box during a build (needs a highlight-mode change). |

**1a COMPLETE (2026-11-07).** Engineer's in-game checklist passed: ghost, exact cost (matched to the pound), build, failure + Retry/Cancel with pinned outline, Shift-click estimate, blocked-tile message, outline consistency, multiplayer (dedicated server + 2 clients: identical result, no desync). 133 test cases, all passing.

## 1b progress

Plan: `PLAN-1b.md` (steps A–F). Test baseline after A: 143 cases (142 pass, plus the outer test `[!mayfail]` until D).

| Step | Commit | Notes |
|---|---|---|
| A foundations | f6187abe83 | Two correct stops: half-piece turn parity ("LR" impossible, "LFR" works) and an arithmetic slip in the brief (301, not 302). Both resolved by ruling. |
| B rule checker | 44a6d792ca, fa8f2c84b8, ea48ec478b | 177 cases (176 + mayfail). One correct stop: my prio-component definition split rung bits from row bits; ruled that a tile's unused bits form one node. |
| C templates | fe3e55b29d harness; 5f0d7af4a7 reference | The reference now has a hugging N→W (an S-bend parallel to E→S, a vertical, then a turn) and a W prio covering the bridge block with one join. It has streamlined stub-free prio joins with the first join on the merge tile (D75, D79, D81) and signals at every bridge (D82). Checker steps B2–B4 done (365316bbe2, 182 cases). Reviewed in game by the engineer. **Next: doubled ML bridges (D80)**: the outer row conflicts with W→S, the bridge merge sits where N→W turns in, and the prio must link two bridge blocks. Design in the harness, bring options, then B5 checker (two paths per movement in doubled sections, line sync, no prio on the bridge merge), then TEMPLATES-1b.md and step D. |
| B5/B5b checker | 98a66a83f5, d02ced719c | Doubled movements (equal-length lanes), ML bridges must be doubled, lane merges carry no prio, whole-train bays (D83/D88). The doubled reference `reference-LR-TL4-doubled` is clean, with rig exports. 198 cases. |
| D1 router | 3f0a2dc040 | `FindJunctionRoute` (A*, F/L/R/B, curve grammar, crossable tiles, start/goal sets). Zero turn cost gives wiggly detours on ties, so D3b adds a turn tie-break. |
| D2 signal pass | de7bcb3f47 | `PlaceJunctionSignals`, phases 0–4. The doubled reference's bare routes come out clean. Two rulings: reserve the lane-merge-block prio join before the required signals; signal a lane-only one-tile block (three signals in a row on lane B; to show the engineer, since more joiner slack would avoid it). 205 cases. |
| D3a candidate | e77b76daaf | `BuildJunctionCandidate`. After four agent stops, I settled the skeleton by experiment (D90) and finished the step myself. 4/128 candidates clean for the test spec; most others have 1–4 violations. Hidden `[.gen]` sweep test in `junction_design.cpp` prints each candidate's count and the best ASCII. 208 cases. |
| D3b generator | 47938aacb9 | `GenerateJunction`: sweep, early stop, score (cost, open rectangle, area). The outer test is green with no `[!mayfail]`. 210 cases, suite 23 s. About 5 s per call in Debug (D62's 200 ms needs Release measurement plus speedups). `[.gensweep]`: 16/54 specs clean. ml_gap 0 never; TL6 mostly fails; TL4 g1 only with b0/b1. Rig exports: `tools/ottd/layouts/generated-LR-TL4*.json`. |
| E game side | 3f0c847dbc | The window has notation editboxes, a TL stepper, and generates (synchronously, D92), with the left-hand mirror and four error strings. Client settings `gui.junction_*` (NotInSave). 213 cases. Manual checks are listed in `CHECKS-1b.md` for the engineer. |
| F1 rig check | f5f793107a | Test circuit (`[.circuit]`: junction plus router-made turnaround loops plus a depot) and `ottd junction-check`. First run: 302 pieces built, 6 trains wandering all six movements for about 100 days, avg 123–127 km/h, no deadlock: OK. The rig now runs `build/openttd` (the spike snapshot is stale); join with `build/openttd -n 127.0.0.1`. |
| F manual | — | engineer: `CHECKS-1b.md` (use `build-rel/openttd`) |

## Test rig (CLI-first, RECON-mcp.md)

- **Spike done** (108e051a6c, `tools/ottd/`): a stdlib Python admin client, plus GameScript ping/pong, over rcon. Two runs as separate connections, about 40 ms per connect.
- **Gotchas:**
  - `-c` makes the cwd the config dir, so `cd` there first.
  - `openttd.cfg` needs `[version] ini_version = 8`, or `secrets.cfg` is ignored and the admin port never opens.
  - Register for Gamescript updates before sending.
  - rcon screenshot replies are empty.
  - Only `screenshot minimap`/`heightmap` work headless. Real views need a GUI listen server.
- **Rig steps 1–2 done** (cherry-picked onto the branch): the `tools/ottd/ottd` CLI (start/stop/status/rcon/gs/company/build-line/depot/train/orders/go/vehicles/watch/build-layout/sample), the `JunctionTestGS` GameScript, IdleAI for a company, layout JSON (`tools/ottd/layouts/`), per-tick speed sampling with drop detection, and save/resume. Flat ground is verified for seed 3 only.
- **Rig step 3 done:** 128×128 map with 1 town and no industries, `build-layout --find-clear`, `start --scenario` (a hand-made `scenarios/blank.scn` would be the default, with steps in its README).
- **Next for the rig:** layout JSON export from the test binary (with D), then `junction-check` (all six movements, braking, prio hold, ML blocking, deadlock), alongside E.
- **The engineer watches rig experiments:** announce them with `build/openttd-spike -n 127.0.0.1`.

## Next steps

**Autonomous queue** (engineer, 2026-11-08: "I don't mind what order, as long as we have everything in the queue for autonomous running"; D89). One agent step at a time through the agent-orchestration skill. Make reversible calls and record them in DECISIONS; batch questions. Engineer: "Don't wait for me if I'm not around to watch". Announce rig runs (`build/openttd -n 127.0.0.1`) and carry on.

**Done today (2026-11-08):**
- B5/B5b doubled bridges and bays in the checker.
- D1 router, D2 signal pass, D3a candidate (D90 skeleton), D3b `GenerateJunction`.
- D3d robustness round 1, plus NeedsMlGap.
- E game side, F1 rig circuit check (OK).
- D78 T1–T5: terrain router and generator, map snapshot, plane rule, hover worker, hilly rig scan (3/3 generated layouts built cleanly).
- D94 slow bridges.
- R1 turn penalty (dbb5277ae5).
- D78 T6 nearest fit (543775bde5): hilly seed 1 goes from 3 to 5 of 12 generated, all built cleanly.
- TL6 G1 B0 fix (3f2bfedd09); D96 P1 profile; `[.slow]` tag (0e5796a72b).
- Suite: 235 cases (1 hidden as `[.slow]`), all passing. Routine gates: Release suite, about 22 s.

**Snapshot, 2026-11-08 evening (for a fresh orchestrator):**
- **Junction.** Done today:
  - D107 prio reach (G9b) and thinning;
  - D108 no split inside a prio (G9c);
  - the frame grows with prio (FRAME steps 1 and 2);
  - footprint tune (no hedged margins);
  - D110 parts 1 and 2 plus J1 (cf26c9ee90): separate e/w slack, lane B q from 0, G9c without its extension, the D113 rule "split in a bridge's block", the G1 tracer allowing a corner into a bridge head, the router forbidding a bridge right off a split start;
  - D110 parts 3 and 4 plus the D114 compact-first score (c6b5286e96): SL split first plus one signal tile, shift-back loop.
  The test spec is 24x15, with the W side at 1 plain tile before the near head and the SL merge 6 tiles from the lane merge.
- **Running:** SPEED pass (brief SPEED-pass.md). Target: full generation under 100 ms (it's 3.3 s+ now; the suite takes 930 s). Fail-fast `JunctionRulesClean`, no duplicate checks, G9b memoisation.
- **Next junction steps, in order:**
  1. J1b, D117: one signal may do several jobs. Back-to-back signals at the bridge entry (lane A x20/x21) and exit (x13/x12) in the rig junction; also find why the tight gap-0 entry is rejected.
  2. D111: prios bend with the ML.
  3. D115 one-tile stubs, plus the D58 crop of 1.
  4. The AUDIT-assumptions.md removal plan (68 OURS items).
  5. Finish D109 (crossing clusters; WIP patch in scratchpad/d109).
  6. M4: prio length from the picked train (`JunctionPrioLength`), then the rig round.
- **Stations:** ST2–ST5 merged (rules, generator, rig export and build, Station window). Next: ST6 (rig drop traffic), ST7 (attach to a line end). The Station window is untested in game.
- **Line painter (D112):** recon (RECON-linepainter.md) and LP2 (multi-track geometry, f36e9cb6ad) merged. Open for the engineer: is L_R 3 diagonal steps apart on diagonals right? Plus the 12 recon questions. Next: LP1 (notation), LP4 (line router on flat ground).
- **The rig:** the engineer plays co-op in company 0. Use `scratchpad/rig_coop.sh` (launches his client before `ottd company`). Ask him to quit normally before restarting, so his settings survive. Debug console commands (`junction_export`, `station_export`, `junction_train`) need the Debug server binary `build/openttd`. The play build (Release) is in the `.claude/play` worktree, `build-play/openttd`.
- **Engineer principles:**
  - margins come from the maths and the checker, never hedging;
  - verify the actual built layout before claiming a fix;
  - say "split", not "switch";
  - the simulator and throughput are the judges (D116).

**Next, in order:**
1. **D96 near-real-time hover** (engineer priority; `PLAN-D96.md`): P1 profile done (the router is 91–95%, and its `best` map about 52%); P2 router lookups done (236119c3fd: S1 1.11→0.48 s, S3 1.79→0.79 s, golden identical); P5 library and fit done (d37a9b5ebf: 37 entries, build 0.72 s, fit 64 µs); P6 two-tier GUI done (d27d205186: the ghost follows the cursor from the library; instant tier about 2–3 ms on the Debug server; hilly scan with `--instant` gives 5/12, the same as the full near search, all built). P3 router inner loop (a742b4dad9) and P4 parallel sweep (727ec3d763): S1 0.35 → 0.085 s (1.11 s before P2), library 0.56 → 0.094 s, TSAN clean, golden identical. Left: then P3 router inner loop, P4 fewer searches and a thread pool, P7 fast reject.
2. **D97 drag-style signal distance:** done (6919168d55). The fill is the drag walk (minimise gaps, from the downstream anchor) and the checker uses 192/128 weights. Fixed-distance mode is a follow-on.
3. **1c physics and traffic** (PLAN-1c, RECON-1c 530616bf5e, D98), moved up. R1 rig traffic heatmap done (df9a8c896a, `ottd traffic`), on the test circuit with wandering empty trains:
   - at 6 trains, almost no waiting;
   - at 12, trains wait on the westbound ML's lane B before the lane merge (about 40% of samples at two tiles) and on the W row before it (12%);
   - at 20, the whole circuit saturates, loops included, with no deadlock.
   Next: R2, per-route spawning (orders) with an "ML train stopped" metric and a backed-up-past-split check, then the pure model checked against it. The engineer, watching: "the prios are not long enough for these trains". Prio length knob (57fd3b5e84, `prio_tl`). D99: single-tile blocks are exempt from prio coverage (8682323379); prio 2 and 3 now generate (47 and 51 wide). Circuit harness fixes: drag-style loop signals (1ea246ccea), and the grid carries on across the junction/loop seam (5ce742079f; the engineer spotted the missing signal at map 37,7). 12-train free-roaming comparison:
   - 1×: ML waiting on lane B inside the junction (about 35% at two tiles);
   - 2×: the junction is clear, and the waiting moves to the west loop;
   - 3×: heavy east-loop waiting, plus W ML trains stopping at the entry (about 40%).
   The circuit's own congestion muddies this, so R2 followed.
   - **R2** (39263ad76a): fixed routes via loop waypoints, `--mix ml:6,bw:3,be:3`, with ML stopped samples inside the junction box.
   - ML stopped in the junction: prio 1×: 603 (2.0% of ML samples); 2×: 948 (3.2%); 3×: 444 (1.5%).
   - In all three the hottest ML tiles are on the W row at its east entry: just downstream of the W→S and lane splits, before the doubled ML bridge, and at the entry tile.
   - Hypothesis, unconfirmed: the bridge block (a bridge can't carry signals) limits ML headway; or W→S queues back up from the S merge, which the closed south loop couples to joiner waiting.
   - Copies: prio 1× at 49,4, 2× at 78,31, 3× at 4,32.
   - **ML-only run (prio 1×, 6 ML trains): 12.6% ML stopped in the junction**, worse than the mix. Every ML train took lane A, so the doubled bridge worked as a single long block. That confirms the imbalance the engineer spotted; D100 (symmetric lanes) follows.
   - **After D100 (442402dcd0) and D99a (340dbf99d7): the ML-only run gives 0 ML stops (0 stop events), against 12.6% before.**
   - D103 bridge margin sweep (0c226b3690): spans 8 → 6. S1 85 → 221 ms.
   - Train model (8a4d67eb26, D36 confirmed at ⅔), prio solver (f8c19c3a45, f218f4bd5a): the sample train needs 21 tiles, about 5 TL.
   - Mixed runs with symmetric bridges, all still with the prio leak (D107): ML stopped 1×: 0.3%, 2×: 1.7%, 3×: 1.2%; joiners wait more as the prio grows. To redo after D107.
   - **D107 prio leak**, found by the engineer ("flips and gives the SL a green"): adjacent ML signals at the coverage edge leave an undetected tile. The prio reach rule plus the fix are running.
   - D107 fix (692fe52904, 643f53112d): prio reach rule G9b plus thinning, a one-segment extension and an N→W margin. D108 (b36dfd69a0): G9c "split inside prio coverage"; prio 1 is clean at 32×14.
   - **D109 stopped:** at prio ≥ 2 the generator's fixed frame (64 wide, hub x 30, search box xO+16) blocks pass-1 routing, and crossings 5–10 apart won't cluster with GAP 14. WIP patch in scratchpad/d109. Next: FRAME recon (running), then a frame refactor so width, hub and bounds scale with the prio length, then finish D109.
   - **Footprint tune (next, after FRAME step 2, before D109):** the engineer: "The build template is still leaving a large gap between the end of the bridge and the prio". Drop the fixed sd+1 margins (D107 3b on the W side, and D108's + sd + 2 on the E side). Sweep the joiner positions from tightest outwards and let G9b/G9c reject leaky ones, so the score picks the tightest safe layout.
   - Queue after the current D110 (1, 2) step: D110 (3, 4), with the SL split first plus a one-tile signal gap or a three-way split, and the shift-back tight to the SL merge; then D111, prio tracks that bend with the ML; then finish D109 (crossing clusters) on the new frame; then M4.
   - D101: use an actual picked train. Next: M3, "pick a train" in the Junction window; then M4, wiring TL and prio length into the generator. the engineer is holding signalling comments until the simulation ("it will all come out in simulation. It may just be down to waiting bay sizing"). Headway, prio length, bays from traffic, D84 presets, the block-occupancy simulation.
4. **D3d robustness round 2:** TL6 is fixed (3f2bfedd09, the search box). TL3 is still no layout: in pass 2 N→E is trapped by E→S in the under-span corridor (hypothesis from the D3d diagnosis).
5. **D78 levers, in D95 order** (fit the landscape first; terraforming is a priced fallback, not the default):
   - first a breakdown of why each hilly no-layout spot fails (height, water or blocked);
   - (b) sloped legs;
   - (c) terraforming as a priced sweep option;
   - (d) ML water bridges.
   Plan each before starting.
6. **Joiner** (`PLAN-joiner.md`): blocked on the engineer's three answers.
6b. **Stations (parallel track,** engineer: "I'd LOVE stations to be on our radar"**):** ST1 is research into coop station rules (`RULES-stations.md`) from the local archive; then a plan sketch. Reuses the router, signal pass, checker, terrain, library and rig.
7. **Follow-ons:** a pathfinder (YAPF cost) model in a later generator version (engineer: "I suspect we will want the pathfinder to feature in a later version of the generator"), for example to check that alternative paths cost the same and to predict train choices; drag fixed-distance mode (D97), multi-lane (D72), option C (widen the gap so the branch bridges over the ML), research the coop prio-over-bridge pattern, the D91 swap (parked until clarified).

**For the engineer when back:**
- **Try it in game:** `build-rel/openttd`, rail toolbar → Junction; the checklist is in `CHECKS-1b.md`.
- **D87:** the route search replaces templates. OK?
- **Lane B:** three signals in a row (D2 ruling). Is that acceptable?
- **D83/D88 wording**, as fixed (tails on bridges are fine).
- **D91:** which pair is "L & R"?
- **Joiner questions** (PLAN-joiner.md): multi-track A/B/C; endpoint picking; J3 in the first milestone?
- **ml_gap 0** gives NeedsMlGap for now (needs a spread, option C).
- **1c (RECON-1c open questions 1–4):** loaded or empty mass for prio sizing (and the live `freight_trains` value or the default)? Design speed: min(engine, rail type), the rail type's max, or capped by bridge speed? The original acceleration model: support it or refuse it? Train picker: engines available now, or all of them? The rest is settled by D98.

**Demo rig:** `tools/ottd/ottd start [--hilly]; ottd company; ottd build-layout tools/ottd/layouts/generated-LR-TL4.json --find-clear`, or `ottd junction-check tools/ottd/layouts/circuit-LR-TL4.json --find-clear`, or `ottd junction-scan --count 12 --seed 1` on `--hilly` (and `-lefthand.json`). Join with `build/openttd -n 127.0.0.1`. The engineer likes to watch: announce rig runs (memory: rig-watch-feedback).

## Roadmap

1. **1a** placement, ghost, pricing and build (done).
2. **1b** T generator (in progress).
3. **Place-time generation** (D78): terrain and obstacles at the hovered spot.
4. **Endpoint solver:** routing to tagged connection points.
5. **1c** train physics and traffic. It may run alongside 3–4 (D89).

The multi-lane follow-on (D72) fits after 1b or 1c.

**Pattern discovery (engineer: "I'd love for this work to result in *new* patterns that no-one has ever thought of before!"):** a later track, after 1c gives trustworthy throughput.
- Loosen the generator's fixed choices: lane shapes, the placement of splits and merges, prio geometry, bridge counts.
- Filter every proposal through the checker plus the prio reach simulator (G9b).
- Rank the survivors by the train model and rig traffic runs, against coop reference layouts.
- Anything that beats the references goes to the engineer for review.

## Ideas parked
- **Prios with a split inside** (engineer: "There may be a way to support prios with splits inside, but I have never figured it out myself"). D108 forbids it for now. The prio reach simulator (G9b, `tools/sigsim.py`) is the tool for trying candidate shapes.

- **Translucent ghost blitter mode** (D57): researched in `RECON-blitter.md`; ~140 additive lines across 11 blitter files, SSE needed to matter. Parked by the engineer.
- **OpenTTD MCP server** (engineer, 2026-11-07: "how cool would an OpenTTD MCP server be"): an MCP server bridging the game's admin port (`docs/admin_network.md`; game state, chat, rcon, GameScript JSON) so Claude can read map state and act in game. Big win for this project: automated in-game checks (build a junction, run trains, watch for braking/deadlock) instead of manual ones. Not yet researched.
- **Coop prio over a bridge** (engineer, 2026-11-08: coop docs have a pattern for a prio across a bridge; not needed now, factor in later). Research it before templates need a prio to cover a bridge block.
- **Option C template** (D85): widen the ML gap locally so the branch bridges over the ML, which leaves no ML bridge to double.
- **Fully hide the cursor box during a build**: currently shrinks to 1x1 (step 9l); hiding needs a highlight-mode change.
- **Cost breakdown** (track / bridge / signals / clearing) in the window, if ever wanted.

- **Automated in-game screenshots for the manual checks.** OpenTTD can run console scripts at game start (`game_start.scr`) and take screenshots. A scripted savegame plus a screenshot might let agents check some visuals themselves, but it couldn't drive the placement tool. Worth a look before 1b's visual checks.
- **Coop savegames** (e.g. `Merging.sav`, `Wsg_0804.sav` in the coop download) could be opened in our build to measure real coop layouts tile by tile, to calibrate the 1b and 1c rules.
