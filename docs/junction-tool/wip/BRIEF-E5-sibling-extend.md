# BRIEF: E5 sibling lanes, range extended over the common run (probe, read-out)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: about 60 minutes in total. Existing caps on every solve. Run each measurement once. No rerun loops, no tuning.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (branch wt-e1), head HEAD_HASH, clean. Release build `/home/pete/OpenTTD/.claude/ng1/build-rel`.
- Previous step 098679a270 (brief `/home/pete/OpenTTD/docs/junction-tool/wip/BRIEF-E5-sibling-lanes.md`, §3 with its Amendment): `sibling_lanes` writes `lane` offset rows for adjacent sibling lanes from their first to their last channel crossing. Result: T3 13, LL2 12; the conflicts left are outside that range (T3: D3's last pieces 13-17 against D2 as D3 heads for its join; LL2: B1#2-4 at the bundle's start). Design: DESIGN-sss-v2 §3.1 item 1 (lanes are offset curves of the spine), SPEC-slots-lengths §11.

## 2. Read first
- `git show 098679a270 -- src/junction/planar/planar_draw.cpp` (the lane block: the §3 rules, `LanePair`, the rows, the keep-apart skip).

## 3. The amended rule (replace rules 1, 3 and 4 of the previous brief; rule 2 stays)
1. **Rule 1:** `fa.size() >= 1` (one channel crossing is enough; skip reason "no channel crossing" when 0).
2. **Rule 3 (side):** over the stretch indices j = fa.front() .. fa.back() + 1 of a, paired with fb.front() + (j - fa.front()) of b (the stretch ending at the first crossing, the interior stretches, and the stretch starting after the last crossing), take every non-zero `map.Side(b, jb, a, ja)`. Used when at least one is non-zero and all non-zero ones are equal (s); else "no side" / "sides differ". A stretch index past the track's last stretch is not queried.
3. **Rule 4 (pieces):** as before, the crossing range [pa0, pa1] on a and [pb0, pb1] on b must have equal length and equal headings mod 8 and flex membership ("pieces differ"). Then **extend**:
   - backward: while pa0 > 0 and pb0 > 0, and pieces pa0 - 1 and pb0 - 1 have equal headings mod 8 and equal flex membership, and no event of a sits on piece pa0 - 1 and no event of b on pb0 - 1 (no `event_piece` entry equal to it), decrement both;
   - forward: likewise while pa1 + 1 < a's piece count and pb1 + 1 < b's, with pieces pa1 + 1 and pb1 + 1.
   - The rows (interior vertices with both normals, the two end vertices with the end piece's normal only) and the keep-apart skip use the extended ranges.
4. **Read-out:** a new `PlanarDrawResult::lane_ranges` (`std::vector<std::string>`), one entry per used pair: `"<a> <b> side <s>: a [<pa0>, <pa1>] of <a piece count>, b [<pb0>, <pb1>] of <b piece count>, crossing range a [<c0>, <c1>]"` where c0, c1 are the pre-extension pa0, pa1.

## 4. Changes
- planar_draw.cpp: the lane block as in §3. planar_draw.h: `lane_ranges` with doc `///< A read-out (sibling_lanes): per used pair, its ranges before and after extension and its side.`
- Both hidden pipeline tests: when `PLANAR_PROBE_SIBLINGS` is set, WARN each `lane_ranges` entry of the last round after the existing sibling WARNs.
- `src/junction/planar/CLAUDE.md`: update the `sibling_lanes` bullet to the amended rule, same commit.
- The fast test "planar draw: sibling lanes are offset curves on T3" must keep passing; if its D2/D3 range check hard-codes the old range, change it to read the range the same way the code now does (extension included). No other test change.

## 5. Do not
- No change with the option off. No fallback for skipped pairs. No change to Shape, flex placement or the commit.

## 6. Stop conditions
- The fast T3 test turns infeasible: report `r.conflict` and stop.
- A gate fails for a reason you did not cause and cannot isolate in one rerun.

## 7. Gates and measurement
- `cd /home/pete/OpenTTD/.claude/ng1 && tools/junction/jt fast build-rel '[planardraw]' 5`; `tools/junction/jt fast build-rel '[planarshape]' 5`; `tools/junction/jt gate build-rel` (30 s).
- `PLANAR_PROBE_SIBLINGS=1 build-rel/openttd_test "planar draw: the pipeline on T2\, T3 and LL2" > build-rel/sibext-hubs.log 2>&1` and `PLANAR_PROBE_SIBLINGS=1 build-rel/openttd_test "planar draw: the pipeline on PSG 197 (D422)" > build-rel/sibext-197.log 2>&1` (197's final CHECK fails at baseline, a 60 s Error; that is known).
- Copy both logs to `docs/junction-tool/wip/probes/e5land/sibext-{hubs,197}-<short head hash before your commit>.log`, same commit.
- One commit: `Add: Junction: sibling lanes over their common run (D388 probe)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. No merge or push.

## 8. Report (short, fixed format)
- Commits: hash and subject; files changed; members added.
- Suite results (count and time per gate).
- Per hub (T1, T2, T3, LL2, 197): pairs used, sep skipped, every `lane_ranges` entry verbatim, skipped pairs grouped by reason (names), the last round's slack by family, the final's status, the stopped stage, and for T3 and LL2 the last round's stretched rows with the hub test's decode lines (join Catch's 80-column wraps first).
- Stops and anything left undone.
- State: `git status --porcelain` and `git log -1 --oneline`.

When you have reported, stop.
