# Brief PRIO-diag: why a joiner gets a green while an ML train is inside the prio

You execute this brief and nothing else. This is a diagnosis. You may add test-only code and a debug-only console command in your worktree, but don't change the generator, signal pass or checker. If something is not covered, stop and report. Keep your context small: send output to files and report findings.

## 1. State

- **You run in a git worktree.** Base: `feature/junction-tool` at `{HEAD}`. Configure a Release build in the worktree (Ninja, clang-18, as in `/home/pete/OpenTTD/build-rel/CMakeCache.txt`), and a Debug build if you need the rig (step 4).
- **The finding** (the engineer, watching the rig): "the ML train was well inside the bounds of the prio when the yield happened. it's defintiely broken". An SL (joiner) train pulled onto the ML while an ML train was within the prio's coverage, and the ML train had to stop.
- **The layout:** `tools/ottd/layouts/circuit-LR-TL4.json` (prio 1×, symmetric bridges, D99a coverage). It's built with `[.circuit]` from `src/tests/junction_design.cpp`.
- **Background:**
  - D25, D59, D75, D79, D81 (prio shape) and D99/D99a (coverage) in `docs/junction-tool/DECISIONS.md`;
  - `RULES-coop.md` (prio rules R25–R31);
  - `RECON-path-prio.md`.
  The engineer validated the prio shape in game earlier (D79–D81). Since then these changed:
  - the drag fill (S1, 6919168d55);
  - D99 and D99a (prio coverage exemptions);
  - symmetric lanes (D100, 442402dcd0);
  - the bridge margin (D103).
- **Scratchpad:** `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/prio/`

## 2. Read first

- `src/signal.cpp`: how pre-signals work. That means `UpdateSignalsInBuffer` / `UpdateSignalsAroundSegment`, how an Entry signal's state depends on the Exit/Combo signals of the block it leads into, which direction those exits must face to count, and how a Combo signal counts.
- `src/junction_signal_pass.cpp`, `PlacePrio`: the prio row's shape, its joins, and which signals it places, with their types and facing. Also how the joiner's own signal before the merge becomes Entry (grep "Entry").
- `src/junction_rules.cpp`, `CheckPrio` (G9).

## 3. Static analysis (do this first)

1. **Reproduce the circuit.** Build `openttd_test` in the worktree and render the circuit's junction (the `[.circuit]` ASCII), cropped to each prio. There are two: the E-side one for the N→E merge, and the W-side one for the N→W merge.
2. **Simulate OpenTTD's signal logic on paper, for each prio:**
   - Put a one-tile "train" on each ML tile within the prio's coverage, one at a time.
   - Decide whether the joiner's signal before the merge would be red, using the exact rules from `signal.cpp`: block segmentation, how exits are found from an entry, facing, and combo propagation.
   - Write a small test-only analyzer if it helps. It can reuse `ComputeBlocks` from `junction_rules_internal.h` for segmentation, but the presignal propagation must follow `signal.cpp`.
   - Report, per prio: the coverage tiles, the tiles where an ML train **would** turn the joiner red, and the tiles where it **wouldn't**.
3. **For each "wouldn't" tile, explain the break precisely.** For example:
   - a join with no exit or combo signal between it and the merge block;
   - a prio signal facing the wrong way, so it doesn't count as an exit;
   - the joiner's signal not being Entry;
   - a fill signal splitting the merge block so the entry leads into a block with no exits;
   - an ML signal inside the coverage creating a block that has no join.
4. **Check the generator's own output for the test spec too** (`GenerateJunction(MakeTestJunctionSpec())`, not just the circuit), at SD 2.

## 4. Empirical confirmation (only if the static analysis is inconclusive)
- Add a debug-only console command, `junction_signals <x> <y> <w> <h>`, that prints every signal in the rect: tile, trackdir, type, and state (`GetSignalStateByTrackdir`).
- On the rig, using the worktree's Debug build via `OTTD_RIG_BIN`:
  - build the circuit;
  - start one ML train (`ottd traffic ... --mix ml:1`, or `build_train` plus `orders_via`);
  - when it is within the coverage, stop it (GS `GSVehicle.StartStopVehicle`, or a red-signal trick);
  - dump the signal states and show the joiner signal's state.
- Print `join: build/openttd -n 127.0.0.1` when the server starts, and don't wait for anyone.

## 5. Report

- Per prio: the coverage, the detected tiles and the undetected tiles, with a cropped ASCII (up to 15 lines).
- The root cause or causes, each with the code line in `PlacePrio`, the fill or the checker that produces it.
- **Why G9 didn't catch it:** which G9 clause should have, and the gap in it.
- A recommended fix in 3–5 sentences, labelled as a recommendation, plus a proposed checker rule ("prio reach") that would catch the class of bug.
- Your worktree branch, and whether anything is committed there. Commit test-only and debug-only code there if you wrote any; the orchestrator decides whether to merge.

When you have reported, stop.
