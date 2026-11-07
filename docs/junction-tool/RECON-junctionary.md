# Recon: #openttdcoop junctionary and terminology

Web research, 2026-11-07. Sources are the archived #openttdcoop wiki (`https://web.archive.org/web/2015id_/http://wiki.openttdcoop.org/<Page>`) and the current OpenTTD wiki.

## What exists

- **The current OpenTTD wiki Junctionary is not empty.** `wiki.openttd.org/en/Community/Junctionary` is only TrueWiki's folder page. The content lives at `/en/Community/Junctionary/Main Page` and its sub-pages: 52 pages and 157 images, stored in the `OpenTTD/wiki-data` GitHub repo.
  - `tools/fetch_junctionary.py OUT_DIR` mirrors them all and writes an `index.md`.
  - The 3-way designs there are Basic 3-Way, Compact 3-Way, Compact 3-Way Branch-Merge, Half Cloverleaf, Half Spaghetti, Half Transmogrified, Line Merge, and Ultimate 3-way (2 and 4 tracks).
  - **Compact 3-Way** is an L_R T-junction with all movements, built with one mainline bridge and one tunnel, i.e. two grade separations.
  - These are general-community designs, not coop-rule designs. Many have tight curves.
- **The original coop Junctionary is archived,** but it's mostly thumbnail galleries with one-line descriptions and no layout specifications.
  - The relevant pages are `Junctionary_-_BBHs_-_3-way_-_LLRR` and `Junctionary_-_New_SLHs`, plus `Merging_Tracks`, `Mainline`, `Ruleset`, `Glossary` and `Backbone_Hub`.
  - In coop vocabulary, a 3-way hub on mainlines is a **BBH** (backbone hub) and a sideline junction is an **SLH**.
- **No L_R (one track per direction) grade-separated T-junction is documented.** Coop mainlines are LL_RR or wider, and LR lines are sidelines ("We build sidelines (LR, sometimes LLRR)", Ruleset).

## Terminology (quoted)

- **CL:** "Curve Length. Mostly used to describe how big a curve must be to let trains with a certain train length pass at full speed." (Glossary)
- **TL:** "TrainLength. Total length of a train measured in the amount of tiles it occupies." (Glossary)
- **Signal distance:** the Glossary has no SD entry. Mainline says "Signalling distance is every other tile", i.e. SD2.
- **LL_RR notation:** "LL are two left direction tracks, RR are two right direction tracks. The '_' indicates space between the directions. Taken literally, it is only one tile, but usually more tiles are used to ease expansions and hub construction." (Mainline)
- **Gap between directions:** "On 2+/2+ lines (Mainlines) be sure to leave some spaces in between the two lanes (the more the better, depending on space restrictions)." (Ruleset). No minimum number is given.
- **Curves:** "No 90 degree turns!" and "No 2*45 degree curves, due to too high speed penalties." (Ruleset)
- **Doubled bridges:** never defined on a page of their own.
  - Backbone_Hub lists them among the coop building standards.
  - Merging_Tracks shows a line split onto two parallel bridges (or tunnels) and merged again, signalled with basic block signals and no entry check, so it "is able to unblock itself" (image `Mergers_bridges_basic.png`).
  - Glossary: "Destructive Interference" (evil mode) is "trains exiting a double-bridge or tunnel interfere with each other's progression".
  - In coop designs, doubled bridges appear mainly in mergers and balancers on LL+ mainlines, not in simple crossings. That fits the engineer's observation that they're rare in junctions.

## Downloaded images

They're in the session scratchpad, `junctionary/t1.png`–`t6.png`. Most are 140 px thumbnails. `t6.png` (700×338, `Mergers_bridges_basic.png`) is the only full-size one, and it's the clearest picture of doubled-bridge signalling.
