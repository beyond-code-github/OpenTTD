# RECON: PSG showcase saves (SV1)

Files: `/mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad/psgsaves/dl/` (original names; the first 12 are `PSG_N.sav` from an earlier pull, the rest `PublicServerGame_N_Final.sav`). Manifest (URL, size, sha256): `psgsaves/manifest.tsv` (new pull; the 12 older ones: sizes below). CDX listings: `psgsaves/cdx_domain_psg.txt`, candidates `psg_candidates.txt`.
Source of the new pull: web.archive.org `id_` copies of www.openttdcoop.org/files/publicserver_archive and /files/PublicServer_archive. ps.openttdcoop.org/public/save has NO archived saves (only autosaves and 404s; the 2000-line index was all autosaves, not truncated saves).

## Totals
- Archived PublicServerGame_N_Final.sav with a 200 capture: 240 (all pulled, 0 download failures), plus the 12 older PSG_N files (11 duplicate a Final; PSG 188 and MainserverGame_04 are not in the archive CDX at all).
- 18 captures (plus old PSG_248) were cut at exactly 1 MiB by the archive; for 10 of them (122, 159, 162, 174, 187, 241, 265, 283, 296, 324) a full earlier capture was fetched instead. Still truncated (inflate/liblzma error, no other capture): 106, 113, 117, 118, 119, 147, 176, 194. PSG_248.sav (old) is truncated; `PublicServerGame_248_Final.sav` (1056104 B, from the PublicServer_archive path) is complete and loads.
- Load check (all 252 files, `psg/loadcheck.tsv`: name, exit code, missing-GRF count, error, top hotspot): 178 load and report hotspots; the rest fail: ~28 "Broken savegame - Road vehicle on invalid road type" (missing road-type NewGRFs), 3 "Waypoint with invalid tile" (PSG 15, 16, 19), ~9 truncated archive files, 17 crash on load from a missing/replaced NewGRF (e.g. 115, 111, 114, 116, 120, 131, 149, 158, 163, 169, 177, 187, 230, 251, 259, 332, 34, 42). Missing NewGRFs themselves do not stop a load (they are disabled); most common missing: dutchstatw, canstnw, indstatr(w), pb_av8w, basic_platformsw, newstatsw, chips, ae_cityw, nuts, brickfreightw, usstatsw.
- The failing road-type ones might load with the engineer's Windows NewGRFs linked in (not tried).

## Method
Build: nodes build-rel binary has a stale language pack (lng newer than binary: "No available language packs"), so the .claude/play build-play binary + its lang dir were copied to scratchpad/psgbin. Scratch config only under the session scratchpad psgcfg (OpenGFX tar copied from the Windows OpenTTD/baseset). Run: `XDG_DATA_HOME=<psgcfg>/data XDG_CONFIG_HOME=<psgcfg>/cfg openttd -v null:ticks=5 -s null -m null -g <save>` with `<psgcfg>/data/openttd/scripts/game_start.scr` holding `script <log>`, console commands, `script`, `quit`. (Dedicated mode `-D` refuses saves with missing NewGRFs: "NewGRF mismatch".)
Export commands: `map_hotspots <k> [window=48]` (top k 48x48 windows by pre-signals/PBS/bridge heads/station tiles), `map_export <x0> <y0> <x1> <y1> <file.json>` (rail, bridge heads, stations, crossings), `map_export_station <id> <margin> <file>`, `junction_terrain_dump <x> <y> <w> <h> <file>` (terrain fixture; needs company 0). `junction_export` generates a junction, it does not read the hub.

