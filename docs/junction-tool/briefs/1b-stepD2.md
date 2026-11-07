# Brief 1b step D2: signal and prio pass (`PlaceJunctionSignals`)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`.
- **Plan:** `docs/junction-tool/GENERATOR-1b.md`, step D2.
- **Decisions to read** in `DECISIONS.md`: D74, D75, D79, D81, D82, D83, D85, D87, D88.
- **The checker** (`src/junction_rules.cpp`) is the specification. This pass must produce what it accepts. Where this brief and the checker disagree, stop.

## 2. Read first (once, with offset/limit)

- **`src/junction_rules.cpp`:**
  - the helpers from `SameTile` to `UsedTracks`
  - `SwitchEventsOf`
  - `IsSignalledPiece` and `IsLegalSignalPiece`
  - `JoinerSignals`
  - `CheckSignals` sections 3–4 (its `nearest` lambda)
  - `CheckWaitingRoom`'s bay block
  - `ComputeBlocks` and `BlockOfPiece`
  - `CheckPrio`: the joiner search, the join table use, the coverage loop and the prio signal clause
  - `CheckJunctionRules`
- **`src/junction_route.h`**
- **`docs/junction-tool/layouts/reference-LR-TL4-doubled.design`:** the route lines are your acceptance input
- **`src/tests/junction_rules.cpp`:** the helpers at the top

## 3. Changes

### 3.1 Share checker helpers
- Create `src/junction_rules_internal.h` (header style as `junction_rules.h`, add it to `src/CMakeLists.txt`).
- Make these functions in `junction_rules.cpp` non-static, with their doc comments moved to that header, and change nothing else about them:
  - `TracksAt`, `FindBridgeByHead`, `UsedTracks`, `SwitchEventsOf`
  - `IsSignalledPiece`, `IsLegalSignalPiece`, `JoinerSignals`
  - `ComputeBlocks`, `BlockOfPiece`
- Move the `SwitchEvent` alias into the header.
- Add one new shared function there, and use it in `CheckJunctionRules` in place of its inline list:

  ```cpp
  /** The six movements of a T junction. */
  const std::vector<std::pair<JunctionPortRole, JunctionPortRole>> &JunctionTMovements();
  ```

### 3.2 New `src/junction_signal_pass.h/.cpp` (add to `src/CMakeLists.txt`)

```cpp
/** Add signals and prios to a layout of tracks and bridges, in the canonical frame. */
void PlaceJunctionSignals(JunctionLayout &layout, const JunctionSpec &spec);
```

**Setup:**
- Build the context with `MakeJunctionCheckContext(layout, spec, FindJunctionPorts(layout), JunctionTMovements())`.
- Path order: ML paths (EIn→EOut, WIn→WOut) first in context order, then the rest in context order.
- Placing a signal means pushing `{tile, trackdir, type, SignalVariant::Electric}`. Skip a piece that is already signalled.
- Re-trace after each phase, because placed tracks change the paths' surroundings. Signals don't change the paths themselves, so traced paths can be reused within a phase.

**Phase 1, required signals** (type `spec.signal_type`), path by path:
1. Before every split and every merge: the nearest piece found by the same scan as `CheckSignals`'s `nearest` (back from index − 1). After every merge: the same scan forward from index + 1.
2. Before every bridge's near head and after its far head, using the same scan.
3. Every signal returned by `JoinerSignals` becomes `SignalType::Entry`.

**Phase 2, bays (D88):** for every queue merge, run the walk exactly as `CheckWaitingRoom`'s bay block does, and place a signal on each boundary piece.

**Phase 3, fill:**
- For each path in order, the anchors are its signalled pieces plus its first and last piece.
- For each consecutive anchor pair (a, b):
  - D = the units of pieces a+1..b, and n = ceil(D / (2·sd)) − 1.
  - For i = 1..n, target t = round(D·i/(n+1)) units back from b.
  - Place a signal on the piece strictly between a and b, other than the first or last piece of the path, that passes `IsLegalSignalPiece` and whose distance back from b is closest to t. On a tie, take the one nearer b. If no piece qualifies, skip it.

