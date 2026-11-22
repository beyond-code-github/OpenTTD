# BRIEF: E5 strict skip rules, second cut (measured with the clash check)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: about 60 minutes in total. Solves keep their existing caps. Run each measurement once. No rerun loops, no tuning.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (branch wt-e1), head HEAD_HASH, clean. Release build `build-rel`.
- Previous step ca3fba870f (brief docs/junction-tool/wip/BRIEF-E5-skip-rules.md; results docs/junction-tool/wip/probes/e5land/strict-036accae1d/) added `strict_span` and `strict_zero` (off by default). Two flaws showed:
  - **strict_span too loose:** it exempts a span piece against every piece of a crossing track. The crossing rule only makes the crosser's crossing piece non-parallel; another piece of the crosser running along the deck is a clash (197: `L2#5` runs 2 half tiles along span `L4#4`; T2 (d): `B2#5` along `F1#5`).
  - **strict_zero too strict:** it gives full spacing to every touching parallel pair, including walls that continue one another along one line (the two flanks of a flex pair at 0 on one track; a trunk and its straight leg at a node), forcing them sideways. Slack rose sharply (T1 0 -> 56, 197 0 -> 896).

## 2. Read first (once each)
- `git show ca3fba870f -- src/junction/planar/planar_draw.cpp src/junction/planar/planar_draw.h`.
- planar_draw.cpp: the `strict_span` `sanctioned` lambda and the zero-contraction `Touch` code (~1210-1290).

## 3. Changes (same two options, semantics replaced; both still off by default; update their doc lines)
### 3.1 strict_span
- `sanctioned(s, other)`: true only if a crossing event k of s's track sits on s's piece (as today) **and** the same crossing appears in other's track events at an index k' with `v.tracks[other.track].event_piece[k'] == other.piece` (the crosser's own crossing piece). Doc: "D427 probe: a span piece is exempt from keep-apart only against the crossing piece of a track that crosses it on that piece."
### 3.2 strict_zero
When `touching` is true and `strict_zero` is set, classify by how the two walls touch (compute per wall which end touches, from the same data `touch` uses):
- **Crossing on both walls:** an event they touch through sits on both walls' own pieces (`event_piece == wall.piece` for both): the walls cross there. Skip with reason `zero contraction crossing`.
- Otherwise, per wall, its touching end: same-track touch: the lower-piece wall touches at its hi end, the higher at its lo end; node touch: the lo end if the node is its track's `from` reached at `lo == 0`, the hi end if `to` at `hi == last`; event touch: lo end if the event's piece < wall.piece, hi end if >, `interior` if ==.
- Direction away from the touch point: lo end: the wall's heading mod 8; hi end: heading + 4 mod 8; interior: none.
- **Full spacing** only if both walls have a direction and the directions are equal (they leave the point side by side). Otherwise **spacing 0** (they may meet at the point, not cross). If several touch ways apply, full spacing wins if any way gives it, and the crossing skip wins over both.
- Dump `kept` reasons: `zero contraction kept, full spacing (same direction)` or `zero contraction kept, spacing 0`; skipped reason `zero contraction crossing`.
Doc: "D427 probe: a pair the zero contraction would skip is kept: skipped only where the walls cross at the touch point; full spacing where both leave the touch point in the same direction; spacing 0 otherwise."

## 4. Do not
- No change with both options off. No other skip rule, default or expectation changes.

## 5. Stop conditions
- The crossing index in the other track's events cannot be matched (same crossing id) from the map: report what is available.
- A fast-gate test changes result with both options off.
- A gate fails for a reason you did not cause and cannot isolate in one rerun.

## 6. Gates and measurement
- `tools/junction/jt fast build-rel '[planardraw]' 5`; `tools/junction/jt gate build-rel` (30 s; known flakes as before).
- The same four any-hub runs as the previous step ((a) neither, (b) span, (c) zero, (d) both; `PLANAR_DUMP=build-rel/dumps2-<run> PLANAR_ANYHUB_CONTRACT=1 PLANAR_ANYHUB_GRID=1`), and (d) once more with `PLANAR_ANYHUB_CONFLICT=1`. `clash` on each hub's final (else last round).
- Copy logs and clash outputs to `docs/junction-tool/wip/probes/e5land/strict2-<short head hash before your commit>/`, same commit.
- One commit: `Change: Junction: strict skip rules, second cut (E5, D427 probes)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. No merge or push.

## 7. Report (short, fixed format)
- Commits; files changed. Suite results.
- The table hub x run (a-d): last-round slack, final status and ms, pairs, clash summary (overlaps, meets, by reason, kept reasons).
- For run (d): every clash line on T1, T2, 197 verbatim (cap 30 each); the conflict rows of every Infeasible final (cap 30 each).
- Stops and anything left undone. State.

When you have reported, stop.
