# Step: PLANAR_DRY prints one line per variant, in order, with bounded threads

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only `openttd_test` in `build-rel`. One heavy job at a time. Time box 60 minutes.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, clean at `4887288d92`. Read `/home/pete/OpenTTD/CODINGSTYLE.md` (C++), `src/junction/planar/CLAUDE.md` (the PLANAR_DRY sentence).
- Today: `SolvePlanarMapped` (`src/junction/planar/planar_components.cpp` ~2880-2930) runs every variant on its own thread when PLANAR_DRY is set (8192 threads at once on LL²); each `SolveMappedVariant` prints its `DRY <tag> | <overs> | <built or reason>` line to stderr from inside (~2606-2616), and with no `PLANAR_SHAPE` it calls itself for the default and the program shape, so a variant can print twice. Observed: LL² gave 8667 and 8218 DRY lines on two runs for 8192 variants, and two unrepeated SIGSEGVs; `tools/junction/pv` reads its LL² tags from this listing, so its tag set varied between runs.

## Change
1. Under PLANAR_DRY, `SolveMappedVariant` builds the default shape only (as `PLANAR_SHAPE=default` does) and does not print; it returns the line text in `VariantOutcome::why` as `dry: |<overs> | <built or reason>` (today it sets `why = "dry"`).
2. Under PLANAR_DRY, `SolvePlanarMapped` runs the variants at most `SEARCH_THREADS` at a time (reuse the search's pool pattern), then prints one `DRY <tag> |<overs> | <built or reason>` line per variant to stderr, in enumeration order, after all have run. Output format otherwise unchanged (pv parses it: `p[0][4:]` tag, last field `built`).
3. Update the PLANAR_DRY sentence in `src/junction/planar/CLAUDE.md` (default shape only, one line per variant in order, bounded threads).

## Verification (a debug knob: verified by command, no unit test)
- Before the change, and after: `PLANAR_DRY=1 build-rel/openttd_test "planar components: LL² from its topology text (mapped) at the probe's area" -s 2> <file>` twice each; count DRY lines and built lines per run; after the change both runs must have exactly 8192 DRY lines and be byte-identical (`cmp`).
- `/home/pete/OpenTTD/tools/junction/pv run build-rel LL2` twice: the same 12 tags both times.
- `/home/pete/OpenTTD/tools/junction/jt gate build-rel` (count, time; ~30 s before, a known overrun).
- Commit on wt-planar: "Fix: Junction PLANAR_DRY prints one line per variant with bounded threads", ending "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>".

## Report
References read; commit; before/after line counts and the cmp result; pv tag sets; gate; `git status --porcelain`, `git log -1`. When you have reported, stop.
