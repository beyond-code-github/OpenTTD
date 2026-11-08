# Junction tool: status

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

**Next, in order:**
1. **D96 near-real-time hover** (engineer priority; `PLAN-D96.md`): P1 profile done (the router is 91–95%, and its `best` map about 52%); P2 router lookups done (236119c3fd: S1 1.11→0.48 s, S3 1.79→0.79 s, golden identical); P5 library and fit done (d37a9b5ebf: 37 entries, build 0.72 s, fit 64 µs); P6 two-tier GUI done (d27d205186: the ghost follows the cursor from the library; instant tier about 2–3 ms on the Debug server; hilly scan with `--instant` gives 5/12, the same as the full near search, all built). Left as follow-ons (orchestrator's call: the feel goal is met, so 1c goes first): then P3 router inner loop, P4 fewer searches and a thread pool, P7 fast reject.
2. **D97 drag-style signal distance:** done (6919168d55). The fill is the drag walk (minimise gaps, from the downstream anchor) and the checker uses 192/128 weights. Fixed-distance mode is a follow-on.
3. **1c physics and traffic** (PLAN-1c, RECON-1c 530616bf5e, D98), moved up. R1 rig traffic heatmap done (df9a8c896a, `ottd traffic`), on the test circuit with wandering empty trains:
   - at 6 trains, almost no waiting;
   - at 12, trains wait on the westbound ML's lane B before the lane merge (about 40% of samples at two tiles) and on the W row before it (12%);
   - at 20, the whole circuit saturates, loops included, with no deadlock.
   Next: R2, per-route spawning (orders) with an "ML train stopped" metric and a backed-up-past-split check, then the pure model checked against it. The engineer, watching: "the prios are not long enough for these trains". Prio length knob (57fd3b5e84, `prio_tl`). D99: single-tile blocks are exempt from prio coverage (8682323379); prio 2 and 3 now generate (47 and 51 wide). Circuit harness fixes: drag-style loop signals (1ea246ccea), and the grid carries on across the junction/loop seam (5ce742079f; the engineer spotted the missing signal at map 37,7). Running: 12-train traffic on the prio 1×, 2× and 3× circuits. the engineer is holding signalling comments until the simulation ("it will all come out in simulation. It may just be down to waiting bay sizing"). Headway, prio length, bays from traffic, D84 presets, the block-occupancy simulation.
4. **D3d robustness round 2:** TL6 is fixed (3f2bfedd09, the search box). TL3 is still no layout: in pass 2 N→E is trapped by E→S in the under-span corridor (hypothesis from the D3d diagnosis).
5. **D78 levers, in D95 order** (fit the landscape first; terraforming is a priced fallback, not the default):
   - first a breakdown of why each hilly no-layout spot fails (height, water or blocked);
   - (b) sloped legs;
   - (c) terraforming as a priced sweep option;
   - (d) ML water bridges.
   Plan each before starting.
6. **Joiner** (`PLAN-joiner.md`): blocked on the engineer's three answers.
7. **Follow-ons:** drag fixed-distance mode (D97), multi-lane (D72), option C (widen the gap so the branch bridges over the ML), research the coop prio-over-bridge pattern, the D91 swap (parked until clarified).

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

## Ideas parked

- **Translucent ghost blitter mode** (D57): researched in `RECON-blitter.md`; ~140 additive lines across 11 blitter files, SSE needed to matter. Parked by the engineer.
- **OpenTTD MCP server** (engineer, 2026-11-07: "how cool would an OpenTTD MCP server be"): an MCP server bridging the game's admin port (`docs/admin_network.md`; game state, chat, rcon, GameScript JSON) so Claude can read map state and act in game. Big win for this project: automated in-game checks (build a junction, run trains, watch for braking/deadlock) instead of manual ones. Not yet researched.
- **Coop prio over a bridge** (engineer, 2026-11-08: coop docs have a pattern for a prio across a bridge; not needed now, factor in later). Research it before templates need a prio to cover a bridge block.
- **Option C template** (D85): widen the ML gap locally so the branch bridges over the ML, which leaves no ML bridge to double.
- **Fully hide the cursor box during a build**: currently shrinks to 1x1 (step 9l); hiding needs a highlight-mode change.
- **Cost breakdown** (track / bridge / signals / clearing) in the window, if ever wanted.

- **Automated in-game screenshots for the manual checks.** OpenTTD can run console scripts at game start (`game_start.scr`) and take screenshots. A scripted savegame plus a screenshot might let agents check some visuals themselves, but it couldn't drive the placement tool. Worth a look before 1b's visual checks.
- **Coop savegames** (e.g. `Merging.sav`, `Wsg_0804.sav` in the coop download) could be opened in our build to measure real coop layouts tile by tile, to calibrate the 1b and 1c rules.
