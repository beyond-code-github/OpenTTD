# BRIEF: E5 two keep-apart skip rules made strict (behind options, measured with the clash check)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: about 75 minutes in total. Solves keep their existing caps. Run each measurement once. No rerun loops, no tuning.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (branch wt-e1), head HEAD_HASH, clean. Release build `build-rel`.
- Context (card docs/junction-tool/wip/CARD-E5-bundle-ends.md, last bullets; clash outputs docs/junction-tool/wip/probes/e5land/clash-d0d1276432/): every clash in the T1, T2 and 197 final drawings comes from a wall pair the pairwise block skips. Two skip rules are too broad:
  - **span:** `if (is_span(w[i]) || is_span(w[j])) skip("span")` exempts a span piece (a structure's separator or copy piece at its crossings) against every wall. It should be exempt only against tracks that cross it there (Structure's crossers, D369): any other track touching a span piece is not sanctioned by the planar map. T2 D1#9/D2#9 share 14 half tiles of one row this way.
  - **zero contraction:** two walls that would touch once every may-be-zero piece is a point are skipped entirely. Touching at one point is allowed (a node, a crossing); overlapping or crossing elsewhere is not. T1 A1#0/B1#1 leave their node on one diagonal for 12 half tiles (B1's turning piece is a flank at 0); T2 C2#3 crosses D2#1 mid-segment.

## 2. Read first (once each)
- `CODINGSTYLE.md`, C++ part.
- `src/junction/planar/planar_draw.cpp`: `span_piece` and `is_span` (~line 1037); the pairwise block from the face loop (~1180) through the `sides_from` candidate loop, the binary branches and the written rows (~1520), noting where `axis.spacing` is used for margins and rows.
- `PlanarTrackMap::events` / `crossings` (how a crossing event names its partner track) in src/junction/planar/planar_regions.h; `PlanarShapeTrack::event_piece`.
- `tools/junction/planar_dump.py clash` (to run it).

## 3. Changes
### 3.1 Two options (production), default false, each with a one-line doc
- `PlanarDrawOptions::strict_span`: "D427 probe: a span piece is exempt from keep-apart only against walls of tracks that cross its track on that piece; every other pair with a span wall gets rows as any pair."
- `PlanarDrawOptions::strict_zero`: "D427 probe: a pair the zero contraction would skip is kept, with spacing 0 when the two walls' headings differ mod 4 (they may meet at the contracted point but not cross) and its full spacing when they are equal mod 4 (parallel walls meeting at a point would overlap)."
- `PlanarDrawPipelineOptions` gains both, passed into every `CheckPlanarDrawable` call where `facing_sides` is passed today, and into the contract path's final.
### 3.2 strict_span
- Replace the span skip with: if `strict_span` is false, as today. If true: let S be the set of walls among w[i], w[j] that are span pieces. Skip ("span") only when for some span wall s in S, the other wall's track crosses s's track at an event that sits on s's piece (`event_piece[k] == s.piece` for a crossing event k of s's track whose partner is the other wall's track). Otherwise fall through to the remaining rules.
### 3.3 strict_zero
- Where `touching` is true today: if `strict_zero` is false, skip as today. If true: do not skip; mark the pair `zero_spacing = (w[i].heading % 4 != w[j].heading % 4)`. For this pair, every candidate's spacing is 0 when `zero_spacing`, else unchanged, in all of: the margin computation (`sides_from` choice, `margin_tags`), the written rows (plain and binary branches, `sephard`), and the dump's candidate spacing. Use a local spacing value; do not change `axes`.
- Record in the dump `skipped`-style list a new entry kind: `{tag, reason: "zero contraction kept, spacing 0"}` or `"zero contraction kept, full spacing"` in a new dump array `kept` (same shape as `skipped`), so `clash` can report them.
### 3.4 Test hooks
- In the hub pipeline test and the any-hub test's K3 block: `strict_span = getenv("PLANAR_STRICT_SPAN") != nullptr`, `strict_zero = getenv("PLANAR_STRICT_ZERO") != nullptr`.
- `planar_dump.py clash`: when a clash's pair is in `kept`, print `kept: <reason>`.
### 3.5 Docs
- `src/junction/planar/CLAUDE.md`: one bullet for the two options (probes, off by default).

## 4. Do not
- No change with both options off. No change to other skip rules, defaults or tests' expectations.

## 5. Stop conditions
- A span piece's crossings cannot be read per piece from the map and the shape: report what is available.
- A fast-gate test changes result with both options off (it must not).
- A gate fails for a reason you did not cause and cannot isolate in one rerun.

## 6. Gates and measurement
- `tools/junction/jt fast build-rel '[planardraw]' 5`; `tools/junction/jt gate build-rel` (30 s; known flakes as before).
- Any-hub test with `PLANAR_DUMP=build-rel/dumps-<run> PLANAR_ANYHUB_CONTRACT=1 PLANAR_ANYHUB_GRID=1` in four runs: (a) neither option, (b) `PLANAR_STRICT_SPAN=1`, (c) `PLANAR_STRICT_ZERO=1`, (d) both. Logs to build-rel.
- `clash` on each run's `anyhub-k3/<hub>/final-0.json` when present, else the last round file, for T1, T2, T3, LL2, 197.
- Copy the four logs and the clash outputs to `docs/junction-tool/wip/probes/e5land/strict-<short head hash before your commit>/`, same commit.
- One commit: `Add: Junction: strict span and zero-contraction keep-apart, probes (E5, D427)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. No merge or push.

## 7. Report (short, fixed format)
- Commits; files changed; members added. Suite results.
- A table: hub x run (a-d): last-round slack, final status and ms, pairs, clash summary (overlaps, meets, by reason).
- For run (d), every clash line on T1, T2 and 197 verbatim (cap 30 each), and the final conflict rows of any hub whose final is Infeasible (from `PLANAR_ANYHUB_CONFLICT=1` if you rerun (d) once with it; that rerun is allowed).
- Stops and anything left undone. State.

When you have reported, stop.
