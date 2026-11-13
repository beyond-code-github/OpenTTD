# GS1: the five slowest gate cases move to a hidden sweep tag run by `jt sweep` (D245)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
The wide gate takes about 103 s Release against its 120 s budget (target under 90 s). Five cases make up about 63 s of it. Following the engineer's ruling D245, they leave the wide gate for a hidden tag `[.sweep]`, run by a new `jt sweep` mode. They keep every tag they have today, so naming those tags still runs them.

This step changes test tags, `jt` and docs only. It never changes a test body.

## State
- Worktree `/home/pete/OpenTTD/.claude/nodes2`. First: `git -C /home/pete/OpenTTD/.claude/nodes2 checkout -B wt-gs1 <HEAD given at launch>`. Then rebuild `openttd_test` in `build-rel`. Leave untracked files alone.
- Another agent builds and tests in `.claude/nodes` at the same time. Never touch it.
- Output files go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/gs1/`.

## Read first
- `tools/junction/jt` (all);
- `docs/junction-tool/TESTS.md`, lines 1-10 and the rows of `[topologysweep]`, `[topologystretch]`, `[identity]` and the wide gate;
- `src/tests/junction/CLAUDE.md`;
- `docs/junction-tool/DECISIONS.md`: grep D245.

## The five cases (names exact; find each with grep)
1. "topology: pushing any point later keeps the seed legal"
2. "topology: LL_RR x LL_RR under each lane pairing seeds or names a reason"
3. "topology: the order sweep places crossings between edges that are not yet adjacent"
4. "topology: branch legs bound for one main lane merge before joining it"
5. "topology: a row node reached only by row edges is placed by the order"

The pattern is the one the identity tests use, for example `TEST_CASE("level solve: identical plans at every near offset", "[junction][realterrainsweep][.identity]")`.

## Changes
1. **Tags.** Append `[.sweep]` to each of the five cases' tag strings. Nothing else in those lines changes.
2. **`tools/junction/jt`.** Add a mode `sweep`, modelled on `identity`:
   - a header comment line: `#   jt sweep <build-dir>                            the slow gate sweeps "[.sweep]", Release builds only`;
   - the usage line gains `| jt sweep <build-dir>`;
   - the case branch: a Release check worded like identity's ("the slow sweeps run in Release only"), `filter="[.sweep]"`, `budget=${3:-90}`, `name=sweep`.
   - Before you change jt, run `build-rel/openttd_test "[.sweep]" --list-tests` once after step 1. If it does not list exactly the five cases, change the filter to `[sweep]` and check again. If neither lists exactly five, STOP.
3. **`docs/junction-tool/TESTS.md`.**
   - In the lines near the top that describe `jt gate` and `jt identity`, add one for `jt sweep`: it runs the hidden `[.sweep]` cases, Release only. Every brief that touches `src/junction/placement/` or `src/junction/topology/` runs it with the gate.
   - Add a table row `[sweep]`: hidden tag, run by `jt sweep` (Release only), not in the wide gate; it lists the five cases with their Release times; budget 90 s; your measured time with the date 2026-11-14 and "GS1".
   - The wide gate row gets the new time and case count, dated 2026-11-14 GS1.
4. **`src/tests/junction/CLAUDE.md`.** In the line that names `[identity]`, add: `[sweep]` is the hidden tag run by `jt sweep`, which holds the slowest gate sweeps; run it with the gate when you change placement or topology. Keep it plain and standalone, without D-ids.

## Do not
- Change any test body, CHECK, fixture or budget other than the ones named.
- Remove any existing tag from the five cases.
- Touch `src/junction/`.
- Use `pkill`, a broad `kill`, or a bare `git stash`.

## Stop conditions
- A test fails.
- The listing check in change 2 fails both ways.
- Anything not covered.

## Gates
- `tools/junction/jt gate build-rel`: 524 cases (529 less 5), all pass. Report the time.
- `tools/junction/jt sweep build-rel`: 5 cases, all pass. Report the time.
- `tools/junction/jt identity build-rel`: unchanged, all pass.
- If a gate is over budget, re-run it once and report both times.

One commit on wt-gs1: `Codechange: Junction tests, slowest gate sweeps moved to jt sweep`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
- references read;
- the commit;
- gate count and time before (529, about 103 s) and after; sweep count and time; identity time;
- stops;
- `git status --porcelain` and `git log -1`.

When you have reported, stop.
