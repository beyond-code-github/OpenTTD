# BRIEF: E5 sibling lanes as offset curves (D388 spine, SPEC-slots-lengths §11), probe behind an option

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: about 90 minutes in total. Each LP or solve keeps its existing cap (pipeline rounds as today; finals 15 s). Run each hidden measurement once (a second time only if the first crashed or printed nothing). No rerun loops, no tuning.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (branch wt-e1), head HEAD_HASH, clean. Release build `/home/pete/OpenTTD/.claude/ng1/build-rel`.
- Design it implements: `docs/junction-tool/DESIGN-sss-v2.md` §3.1 item 1 ("one heading list per channel (spine) ... lanes are offset curves at whole lanes") and §3.3 ("Corridors: lanes as offset curves of their spine (miter rows at every bend, flex inserts shared, SPEC-slots-lengths §11)"); `docs/junction-tool/wip/SPEC-slots-lengths.md` §11 (the miter row). Ruling D388. Evidence for doing it now: the side-probe IIS (STATUS NOW item 0): T3's and LL2's keep-apart conflicts are between sibling lanes of one channel (D2/D3, B1/B2, D1/D2).
- The corridor family (D421) already writes these rows for beside tracks (copies, prio rows): `src/junction/planar/planar_draw.cpp` ~lines 595-617. This step writes the same row form for sibling lanes.

## 2. Read first (once each, with offset/limit)
- `CODINGSTYLE.md`, C++ part.
- `src/junction/planar/planar_draw.cpp`: the corridor block (~535-617: `Beside`, the `corridor {} {}` rows with `normal = STEP[(h + 2) % 8]` and `2 * beside.side`); the pairwise keep-apart pair loop (search `"sep {} {} {}"` and the skip rules before it, e.g. zero contraction, span pieces), to know where a pair is skipped.
- `src/junction/planar/planar_shape_layer.cpp` ~155-225: `SpinePairs` (static) and its helpers `channel_events`, `partner_of`, `legs_of_one_node`.
- `src/junction/planar/planar_regions.h`: `PlanarTrackMap::Side(p, ps, q, qs)` and `side` (per stretch pair; stretch k of a track runs from its event k-1 to event k).
- `src/junction/planar/planar_shape_layer.h`: `PlanarShapeTrack::event_piece`, `flex`, `pieces`.
- `src/junction/planar/planar_draw.h`: `PlanarDrawOptions`, `PlanarDrawResult`, `PlanarDrawPipelineOptions`.
- The hidden tests "planar draw: the pipeline on T2, T3 and LL2" and "planar draw: the pipeline on PSG 197 (D422)" in `src/tests/junction/planar/planar_draw.cpp`.

## 3. The rule (settled; implement exactly)
**Sibling pairs.** Take the pairs `SpinePairs` returns whose last element (`same`, both cross the same channels in order) is true: (channel c, track a, track b, fa, fb), fa/fb the indices into `map.events[a]`/`map.events[b]` of their channel-crossing events. A pair is **used** only when all of these hold; otherwise it is **skipped** with the first failing reason (text in brackets):
1. `fa.size() >= 2` ("one channel crossing").
2. Between its first and last channel crossing each track has only channel crossings: `fa.back() - fa.front() + 1 == fa.size()` and the same for b ("other events inside").
3. Side: for every interior stretch index j = 1 .. fa.size() - 1, `map.Side(b, fb.front() + j, a, fa.front() + j)` is non-zero and the same value s for every j ("no side" / "sides differ"). s = +1 means b lies right of a's travel.
4. Pieces: let `pa0 = event_piece[fa.front()]`, `pa1 = event_piece[fa.back()]` on a and `pb0`, `pb1` likewise on b. Then `pa1 - pa0 == pb1 - pb0`, and for every offset i in 0 .. pa1 - pa0 the headings mod 8 are equal (`pieces[pa0 + i]` vs `pieces[pb0 + i]`) and piece pa0 + i is a flex middle of a exactly when pb0 + i is one of b ("pieces differ").

`SpinePairs` is static in planar_shape_layer.cpp. Make it public as `PlanarSpinePairs(const PlanarTrackMap &, const PlanarStructureVariant &)` declared in planar_shape_layer.h, with its existing doc comment moved to the declaration (the comment moves with it, unchanged), and its two existing call sites renamed. No change to what it returns.

