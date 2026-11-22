# BRIEF: E5 LL2 face 33, which corridor rows are in the core (exact deletion filter, read-out only)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: about 40 minutes in total. Each solve has a 15 s cap. Run the hidden test once. No rerun loops, no tuning.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (branch wt-e1), head HEAD_HASH, clean. Release build `/home/pete/OpenTTD/.claude/ng1/build-rel`.
- Previous step 35bc8cedfd (brief `/home/pete/OpenTTD/docs/junction-tool/wip/BRIEF-E5-face33.md`, test "planar draw: LL2 face 33 infeasible core"): with the exact face-33 options, the minimal set of keep-apart pairs is `sep 33 3 7` (D1#11 h2 | B1#1 h5) and `sep 33 3 10` (D1#11 h2 | B1#4 h6); the core alone is Infeasible; without the corridor family face 33 is Optimal. This step finds which corridor rows that core needs.
- Corridor rows (planar_draw.cpp ~lines 585-614): tags `corridor <beside name> open`, `corridor <beside name> close` (each written once per axis, two rows with the same tag) and `corridor <beside name> <k>` for k = 1 .. pieces - 1 (one row per heading of the two pieces at vertex k, same tag).

## 2. Read first (once each)
- `git show 35bc8cedfd -- src/tests/junction/planar/planar_draw.cpp src/junction/planar/planar_draw.cpp`.
- planar_draw.cpp: the `row` lambda (~line 290) and the corridor block (~535-617: `Beside`, `besides`, which tracks are beside tracks).

## 3. Changes
### 3.1 One option
- `PlanarDrawOptions`: `std::vector<std::string> skip_rows;` doc `///< A measurement aid: rows whose tag equals one of these, or starts with one ending in a space, are not written (a deletion filter).` In the `row` lambda, return without writing when the tag equals an entry, or when an entry ends with ' ' and the tag starts with it.

### 3.2 Extend the hidden test "planar draw: LL2 face 33 infeasible core" (after its existing steps)
All solves: the base face-33 options plus `skip_pairs` = every face-33 candidate pair except the two core pairs (so only the core's two pairs separate).
1. **Check:** solve once; WARN `rows check: <status>` (expected Infeasible; if not, WARN and stop the test here).
2. **Per beside track:** list every beside track of the corridor block (the tracks that get corridor rows: build the list in the test the same way the block does, or read it from the tags the block would write; if the test cannot get the list without new production code, stop and report). For each beside track name X: solve with `skip_rows = {"corridor " + X + " "}` (all of X's rows). WARN `rows without <X>: <status>`. The tracks whose removal makes it Optimal are needed.
3. **Per row of each needed track X:** candidate tags `corridor X open`, `corridor X close`, `corridor X k` for k = 1 .. (X's piece count - 1). Deletion filter as before: S = all candidates; for each tag p in order, solve with `skip_rows` = (every candidate of X not in S) plus p, plus the full prefixes of every other beside track that step 2 found not needed; if Infeasible, drop p from S. WARN `rows core <X>: <tags in S>` and the core-only check status.
4. For each core row, WARN the beside track's pieces (headings), its parent chain's track names (`ParentChain`), and, if the tag is `open` or `close`, the node it opens or closes at (`map.expanded.lines[X].from` / `.to`, port or node and index) and the opening or closing vector the block uses (if reachable from the test without new production code; otherwise say so).

## 4. Do not
- No change to any default or behaviour (`skip_rows` empty by default). No interpretation.

## 5. Stop conditions
- The beside-track list cannot be built in the test without new production code (report what you would need).
- A gate fails for a reason you did not cause and cannot isolate in one rerun.

## 6. Gates and measurement
- `cd /home/pete/OpenTTD/.claude/ng1 && tools/junction/jt fast build-rel '[planardraw]' 5`; `tools/junction/jt gate build-rel` (30 s). Known flakes: "planar components: T2 mapped lands in its box at the anchor", "joint: a port fan's size follows the rows".
- `build-rel/openttd_test "planar draw: LL2 face 33 infeasible core" > build-rel/face33-rows.log 2>&1`; copy to `docs/junction-tool/wip/probes/e5land/face33-rows-<short head hash before your commit>.log`, same commit.
- One commit: `Add: Junction: corridor-row deletion filter on LL2 face 33 (E5 certificate)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. No merge or push.

## 7. Report (short, fixed format)
- Commits: hash and subject; files changed; members added.
- Suite results (count and time per gate).
- The check line; every "rows without" line; every "rows core" line with its check; the step 4 lines verbatim. Join Catch's 80-column wraps first.
- Stops and anything left undone.
- State: `git status --porcelain` and `git log -1 --oneline`.

When you have reported, stop.
