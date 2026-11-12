# C5a (recon by experiment): why the prio element breaks under a row stretch on LL_RR (RECON-stretch C5; D217)

Read-only recon by experiment: you may change code in your worktree to measure, but commit nothing and leave the worktree clean (`git checkout -- src`; delete scratch files). Keep your context small: send command output to files and report only what matters. Never use git stash. Mark anything not measured or read as INFERRED.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git checkout -B wt-c5a <HEAD>` (given at launch).
- `[topologystretch]`'s Y part (y_floors: each stack boundary +1 or +2) counts rule 9 (Prio) instead of requiring it clean: LL_RR test spec, 3 runs, Prio x6 at (55,6), (55,5) ("joiner's signal must be Entry / missing prio") at items 3, 4, 5 with s 1. Everything else is clean under row stretches. D217: the fix must be a rule family, not a site relation.

## Read first (once each, offset/limit)
- DESIGN-G1.md §12.4 (the prio element), RECON-prio-element.md; RECON-stretch.md; DECISIONS D108, D148, D217.
- src/junction_topology.cpp the prio element relations ("prio element" reasons), SignalJunctionTrackRoutes (the prio element and the joiner's Entry signal); src/junction_rules.cpp the Prio rule's checks (by message).
- src/tests/junction_topology.cpp `[topologystretch]` (the Y part).

## Experiments (Release)
1. Reproduce the three runs; for each: the routes and pieces at (55,5), (55,6) before and after the stretch (RenderJunctionAscii of the area to a file), the signal tokens there, and which check of the Prio rule fails and why (the rule's own reasoning, file:line).
2. Which placement relation or signal-token rule lets the stretched layout put the joiner's signal or the prio coverage where the rule rejects it: the relation (reason chain via Explain on the involved points) or the token rule (file:line).
3. PROPOSED: the missing relation as a family over every prio element (or the token rule's fix), derived from the Prio rule; CHECKED by experiment: the three runs clean, every other stretch run still clean, the seeds unchanged (SamePlacement), the 44 sweep cells clean.

## Output
docs/junction-tool/RECON-prio-stretch.md (untracked, ~60 lines).

## Report
The conclusions in 8 lines, the proposal with its check, `git status --porcelain` (clean), `git log -1`. When you have reported, stop.
