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
| E game side | — | |
| F manual | — | engineer |

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

**Autonomous queue (engineer, 2026-11-08: "I don't mind what order, as long as we have everything in the queue for autonomous running"; order D89).** Each item is one agent step through the agent-orchestration skill. Make reversible calls, record them in DECISIONS, and batch questions for the engineer.

2. **D3b `GenerateJunction`** (done):
   - sweep height, crossing cost and joiner slack;
   - score by cost, then open space (D76), with a turn tie-break against wiggles;
   - early stop, a candidate cap and a time budget (D62; measured, so Debug may need a test-only budget);
   - `NoLayout` naming the most-failed rule group;
   - the outer test green, with `[!mayfail]` removed;
   - the sweep test (TL 3–6, SD 2–4, gaps 0–3 on both lines, both road sides): each case either rule-clean or a named error.
2a. **D3d robustness** (first round done, e0ecc963e1: lane-B start margin, joiner clearance of TL after the lane merge, straight track off far bridge heads, fill gap count. Every built candidate in the TL3/4/6 × g1/3 probes is now clean; the remaining failures are unbuildable candidates). Still to do: make the failing `[.gensweep]` specs clean or give a named error. First get a per-spec histogram of the failing step and violations (the `[.gen]` style). Likely levers: ml_gap 0 (no gap row; the E→S fence and lane geometry assume one), TL6 (prio coverage and bays need more room, so the height or slack range scales with TL), and wide branch gaps.
2b. **D3c: early branch swap (D91), parked.** As I read it (the outbound line crosses under the inbound on the leg), it adds crossings: N→W×S and W→S×N plus the crossover, against E→S×N→E before. The crossing zone also widened to span 11, so no clean candidate. The code is saved as `docs/junction-tool/d3c-branch-swap.patch`. Waiting for the engineer to say what "L & R" means (the branch pair? the ML pair? the lanes at the hub?).
3. **Rig: a generated layout in game.** The design harness, or a small test-binary mode, exports `GenerateJunction` output as rig JSON. Give the IdleAI company more money. Build it, and announce it to the engineer (rig-watch memory), but do not wait for them to join.
4. **E, game side** (PLAN-1b §7): the Junction window calls the generator.
   - notation and TL in the dialogue, static settings in Settings (D64), the signal-type setting (D68);
   - the left-hand mirror (D66);
   - the error message;
   - generation off the UI thread only if it is too slow;
   - no savegame changes (D69).
5. **F:** the manual checks list for the engineer, and the rig command `junction-check`: trains on all six movements, braking samples, prio hold, queues and deadlock.
6. **D78, place-time terrain:**
   - a map-backed seam for the router: buildable, slope, foundation, clearing cost, water, existing track;
   - terrain rules in the checker (bridge heads and slopes);
   - generation at the hovered spot, within the ghost or hover cache (D63).
   - Research first: a read-only recon of the terrain and slope APIs and of the build commands' slope rules.
7. **Endpoint solver/joiner:**
   - a UI to tag two or more connection points (tile and heading);
   - the router between them, then the signal pass and the checker (connectivity generalised from fixed T ports to tagged ports).
   - Research first: what coop calls joins, and the UI pattern.
8. **1c physics and traffic** (PLAN-1c):
   - the headway model, prio length from braking, bay counts from traffic (item 7);
   - the D84 presets;
   - the in-memory block-occupancy simulation (engineer's idea), with the rules kept as a fast filter;
   - single versus doubled bridges by traffic (D32/D80).
9. **Follow-ons:**
   - multi-lane with balancing (D72);
   - option C (a widened gap with the branch over the ML; D85);
   - research the coop prio-over-a-bridge pattern.

**For the engineer when back:**
- **D91 swap:** which pair is "L & R"? Swapping the branch pair as I built it adds two crossings (see 2b).
- **Signal pitch:** the checker allows SD empty tiles between signals (a 3-tile pitch at SD=2), while in-game drag at density 2 gives a 2-tile pitch. The fill now matches the checker. Keep this, or tighten both to the drag pitch?
- **ml_gap 0 (LR notation):** the W prio has no room inside (row −1 of the W track is the E track) and the joiner's side is blocked, so LR needs a local ML spread (option C territory). For now: a named error.
- Confirm D87 (route search instead of templates).
- Rule on lane B's three-in-a-row signals.
- Confirm D83/D88's wording.

**Demo rig:** `tools/ottd/ottd start; ottd company; ottd build-layout tools/ottd/layouts/reference-LR-TL4-doubled.json --find-clear` (and `-lefthand.json`). Join with `build/openttd-spike -n 127.0.0.1`. The engineer likes to watch: announce rig runs (memory: rig-watch-feedback).

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
