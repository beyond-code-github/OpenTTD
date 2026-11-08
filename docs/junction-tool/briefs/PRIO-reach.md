# Brief PRIO-reach: prio reach rule (G9b) and adjacent-signal fix (D107)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`. The Release suite passes.
- **Decision:** D107 in `docs/junction-tool/DECISIONS.md`. Read it, and the full diagnosis in `docs/junction-tool/briefs/PRIO-diag.md`. The findings are summarised in D107.
- **Reference implementation:** `docs/junction-tool/tools/sigsim.py` (151 lines). It's a verified Python port of OpenTTD's `ExploreSegment` and `UpdateSignalsAroundSegment`, iterated to a fixpoint, covering:
  - signal tiles in both segments;
  - whole-tile propagation;
  - per-track detection on HORZ/VERT tiles;
  - Exit/Combo counted only when facing out of the segment;
  - Entry red = some Exit, and none of them green.
  Port its semantics exactly. Don't reinvent them.
- **Known failures, at head, for `GenerateJunction(MakeTestJunctionSpec())` in the working (layout) frame the diagnosis used:**
  - the E prio leaves (20,3) undetected (Entry at (24,4) Upper_W);
  - the W prio leaves (15,5) undetected (Entry at (11,6) Left_N).
  Coordinates may shift; identify them by role.
- **Scratchpad:** `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/reach/`
- **Don't run any `tools/ottd` command.**

## 2. Read first

- `docs/junction-tool/tools/sigsim.py`, all of it.
- `src/signal.cpp`: `ExploreSegment` and `UpdateSignalsAroundSegment`, so the port is faithful.
- `src/junction_rules.cpp`: `CheckPrio` (G9), how it finds ML merges with prios and their Entry signal, and `CheckJunctionRules`.
- `src/junction_rules_internal.h`.
- `src/junction_signal_pass.cpp`: `FillSignals`, `PlaceRequiredSignals`, `PlacePrio` (the coverage walk), and `PlaceJunctionSignals` (the phase order).

## 3. Step A: the rule, not yet wired in. Commit separately.

1. **In `junction_rules.cpp`**, declared in `junction_rules.h`: `std::vector<RuleViolation> CheckPrioReach(const JunctionCheckContext &ctx);`, with a new `JunctionRule::PrioReach` enum value if `JunctionRule` is an enum. Mirror how other rules are listed, including any name or string table.
2. **What it checks.** For each ML merge that has a prio (find them the way `CheckPrio` does):
   - Take the joiner's signal before the merge (the Entry).
   - Collect the covered positions: walk upstream from the merge along every ML lane path of that movement through the merge piece, accumulating units, while `covered < 2 · tl · prio_tl` (as the coverage loops do). Include every piece, signal tiles and bridge heads among them.
   - Then also include the next upstream piece beyond the walk if the walk ended on a signalled piece. That is where a held train stands.
   - For each covered (tile, track), simulate a one-tile train there, with sigsim's semantics settled to a fixpoint, and evaluate the Entry. If it isn't red, add a violation `"ML tile in prio coverage does not hold the joiner"` at that tile.
3. **The simulation is a file-static helper** in `junction_rules.cpp`. It takes the layout and returns the settled signal states (green or red per signal) given a set of occupied (tile, track) positions. Comment it with the `signal.cpp` functions it mirrors.

**Tests, A (`src/tests/junction_rules.cpp`):**
- **Red-first outer test:** for the test spec's generated layout at head, `CheckPrioReach` reports exactly the two known undetected tiles (by role: one on the E prio, one on the W prio), and nothing else.
- **Unit test:** a hand-built minimal prio (use an existing CheckPrio fixture if one is clean) has no reach violations. The same fixture with one prio-row signal flipped to face the wrong way has at least one.

Commit A: `Add: Junction checker rule for prio reach`. Don't add it to `CheckJunctionRules` yet.

## 4. Step B: the signal-pass fix, then wire the rule in. Commit separately.

1. **In `PlaceJunctionSignals`**, after phase 3 (fill) and before the prios, add a phase 3c `ThinPrioCoverageSignals`:
   - For each ML merge with a prio geometry, walk the coverage plus one piece beyond it, on every lane of the movement.
   - Wherever two consecutive ML pieces both have signals and at least one of them was placed by `FillSignals`, remove the fill signal. If both are fill signals, remove the upstream one.
   - To know which signals came from fill, make `FillSignals` return the set of (tile, trackdir) it placed, and pass it to phase 3c. That's a signature change on a file-static function, which is allowed.
2. **Wire `CheckPrioReach` into `CheckJunctionRules`.**
3. **Checker spacing rule (rule 4):** removing a fill signal may leave a gap over SD in a legal stretch. If the spacing rule then flags it, add an exemption in rule 4 for gaps inside a prio's coverage, citing D107. Otherwise make no change. Report which happened.

**Tests, B:**
- The A outer test flips: no reach violations for the test spec.
- Generator outer, determinism and clean-candidate tests, and "prios keep their joins at SD 2", all stay green.
- `[.gen]` candidate counts at prio_tl 1, 2 and 3 (`JUNCTION_GEN_PRIO`): report them. If any is 0, stop and report.
- `[.golden]` will change; don't compare it.
- Release suite, all passing.

**Regenerate the circuits:** `JUNCTION_GEN_PRIO=n JUNCTION_CIRCUIT=tools/ottd/layouts/<file> build-rel/openttd_test "[.circuit]"` for n=1 (`circuit-LR-TL4.json`), 2 (`-prio2.json`) and 3 (`-prio3.json`). Commit them.

Also run `python3 docs/junction-tool/tools/sigsim.py` on the new `circuit-LR-TL4.json`, if its CLI allows it. Report that it agrees.

Commit B: `Fix: Junction prios hold the joiner over their whole coverage`.

## 5. Do not

- change `ComputeBlocks` or the other rules' block model (a later cleanup)
- change prio geometry or coverage length

## 6. Stop conditions

- The port disagrees with sigsim on the test spec (a different set of undetected tiles).
- Step B leaves no clean candidates.
- Anything not covered.

## 7. Gates (both commits)

1. `cmake --build build-rel --target openttd openttd_test > reach/rel.log 2>&1`, then the Release suite, all passing.
2. `cmake --build build --target openttd openttd_test > reach/dbg.log 2>&1`: no errors or warnings in the files you touched.

Signing is off; end each commit message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 8. Report

- both commits
- red evidence
- the undetected tiles found by A
- B's result, and the spacing-rule outcome
- `[.gen]` counts
- the sigsim agreement
- suite totals
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
