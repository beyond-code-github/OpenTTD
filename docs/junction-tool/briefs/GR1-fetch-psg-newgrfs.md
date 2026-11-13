# GR1: get the NewGRFs the PSG saves need, so they load and run

Network access is allowed only to:
- (a) OpenTTD's official content service (BaNaNaS), through the game's own `content` console command, or through bananas.openttd.org / content.openttd.org downloads;
- (b) the public #openttdcoop archives, meaning web.archive.org copies of openttdcoop.org, including the ottdc_grfpack.

Send no personal data and use a generic User-Agent. Never start a GUI client. Never touch the engineer's running game or the rig servers (pids 316883, 381268, 1090844; ports 4077/4079). Never use or modify worktrees under /home/pete/OpenTTD/.claude other than READING the binary at .claude/play/build-play. Keep your context small: write output to files and use Python for the tallies.

## State
- 252 saves are in /mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad/psgsaves/dl/. The load check is in .../scratchpad/psg/loadcheck.tsv, and docs/junction-tool/RECON-psg-saves.md gives the headless run recipe and the load results.
- Missing NewGRFs are disabled at load, so the games load but don't run properly. About 28 saves fail outright with "Road vehicle on invalid road type", which means road-type GRFs are missing. 17 crash on load.
- The engineer's GRFs live in ~/.local/share/openttd/newgrf/ and content_download/newgrf/. The game finds GRFs there whatever `-c` config is used, so his GUI client and the headless runs both see them. Add files only. Never delete or overwrite an existing file there.

## Steps
1. Inventory. For each save, collect the GRF list with ID, md5 and filename. Get these from the load logs; the client logs "NewGRF <id> (<path>) not found; checksum <md5>". Tally every unique (ID, md5) pair that is missing, with the number of saves that need it. Write psg/grf_missing.tsv.
2. Fetch, preferring an exact md5 match:
   - BaNaNaS via the `content` console command in a headless run: `content update`, `content state`, `content select <id>`, `content download`. Read src/network/network_content.cpp and the console's `content` help for its exact use.
   - The archived ottdc_grfpack, or individual GRFs on the coop archive.
   - Install into ~/.local/share/openttd/newgrf/psg/, which is a new subfolder; files only.
   - Where only another version with the same GRF ID exists, take the newest one. OpenTTD loads a "compatible" GRF by ID.
3. Re-run the load check on every save with the same recipe as SV1, and write psg/loadcheck2.tsv. For each save, record: loads?, still-missing GRFs, compatible substitutions, and whether it runs 200 ticks without a crash. Use `-v null:ticks=200`.
4. For the saves that still crash on load, record the crash message from the crash log's first lines. Do not debug them.

## Output
- Append a "NewGRFs" section to docs/junction-tool/RECON-psg-saves.md with:
  - the GRF table: id, name, saves needing it, source, exact or compatible, fetched?;
  - before and after load counts;
  - the GRFs still missing and where they might come from.
- Report in 10 lines: GRFs needed, GRFs fetched (exact and compatible), what could not be found, the before and after load and run counts, and the crash causes. When you have reported, stop.
