# G3 R1: bend sizes and fan hands are inputs of build() (DESIGN-G3 §G3.6 v2, D240)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
Today `refresh` sizes the row S-bends and the W/E fans, and picks each fan's hand, by reading the Y solution (`vy`), `port_row` and `fan_m`. The curve-gap routes read the same sizes again from `vy`. After this step:
- one lambda, `measure`, reads those sizes from the solution;
- `refresh` only applies sizes it is given;
- a recording can be given every bend's size and fan hand from outside, as the joint loop will give them (R3).

This is a pure refactor. Today's placements, errors and identity do not change.

## State
- Worktree `/home/pete/OpenTTD/.claude/nodes`. First: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-r1 <HEAD given at launch>`. Its builds (`build`, `build-rel`) are configured with HiGHS. Rebuild `openttd_test` in both before the first run. Leave the untracked files in it alone.
- Other agents work in `.claude/nodes2` and in the main checkout (read-only). Never touch them.
- Output files go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/r1/`.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`;
- `docs/junction-tool/DESIGN-G3-vectors.md` §G3.6 (lines 95-170);
- `src/junction/placement/junction_placement.h`:
  - `JunctionPlacementChoices`;
  - `JunctionSystemBend`;
  - `JunctionPlacementSystem`;
  - `RecordJunctionPlacementSystem`;
- `src/junction/placement/junction_placement.cpp`:
  - `PlaceTrackGraph`'s signature (line ~87);
  - the Elem struct (~200-280);
  - `record_system` (~406-485);
  - `refresh` (~603-632);
  - the curve-gap routes' fan and S-bend reads (grep "if (fan_m\[e\] == 0) continue;", ~870-884);
  - `set_port_rows` and `plan_clearance` (~2228-2285);
  - the fixed path and the rounds (~2287-2370);
  - `RecordJunctionPlacementSystem` (~2609);
- `src/tests/junction/placement/junction_joint.cpp`: the helpers and the test "joint: recordings at other sizes keep their points".

## The sizes (names fixed)
In `junction_placement.h`, after `JunctionSystemBend`:
```cpp
/** The sizes the rule source is given instead of reading them from a solution (DESIGN-G3 §G3.6, R1), indexed by JunctionSystemBend::id. Only bends of source "rows" (a row S-bend) and "port_rows" (a W/E fan) read it; every other bend keeps its sizing from the choices and the rules. */
struct JunctionBendSizes {
	std::vector<int> m; ///< Per bend id, its size.
	std::vector<int8_t> hand; ///< Per bend id, a "port_rows" fan's hand (+1 or -1); the fan's sign against its port row (§G3.6 F4) in the bend's own terms. Ignored for a fan of size 0 and for other bends.
};
```
Bend ids are the ids `record_system` gives: Corner, SBend and Fan elements in the order of `chain[e]` then `chain_copy[e]`, for e ascending. They depend only on the chains, never on a solution.

## Changes in `junction_placement.cpp`
1. **`measure`.** Add a lambda `measure(bool rows_known) -> JunctionBendSizes`, just above `refresh`. It holds every solution read that `refresh` has today, unchanged:
   - a fan's m = `fan_m[e]`;
   - a fan's hand by today's formula from `vy[item_pt[el.index]]` and `port_row[el.slot]`, when m != 0;
   - a row S-bend's m = `rows_known ? |vy[item_pt[el.index]] - vy[el.first]| : 0`;
   - an item-holding horizontal S-bend that is not on a row edge = `rows_known ? |...| : 1`.
   
   It walks the bends in the id order above (the same walk `record_system` does) and fills `m` and `hand` for every id. For bends it does not size, it writes `el.m` and `el.hand` as they stand.
2. **`refresh`.** It becomes `refresh(bool rows_known, const JunctionBendSizes &sizes)`. It does exactly what it does today, except that every value it read from `vy`, `port_row` or `fan_m` comes from `sizes` by the bend's id. The `joiner_div` Corner's `kk` stays as it is (it already equals `fixed->corner` in the fixed path). Keep its `rows_known` parameter only for that Corner.
   - Every call `refresh(x)` becomes `refresh(x, measure(x))`, except in rule 4.
3. **Curve-gap routes (~877-880).** A fan's entry reads `el.m` for `fan_m[e]`, and `el.hand` for the hand it computes from `y_far > y_fan`. A row S-bend's entry reads `el.m` for `|vy[item_pt[el.index]] - vy[el.first]|`, in both the skip test and the size. Its start row `vy[el.first]` and the fan's `y_fan` stay: they are positions, not sizes.
   - If identity moves after this change, STOP. It means the two hands differ somewhere; report the hub and the edge.
4. **The input.** Add a trailing parameter `const JunctionBendSizes *sizes = nullptr` to `PlaceTrackGraph` (definition and every call), and to `RecordJunctionPlacementSystem` (header and definition), which passes it on.
   - It is used only when `fixed != nullptr`.
   - In the fixed path's X phase, the `refresh(true)` call becomes `refresh(true, sizes != nullptr ? *sizes : measure(true))`.
   - If `sizes->m.size()` is not the bend count, fail with "the given sizes do not fit the bends".
   - `set_port_rows` is unchanged; `fan_m` stays the measured value. Nothing else reads `sizes`.

## Tests (`src/tests/junction/placement/junction_joint.cpp`, `[junction][milp]`)
1. **"record: given sizes are what the rule source reads"** (red first). LL_RR x L_R at the seed's choices (as "joint: choices round-trip" gets them):
   - record without sizes;
   - take the first bend of source "rows" with m >= 1. If there is none, STOP;
   - build `JunctionBendSizes` from the recorded bends (m, and hand for every bend), with that bend's m + 1;
   - record again with those sizes;
   - CHECK that the second record's bend has m + 1;
   - CHECK that every X relation tagged with that bend (`bend == id && !nonlinear`) has `c == a * (m + 1) + b`, with at least one such relation.
   - Red: add the parameter first and ignore it; the test fails on the bend's m. Record the message.
2. **"record: the measured sizes given back change nothing"** (guard, written after the change). On T and LL_RR x L_R at the seed's choices:
   - record without sizes;
   - record with the sizes taken from that recording's bends;
   - CHECK that the relations (axis, v, u, c, bend, a, b, flags, reason), the bends, the clashes and the routes are equal.
   - If it fails, STOP and report the first difference.

Every existing test stays exactly as it is.

## Do not
- Touch `build()`'s relations, the occupancy, hug or clearance families, `plan_clearance`, the rounds' logic, the joint model, the hover, or the pipeline.
- Add `t` (G3.4a) or a port-offset input. The offset already comes from the choices.
- Change the D215 push's `fan_m` read (~532).
- Use `pkill`, a broad `kill`, or a bare `git stash`.

## Stop conditions
- Identity or any existing test changes.
- Test 2 fails.
- Anything not covered.

## Gates
- 0 new warnings;
- `tools/junction/jt fast build '[milp]' 10`;
- `tools/junction/jt gate build-rel`;
- `tools/junction/jt identity build-rel`. Another agent builds and runs tests in parallel. If a gate is over budget, re-run it once and report both times.

No CLAUDE.md change (a refactor). One commit on wt-r1: `Codechange: Junction placement takes bend sizes as inputs`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
- references read;
- the commit;
- red evidence;
- members added;
- gate and identity counts and times;
- stops;
- `git status --porcelain` and `git log -1`.

When you have reported, stop.
