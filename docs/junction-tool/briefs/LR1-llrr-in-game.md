# LR1: the LL_RR hub in the hover: topology from the notations, measured on real terrain (engineer: "I'd like to prioritise building a real LL_RR hub in game")

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1 (Debug `build`, Release `build-rel`, both stale: rebuild, ccache is on). First: `git -C /home/pete/OpenTTD/.claude/ng1 checkout -B wt-lr1 <HEAD>` (given at launch; branch wt-ng1 there holds old WIP: leave it). Never use git stash. Other agents work in /home/pete/OpenTTD/.claude/nodes and .claude/nodes2: never touch them. Never start a game client or touch the engineer's game or rig servers. Use absolute paths.
- The GUI's projection path makes its seed from `JunctionTTopology()` only (src/junction_gui.cpp StartProjectionJob ~:884-900: `MakeJunctionGraphSeed(JunctionTTopology(), job->spec, &error)`), whatever the Mainline/Branch notations say. `JunctionLLRRSidelineTopology()` (src/junction_topology.h ~:396) is the LL_RR main line with an L_R sideline; it places, renders and signals clean on flat ground (the circuit `[.circuitllrr]`, the `[topology]`/`[topologysweep]` seeds) but the hover sweep on the 48 real fixtures (`[realterrainsweep]` "hover plan on real terrain", src/tests/junction_real_terrain.cpp) runs only the T.
- Window notations: Mainline and Branch edit boxes (e.g. "L_R"); the window parses them (find where: ml_gap/branch_gap come from them).

## Read first (once each)
CODINGSTYLE.md (C++ part); src/junction_gui.cpp (notation parsing, SpecKey/seed_key, StartProjectionJob, the seed cache); src/junction_topology.h the two topology accessors and what JunctionLLRRSidelineTopology's notations are (main "LL_RR", branch "L_R"? confirm from its data); src/tests/junction_real_terrain.cpp the hover sweep.

## Changes
1. **Topology from the notations** (src/junction_gui.cpp): a file-local `static const JunctionTopology *JunctionTopologyForNotations(std::string_view mainline, std::string_view branch)` returning the T for the T's notations, the LL_RR sideline topology for its notations (whatever its data says they are), nullptr otherwise. The seed is made from it and cached per (topology, spec) (the seed_key gains the topology pointer, or the notations already in the key select it: say which). nullptr -> the hover status says the notations have no topology yet (an existing string if one fits; else one new string `STR_JUNCTION_STATUS_NO_TOPOLOGY :{BLACK}No hub layout for these lines yet`).
2. **Measured on real terrain:** the `[realterrainsweep]` hover test gains an LL_RR pass, the same as the T's (PlanJunctionHover and PlanJunctionHoverNear at the centred offset; heights T3 on the terraformed terrain, CheckJunctionRules clean, height cost equal on re-placement), printing `HOVERPLAN_LLRR ...` and `HOVERNEAR_LLRR ...` lines (fixtures, planned, moved, ms_mean, ms_max). Floors = the measured planned counts (may be 0: then CHECK nothing and report). No 100 ms REQUIRE for LL_RR in this step: report the times; if near ms_max > 100, report the slowest 5 and their stage times (placement / levels / render+signals, temporary timers removed after).
3. TESTS.md rows.

## Stop conditions
- An LL_RR plan fails T3 or CheckJunctionRules on real terrain (report fixture, offset, rule/tile): a real defect, do not paper over it; the LL_RR topology's notations cannot be typed in the window's edit boxes (report what they are); anything not covered.

## Gates
- `tools/junction/jt fast build '[hoverplan]' 5`; `build-rel/openttd_test "[realterrainsweep]"` (all lines); `tools/junction/jt gate build-rel` (count, time); the `openttd` target builds Debug and Release.
- One commit: `Feature: Junction hover picks the hub topology from the line notations (LL_RR with an L_R sideline)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus the notations to type for LL_RR, the LL_RR counts and times, and what the engineer should try in game. When you have reported, stop.
