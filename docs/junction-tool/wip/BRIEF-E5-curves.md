# BRIEF E5-curves: curve spacing rows in the drawing program (Sonnet)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head: given in the launch message (it includes the bays step, `PlanarDrawOptions::bays`). Release build dir `build-rel`; build with `-j 4` only.
- Source: DECISIONS D419, D420 (3): curve spacing is the second missing family in the drawing program (E5's LP oracle). Rule: SPEC-slots-lengths §8.1 row `curve` and §8.2 "Curve rows": Shape's bends per route path, consecutive same hand >= `curve_same`, opposite >= `curve_opposite`, k = 2 and 3 apart >= `curve_window[k - 2]`. §10.3: no curve rows on flex inserts' bends (their length still counts as travel). Already written in the Slots probe; this step ports it.

## 2. Read first (once each, with offset/limit)
- `src/tests/junction/planar/planar_slots_probe.cpp` lines 1474-1533 (section 12, the `Bend` walk and the rows). Port only these lines; the `Marker` rows after them (lines 1534-1567) are Slots' step binaries and are NOT ported.
- `src/junction/planar/planar_structure.h` lines 44-58 (`PlanarRuleFields`: `curve_same`, `curve_opposite`, `curve_window`).
- `src/junction/planar/planar_draw.cpp`: the bays block (`options.bays`) and its `once_row`, which this step reuses; lines 285-330 (flex groups: `v.tracks[t].flex` holds the middle piece of each flex pair; `v.tracks[t].steps` the option A probe's used-step middles).
- `src/tests/junction/planar/planar_draw.cpp`: the bays tests and their `DrawnBays` helper (the pattern to follow).

## 3. Tests (outermost first; red before green), tag `[junction][planar][planardraw]`, placed after the bays tests
1. **"planar draw: curves keep their spacing along a route"** (outer).
   - `ShapeFirst(PlanarTText(1), 1)`, `.clearance = false`, solved with `.curves = false` and with `.curves = true`; REQUIRE both feasible.
   - Helper `static std::vector<std::pair<double, int>> DrawnCurveGaps(const DrawCase &c, const PlanarDrawResult &r)`: per route (prio included), per path, walk the tracks' pieces in order with a running travel sum of `r.lengths[t][i]`; a piece that is a flex middle (in `c.v.tracks[t].flex`) or a step middle (in `c.v.tracks[t].steps`) only adds its length; every other piece whose heading differs from the last such piece's heading is a bend at the running sum before it, right-handed when `PlanarModel::Rot(last, heading) > 0`. For each bend i and the next up to three bends j (k = 1, 2, 3 apart), return (travel_j - travel_i, least) with least = `rules.curve_same` or `rules.curve_opposite` by hands for k = 1, `rules.curve_window[k - 2]` for k = 2, 3.
   - REQUIRE at least one gap. CHECK the drawing without curves has some gap below its least (premise; if it fails, STOP and report the gaps), and every gap of the drawing with curves is >= its least.
   - Red: add the option field first (4 a) with no rows; record the failing CHECK.
2. **"planar draw: elastic mode names a short curve"** (inner): as the bays elastic test, with `.curves = true, .elastic = true, .elastic_families = {"curve"}` and `.fix_points` = the `.curves = false` drawing's points, `.fix_window = 0`; CHECK a `stretched` tag starts with `"curve "`. Red: none before the rows.

## 4. Changes
a. `PlanarDrawOptions`: `bool curves = false; ///< The curve spacing rows (SPEC-slots-lengths §8.1, §8.2, D420): Shape's bends along every route path keep curve_same, curve_opposite and curve_window; flex and step inserts count as travel, not bends (§10.3).` Directly after `bays`.
b. `CheckPlanarDrawable`, right after the bays block, when `options.curves`: the probe's section 12 walk over every route (prio included) and path, with
   - travel terms `{length[t][i].variable, length[t][i].coefficient}` per piece, as in bays;
   - flex middles and step middles skipped for bends (length added only);
   - a heading change of other than one eighth between consecutive non-middle pieces: `return unsupported(fmt::format("route {} turns by {} eighths at track {}", ...))` instead of the probe's throw;
   - the rows exactly as the probe's i/j loop (`mand` up to 3), tag `fmt::format("curve {}", route.name)`, through the bays block's `once_row` (move it out of the bays block so both use it; no other change to it).
c. The file comment: drop "curve spacing" from "Not in it yet", add "curve spacing (option)".

## 5. Do not
- Do not port the `Marker` (step binary) rows or any `g2_*` switch.
- No default changes; no existing test or probe turns `curves` on.
- No other family; no change to Shape, Structure or the probes.

## 6. Stop conditions
- The test 1 premise fails, or the helper finds no gap.
- The walk meets a heading change other than one eighth on the one-lane T.
- Anything outside `planar_draw.h`, `planar_draw.cpp`, `src/tests/junction/planar/planar_draw.cpp` seems needed.

## 7. Gates
- `tools/junction/jt fast build-rel '[planardraw]' 5`; over budget: STOP.
- `tools/junction/jt gate build-rel` (30 s). Known flake: "planar components: T2 mapped lands in its box at the anchor"; rerun alone and report.
- One commit: `Add: Junction: curve spacing rows in the drawing program (option)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. No merge or push.

## 8. Report (short, fixed format)
- References read
- Commits: hash and subject
- Red evidence: each new test and the failure message seen before the rows existed
- Types and members added or removed: names only
- Suite results, naming any failure you did not cause
- Stops and anything left undone, with the reason
- State: `git status --porcelain` and `git log -1 --oneline`

When you have reported, stop.
