# Recon: what coop means by "not bridging the prioritised line" (read-only)

Read-only: no code edits, no builds; commit only your findings file. Never use git stash, never start a game client, never touch ports 4077/4079. Light work only (another probe is building/testing on this machine: no builds, no test runs). Time box 40 minutes. Cite every fact (file:line, page heading, image name); mark inferences "I". Quote the corpus wording exactly where it states a rule. If the corpus does not settle a point, say so; do not invent.

## Why
Ruling D347 (`/home/pete/OpenTTD/docs/junction-tool/DECISIONS.md`, rows D107, D108, D342, D347): a user toggle "bridge the prio line"; when off, the solver must keep the coop rule. I wrote it as "no bridge or switch in coverage"; the engineer: "I'm not sure the rule is as simple as 'bridge doesn't sit within the coverage' - maybe it's worth checking the corpus for the exact meaning".

## Sources
`S=/mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad`: `S/coop-site/site/` (Priorities.html and any page grep finds for bridge + prio/priority/evil), `S/coop/files/` (images and archived wiki pages), `S/junctionary-mirror/pages/` (Priority Merge and any other hit), `/home/pete/OpenTTD/docs/junction-tool/RULES-coop.md` (R25, R31, R40, R51 and their sources), `wip/RESEARCH-prio-freedom.md`, `wip/RECON-prio-bridge.md`. Grep wide (bridge, tunnel, evil, "entry check", "can't stop", prio, priority, presignal) before reading.

## Answer (findings under 40 lines in `/home/pete/OpenTTD/docs/junction-tool/wip/RECON-prio-bridge-rule.md`)
1. Every corpus statement about bridges/tunnels and prios, quoted, with source.
2. Exactly which track the rule covers: the main line inside coverage; the main line upstream where a held train would stand or brake (between which signals); the prio row; the joiner (where the joiner is held at its Entry signal); the merge's out line; anything else. For each: stated (quote), implied (I, why), or not covered.
3. The mechanism behind it (why a bridge breaks the prio: e.g. a train on a span cannot be stopped / is one block member / "evil mode"), as stated.
4. Is a tunnel treated like a bridge? Is a bridge carrying the row (standard prio over a bridge) or the joiner treated differently?
5. A one-paragraph rule in our terms (coverage units, signal blocks, TL) that the toggle-off case should enforce, marking each part F or I.

Commit only that file on feature/junction-tool in /home/pete/OpenTTD (message "Doc: Junction recon of the coop no-bridge prio rule", ending "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"). Report in under 10 lines, then stop.
