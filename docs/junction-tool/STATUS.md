# Junction tool: status

## Where things stand (2026-11-07, end of session 1)

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

## Next steps

1. **Engineer reviews `PLAN-1a.md`.** Worth a closer look:
   - the test-junction fixture
   - the cursor at the layout's north corner
   - `.` and `,` for rotate and mirror (letter keys are taken)
   - a red outline only on non-flat tiles
   - the drawing-refactor approach
2. **Commit the plan folder** to `feature/junction-tool`, once the engineer agrees.
3. **Run 1a steps 1–9** through Sonnet agents, one per step, checking each handback against the brief. Steps 6–9 end with the engineer's manual check list.
4. **During 1a,** settle the 1b open items, then write 1b in full.
5. **Before 1c:**
   - research the acceleration model
   - decide D39 (traffic level) and the other 1c open items

## Ideas parked

- **Automated in-game screenshots for the manual checks.** OpenTTD can run console scripts at game start (`game_start.scr`) and take screenshots. A scripted savegame plus a screenshot might let agents check some visuals themselves, but it couldn't drive the placement tool. Worth a look before 1b's visual checks.
- **Coop savegames** (e.g. `Merging.sav`, `Wsg_0804.sav` in the coop download) could be opened in our build to measure real coop layouts tile by tile, to calibrate the 1b and 1c rules.