**Phase 4, prios.** For each ML path p and each merge event at index j on it (lane merges excluded, as `CheckPrio` excludes them):
- **Skip** the merge if its tile already holds a join piece (h1) from an earlier lane's pass.
- **Side:** the joiner's piece before the merge tile is offset from it to one side of p's axis. The prio goes on the other side. Take h1 and h2 from D75's table.
- **Blocks:** `ComputeBlocks(layout)`. The required blocks are exactly those `CheckPrio`'s coverage loop would visit, from j upstream along every ML path of p's movement that contains the merge piece. The merge block is excluded.
- **Join tiles:**
  - The merge tile M gets h1, and its prio-side neighbour gets h2 only (D81).
  - For each required block, walking upstream along p from j: the first piece of p in that block whose tile holds exactly p's axis track, carries no signal, and whose prio-side neighbour holds no track. That tile gets h1, and its neighbour gets axis + h2.
  - If a required block has no such piece on p, abandon this prio: add nothing for it.
- **Row:**
  - Every prio-side tile between M's neighbour and the farthest join's neighbour gets the axis track.
  - If any of them already holds track, abandon this prio.
- **Prio signals:**
  - Between each pair of consecutive joins, put one signal on a row tile strictly between them: the tile next to the downstream join.
  - It faces upstream (`ReverseTrackdir` of p's trackdir). The farthest one is Exit and the others are Combo.
  - If two joins are adjacent, abandon this prio.
- **Abandoning** removes everything this prio added. The checker then rejects the layout, and that is the generator's business.

## 4. Tests: new `src/tests/junction_signal_pass.cpp` (register it), tag `[junction][signals]`

Outermost first. Record each red message against a stub `PlaceJunctionSignals` that does nothing.

1. **Acceptance:**
   - Rasterize the 7 `route` lines of `reference-LR-TL4-doubled.design` (copy them as `JunctionRoute` literals) into a layout with no signals and no `track` lines.
   - Run `PlaceJunctionSignals` with `MakeTestJunctionSpec()`, normalise, and `REQUIRE(CheckJunctionRules(...).empty())`.
   - On failure, print the violations with `INFO`.
2. **Required signals:** on a split line and a merge line (reuse the shapes of the checker's `SplitLayout`/`MergeLayout`, copied), a signal sits on the nearest legal piece before the split, before the merge on both incoming tracks, and after the merge.
3. **Bridge signals:** a line over a bridge has signals right before the near head and after the far head.
4. **Entry:** at an ML merge, the joiner's signal is Entry, and a lane merge's signals are not.
5. **Bays:** a queue with L ≥ 2B gets signals at the D88 boundaries.
6. **Fill:** on a straight line, every gap is ≤ 2·sd units, and no gap is smaller than sd units unless it touches a required signal.
7. **Prio side and pieces:** for each of D75's four table rows, a small ML merge whose joiner comes from one side gets the prio on the other side, with the table's h1/h2 and D81's merge-tile piece.

## 5. Do not

- change any checker behaviour; `junction_rules.cpp` changes are limited to §3.1
- add sweeps, scoring or retries; those are D3
- touch the game side

## 6. Stop conditions

- The acceptance test can't pass and the cause is the pass following this brief. Report the violations and which phase placed the offending signals.
- The checker contradicts this brief.
- Anything not covered.

## 7. Gates

1. `cmake --build build --target openttd openttd_test > /tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/d2_build.log 2>&1`. There must be no errors or warnings in junction files.
2. Full suite. The baseline is B5b's count; everything passes except the `[!mayfail]` outer test.

Commit message: `Add: Junction signal and prio pass for the generator`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 8. Report

- references read
- commit
- red evidence
- functions added or made shared
- suite totals
- the acceptance layout's ASCII (`RenderJunctionAscii`, printed through `INFO` or a temporary print you remove before committing)
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