## The 12 Junctionary-linked saves
| PSG | file (bytes) | loads? | missing GRFs | hub location | export |
|---|---|---|---|---|---|
| 115 | PSG_115.sav (791869); Final also fetched | NO: crash on load (68 missing GRFs) | many | "5 lines" BBH, coords unknown | none |
| 173 | PSG_173.sav (2527087) | yes, 0 missing | none | Mark's 4-way hub, coords not on pages | hotspots only (psg/load_173.log) |
| 186 | PSG_186.sav (1213658) | yes | 17 (testw, jcindstaw, indstatrw, pgs-*, harbourw_456, ttrs3w, total_bridges, dutchstatw, jpstatsw, canstnw, pb_ukrs, pb_av8w, heqs, bktunw) | MSH03/Brainmelter, coords unknown | psg/psg186_{1,2,3}.json (top hotspots, earlier run) |
| 188 | not archived | - | - | - | - |
| 191 | PSG_191.sav (835821) | yes | 4 | diagonal SLH, unknown | hotspots only |
| 197 | PSG_197.sav (531723) | yes | 14 (indstatrw, jpstationsw, ae_ruraw, pb_av8w, vast, ae_cityw, dep2.1w, airportsplus, egrvts, ttrs3w, pb_ukrs, ukrsap1w, opengfx+trees, jplandw) | 3-way BBH (two LLLRRR, one LLRR), unknown | psg/psg197_{1..4}.json (top 4 hotspots: 144,144 / 72,240 / 96,240 + 4th) |
| 204 | PSG_204.sav (432632) | yes | 23 | fast-joiner SLH, unknown | hotspots only |
| 206 | PSG_206.sav (703476) | yes | 20 | BBH03 merger / SLH, unknown | hotspots only |
| 219 | PSG_219.sav (854800) | yes | 14 (dbsetxlw, dutchcatw, station_name, indstatrw, jpstationsw, chips, ae_cityw, canstnw, nogridland, yarrs, NACityw, swehouses, irv, PlaneSetw) | "Balanced" BBH 4+4+5, unknown | psg/psg219_{1,2,3}.json (earlier run) |
| 223 | PSG_223.sav (177620) | yes | 18 | - | hotspots only |
| 224 | PSG_224.sav (323436) | yes | 23 | - | hotspots only |
| 227 | PSG_227.sav (1016356) | yes | 15 (airportsplus, PlaneSetw, ussetw, troprsw, total_bridges, bktunw, newtree, irv, canstnw, ae_cityw, dutchcatw, indstatr, dep2.1w, opengfx+trees, chips) | "Simple Classic LLRR hub" and "Snake Nest": NO coordinates on the Junctionary pages | psg/psg227_{1..4}.json: windows 408,216-455,263; 432,216-479,263; 216,72-263,119; 144,216-191,263 (hotspot candidates, hub not identified) |
| 248 | PSG_248.sav truncated; PublicServerGame_248_Final.sav complete, loads | yes (Final) | 0 reported for the Final | "Furball" BBH (V453000), unknown | hotspots only (48,48 top) |
| MainserverGame_04 | not archived (PublicServerGame_04_Final.sav was pulled and loads, 0 missing; unconfirmed that it is the same game) | - | - | - | - |

## What is needed to export the real hubs
The Junctionary pages give no tile coordinates, so a hub window must be picked: by hotspot (most hubs sit in the top windows, but a hotspot box is not a hub), or by station name / a screenshot match (e.g. Psg248_bbhfurball.png, hubs_186msh03.png) opened in a GUI client (not done: GUI forbidden for this task). Once a window is chosen: `map_export x0 y0 x1 y1 file` in the same recipe.
All 178 loading saves have a hotspot summary at `/mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad/psg/loadlogs/<name>.log`; exports for more windows can be made in one run per save.


## NewGRFs (GR1)

Goal: get the NewGRFs the PSG saves need so they load and run. 240 save files are on disk (the "252" counted duplicates). Logs and scripts: `scratchpad/gr1/` of the session scratchpad; copies of the scripts and the full per-save tables are in `.../scratchpad/psg/` (`grf_missing.tsv`, `loadcheck2.tsv`, `gr1/`).

