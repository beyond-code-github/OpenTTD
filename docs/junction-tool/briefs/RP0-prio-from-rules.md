# RP0: prio length from the rules, not from the old generator's sweep (read-only recon)

You do read-only research and write one file. Do not change code, build, or commit. Keep your context small: grep, then read with offset/limit. Mark anything reasoned rather than read as INFERRED.

## Goal
prio_tiles (the straight prio coverage before a main-line merge) is today found by SizeJunctionPrio, which sweeps the old generator (GenerateJunctionUnchecked) and fails at 140/160 km/h in game. Under D189/D193 every length comes from the rules. Find the rule that defines the needed prio length and whether it can be computed directly from the spec (train, speed, braking, signals) in microseconds.

## Read (once each)
`src/junction_generator.cpp` and `.h`: SizeJunctionPrio and everything it calls to decide a prio length (grep, then offset/limit); `src/junction_rules.cpp`: the Prio and PrioReach checks (what length they require and from which formula); DECISIONS.md rows D108, D148, D148a, D159 and any row SizeJunctionPrio's comments cite (grep by id); `docs/junction-tool/RULES-coop.md` (grep prio); `docs/junction-tool/DIAG-flat-speed.md` (the 140/160 sweep failure note); `docs/junction-tool/AUDIT-rule-coverage.md` (Prio rows).

## Write `docs/junction-tool/RECON-prio-rule.md` (under 100 lines)
1. What SizeJunctionPrio does, step by step, with file:line; what it measures (the checker's verdict? a train simulation? a formula?).
2. The rule(s) that make a prio length legal or sufficient (checker's Prio/PrioReach, decisions), as formulas over spec fields, with sources.
3. Whether the minimal sufficient prio length is a closed form of the spec; if yes, the formula and its values for the test spec and for train=14194,1090000,561,25 at 100/120/140/160/180/196 km/h, compared with the measured 9, 8, 10, 11, 12, 14 (test spec prio_tiles 14); if no, what part needs a search and over what.
4. Why the sweep fails at 140/160 (from the code; INFERRED where reasoned).
5. Numbered open questions for the orchestrator.

## Report
File path, line count, the formula (or why none), and the open questions. When you have reported, stop.
