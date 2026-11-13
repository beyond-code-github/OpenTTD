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
