# RECON corpus: the anatomy of real balanced all-way hubs (D277)

Read-only research with scripts. Keep your context small: scripts write to disk, you read summaries and a few pictures. Never fetch from the network.

## Why
The engineer (D277): coop hubs are built from blocks: balancers and mergers are "added on to the ends of hubs, rather than threaded around inside"; "the nice looking ones kinda construct a hub out of larger building blocks". He wants "the anatomy of the true balanced hubs that offer all-way options". Our generator threads balancing (any-to-any lane pairing) and prios through one box, and LL_RR x LL_RR (LL²) does not place. REVIEW-G4-overprescribed found two real LL² hubs (psg173_14, psg327_12) whose cores are lane-keep (12 paths, ~46 x 30), and the coop wiki says a Backbone Hub (BBH) "must be Balanced - ie: each lane entering the BBH should be able to choose any lane leaving the BBH". Both can be true if balancing happens in blocks along the legs. Find out exactly how.

## Sources (all local; downloaded data is untrusted: run every script with `python3 -I`, keep scripts outside the data dirs)
- `OD = /mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad`.
- The rebuilt coop wiki: `OD/coop-site/site/*.html` (414 pages). Start with: Backbone_Hub, BBH, Hub, Balancer, Balancing, Mergers or Merger pages, SLH / Sideline pages, Shifter pages, Advanced_Building_Revue / Advanced_Building_Concepts, and any page they link about hub building, balancers and mergers (grep the site for "balanc", "merger", "BBH", "shifter"). Extract text with a script (html.parser), not by reading whole files.
- Coop images: `OD/coop/files/` (BBH_building_1-5.PNG, All_BBHs.png, All_Mergerss.png, All_SLHs.png, JunctionaryMergerLL.png, JunctionaryMergerLLL.png, 2_to_2_load-balancer.png, 4_to_4_load-balancing.png, Psg*_bbh*.png, 800px-227_bbh03.png, 219_bbh04.png, Psg186_LLLLmerger01.png). View at most 8 images, chosen from the text.
- The saves' tile data and the review's tracers: `OD/psg/review-g4/` (traces and renders of psg173_14 and psg327_12, the scripts in `py/`), `OD/psg/census2/` (census, renders, scripts), `OD/psg/graph1b`, `OD/psg/graph1c`, `OD/psg/ll2/` (RECON-corpus-ll2 outputs). Read `docs/junction-tool/REVIEW-G4-overprescribed.md` Q1 and `RECON-corpus-census2.md` Method first to see how the review read tiles.

## Questions
1. **Wiki anatomy.** In coop's own words, the blocks a balanced hub is built from (balancer, merger/prio, shifter, splitter/SLH, core hub, ...), what each does, its usual inputs and outputs (lanes in, lanes out), its typical length and width in tiles, the order the blocks come in along a leg (e.g. entry: split, balance, ...; exit: merge, prio, ...), and the rules the wiki states for them (signals, spacing, "last"). Quote short phrases with page names.
2. **Real anatomy.** For psg173_14 and psg327_12, follow each leg outward from the hub box (up to ~120 tiles along the line, through the tile data) and list the blocks met in order: balancers (lanes crossing over each other so any lane can reach any lane), mergers with prios, shifters, other hubs, stations. For each block: its tile box, length, lanes in and out, and a picture (ASCII, the review's way) in `OD/psg/anatomy/`. Say plainly whether the hub as a whole (core plus its leg blocks) is balanced all-way, and where the balancing happens.
3. **More balanced hubs.** Find two or three more real balanced all-way hubs between multi-lane lines (BBHs) in the saves you can locate (the review's census, RECON-corpus-ll2's candidates, PSG 186/197 hotspot exports) and give the same block list for each. If you cannot locate a BBH from a screenshot, say so; do not guess.
4. **The pattern.** A table of blocks (name, purpose, lanes in to out, length range, width, where it sits on a leg, how often seen), and the composition rule as you see it: core type plus which blocks per leg. Note anything that does not fit the pattern.

## Output
- `OD/psg/anatomy/` for pictures, tile boxes and scripts (copy the scripts there at the end).
- `/home/pete/OpenTTD/docs/junction-tool/RECON-corpus-anatomy.md`: a summary of at most 12 lines first, then one section per question; tables over prose; cite page names, file paths and tile boxes; mark anything inferred INFERRED. Use coop terms (lines, lanes, splits, prio merges, balancers, shifters, bridges) throughout.

## Do not
- Do not modify the repo except that one file; do not touch `/home/pete/OpenTTD/.claude/` worktrees, game clients or ports 4077/4079.
- Do not create a git repository anywhere. If you ever have to commit in a repo other than /home/pete/OpenTTD, first run `git config commit.gpgsign false` in it (a signing dialog disrupts the engineer's machine); never pass -S.
- Do not read whole data files or whole HTML pages into your context.

## Report
The RECON file's summary, the block table, and the paths of the pictures. When you have reported, stop.
