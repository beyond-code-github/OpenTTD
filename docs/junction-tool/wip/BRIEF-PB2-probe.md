# Brief PB2 (probe, throwaway): bendy prios form B, take 2: the near lane crosses out under the joiners it passes and merges between them; the joiners spread apart to make room

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`. Branch `probe-pb` is at {HEAD} (probe PB). Create `probe-pb2` from it and commit only there. When finished: `git switch wt-planar` and rebuild `openttd_test` in build-rel.
- Time box: 75 minutes.
- Read first: `/home/pete/OpenTTD/docs/junction-tool/wip/BRIEF-PB-probe.md` (what PB built) and `wip/PROBE-PB.md` (its result); `git show probe-pb --stat` and the PB diff to `wt-planar`.

## Why PB failed
In T2 the joiners into B (C1, C2) are a bundle one lane spacing apart. PB swung near lane S[j] out under joiner J[k<j]'s straight deck and back. Out there it has to sit, with its row, between J[j-1] and J[j], where the bundle leaves no room (IIS: `bundle offset C1/C2`, `sep C1 C2`, the out = back equality). Also, it need not come back: once past J[0..j-1] it merges with its own joiner J[j] from the inside.

Order across, joiners nearest the lanes first: S[0] (far), ..., S[n-1], then J[0], J[1], ..., J[n-1] toward `sigma`. S[j] must cross exactly J[0..j-1].

## Changes
### `planar_slots.cpp`
1. Keep PB's changes (Row tracks in the straight channel's `post`, no entry-gap rows).
2. **Joiner spread.** For each site s with kind `Join` and `sites[s].prio`, with straight channel `sc = sites[s].straight` and joiner channel `oc = sites[s].other`: build the pairs (S line, J line) per node of the site exactly as `build_join` does (legs ordered far..near by the S line's position, `side > 0` keeps `cc.lines` order, else reversed). For k = 1..n-1, in `oc`'s last interval add `s(J[k]) - s(J[k-1]) >= 3` when `sites[s].side > 0`, else `s(J[k-1]) - s(J[k]) >= 3`. (Room for J[k-1], S[k]'s row, S[k], J[k].) If `sites[s].other` is not a valid channel, or a J line is not in `oc`'s lines, stop and report.

### `planar_components.cpp`, prio `Join` (PB's code)
1. **Near lanes**, j >= 1: `M.AddSeg(S[j], h, 2)`; `out_j = M.AddSeg(S[j], Turn(h, sigma), 2)`; `M.AddSeg(S[j], h, 2)`. Remove `back_j`, the segment after it, and the `out = back` equality.
2. **Joiners** as in PB (k + 1 < n: a 2 run, straight deck `jdeck[k]`, turn-in `Turn(h, -sigma)` of at least 4; k = n - 1 as today), but change the turn-in of every joiner to `AddSeg(J[k], Turn(h, -sigma), 2)` (least 2, no greatest).
3. **Rows**, coverage, decks: as in PB (the row follows S[j]'s three segments now; the decks cross `out_j` and the row segment beside it, for j > k).
4. **Out lanes.** Replace PB's `SlotBend(O[j], h, delta_j)` with: `M.AddSeg(O[j], h, 2)` then `M.AddSeg(O[j], Turn(h, -sigma), 2, std::nullopt, 1.0, true)` (optional: length 0 or at least 2), for j >= 1. The join-exit gap rows after it stay unchanged (they fix the out gaps from the out channel's `pre` slots).
5. Nothing else.

## Measure (as PB)
- pv one at a time: T2, T3, LL2 to `<S>/pb2-<hub>.txt`, S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`; compare with `<S>/s3-<hub>.txt` (T2 28/32 862.25; T3 28/32 1933.25; LL2 12/12 2230.25).
- Per hub: built/failed with reasons and counts; best and tag vs S3; better/worse/equal, median change; median ms.
- If T2 or T3 builds none: IIS on o1t3f1s0 as in PB (`pb2-iis-<hub>.txt`), rows with file:line.
- End to end on T2's and T3's best variant: the mapped dump test as described in BRIEF-PB-probe.md (Measure, last item). Report PlanPlanarJunction success or message, the checker violations (rule, count), and save output to `<S>/pb2-e2e-<hub>.txt`.

## Do not
As in BRIEF-PB-probe.md. Also: no other slot rule changes.

## Stop conditions
As in PB, plus the joiner-channel condition in slots item 2.

## Commit and report
- Commit on probe-pb2: `Probe: Junction bendy prio take 2, lanes cross out under joiners, joiners spread (throwaway)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Add a "Take 2 (PB2)" section to `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-PB.md` (do not commit).
- Reply with: tables, IIS if any, end-to-end result, commit hash, `git status --porcelain`.

When you have reported, stop.
