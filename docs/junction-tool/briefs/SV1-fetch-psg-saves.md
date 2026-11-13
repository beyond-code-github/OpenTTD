# SV1: pull the #openttdcoop showcase save games (PSG) that the Junctionary links for hubs, and check we can read them

Recon/fetch task. Network access is allowed ONLY to the public #openttdcoop archives (web.archive.org copies of openttdcoop.org / wiki.openttdcoop.org, and any public mirror the archived pages link to). Send no personal data in requests (generic User-Agent; no email). Write files only under /mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad/psg/ and the output doc. Never start a GUI game client or touch the engineer's game or rig servers (pids 316883, 381268, 1090844). Keep your context small.

## State
- The pulled corpus (memory: OneDrive Documents/OpenTTD-junction-tool/scratchpad/coop/ and junctionary-mirror/; tools/fetch_coop_archive.py there and docs/junction-tool/tools/fetch_junctionary.py) has the Junctionary pages but not the saves. RULES-hubs.md (docs/junction-tool/) lists hub-showcase games linked as "Download PSG #N": PSG 115, 173, 186, 188, 191, 197, 204, 206, 219, 227, 248, MainserverGame_04; PSG227 holds "Simple Classic LLRR hub, following the standard coop building rules" (mfb); RECON-junctionary.md lists the Junctionary BBH pages.
- The engineer: "Oh definitely pull the save games".

## Steps
1. From the pulled Junctionary/BBH pages (coop/txt or pages under the mirror), collect the PSG download links and the hub each one shows (page, caption, coordinates if given).
2. Download each save (prefer the final save of the game the hub is in; web.archive.org `id_` URLs), into scratchpad/psg/ with its original name; record URL, size, sha256.
3. Check each opens in this OpenTTD: use the existing Release build's dedicated server in a throwaway config dir (e.g. `/home/pete/OpenTTD/.claude/nodes/build-rel/openttd -D 127.0.0.1:0 -c <scratch cfg> -g <save>` with a short timeout, or the `-x`/headless options the repo's tools use; read docs/junction-tool for rig_coop.sh's pattern) and report: loads / missing NewGRFs (list) / version error. Do not change the engineer's config; use a scratch config dir.
4. For saves that load: say how a region can be exported (the repo's `junction_export` console command or `junction_terrain_dump`: find them and their arguments) and export the hub regions named on the Junctionary pages if coordinates are known; else note what is needed.

## Output
/home/pete/OpenTTD/docs/junction-tool/RECON-psg-saves.md (untracked): per PSG: URL, file, loads?, missing GRFs, hub location, export done?

## Report
10 lines: saves pulled, which load, missing GRFs, which hubs are exported and where. When you have reported, stop.
