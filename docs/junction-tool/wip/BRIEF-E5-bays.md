# BRIEF E5-bays: bay rows in the drawing program (Sonnet)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head: given in the launch message. Release build dir `build-rel` (already configured; build with `-j 4` only, the machine is unstable under load).
- Source of this step: DECISIONS D419 and D420 (3): bays are the first missing rule family added to the drawing program (E5's LP oracle). The rule is SPEC-slots-lengths §8.1, row `bay`: `2 tl + 4` units on "a non-prio route path: from its last topology split or port to each topology merge". It is already written in the Slots probe; this step ports it.

## 2. Read first (once each, with offset/limit)
- `CODINGSTYLE.md` (repo root), the parts on naming, comments and Doxygen.
- `src/tests/junction/planar/planar_slots_probe.cpp` lines 845-856 (`site_of`, `is_split`) and 1441-1471 (section 11, bays, with `topology_node` and `once_row`). This is the code to port.
- `src/junction/planar/planar_draw.h` lines 55-95 (`PlanarDrawOptions`), and `src/junction/planar/planar_draw.cpp` lines 200-260 (`var`, `row`, `eq`, the elastic family filter at about line 247) and lines 285-360 (`length[t][i]` with `.variable` and `.coefficient`; a piece's length in half-tile units is `variable * coefficient`).
- `src/tests/junction/planar/planar_draw.cpp` lines 40-120 (`ShapeFirst`, `PlanarTText`, the one-lane T test).

## 3. Tests (outermost first; red before green)
Tag both `[junction][planar][planardraw]`, in `src/tests/junction/planar/planar_draw.cpp`, after the test "planar draw: the one-lane T is drawable".

1. **"planar draw: bays hold the route from a split or port to a merge"** (outer).
   - `const DrawCase c = ShapeFirst(PlanarTText(1), 1);` Solve twice with `CheckPlanarDrawable(c.v, 5.0, {...})`, both with `.clearance = false` (and `.tidy = true`, the default): once with `.bays = false`, once with `.bays = true`. REQUIRE both feasible.
   - For each result, compute every bay the rule names, from the drawing: walk each non-prio route (`map.expanded.routes`, skip `route.prio`), each of its `paths`, summing `r.lengths[t][i]` over every piece of each track in order; at a track whose `to` end is not a port and is a topology node (every track at the node has `map.part_of != UINT32_MAX`): at a split remember the running sum; at a merge the bay is the running sum minus the remembered one (0 at the route's start). Write this walk as one static helper in the test file, `static std::vector<double> DrawnBays(const DrawCase &c, const PlanarDrawResult &r)`, with the split test copied from the probe's `is_split` (it needs `site_of` over `c.v.structure.sites`).
   - REQUIRE the helper finds at least one bay. CHECK that the drawing without bays has some bay below `c.v.structure.rules.bay` (the premise: the tidy drawing is shorter than the rule; if this CHECK fails, STOP and report the bays found and `rules.bay`), and that every bay of the drawing with bays is `>= rules.bay`.
   - Red: add the option field `bays` first (section 4 a) with no rows written; the test must then fail on the last CHECK (a bay below `rules.bay` with `.bays = true`). Record that failure message.
2. **"planar draw: elastic mode names a short bay"** (inner, the row family and its tag).
   - Same case with `.bays = true, .elastic = true, .elastic_families = {"bay"}` plus `.fix_points` holding every point of the `.bays = false` drawing (`r.points`; window 0, i.e. `.fix_window = 0`), so the short drawing is forced and only bay rows may stretch.
   - CHECK some entry of `result.stretched` has a tag starting with `"bay "`. Red with the field added and no rows: no `bay` entry.
   - Before writing it, read how `fix_points`, `fix_window` and `stretched` are declared in `planar_draw.h`; if `fix_points` cannot take the earlier drawing as is, STOP and report.

## 4. Changes
a. `PlanarDrawOptions` (planar_draw.h): add `bool bays = false; ///< The bay rows (SPEC-slots-lengths §8.1, D420): on every non-prio route path, the travel from its last topology split or port to each topology merge is at least rules.bay.` Place it directly after `sep_extremal` (which follows `pairwise_all_faces`), keeping designated-initializer order valid for every existing caller (all callers use field order; check the build).
b. `CheckPlanarDrawable` (planar_draw.cpp): when `options.bays`, after the crossing rows and before the pins block, port section 11 of the probe:
   - `site_of`, `is_split`, `topology_node` as in the probe, over `v.structure.sites` and `map.expanded.nodes`;
   - the running sum is a `Terms` list: for each track `t` of the path, for each piece `i`, `{length[t][i].variable, length[t][i].coefficient}`;
   - a merge's row: the running terms minus the remembered split terms (negate the coefficients), merged by variable, written with the file's `row(terms, rules.bay, JunctionMilp::INF, fmt::format("bay {} {}", route.name, to.index))`;
   - `once_row`: skip a row whose (sorted terms, lower bound) was already written, as the probe does.
   - Worked call site (the merge branch):
     ```cpp
     Terms d = acc;
     for (const auto &[variable, coefficient] : from_split) d.emplace_back(variable, -coefficient);
     once_row(std::move(d), rules.bay, fmt::format("bay {} {}", route.name, to.index));
     ```
     where `once_row` sorts and merges `d` like `row` does, drops zero coefficients, keys a `std::set` on (terms, lower), and calls `row` once per key.
c. The file comment at planar_draw.cpp line 10: remove "bays, " from "Not in it yet: ...", and add "bays (option)" to the list of what it has on line 9.

## 5. Do not
- Do not change any default: `bays` stays false; no existing test or probe turns it on.
- Do not add coverage, curve spacing, node legs or any other family; do not touch Shape, Structure or the Slots probe.
- Do not port `g2_*` switches or the probe's `Lin` type.
- Do not change the elastic family filter; the tag's first word `bay` is the family.

## 6. Stop conditions
- The premise CHECK in test 1 fails (the tidy T without bays already meets every bay).
- The one-lane T has no non-prio route path with a topology merge (the helper finds no bay).
- `fix_points` cannot hold the earlier drawing (test 2).
- Any change outside `planar_draw.h`, `planar_draw.cpp` and `src/tests/junction/planar/planar_draw.cpp` seems needed.

## 7. Gates
- `tools/junction/jt fast build-rel '[planardraw]' 5` (Release). If over budget, STOP and report the time.
- `tools/junction/jt gate build-rel` (budget 30 s). Known flake: "planar components: T2 mapped lands in its box at the anchor" may fail once under parallel load; rerun it alone and report both results.
- One commit: `Add: Junction: bay rows in the drawing program (option)`, ending with the line `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Do not merge, push or touch other branches.

## 8. Report (short, fixed format)
- References read
- Commits: hash and subject
- Red evidence: each new test and the failure message seen before the rows existed
- Types and members added or removed: names only
- Suite results (fast tag time, gate counts), naming any failure you did not cause
- Stops and anything left undone, with the reason
- State: `git status --porcelain` and `git log -1 --oneline`

When you have reported, stop.
