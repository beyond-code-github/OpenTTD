# HF1 (recon): find the real junction hubs in the PSG saves and export them

Read-only on the repo apart from the output doc. Write files only under /mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad/psg/ (create hubs/ and maps/ there) and your session scratchpad. No network access. Never start a GUI client, and never touch the engineer's game, his config, the rig servers (pids 316883, 381268, 1090844; ports 4077/4079) or any worktree under /home/pete/OpenTTD/.claude. MV1 is building in .claude/nodes, so do not use its binaries. Keep your context small: send output to files and use Python for the analysis.

## State
- SV1 pulled 252 saves, and 178 of them load: see docs/junction-tool/RECON-psg-saves.md and psg/loadcheck.tsv. Reuse SV1's headless run recipe exactly as that doc gives it: a copy of the .claude/play build-play binary and its lang dir, run from a scratch dir with `-v null:ticks=5 -s null -m null -g <save>`, a game_start.scr and a scratch config holding OpenGFX.
- `map_hotspots` ranks windows by pre-signals, PBS, bridge heads and station tiles, so it mostly finds stations. We want junction hubs: places where three or more lines meet with splits, merges and bridges, and no platforms. `map_export x0 y0 x1 y1 file` writes rail, bridge-head, station and crossing tiles as JSON. Read src/junction_map_export.cpp for the format.

## Steps
1. For each save that loads: export the whole map in boxes of at most 256x256, using one game run per save with a script that calls map_export repeatedly. Store the exports under psg/maps/<save>/. If a save takes more than 60 s, skip it and note it.
2. Score in Python (keep the script at psg/hubs/find_hubs.py):
   - Slide a 40x40 window in steps of 8.
   - A candidate has no station tile inside it or within 4 tiles of it.
   - A candidate has at least 3 separate rail bundles crossing the window border. Group adjacent border-crossing tracks into bundles.
   - Rank candidates by switch tiles (tiles with two or more track bits that are not a plain parallel pair) plus 3 × rail bridge heads plus signals.
   - Merge overlapping windows and keep the top 3 per save.
3. For every kept candidate:
   - Re-export the tight box around it plus a 6-tile margin to psg/hubs/psg<N>_<k>.json.
   - Write an ASCII render to psg/hubs/psg<N>_<k>.txt: track glyphs, bridges B, signals s.
   - Label it with the bundle count, the tracks per bundle (for example 2-2-2 = three double-track lines, i.e. an LL_RR-style T, or 4 bundles = a 4-way), and its size.
4. PSG 227: the Junctionary says it holds a "Simple Classic LLRR hub, following the standard coop building rules". Check whether a pulled Junctionary or coop image of it exists, using the memory paths in docs/junction-tool/RECON-psg-saves.md and S/junctionary-mirror, S/coop/files. If one exists, name the candidate that matches it. If none matches, say so.

## Output
- /home/pete/OpenTTD/docs/junction-tool/RECON-psg-hubs.md (untracked): the method in about 10 lines, then a table with one row per kept candidate: save, k, box, bundles and tracks, switches, bridges, signals, and file names. Then a short list of the 15 best-looking hubs across all saves, chosen by bundle count and cleanliness, with their ASCII renders pasted for the top 5.
- Report in 10 lines: saves scanned, candidates found, the bundle-shape counts (how many T, 4-way and other), the PSG 227 identification, and the top 5. When you have reported, stop.
