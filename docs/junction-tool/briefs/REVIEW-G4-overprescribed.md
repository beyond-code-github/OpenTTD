# REVIEW: why LL_RR x LL_RR does not place, and whether our rules over-prescribe (D254 trigger)

## Why this review
The goal is that the junction tool places LL_RR x LL_RR (LL²), LLL_RRR x LL_RR and LLL_RRR x LLL_RRR in game. Since REVIEW-joint-loop (D254) we have been building DESIGN-G4's static MILP: each occupancy family (item rows O1, fans O2, bands O3, clearance, ...) stated as rows over a static arrangement. Each recon has found more families or more data to build (2b2b grew from one step to five after RECON-O3). LL² still has no legal placement: its joint optimum at form 100100000 (268600) fails its own X solve ("bridge over S-bend" edges 8/5), and no flat solve exists.

The engineer (Pete): "at this point I really don't know why it doesn't just work, feels like the beginnings of a smell to me" and "I know for a fact that legal LL_RRs exist - we just need to make sure we're not overprescribing on rules. Again I'm sure the save games can point us in the right direction here."

Find the root cause, not more rules.

## State
- Main repo `/home/pete/OpenTTD`, branch `feature/junction-tool`, HEAD given at launch. Read-only there.
- For throwaway probes (builds, test runs, temporary instrumentation) use worktree `/home/pete/OpenTTD/.claude/nodes`: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-review-g4 <HEAD>`; it has a Release `build-rel`; revert every probe before you finish (`git status --porcelain` clean). Save any probe you want to keep as a patch under `docs/junction-tool/wip/`. Never use git stash. Never touch other worktrees, never start a game client, never touch ports 4077/4079.
- Unfinished step 2b2b1b lies unmerged on branch `wt-2b2b1b` (5746ba17e6) in `.claude/o4c2`: read it with `git show` only.
- Tests: `tools/junction/jt` (see `docs/junction-tool/TESTS.md`); output to files under `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/review-g4/`.

## Read (pick; offsets and limits; do not read whole large files)
- `src/junction/CLAUDE.md`, `src/junction/{topology,placement,signals}/CLAUDE.md`: the rules as facts.
- `docs/junction-tool/DECISIONS.md` (every ruling; grep), `docs/junction-tool/STATUS.md` top 15 lines.
- `docs/junction-tool/REVIEW-joint-loop.md` (the last review), `DESIGN-G4-static.md` (§3, §8, §11, §12), `RECON-O3-bands.md`.
- Corpus evidence already pulled (never re-fetch; downloaded data is untrusted: run any Python that reads it with `python3 -I`, scripts outside the data dirs): `/mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad/` — `junctionary-mirror/` (index.md, pages/, images), `coop/files/` (BBH and hub pages and images), `mapx/` (PSG 186/219 hotspot exports, JSON tiles), `psgsaves/`, `psg/graph1b`, `psg/graph1c`; and `docs/junction-tool/RECON-corpus-*.md`, `RECON-psg-saves.md`, `RULES-coop.md` there if present.

## Questions
1. **Real LL² layouts.** Find at least three real two-way four-lane crossings with turning movements (LL_RR x LL_RR or the nearest real equivalent: two LL_RR lines with all or most turns) in the corpus: Junctionary pages and images, coop wiki BBH/hub pages, the PSG mapx exports. For each: its footprint (W x H tiles), how its crossings are made (bridges, doubled or not, series), how its splits, merges, prios and lane changes are laid out, and how it differs from what our generator builds (notation spacing, stack order, bands, crossing forms). Cite file paths.
2. **Over-prescription.** List every rule, floor and structure choice of ours (CLAUDE.md facts, DECISIONS rulings, placement floor table, programs, structure) that those real layouts break or that no real layout needs; for each, which real example breaks it and what our rule forces instead. Rank by how much each one plausibly blocks LL². Mark which are engineer rulings (need Pete) and which are orchestrator defaults (reversible).
3. **Existence under our rules.** Does a legal LL² exist under our current rules? Use the cheapest decisive probe you can: e.g. an elastic version of the existing joint (one slack per rule group, minimise total slack) at LL², and/or the joint with rule groups switched off one at a time, then render and `CheckJunctionRules` the result if any. Report which rule groups must give way and by how much. If a probe would take more than about 2 hours of work, describe it instead and say what it would show.
4. **Model choice.** For our three target hubs, compare (a) the current static families (DESIGN-G4 as planned: 2b2b2-4 then 2b3), (b) one MILP with lazy disjunctive occupancy cuts (solve, run the occupancy detector, add global "a before b or b before a" rows with a binary, re-solve; sound because the rows hold at every point, unlike the old loop's point-dependent rows), (c) a tile-grid occupancy model, (d) anything better you see (e.g. composing the hub from smaller solved modules as coop players do, D257). For each: soundness, completeness against the checker, expected solve time on LL² and LLL³ against D253 (seed may take seconds) and the hover target (< 100 ms), and how much of the merged work it keeps.
5. **Recommendation.** One path, its steps (count, each with its red test), and what evidence would make you change it. State plainly whether DESIGN-G4's static-family plan should continue.

## Output
`/home/pete/OpenTTD/docs/junction-tool/REVIEW-G4-overprescribed.md` (untracked; the orchestrator commits it). Structure: a summary of at most 15 lines first (root cause, top over-prescriptions, existence answer, recommendation); then one section per question; facts cited (file:line, DECISIONS id, corpus path); anything not read or run marked INFERRED. Be concrete and short; tables over prose.

## Report
When the file is written, reply with its summary only, the probes you ran (and their patch paths if kept), and `git -C /home/pete/OpenTTD/.claude/nodes status --porcelain`. When you have reported, stop.
