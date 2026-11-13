# S0b: land the band chain heading fix (REVIEW-port-fans F1, F4) with tests that fit it

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes. Run `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-s0b <head>` with the head given in your launch prompt. Builds `build` (Debug), `build-rel` (Release). Ignore docs/junction-tool/wip/. Never use git stash.
- S0 (docs/junction-tool/briefs/S0-band-headings.md, read it: its rule, change and CLAUDE.md line stand) stopped only because its test 1 premise was wrong: `L_R` x `L_RR` has two S out lanes, which need the S fans (a later step, S2) to place. The S0 change itself kept every existing test and identity unchanged. Its diff is /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/s0-stopped.patch.
- Red evidence already recorded (before the change, `L_R` x `L_RR` through `PlacementOf(t, 64, &error)`): "the placement rules contradict each other: bridge over S-bend (edge 9) <- chain run (edge 9) <- chain run (edge 2) <- S port spacing <- chain run (edge 14) <- chain run (edge 5) <- S-bend size (edge 5) <- chain run (edge 1) <- S-bend size (edge 1) <- ...". After the change: "chain run (edge 11) <- S-bend size (edge 11) <- S-bend size (edge 12) <- chain run (edge 6) <- bridge over S-bend (edge 6) <- ...".

## Change
1. `git -C /home/pete/OpenTTD/.claude/nodes apply` the S0 patch.
2. Replace its test with these two (tags as named; in src/tests/junction/topology/junction_topology.cpp, same place):
   a. "placement: S branch groups of several lanes place (D230, S0-S2)", tags `[junction][topology][.]` (hidden: the outer test of the S steps, green at S2). Asserts `PlacementOf` gives an empty error for `L_R` x `L_RR` and for `L_R` x `LL_RR`. Comment above it: "Hidden until S2 (S fans) lands; REVIEW-port-fans Q4."
   b. "placement: an exit split on a main row no longer reads as heading north (REVIEW-port-fans F1)", tags `[junction][topology]`: `L_R` x `L_RR` through `PlacementOf(t, 64, &error)`; assert `error` does not contain "S-bend size (edge 1)" and does not contain "S port spacing" (edge 1 is the In edge from the exit split on row 0; before the fix its Corner was read heading north, and the row-node relations printed as "S port spacing"). Before writing it, confirm with the graph that edge 1 of this topology starts at a row node (`order.node_level` odd) and say so in the report; if not, stop.
   Run both before the change (revert the source part of the patch, keep the tests) to record red for (b): it must fail on the "S-bend size (edge 1)" check. Then re-apply.
3. Report-only probe, not committed: on top of the change, apply /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/sf1-stopped.patch (with `--3way` if needed; if it does not apply cleanly, skip the probe and say so). Run test (a) explicitly (`build-rel/openttd_test "placement: S branch groups of several lanes place (D230, S0-S2)"` or via the test binary's name filter) and report each topology's error text verbatim. Then revert the SF1 patch completely before the commit (`git diff --stat` must show only the S0 files).

## Do not
- Change anything in the S0 source change beyond applying it. Do not change any existing test.

## Stop conditions
- Any existing test or identity number changes.
- Edge 1 of `L_R` x `L_RR` is not an edge from a row node.

## Gates
- After: Debug and Release build with no new warnings (known: `unused variable 'n'` in junction_topology.cpp, `unused function 'TravelY'` in the test file, 10 `-Wmissing-field-initializers` in legacy/junction_nodes.cpp); `tools/junction/jt gate build-rel` passes with 1 case added (baseline 498 cases, 3042017 assertions); `tools/junction/jt identity build-rel` unchanged (4 cases, 255 assertions); `tools/junction/jt fast build '[topology]'` within budget.
- One commit: `Fix: Junction band chains start on the edge's real heading`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
Commit hash and subject; red evidence for (b); counts; the probe's two error texts (or why skipped); stops; `git status --porcelain` and `git log -1`.

When you have reported, stop.
