# CR1: the seed crashes on an L_R main with a multi-lane branch (NM1); make it refuse with a reason

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`), branch wt-guard at 0d344cd1de, with NM1's experiment patch applied UNCOMMITTED (the hidden `[.nm1]` test; `NM1_ONE=<i>` runs one pair; pairs 3 and 4 = main "L_R" with branch "LL_R" / "LL_RR"; also "L_RR"). Keep that patch out of your commit (`git -C ... diff` it to a scratch file first, revert it before committing, or commit only the files you change). Never use git stash. Other agents work in /home/pete/OpenTTD/.claude/nodes2 and .claude/ng1: never touch them. Never touch any game client or the rig servers.
- NM1 (docs/junction-tool/RECON-notation-matrix.md): 9 pairs (main L_R, branch L_RR / LL_R / LL_RR at 3 gaps) SIGSEGV. NM1 located it in OrderJunctionTrackGraph (`order.rows[row]` out of range); the orchestrator's guards (0d344cd1de, three `order.rows.size()` checks) did NOT stop the crash: `NM1_ONE=3` and `=4` still segfault. The engineer types notations in the game window, which seeds the pipeline from MakeJunctionHubTopology: a crash takes down his game.

## Steps
1. Find the real crash site (Debug build under gdb, or ASan if there is an ASan build dir: `.claude/crash/build-asan` is another worktree, do not use it; a backtrace from `build/openttd_test "[.nm1]"` with NM1_ONE under gdb -batch is enough). Report the stack.
2. Fix it the general way: the stage that meets a shape it cannot handle returns its error ("... not ruled yet" naming what), as every other refusal does; no special case on the notations. If my three guards are unnecessary after your fix, remove them; if one is the right place, keep it and say why.
3. Test `[topology]` "topology: an L_R main with a multi-lane branch is refused, not crashed": MakeJunctionGraphSeed on MakeJunctionHubTopology(L_R, LL_RR) and (L_R, LL_R) and (L_R, L_RR) returns empty routes and a non-empty error. Red: it crashes at HEAD (record the signal).
4. Run all 243 NM1 pairs one by one with NM1_ONE (a shell loop, rc per pair) and report: 0 crashes.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `tools/junction/jt gate build-rel` (count, time).
- One commit (without the NM1 patch): `Fix: Junction seed refuses an L_R main with a multi-lane branch instead of crashing`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus the stack, the refusal text and the 243-pair crash count. When you have reported, stop.
