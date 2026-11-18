# Probe C: prio channel crossings open to over/under, explored by the variant search (D347b, channel level)

Throwaway probe: nothing committed in ng1. Time box: 75 minutes; at the box, report what you have. Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only `openttd_test` in `build-rel`, never `openttd`. One heavy job at a time (no parallel test runs). Outputs under `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/probe-po/`; read code with offset/limit; command output to files. If anything does not fit the code as described, stop and report; do not invent another rule. Do not start background work that outlives your report.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, clean at `e4b50580e1` (includes the variant local search: above 6 variant bits `SolvePlanarMapped` hill-climbs from the all-zero variant; `PlanarComponentLayout::variants_solved`).
- Read first: `docs/junction-tool/wip/RECON-prio-under.md` section 1 and 4; DECISIONS rows D328, D347b; `src/junction/planar/CLAUDE.md` "Channel paths" and "Shape program".

## Change (all behind env `PLANAR_PRIOOPEN=1`; unset = today exactly)
1. `PlanarOpenCrossings` (`src/junction/planar/planar_paths.cpp` ~47-55): every channel crossing is open (not only equal-prio pairs). Confirm `CountMappedBits` takes its open count from it; if not, stop and report.
2. `BuildPlanarPaths` (~209-218): for an open crossing whose two channels differ in prio, bit value 0 puts the prio channel over (today's fixed choice, so the all-zero variant is unchanged) and 1 the other channel; equal-prio crossings keep today's encoding.
3. `SolveMappedVariant` (`planar_components.cpp` ~2556-2565): when the default shape fails (`dflt.why` non-empty), still run `SolvePlanarShape` on `dflt.over` (read and confirm `dflt.over` is filled before the failure; if not, stop and report), and solve only the program shape when it exists (as the `program` mode does); a failure of both is the variant's failure with both reasons.
4. Nothing else (no change to the composer's core/bundle choice, candidate steps or JoinEntry; their failures are data).

## Runs (sequential)
- T2, T3 and LL² full mapped solves (the three `planar components: ... (mapped)` tests, each alone, `-s`) without and with the env: area, size, variant tag, `variants_solved`, wall time, and which open bits are prio/non-prio pairs in the winning tag.
- With the env, for T3 and T2: each tag that is the all-zero variant with exactly one prio/non-prio bit set, `PLANAR_ONLY=<tag> PLANAR_TIMES=1`: solved area, or the failure text (default-shape reason, program-shape reason, composer reason such as "a candidate step on a channel that is not a core").
- If a with-env winner differs from today's: the end-to-end mapped-dump test (plan + signals + `CheckJunctionRules`, `src/tests/junction/planar/`) on that hub with the env: checker clean or violation texts.

## Report (findings under 30 lines in `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-prio-open.md`, committed alone on feature/junction-tool in `/home/pete/OpenTTD`, message "Doc: Junction probe C prio crossings open", ending "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>")
Then in ng1: `git diff > /home/pete/OpenTTD/docs/junction-tool/wip/probe-prio-open.patch` (commit with the findings), `git checkout -- .`, remove untracked files you added, `git status --porcelain` empty, rebuild `openttd_test`. Report in under 12 lines. When you have reported, stop.
