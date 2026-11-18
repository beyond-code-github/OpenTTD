# Brief S3 (landing): the composer takes every lane offset from the slot layer

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar` at {HEAD}. Commit on `wt-planar`.
- The reference implementation is the throwaway branch `probe-s4e` (fbd1b3c7ec). `git diff wt-planar probe-s4e` is about 240 lines in two files: `src/junction/planar/planar_slots.cpp` and `src/junction/planar/planar_components.cpp`. This step lands that diff for real, minus the probe scaffolding listed under Changes.
- Design: `/home/pete/OpenTTD/docs/junction-tool/DESIGN-tracks.md` §15 (read 15.1-15.3 only). Rulings: `DECISIONS.md` D350, D351, D352 (grep them).
- Time box: 2 hours.

## Read first (once each)
- `CODINGSTYLE.md` (repo root): the sections on naming, comments and braces.
- `git diff wt-planar probe-s4e` (save it to a file, read it once).
- `src/junction/planar/planar_slots.cpp` and `.h` on wt-planar.
- `src/tests/junction/planar/planar_slots.cpp` and `src/tests/junction/planar/planar_components.cpp` lines 1-130.
- `src/junction/planar/CLAUDE.md`, section "Slot layer".
- `src/tests/junction/CLAUDE.md` (how to run tests).

## Tests (write them first, in this order; record each red failure text before writing code)
Outermost first.

1. **New, composer, `[junction][planarcomponents][slow]`** in `src/tests/junction/planar/planar_components.cpp`: `"planar components: LL² o0t1f0s1 lanes stay on their copies' tracks"`. Parse `PlanarLL2Text()`, `MakeTestJunctionSpec()`, call `SolvePlanarMappedVariant(topology, spec, "o0t1f0s1", nullptr, layout, &why)` (as the test "a variant solved with its own shape given back is identical" does), `REQUIRE` it returns true, and `CHECK(layout.area == Approx(2230.25))`. Red today: area 2354.25.
2. **New, slots, `[junction][planar]`** in `src/tests/junction/planar/planar_slots.cpp`, each on both `BuildVariant(PlanarTText(2), 1, 3, 1)` and `BuildVariant(PlanarLL2Text(), 17, 0, 1)` unless stated:
   - `"planar slots: the copy runs on between consecutive doubled crossings"`: for every channel c and every `mid<k>` interval (index iv, 2 <= iv, iv + 1 < size) whose crossings on both sides (`crossings[(iv-1)/2]` and `crossings[(iv+1)/2]`) have `over == c`, each line of c has a `Copy` track there. Check that at least one such interval exists over the two variants together; if neither variant has one, stop and report (the test would prove nothing). Red: no copy track in mid.
   - `"planar slots: a prio join's straight channel keeps the entry gaps"`: for each site s with kind `Join`, `sites[s].prio` and `sites[s].straight != UINT32_MAX`, channel c = straight, n = its line count, e(j) = j == 0 ? 0 : 2j - 1, far_left = `sites[s].side > 0`: in c's last interval, `LineSlot(lines[j]) - LineSlot(lines[0]) == (far_left ? e(j) : e(n-1) - e(n-1-j))` for j = 1..n-1. Check at least one such site exists (T2 has B and E). Red: the gap differs.
   - `"planar slots: a prio fray's straight legs step one slot toward the far lane"`: for each site s with kind `Fray` and `sites[s].prio`, fc its channel, far line = `side > 0 ? fc.lines.front() : fc.lines.back()`: every entry of `slots.links` whose `from` is a line of fc other than the far line has `step == (side > 0 ? -1 : +1)`. Check at least one such link exists. Red: a step differs.
3. **Changed, slots**: `"planar slots: tracks"` loses its "A row beside each lane..." block (rows are not tracks in this step; they return as real tracks in the next step). Replace it with: no interval of any channel holds a `PlanarSlotKind::Row` track.
4. **Re-pinned, slots**: `"planar slots: T2 o1t3f1s0 gap changes"` and `"planar slots: LL² o17t0f1s0 gap changes"` pin values that move by stated rules (the bridge's move onto the copy is no longer charged; the width weight becomes 1e-4; the prio fray and join moves are fixed). After the code is in, set their objective and gap changes to the values the new code produces, and list every changed number old -> new in the report. Do not change any other test's expectation; any other failing test is a stop.

## Changes
### `planar_slots.cpp`
Take these from probe-s4e exactly:
1. In `BuildIntervals`: the `mid` interval between two doubled crossings of the channel holds the copy (the probe's `is_over` line for `iv % 2 == 0`). Remove the Row track push and its `row`/`is_last` code. Keep the `PlanarSlotKind::Row` enum value.
2. The copy rows: when the next interval holds the line's copy, `s(L, iv+1) = s(L, iv)` (one parallelogram); else a charged step from the copy, `s(L, iv+1) - s(copy, iv) - up + dn = 0`, with `up` and `dn` integer, cost **1** each. Do NOT take the probe's `late` weight (`1 + 0.001 * ...`): cost is 1.
3. The prio join entry-gap rows (probe's "Probe S4c" block).
4. The prio fray fixed link (probe's "Probe S4a" block, the `fixed` member of `LinkStep`, and the `links` output using it).
5. The line step objective: skip the step from an interval that holds the line's Copy (probe's "Probe S4e" line). Other line steps cost **1** (not `late`).
6. Width weight: a constant `1e-4`, no environment variable. Comment: width only breaks ties between layouts with equal steps.
7. Remove `#include <cstdlib>` if nothing else needs it.
8. Comments: rewrite every probe comment ("Probe S4a:" etc.) as a plain statement of the rule (for example "A prio fray's straight leg off a non-far lane doubles one slot toward the far lane."). No D-refs, no "probe".