### Method
- Load check as SV1, but on the engineer's normal data dir (so `~/.local/share/openttd/newgrf` is scanned) with only a scratch config dir: `XDG_CONFIG_HOME=<scratch>/cfg openttd -d grf=1 -v null:ticks=5 -s null -m null -g <save>`, with `scripts/game_start.scr` (`script <log>`, `map_hotspots 1`, `script`, `quit`) next to a copy of the play binary. "Loads" = the script printed a hotspot line. "Runs" = a second run without the script and with `ticks=200` exits 0. GRF lists come from the `NewGRF <id> (<path>) not found; checksum <md5>` lines; a line ending "Compatibility mode on" is a same-ID substitution, any other is a hard miss.
- "before" pass (engineer's GRFs only): 455 unique (id, md5) pairs, 278 GRF ids, not exactly present. Written with save counts to `psg/grf_missing.tsv`.
- Fetch 1, BaNaNaS through the game's `content` console command (dedicated server fed on stdin with delays: `content update`, `content select <id>` for each exact-md5 hit, or the only entry of that GRF id, `content download`): 170 packages, 1.3 GB (RAWR 359 MB, BRIX 260 MB, YETI 247 MB, Pineapple 217 MB are most of it). Downloaded into a scratch data dir, copied without overwrite to `~/.local/share/openttd/newgrf/psg/`.
- Fetch 2, `ottdc_grfpack_8.0.zip` from web.archive.org (capture 20150618073433 of `bundles.openttdcoop.org/grfpack/releases/LATEST/`; 156 GRFs): the 95 files whose md5 or GRF id matched a still-missing pair, in `newgrf/psg/ottdc_grfpack_8.0/` with the pack layout. The other pack releases (6, 7.0, 7.1, 7.3, 7-beta1 as .zip/.tar.gz) are 404 or redirect to 404 in the archive; the 2008 `ottdc_grfpack_6.7z` and `7-beta1.7z` exist but no 7z extractor is installed. The pack's `md5grfid.list` marks most of the rest as `z_bananas/` (not in the zip).
- Fetch 3, exact old versions from the content service. BaNaNaS keeps every version and the content server answers a query by (type, GRF id, md5), which the console `content` command cannot do. A small Python client of the content protocol (`gr1/cs.py`, `gr1/csdl.py`: ClientInfoExternalIDMD5, then ClientContent to content.openttd.org:3978, gunzip to .tar; wire format from `network_content.cpp`) found 112 of the 117 pairs still missing, and then 97 of the 124 pairs that were only compatible. 206 packages downloaded (about 1.1 GB), in `newgrf/psg/content_service/`. Skipped for size: YETI 0.1.0, 0.0.5 and 0.1.1 (151 to 191 MB each).
- The engineer's own files were not changed. `newgrf/psg/` is 2.4 GB. Crash dumps that my runs wrote into `~/.local/share/openttd` (about 140 files) were moved to `scratchpad/gr1/crashes/`.

### GRF table (top 20 by saves needing it; all 455 pairs with source and result are in `psg/grf_missing.tsv`)
| GRF id | name | saves | source | result | fetched |
|---|---|---|---|---|---|
| 52455400 | City stations | 123 | BaNaNaS | exact | yes |
| 58534453 | Dutch Stations Set | 105 | BaNaNaS | exact | yes |
| 43415463 | canstnw.grf | 100 | ottdc_grfpack 8.0 | exact | yes |
| 45520102 | basic_platformsw.grf | 88 | ottdc_grfpack 8.0 | exact | yes |
| 4A430002 | Industrial Stations Renewal | 82 | content.openttd.org md5 query, content id 5140265 (0.8.0) | exact | yes |
| 52455200 | Rural Stations | 82 | BaNaNaS | exact | yes |
| 6D620601 | newstatsw.grf | 77 | same-ID file: grfpack 8.0 same id | compatible | yes |
| 45520400 | brickfreightw.GRF | 76 | ottdc_grfpack 8.0 | exact | yes |
| 52455300 | Suburban Stations | 73 | BaNaNaS | exact | yes |
| 54570200 | US Stations Set | 69 | BaNaNaS | exact | yes |
| 45520500 | modernsuburbanw.GRF | 66 | ottdc_grfpack 8.0 | exact | yes |
| 504E0002 | Generic Buffers | 60 | BaNaNaS | exact | yes |
| 44573452 | DWE-StationTiles -Industrial Te | 56 | BaNaNaS | exact | yes |
| 52571201 | eGRVTS | 56 | BaNaNaS | exact | yes |
| 6D620004 | newshipsw.grf | 54 | ottdc_grfpack 8.0 | exact | yes |
| 45530500 | Japanese Stations | 52 | content.openttd.org md5 query, content id 16752298 (2.0a) | exact | yes |
| 535A0501 | newbridgesw.grf | 51 | ottdc_grfpack 8.0 | exact | yes |
| 504E0001 | UK Waypoints | 50 | BaNaNaS | exact | yes |
| 45530400 | Japanese Stations | 44 | ottdc_grfpack 8.0 | exact | yes |
| 44573553 | DWE-NewObjects | 42 | BaNaNaS | exact | yes |

Pairs still not exact (28 compatible, 5 missing; full list in the TSV). Missing:

| GRF id | name | saves | source | result | fetched |
|---|---|---|---|---|---|
| 52453C00 | stolentreesw_162_108.grf | 32 | not found | still missing | no |
| 5245F0F0 | newtree.grf | 6 | not found | still missing | no |
| 56420303 | YETI Extended Towns & Industrie | 3 | content.openttd.org (exact exists, 151-191 MB) | still missing | no (skipped for size) |
| 56420303 | YETI Extended Towns & Industrie | 2 | content.openttd.org (exact exists, 151-191 MB) | still missing | no (skipped for size) |
| 9787F081 | bktunnel.grf | 2 | not found | still missing | no |

Of the 455 pairs: 422 are now exact (128 mainly from BaNaNaS, 90 from grfpack 8.0, 204 from the content service), 28 compatible (a same-ID version is used), 5 missing. Of the 278 GRF ids: 248 fully exact, 26 exact or compatible, 4 with a missing variant (52453C00, 5245F0F0, 56420303, 9787F081).

### Load counts (240 saves; per save in `psg/loadcheck2.tsv`)
| stage | loads | runs 200 ticks |
|---|---|---|
| SV1 (scratch data dir, no engineer GRFs) | 178 | not measured |
| before (engineer's GRFs only) | 190 | 190 |
| + BaNaNaS | 205 | 205 |
| + grfpack 8.0 | 206 | 204 |
| + content service, exact for missing pairs | 205 | 172 |
| + content service, exact for compatible pairs (final) | 219 | 217 |

Final: 219 load and 217 run. 179 of the loading saves have no hard-missing GRF; 121 of those have every GRF exact and all 121 run.
- Do not load (21): 6 crash on load; 8 truncated archive files (inflate error: 106, 113, 117, 118, 119, 147, 176, 194) and the truncated old PSG_248 (liblzma); 3 "Road vehicle on invalid road type" (116, 129, 320; 31 before, 17 after BaNaNaS, 5 after the pack); 3 "Waypoint with invalid tile" (15, 16, 19).
- Crash on load (6: 34, 42, 43, 105, 109, 187): the game's load-crash handler prints "Loading your savegame caused OpenTTD to crash" with the NewGRF explanation: 4 to 6 same-ID substitutions each (OpenGFX newGUI, newstats, long vehicles, indstatr 6, eGRVTS and similar, from the 28 pairs not found exactly). 187 wrote a dump: assertion in saveload/afterload.cpp `v->tile != TileVirtXY(v->x_pos, v->y_pos) || v->z_pos == GetSlopePixelZ(...)`; the other five are a signal during load with no dump, so no further message. Counts went 7 (before), 6, 17, 19, 6 as GRFs were added: a same-ID replacement crashes, the exact version does not. Not debugged, as briefed.
- Crash within 200 ticks (2 saves that load: 151, 174): assertion `ground_vehicle.hpp` `z_pos == GetSlopePixelZ(x_pos, y_pos, true)`. (The intermediate pass had 33 such crashes, 17 `follow_track.hpp:140`, 14 "Disconnecting train", 1 `ground_vehicle.hpp`, 1 segfault; all but these two went away when the exact versions replaced the substitutes.) Dumps: `scratchpad/gr1/crashes*/`.

### Still missing and where they might come from
- stolentreesw_162_108 (52453C00, 32 saves), newtree-2010.08.29 (5245F0F0, 6), canadian_bk_tunnels 1.6.0 (9787F081, 2): not on the content service at that md5. Candidates: the 2008 `ottdc_grfpack_6.7z` (11 MB) and `ottdc_grfpack_7-beta1.7z` (14 MB), 200 captures on web.archive.org, once a 7z extractor is available.
- YETI Extended Towns and Industries 0.1.0 and 0.0.5 (56420303, 5 saves): exist on the content service by md5 (191 MB and 151 MB); not downloaded.
- 28 pairs still compatible (the 27 the content service did not have exactly, plus YETI 0.1.1): listed in `grf_missing.tsv` with result "compatible".
