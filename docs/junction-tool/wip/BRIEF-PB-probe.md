# Brief PB (probe, throwaway): bendy prios, form B: the joiner bridges straight, the prio lanes S-bend under it inside their coverage

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar` at {HEAD}. Create branch `probe-pb` from it and commit only there. When finished: `git switch wt-planar` and rebuild `openttd_test` in build-rel.
- Time box: 90 minutes. This is a probe; its product is the measurement.
- Engineer's ruling (DECISIONS.md D347a, D347b, D342, D349; grep them in `/home/pete/OpenTTD/docs/junction-tool/DECISIONS.md`): "the choice line bridged (double if needed) and then a bend prio s-bends underneath it"; a prio's coverage may bend; the row is a line of the bundle owned by its lane. Bridges are axis-only in OpenTTD, so the joiner bridges on its axis heading and the prio lanes cross under it on 45-degree pieces.
- Background: `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-joiner-over.md` (an earlier form-B probe, before the slot layer; it kept a straight 2P+2 after the S-bend and the old fixed row, and its patch `wip/probe-joiner-over.patch` shows the joiner deck and lane S-bend shapes this brief reuses).

## Why
Today each near lane of a multi-lane prio join doubles over the joiners in series and then needs its own straight 2P+2 coverage, so the coverages chain (wip/PROBE-prio-stagger.md: T3's length). In form B no main-line lane is doubled in the join; each lane's coverage includes its S-bend under the joiners, so the coverages run side by side.

## Read first (once each)
- `CODINGSTYLE.md`: naming, comments.
- `src/junction/planar/planar_components.cpp` on wt-planar: the n-lane `Join` (~:1031-1100), `Double`, `PairDecks`, `SlotBend`, `SlotOf`, `build_join` (~:1474), the prio-row loop after the box (~:1858-1876).
- `src/junction/planar/planar_model.h`: `Seg`, `AddSeg`, `Cross`, `Deck`, `Tie`, `NewStrand`, `End`.
- `src/junction/planar/planar_slots.cpp`: `BuildIntervals`, the prio join entry-gap block. `git show 20f98229e0:src/junction/planar/planar_slots.cpp` shows the Row track push this brief restores.
- `wip/probe-joiner-over.patch`.

## Changes
### `planar_slots.cpp`
1. Restore the Row tracks exactly as at 20f98229e0: in the last interval (`post`) of a prio join's straight channel, a `PlanarSlotKind::Row` track beside each line, on the side `row = -sites[s].side` (pushed before the Line when negative, after it when positive). Consecutive tracks keep `>= 1`, so neighbouring lanes there are at least 2 apart with the row between.
2. Remove the prio join entry-gap rows block (it modelled the old stagger).
3. Nothing else.

### `planar_components.cpp`, n-lane `Join`, the `prio` case only (the non-prio case is unchanged)
Lanes are far..near as today (`S[0]` far from the joiners; the joiners come from the `sigma` side).
1. **Joiners.** For k + 1 < n: `jdeck[k] = M.AddSeg(J[k], h, 2)`, then `js[k] = M.AddSeg(J[k], Turn(h, -sigma), 4)`. For k = n - 1: as today. Before building, for each joiner line: if `this->FullShare(<its line>)`, `M.Fail("a full-share joiner over a prio is not composed yet")` and return.
2. **Far lane.** `M.AddSeg(S[0], h, 2)` (its coverage comes from item 4, not a fixed 2P+2).
3. **Near lanes**, j >= 1, in order: `M.AddSeg(S[j], h, 2)`; `out_j = M.AddSeg(S[j], Turn(h, sigma), 2)`; `M.AddSeg(S[j], h, 2)`; `back_j = M.AddSeg(S[j], Turn(h, -sigma), 2)`; `M.AddSeg(S[j], h, 2)`. Add `Eq(out_j.L - back_j.L, 0)` (the S-bend comes back to its slot). No `Double` for main-line lanes in this case.
4. **Coverage.** Record, for each lane k, `first_k` = the number of segments of S[k]'s strand when `Join` starts adding to it. Add `Ge(sum of L of S[k]'s segments with index >= first_k, 2 * this->P + 2)`, tag "prio coverage". Every segment's L counts as it is.
5. **Ties and merges** as today: `M.Tie(M.End(J[k]), M.End(S[k]))` for every k; `ms`, `NewStrand(O[k], ms[k])`, `Node`.
6. **Rows (real strands, not reservations).** Do not push to `M.prio_rows` in this case. For each lane k, a strand named `"prio:" + S[k]`:
   - start: `NewStrand(name, M.NewPoint())`, tied (both coordinates) to `S[k].segs[first_k].a + (-sigma) * 2 * Right(h)`;
   - for each segment `g` of S[k] with index >= first_k, in order: `const Seg rs = M.AddSeg(name, g.d, 0, std::nullopt, 0)` and `Add(Eq(Dot(Right(g.d), rs.b - g.b), -sigma * 2))`, tag "prio row offset";
   - end: `Add(Eq(Dot(STEP[h], M.End(name) - ms[k]), 0))`;
   - box: the four box rows on each row point, as the old prio-row loop writes them (`M.X0`, `M.Y0`, `M.X`, `M.Y`). If the strand is already covered by the box loop over `M.order`, skip this and say so.
   - Keep the row strands out of `M.related` (they clash with every other strand, as the old rows did).
7. **Decks.** For k + 1 < n: `xs` = `M.Cross(jdeck[k], out_j, 4)` for every j > k, plus `M.Cross(jdeck[k], <row segment of S[j] beside out_j>, 4)` for every j > k; then `M.Deck(jdeck[k], xs)`.
8. **Out lanes.** Replace the fixed step back (`AddSeg(O[j], h, 2)` then `AddSeg(O[j], Turn(h, -sigma), 2, 2 * j)`) with `this->SlotBend(O[j], h, delta_j)`, where, with `sc` the straight channel, `oc` the out lanes' channel, and lines looked up by name (`PlanarLineByName`):
   `delta_j = (SlotOf(oc, "pre", O_j) - SlotOf(oc, "pre", O_0)) - (SlotOf(sc, "post", S_j) - SlotOf(sc, "post", S_0))`.
   If `SlotBend` returns false, return. The join-exit loop after it is unchanged.
9. Comments: one line per item, stating the shape plainly.

## Measure
- Build `openttd_test` in build-rel on probe-pb.
- pv, one at a time, never in parallel: `/home/pete/OpenTTD/tools/junction/pv run build-rel T2 --out <S>/pb-T2.txt`, then T3, then LL2, with S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`. Compare against `<S>/s3-T2.txt`, `s3-T3.txt`, `s3-LL2.txt` (wt-planar after S3: T2 28/32 best 862.25; T3 28/32 best 1933.25; LL2 12/12 best 2230.25).
- **Per hub:** built and failed with reasons and counts; best area and its tag against the S3 best; better / worse / equal and median change; median ms against S3.
- **If T3 builds no variant:** run `PLANAR_ONLY=o1t3f1s0 PLANAR_IIS=1 build-rel/openttd_test "planar components: T3 from its topology text (mapped) at the hand composition's area" -s > <S>/pb-iis-T3.txt 2>&1`; report the IIS rows (tags), each with the file:line that adds it. Same for T2 with its mapped test name if T2 builds none.
- **End to end on each hub's best variant (T2 and T3):** find the mapped dump test (`grep -n "dump a mapped component layout" src/tests/junction/planar/*.cpp` and how it picks the topology and variant, e.g. `JUNCTION_COMPONENTS_TOPOLOGY`). Run it on the best tag. Report: does `PlanPlanarJunction` succeed; if not, its message; if so, the `CheckJunctionRules` violations (rule and count), and save its ASCII or layout output to `<S>/pb-e2e-<hub>.txt`. If the dump test cannot take a variant tag, run it as it is and report which variant it used; do not change the test.
- `tools/junction/jt fast build-rel '[planar]'`: list failures (slot tests pinning entry gaps and "no Row tracks" are expected to fail; do not fix them).

## Do not
- Change the non-prio `Join` case, the fray, cores, bundles, or the 2-lane `Join`/`Block227` composers.
- Add a variant bit or environment variable.
- Land anything on `wt-planar`.
- Run two heavy jobs at once. Touch ports 4077 and 4079, or start a game client. Use git stash.

## Stop conditions
- `ms[k]` or `first_k` not obtainable as described.
- A compile error you cannot fix within the listed changes.

## Commit and report
- Commit on probe-pb: `Probe: Junction bendy prio, joiner over, lanes S-bend under in coverage (throwaway)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Write `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-PB.md` (do not commit it): change summary, tables, IIS, end-to-end result.
- Reply with: the tables, the IIS if any, the end-to-end result, the commit hash, `git status --porcelain`.

When you have reported, stop.