### `planar_components.cpp`
Take these from probe-s4e:
1. `#include "planar_slots.h"`, the `slots` member, `SlotOf`, `IvName`, `Offs`, `SlotBend`; the `BuildPlanarSlots` call in `Build()` with its failure.
2. `Offs` keeps `M.Fail("slot change inside one piece")` but drops the `PLANAR_S2LOG`/`S2SPLIT` print.
3. Drop `LogDiff` and every call to it.
4. `Fray`'s `gaps` parameter (equality from slots) and its caller's gap computation; the `Join` out-gap from slots; `Bundle`'s slot offsets and `SlotBend`s; the core's `offs`/`x_offs`/`post_offs` and its `SlotBend`s; `bundle` lambda returns `M.error.empty()`.
5. Where the probe kept the old constants only to log (`old`, `e` in the core loop), drop them. Remove `JoinEntry` only if nothing calls it after this change.
6. Comments as plain rules; the `slots` member's doc comment: "The slot layer of the variant." No "probe".

### Docs
- `src/junction/planar/CLAUDE.md`, section "Slot layer": the composer now reads it (replace "The composer does not read it yet"); the copy runs through `mid` between consecutive doubled crossings; after the last one a step back from the copy is charged (one per lane spacing), staying is free; the move onto the copy is not charged; prio fray straight legs off non-far lanes are fixed one slot toward the far lane; prio join entry gaps in the straight channel's last interval (e = 0, 1, 3, 5, ...); no row tracks yet; objective = steps, width 1e-4 as tie-break only; the new objective values of T2 o1t3f1s0 and LL² o17t0f1s0. In the composer section (find where lane offsets/`JoinEntry`/fray gap are described), state that lane offsets, fray gaps and join out gaps come from slots and each slot change is an S-bend (an axis piece then a 45-degree piece of length 2|delta|), refused on a diagonal piece ("a slot step on a diagonal piece is not composed"). Decided facts only, no D-refs.

## Gates (in this order, one at a time, never in parallel)
1. Debug: build `openttd_test` in `build`; `tools/junction/jt fast build '[planar]'`. Must pass inside budget.
2. Release: build `openttd_test` in `build-rel`; `tools/junction/jt gate build-rel`. Report failures by name; a failure you did not cause, name it and carry on.
3. Release: the new composer test (1) by name with `build-rel/openttd_test "<name>"`.
4. pv, Release, one at a time: `tools/junction/pv run build-rel T2 --out <S>/s3-T2.txt`, then T3, then LL2, where S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`. Compare each per variant with `<S>/s4e1-T2.txt`, `<S>/s4e1-T3.txt`, `<S>/s4e1-LL2.txt` (the probe's results): built/failed status and area must match on every variant. Expected: T2 28 built, best 862.25; T3 28 built, best 1933.25; LL2 12 built, best 2230.25. **Any per-variant mismatch is a stop**: report the tags, both areas, and do not commit.

## Do not
- Take the probe's `late` weights, the `PLANAR_S2W` knob, `LogDiff`, `S2DIFF`/`S2SPLIT` prints.
- Change the variant bits, candidate steps (`jog`), or anything outside the two source files, the two test files and the CLAUDE.md.
- Touch ports 4077 and 4079, or start a game client.
- Use git stash. Run two heavy jobs at once.
- Delete the probe branches.

## Stop conditions
- Any per-variant pv mismatch against the probe.
- Any test other than the two re-pinned ones changing expectation.
- A new test that passes before the code (it proves nothing).

## Commit
- One commit on wt-planar: `Change: Junction composer takes lane offsets from the slot layer`, body: two lines on what moved (offsets, fray gaps, join out gaps and S-bends from slots; bridge move onto copy uncharged; width tie-break), ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Then `git push fork wt-planar`.

## Report
- References read
- Commit: hash and subject
- Red evidence: each new test and its failure text
- Types and members added or removed: names only
- Re-pinned numbers: old -> new
- Suite results; pv per hub: built, failed, best, mismatches (must be none)
- Stops, anything left undone
- `git status --porcelain` and `git log -1`

When you have reported, stop.
