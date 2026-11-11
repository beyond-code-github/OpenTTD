# RC0: which checker rules the node template already guarantees (read-only audit, D193)

You do read-only research and write one file. Do not change code, build, or commit. Keep your context small: grep, then read with offset/limit. Mark anything reasoned rather than read as INFERRED.

## Goal
D193: the node model must get every rule right by construction, so CheckJunctionRules is only a test oracle. List every rule the checker checks and say whether the node template (DESIGN-N3 §1-§5, src/junction_nodes.cpp) guarantees it, and if not, what constraint would.

## Read (once each)
`docs/junction-tool/DESIGN-N3.md`; `src/junction_rules.h` (the rule enum and CheckJunctionRules); `src/junction_rules.cpp` (each rule's check: grep the enum names, read each function with offset/limit); `src/junction_signal_pass.h` (what PlaceJunctionSignals guarantees); `src/junction_nodes.cpp` (BuildJunctionNodeTemplate's rules: AddPathDiffs, AddCrossings, AddBoxEdges; SolveJunctionNodes' facts); `docs/junction-tool/RULES-coop.md` headings only; DECISIONS rows cited by the rules you read.

## Write `docs/junction-tool/AUDIT-rule-coverage.md` (under 150 lines)
A table, one row per checker rule (enum value): rule; what it checks (one line, with file:line); decision ids; covered by the template? (yes: which rule of DESIGN-N3 §2/§5; by the signal pass: say so; partly; no); if not fully, the constraint that would cover it, stated as a difference constraint, a domain restriction, a structure rule or "needs design"; and whether it can fail on the flat T at other speeds or on terrain (INFERRED). Then: collisions between unrelated tracks (no rule id?) and anything the checker does not check but the game needs (e.g. track overlap, 90-degree turns): same columns. End with a numbered list of the uncovered rules ordered by how likely they fail in the node search (house/hub/incline cases, 100-196 km/h).

## Report
File path, line count, the number of rules covered / partly / not, and the ordered list. When you have reported, stop.
