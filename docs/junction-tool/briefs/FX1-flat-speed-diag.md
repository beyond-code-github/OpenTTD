# FX1: why the flat T fails at some design speeds (diagnosis, D185 part 1)

You execute this brief and nothing else. Diagnose; do not commit source changes. If something is not covered, stop and report. Keep your context small: output to files, report only what matters.

## Facts (DIAG-near.md, B0b)
- ConstructJunctionTMap for the flat case (terrain nullptr, the flat map GenerateJunctionNear builds first, src/junction_game.cpp:427-435) fails for the design train `train=14194,1090000,561,25,196` at max speed 140, 160, 196 km/h and builds at 100, 120, 180. Log: "height 11 A->S failed ... height 14 no valid skeleton".
- The test spec (MakeTestJunctionSpec) builds. Rail price 75 or bridge prices 800+200n also make it fail (B0b scratch test).

## Questions
1. At 160 km/h with the design train (prio sized by SizeJunctionPrio as junction_export does without prio=), for each height tried: which edge or check fails, and the exact reason (the router's failure, the skeleton's cycle rules via FindCycle/CycleRules). Why does the height loop stop at 14 (the bound and where it comes from)?
2. Is the failure a bound (heights, box width, expansion cap), a rule that no flat layout of this spec can meet, or a cost/price effect in the router? Show the evidence (e.g. raising the bound in a scratch build makes it build at height h; or a rule list).
3. Propose the smallest fix that makes the flat T build on flat land for every spec, derived from the rules (no padding): the change, file:line, and what it costs (time per flat build at 100/160/196 km/h before and after, measured in a scratch build).

## Setup
Worktree: `git -C /home/pete/OpenTTD worktree add /home/pete/OpenTTD/.claude/fx1 feature/junction-tool --detach`; build Release there (`build-rel`, clang-18, ccache), target `openttd_test`; a scratch Catch2 test in `src/tests/junction_map.cpp` or a new scratch file, not committed. Build the spec as junction_export does (read ConJunctionExport in src/console_cmds.cpp ~320-440: notation L_R L_R, tl 4, the train fields, SizeJunctionPrio). Tests only through `tools/junction/jt` or the test binary with one exact test name; output to `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/fx1/`.

## Output
`docs/junction-tool/DIAG-flat-speed.md` (at most ~50 lines): answers with file:line and evidence, the proposed fix and its measured cost. Commit only that file on feature/junction-tool in /home/pete/OpenTTD: `Doc: Junction diagnosis of flat T failures by design speed`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Remove your worktree afterwards (`git worktree remove --force`). No `pkill`, broad kill, bare `git stash`.

## Report (short)
One line per question, the commit hash. When you have reported, stop.