**Rows (tag `lane <a name> <b name> <i>`, hard, never elastic).** For a used pair, with point k of a track being the start of its piece k (as the corridor block indexes `point[t][k]`):
- For each vertex i = 1 .. pa1 - pa0 (between pieces pa0 + i - 1 and pa0 + i), for each distinct heading h of those two pieces of a: `STEP[(h + 2) % 8] · (point[b][pb0 + i] - point[a][pa0 + i]) == 2 * s`. This is the corridor row (§11 miter, one lane, side s).
- At the range's ends, only the end piece's normal: vertex pa0 with heading of piece pa0, and vertex pa1 + 1 with heading of piece pa1, each `STEP[(h + 2) % 8] · (point[b][pb0 (or pb1 + 1)] - point[a][pa0 (or pa1 + 1)]) == 2 * s`.
- The family name (first word) is `lane`; it must not appear in any `elastic_families` list, so it stays hard. Confirm that the elastic test at ~line 265 makes it soft only when `elastic_families` is empty (the seed's elastic-all mode); if the seed's mode would soften it, that is acceptable (the seed is elastic everywhere today), report which it is.

**Keep-apart skip.** With the option on, the pairwise loop skips a wall pair whose two walls are pieces of a used pair's tracks a and b with a's piece in [pa0, pa1] and b's piece in [pb0, pb1] (either order); count them in the result.

## 4. Changes
- `PlanarDrawOptions`: `bool sibling_lanes = false;` doc `///< A probe (D388 spine, SPEC-slots-lengths §11): sibling lanes of one channel are offset curves at one lane between their first and last channel crossing (tag lane), and keep-apart skips their wall pairs there.`
- `PlanarDrawResult`: `uint32_t lane_pairs = 0;` (used pairs), `uint32_t lane_sep_skipped = 0;` (wall pairs skipped), `std::vector<std::string> lane_skipped;` (`"<a> <b>: <reason>"` per skipped pair).
- `PlanarDrawPipelineOptions`: `bool sibling_lanes = false;` passed as `.sibling_lanes` into every `CheckPlanarDrawable` call of `DrawPlanarPipeline` (seed, discovery, commit, prune, additions, free sides, side probe, final), exactly as `.corridor` is passed.
- `PlanarDrawPipelineResult` gets nothing new; the tests read `r.rounds.front()` / `r.last_round` for the counts.
- Hidden tests: in both pipeline tests, `o.sibling_lanes = std::getenv("PLANAR_PROBE_SIBLINGS") != nullptr;` next to the other env reads (add it to the comment listing the aids), and when set, WARN once per hub: `<hub> sibling lanes: pairs <lane_pairs> sep skipped <lane_sep_skipped>` from the last round, then one WARN per `lane_skipped` entry.
- `src/junction/planar/CLAUDE.md`, Drawing program section: one bullet stating the option and the rule as in §3 (present tense, off by default, a probe), in the same commit.

## 5. Tests (fast, outermost first)
1. New test in `src/tests/junction/planar/planar_draw.cpp`, tag `[planardraw]`: "planar draw: sibling lanes are offset curves on T3". Build T3 as the hub test does (`ShapeFirst(PlanarTText(3), 1)`), `CheckPlanarDrawable(c.v, 5.0, {.clearance = false, .corridor = true, .sibling_lanes = true})` (no keep-apart, so only the new rows decide). Assert: status Optimal; `lane_pairs >= 1`; and for each used pair recomputed in the test from `PlanarSpinePairs` with the §3 rules... no: keep the test small. Assert instead that for the result's points, the D2/D3 pair (find tracks by name "D2" and "D3") has, at every vertex i of the used range, the offset row satisfied (recompute the range from `event_piece` of the first and last channel crossing; the test may hard-code that the pair is used and read s from `map.Side`). Red: with the members added and the rows not yet written, `lane_pairs >= 1` fails (record that message), then write the rows.
   - If D2/D3 is skipped on T3 by the §3 rules, stop and report the reason (do not loosen a rule).
2. No other new test. The gates below must stay green with the option off.

## 6. Do not
- Do not change any default (option off everywhere, including the hub test unless the env var is set).
- Do not write rows for pairs the §3 rules skip, and do not invent a fallback for them (no "use the drawing's side", no distance other than one lane).
- Do not touch flex placement, Shape or the commit; corresponding inserts follow from the rows.
- Do not tune or rerun measurements.

## 7. Stop conditions
- `PlanarTrackMap::Side` has no entry for the interior stretches of a pair you expected to be used on T3 (D2/D3): report the stretch indices you queried.
- `event_piece` is not indexed as §3 assumes (per map event of the track).
- The `lane` rows make the fast T3 test infeasible: report the conflict tags (`r.conflict`) and stop; do not change the rule.
- A gate fails for a reason you did not cause and cannot isolate in one rerun.

## 8. Gates and measurement
- `cd /home/pete/OpenTTD/.claude/ng1 && tools/junction/jt fast build-rel '[planardraw]' 5`; `tools/junction/jt fast build-rel '[planarshape]' 5` if that tag exists (it covers `SpinePairs`' callers), else name the tag you used; `tools/junction/jt gate build-rel` (30 s). Known flakes: "planar components: T2 mapped lands in its box at the anchor", "joint: a port fan's size follows the rows"; rerun alone and report.
- Measurement 1: `PLANAR_PROBE_SIBLINGS=1 build-rel/openttd_test "planar draw: the pipeline on T2\, T3 and LL2" > build-rel/siblings-hubs.log 2>&1`.
- Measurement 2: `PLANAR_PROBE_SIBLINGS=1 build-rel/openttd_test "planar draw: the pipeline on PSG 197 (D422)" > build-rel/siblings-197.log 2>&1`.
- Copy both logs to `docs/junction-tool/wip/probes/e5land/siblings-{hubs,197}-<short head hash before your commit>.log` in this worktree, same commit.
- One commit: `Add: Junction: sibling lanes as offset curves in the drawing program, a probe (D388)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. No merge or push.

## 9. Report (short, fixed format)
- References read
- Commits: hash and subject; files changed
- Red evidence: the new test's failure before the rows existed
- Types and members added or removed: names only (including the `SpinePairs` rename)
- Suite results (count and time per gate)
- Measurement, per hub (T1, T2, T3, LL2, 197): sibling pairs used, sep skipped, every skipped pair with its reason; the pipeline log lines (seed, each round, final) with status and slack; the last round's slack by family; stopped stage if any; on any non-Optimal round, its conflict tags. Join Catch's 80-column wraps before quoting.
- Stops and anything left undone
- State: `git status --porcelain` and `git log -1 --oneline`

When you have reported, stop.
