# Brief ST step 1: coop station rules from the local archive (read-only research)

You execute this brief and nothing else. Do not edit any source file. Write exactly one file, `docs/junction-tool/RULES-stations.md`, and **do not commit it** (another agent is committing in this worktree; the orchestrator commits your file). If something is not covered, stop and report. Keep your context small: read the text files in sections, grep, and view only the images you cite.

## 1. Goal

The engineer wants procedurally generated #openttdcoop-quality **stations**, as a parallel track to the junction tool ("I'd LOVE stations to be on our radar").

This step produces the sourced rule base a station generator would rest on. It follows the model of `docs/junction-tool/RULES-coop.md` (read it first for format and tone: a table of rules with ID, rule, why, source quote and generator impact).

## 2. Sources (all local)

- **Blog posts** in `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/abr/`, as `.txt`:
  - 07 stations (the main one);
  - 09 and 13 self-regulating stations;
  - 11 refit stations;
  - 05 S-Bahns and city networks (station parts only);
  - 06 hubs and 10 invisible hubs (only where they discuss station entries or balancers).
- **Wiki text** in `.../scratchpad/txt/`: Glossary, Ruleset, Junctionary, Merging_Tracks, Mainline. Use only the station-related passages.
- **Images** in `.../scratchpad/coop/files/`, about 37 station-related ones:
  - `ls | grep -i "station\|roro\|terminus\|platform\|balanc"`;
  - the `Junctionary*Stations.png` overview sheets;
  - `RORO1.png`, `Pre_balanced_roro.PNG`, `Non-blocking_terminus.png`, `Dedicated_terminus_*.PNG`, `Pbs_terminus.jpg`, `Station_entry_load-balancer.png`, and the PSG239 ones;
  - `abr/images/` (the blog's images).
  View images with the Read tool, and only those you need to state a rule. Cite each by filename.
- `docs/junction-tool/IMAGE-INDEX.md` may already caption some images; reuse its captions.

## 3. What to extract

Sections, each with a rules table:
1. **Station types:** ro-ro (drive-through), terminus, combined pickup/drop, and the Junctionary's taxonomy (basic, pickup, drop, combined, special-case). When coop uses each.
2. **Platforms:** count versus traffic, platform length versus train length, and synced stations (equal path lengths).
3. **Entry:** splitting to platforms, balancing (what coop means by it), pre-signals versus PBS at the entry, entry-side waiting bays, and the "choice" rules.
4. **Exit:** merging platform exits back to the line (prio or not), and exit signalling.
5. **Termini:** non-blocking termini, how reversing trains cross the entry, and PBS termini.
6. **Self-regulating stations** (ABR09/13): what problem they solve, and the mechanism in one paragraph.
7. **Refit stations** (ABR11): the same, briefly.
8. **Signals in stations:** types, placement and spacing.
9. **Parameters a generator would need:** platforms, TL, traffic, ro-ro or terminus, side of line, and so on.

## 4. Output

`docs/junction-tool/RULES-stations.md`:
- the sources list with short names;
- the sections above;
- "What the sources don't say";
- "Open questions for the engineer", at most 8.

Every rule has a short verbatim quote and a source.

## 5. Report

- the file path
- a 10-line summary of the most important rules
- the open questions
- `git status --porcelain` (expect your new untracked file, plus whatever else is in progress)

When you have reported, stop.
