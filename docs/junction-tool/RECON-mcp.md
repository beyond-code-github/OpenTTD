# RECON: an OpenTTD MCP server for in-game checks

Research only (2026-11-07). File:line refers to `feature/junction-tool` at c157513559. Points marked *(inference)* are my reading of the code, not yet confirmed by running the game.

## 1. What the admin port gives today

- **Transport:** TCP on port 3977 (`src/network/core/config.h:24`). The protocol is in `docs/admin_network.md`. Packet types are listed at `src/network/core/tcp_admin.h:22-60`.
- **Server only, and only with auth configured.** The admin listener starts inside `NetworkServerStart`, and only when `AdminAuthenticationConfigured()` is true (`src/network/network.cpp:936-940`). A plain single-player game has no admin port. You need either a dedicated server (`-D`) or a GUI game started as a multiplayer server (a "listen server"). The welcome packet reports which kind it is (`network_admin.cpp:184`).
- **Auth:**
  - `AdminJoin` sends the password in plain text. It is refused unless `network.allow_insecure_admin_login=true`, which defaults to false (`network_admin.cpp:668`, `src/table/settings/network_settings.ini:182-185`).
  - `AdminJoinSecure` offers a PAKE password exchange or authorized keys, followed by encryption (`network_admin.cpp:835-851`, `docs/admin_network.md:55-69`).
  - For a localhost test rig, the simplest setup is the insecure login plus `admin_password` in `secrets.cfg` (`network_secrets_settings.ini:55`).
- **Readable state (all of it):**
  - date
  - client info
  - company info, economy and stats
  - chat
  - console output
  - DoCommand names
  - DoCommand logging (a copy of every command packet)
  - GameScript JSON (`tcp_admin.h:79-90`)
  - The welcome packet adds the seed, landscape, start year and map size (`network_admin.cpp:182-191`).
- **What it cannot read:** vehicles, tiles, map contents or signals. All of that has to come from a GameScript (section 2).
- **Update frequencies:** see the table at `network_admin.cpp:48-60`.
  - Date: poll, or daily up to annually.
  - Client and company info: poll, or automatic (on change).
  - Economy and stats: weekly or less often.
  - Chat, console, command logging and GameScript: automatic only.
- **Actions:**
  - rcon runs any console command on the server. Its output comes back as `ServerRemoteConsoleCommand` lines, then `...End` (`network_admin.cpp:516-526`, `_redirect_console_to_admin`).
  - Chat and external chat (`:786`, `:812`).
  - GameScript JSON in, as `ScriptEventAdminPort` (`:530-539`).
  - Ping.
- **JSON size limits:** admin to GS, up to 9000 bytes (`config.h:58`). GS to admin, about 1450 bytes per message (`script_admin.hpp:32-34`), because the packet limit is COMPAT_MTU, 1460 (`packet.h:71`, `config.h:44`). Larger results have to be split into chunks.

## 2. What a GameScript can do

- **Acting as a company:** `ScriptCompanyMode` exists in the game API. Commands run "like the real player", costs included (`src/script/api/script_companymode.hpp:17-30`). Without it, a GS acts as the deity company.
- **Building:**
  - track: `ScriptRail::BuildRailTrack` and `BuildRail` (`script_rail.hpp:387`, `:437`)
  - signals: `BuildSignal`, with all types including PBS and one-way PBS (`:477`, `:70-83`)
  - depots and stations (`:242`, `:280`)
  - bridges: `ScriptBridge::BuildBridge` (`script_bridge.hpp:156`)
  - demolition: `ScriptTile::DemolishTile` (`script_tile.hpp:482`)
- **Trains:** `ScriptVehicle::BuildVehicle` and `BuildVehicleWithRefit` (`script_vehicle.hpp:340`, `:362`), `ScriptOrder::AppendOrder` (`script_order.hpp:446`), `StartStopVehicle` (`script_vehicle.hpp:529`).
- **Reading vehicles:**
  - `GetCurrentSpeed` (`:236`)
  - `GetState`: running, stopped, in depot, at station, broken or crashed (`:244`)
  - `GetLocation`, at **tile** resolution only, with no sub-tile position (`:161`)
  - `IsStoppedInDepot` (`:316`)
  - `ScriptEventVehicleCrashed` is in the game API. `VehicleLost` is AI only (`script_event_types.hpp:26`, `:571`).
  - There is no "waiting at signal" query. Jams would have to be inferred from speed 0 while the state is running. *(inference)*
- **Reading tiles:** `GetRailTracks` (`script_rail.hpp:369`), `GetSignalType` (`:464`), `GetSlope`, `GetOwner`, `IsBuildable` (`script_tile.hpp:303`, `:351`, `:151`).
- **Limits:**
  - About 10,000 opcodes per tick by default, range 500-250,000, set at new game only (`src/table/settings/script_settings.ini:46-53`).
  - Each DoCommand suspends the script for at least 1 tick (`script_object.cpp:319-330`). A 2000-piece junction therefore takes about 2000 ticks, roughly 1 game month. `ScriptAsyncMode` queues commands without waiting (`script_asyncmode.hpp:16-24`).
- **Exchanging JSON with the admin port:**
  - In: the GS polls `ScriptEventController` for `ScriptEventAdminPort` and reads the decoded table.
  - Out: the GS calls `ScriptAdmin.Send(table)` (`script_admin.cpp:118-136`). This reaches every admin that registered `Gamescript` = automatic (`network_admin.cpp:1076-1082`).
  - There is no request ID built in, so the companion GS has to echo one back.

## 3. Console commands that help testing (all reachable via rcon)

- **Registered commands** (`src/console_cmds.cpp:2966-3067`):
  - `load` / `load_save`, `load_scenario`, `save`, `newgame`, `restart`, `reload`
  - `pause` / `unpause` (server, or no network)
  - `setting <name> [value]`, `exec <script>`
  - `screenshot`, `zoomto`, `scrollto`
  - `companies`, `reset_company`, `move`
  - `rescan_game`, `list_game`, `getdate`, `getseed`, `fps`, `dump_info`, `resettile`, `debug_level`
  - `script` (logs console output to a file)
- **`screenshot`** takes `viewport | normal | big | giant | heightmap | minimap` with `size W H` and a file name (`console_cmds.cpp:1770-1840`). The PNG lands in the server's `screenshot/` folder, where Claude can `Read` it, so **yes, the model can see the game.**
  - The dedicated driver keeps a real framebuffer when the blitter has depth (`video/dedicated_v.cpp:111-116`). So `giant`, `minimap` and probably `normal` should work headless. *(inference, verify)*
  - The `-v null` driver forces the null blitter (`video/null_v.cpp:37`), so no screenshots there.
- **There is no fast-forward command.**
  - The toolbar disables fast-forward whenever `_networking` is set (`toolbar_gui.cpp:234`, `:2014`).
  - The tick interval itself does not check networking. It honours `_game_speed`, and 0 means as fast as possible (`video/video_driver.hpp:334-339`), set via `ChangeGameSpeed` (`gfx.cpp:1899`).
  - A fork-only `gamespeed <pct>` console command would therefore speed up a server that has no remote clients. *(inference)*
- **Headless batch run:** `openttd -g save.sav -v null:ticks=N` runs N ticks flat out, then exits, and saves on exit if `autosave_on_exit` is set (`video/null_v.cpp:31`, `:44-55`). It has no network and so no admin port. Results would come from the GS log (`-d script=…`) or the save file.
- **Console commands from fork code:** the code can register its own commands with `IConsole::CmdRegister` from its own file. Precedent: `music/midifile.cpp:1119-1130`. This needs no edit to `console_cmds.cpp`, or at most a one-line hook.

## 4. Architecture options

**(a) External client (MCP or CLI, see 4b) + companion GameScript.** *Recommended base.*
- **Pieces:**
  - The external client (Python) speaks the admin protocol. It owns the process: it launches `openttd -D` or a GUI listen server with a test config.
  - The GS (Squirrel) carries out JSON requests in company mode and reports back.
- **Pros:**
  - Upstream edits are zero. The GS lives outside `src/`.
  - The save format is untouched.
  - It works against any OpenTTD build.
- **Cons:**
  - 1450-byte replies need chunking.
  - The GS builds at 1 command per tick.
  - Vehicle positions are tile-level only.
  - Two languages to maintain.
- **Effort:** about 600-1000 LOC (Python 300-500, GS 300-500). *(estimate)*

**(b) MCP or HTTP endpoint compiled into the fork.**
- **Pros:** full state access: exact vehicle position and speed, reservations, signal states. No size limit.
- **Cons:**
  - Needs a socket or HTTP server and JSON in core.
  - Thread-safety around the game loop.
  - Clashes with the "minimal upstream edits" rule.
  - Effort about 1500+ LOC.
- **Not worth it now.**

**(c) Headless harness without MCP.**
- A shell or Python runner does: dedicated server or `-v null:ticks`, then a scenario save, then a scripted GS, then a JSON verdict (deadlock yes/no, minimum speed at merges, cost).
- **Pros:** deterministic, CI-able, no model in the loop.
- **Cons:** cannot explore interactively.
- **Fit:** good as the second stage. The MCP tools from (a) can call the same GS scenarios.

**Exposing the fork's junction tool** (`JunctionSequencer` / `CcJunctionStep`, `src/junction_game.h:50-51`):
- Add a fork-only console command, for example `junction_place <tile> <layout> <transform> [company]`, in a new `src/junction_console.cpp`, and reach it via rcon. Errors and cost print through `IConsolePrint` and come back as rcon replies.
- **Catch:** console commands run as `_local_company`, which is a spectator on a dedicated server. The command must switch `_current_company` and execute with a callback that does not depend on a GUI. *(inference)* A listen server avoids this, because the host owns a company.
- The alternative, a new `ScriptJunction` API class, needs script-API generation and CMake changes. That is heavier, so defer it.
- **Fork constraints:** both routes add new files only, plus at most a one-line registration hook. Neither touches the save format. Test saves are ordinary saves, and the GS keeps its state in memory.

## 4b. CLI instead of (or beside) MCP

The idea: an `ottd` command that Claude runs through Bash, using the same admin port and GameScript underneath, with compact text output. Examples: `ottd status`, `ottd junction place 0x1a2b T2 r90`, `ottd watch --until jam|60s`.

- **Token cost:**
  - MCP: every connected server's tool schemas sit in context on every turn. About 15 tools at about 100-200 tokens each is roughly 2-3k tokens permanently, even in sessions that never touch the game. *(estimate)* Claude Code can defer MCP tool schemas behind a search, which softens this, but each first use still pays for the schema.
  - CLI: costs nothing until called. `ottd --help` is read once, on demand. Output is exactly what we print, so it can be one line per train or a terse verdict.
  - **Winner: CLI.**
- **Statefulness:**
  - Each admin connect is a TCP open, a join, then the welcome packets: milliseconds on localhost. The server allows up to 16 admins at once (`AdminID` pool size, `network_type.h:53`; `AllowConnection`, `network_admin.cpp:92-95`).
  - So **per-call connect is viable** for request/reply verbs: rcon, and a GS request whose reply is broadcast to every admin subscribed to `Gamescript` (`network_admin.cpp:1076-1082`; match replies by `id`).
  - A daemon is only needed to keep the OpenTTD process itself alive. `ottd server start` can spawn it detached and record its PID and port in a state file.
  - MCP keeps one long-lived connection for free, but a crashed game or a restarted MCP server is harder to recover from inside a session.
- **Waiting and streaming:**
  - CLI: `ottd watch --until <cond> --timeout <s>` blocks, filters the event stream (console, GS JSON, date) inside the tool, and prints only the verdict. Claude Code's Bash supports background runs and timeouts, so long watches don't block the turn.
  - MCP: a tool call also blocks until it returns. Progress notifications exist, but the model only sees the final result. So the two are equal here; the filtering logic matters more than the transport.
- **Images:**
  - MCP can return an image content block directly.
  - CLI prints a PNG path and Claude `Read`s it: one extra call, same pixels.
- **Effort:**
  - The CLI is slightly less work: argparse/click plus the core, no protocol layer, and testable from a shell by the engineer too.
  - MCP adds about 100-150 LOC of tool declarations on top of the same core. *(estimate)*
- **Shared core: yes.** Put `ottd_core` (admin client, GS request/reply with chunk reassembly, process launch, watch/verdict logic) in one Python package. Then `ottd` (click CLI) and `ottd-mcp` (MCP stdio) are thin front ends, about 100-150 LOC each.

## 5. Prior art

**MCP servers:**
- **OVER99K/openttd-directorate-mcp** (https://github.com/OVER99K/openttd-directorate-mcp)
  - TypeScript MCP + GameScript over the admin port, JSON envelopes. This is architecture (a).
  - Tools: `observe`, `plan`, `apply`, `commission`, `verify`, `execute`, `chat`, `rcon`. Targets 15.3. Its README says `AdminJoinSecure` is not the default. MIT, 0 stars, last push 2026-08-11. Worth reading for the GS bridge.
- **dylanfetch/openttd-mcp** (https://github.com/dylanfetch/openttd-mcp)
  - Python. Launches a headless OpenTTD 15.1 with Squirrel bridge scripts. Road vehicles only. Runs in real time, about 40 minutes for 3 game years. MIT, last push 2026-10-16.
- **willibrandon/openttd-mcp:** a Go admin-port MCP with "25+ tools", listed at https://lobehub.com/mcp/willibrandon-openttd-mcp. The GitHub repo now returns 404, so treat it as dead.
- **nttd** (https://pypi.org/project/nttd/0.0.7/): an LLM planning benchmark with its own GS (`nttd-gs/main.nut`).

**Admin-port libraries** (from the GitHub API, last push):
- Python: `liki-mc/pyOpenTTDAdmin` (https://pypi.org/project/pyOpenTTDAdmin/), 2026-06, sync and async, **best maintained**; `xaroth/libottdadmin2`, 2024-07; `horazont/python3-openttd`, 2021.
- Others: Java `erenard/openttd-admin` 2026-02; Go `tardisx/openttd-admin` 2025-05, `sdassow/openttd-admin` 2020; Rust `ThomasdenH/rust-openttd-admin` 2023 (WIP); Node `sambol/node-libOpenttdAdmin` 2015.
- Whether any of them supports `AdminJoinSecure` is unverified. Assume insecure login.

**Context:** an OpenTTD developer on TT-Forums says the admin port cannot be a playing client, so in-game actions must go through a GS (https://www.tt-forums.net/viewtopic.php?p=1242694).

## 6. Recommended MVP (junction testing)

**Choice: CLI first, on a shared core. Add an MCP front end later only if needed** (for other users or general play), reusing the core. The CLI fits Claude Code best: no permanent schema tokens, terse output, and the engineer can run the same commands by hand.

**Components:**
1. **`tools/ottd/` (Python package):**
   - `ottd_core`: admin client via `pyOpenTTDAdmin`, or about 200 LOC of its own packet code if secure join or GS chunking needs control; GS request/reply by `id` with chunk reassembly; launching the fork binary as a dedicated server with a test `openttd.cfg` (admin password, insecure login on localhost, our GS, high opcodes); watch/verdict logic.
   - `ottd`: a click CLI over the core.
2. **`JunctionTestGS` (Squirrel), in the fork's content dir:** request/response with an `id`, chunked replies, company mode. Verbs: build line, depot, train with orders, start; vehicle snapshot; tile query; jam watch.
3. **Fork console commands** in new files: `junction_place …` (placement through the real tool code), and later `gamespeed <pct>`.

**CLI verbs** (the same names become MCP tools later):
- `ottd server start [--save X]` / `ottd server stop [--save Y]`: launch detached, write a state file, quit
- `ottd rcon "<cmd>"`: run any console command, print its output
- `ottd load X` / `ottd save Y` / `ottd pause on|off` / `ottd speed <pct>`
- `ottd date`, `ottd wait <ticks|days>`
- `ottd shot <kind> [--tile T] [--size WxH]`: print the PNG path for Claude to `Read`
- `ottd junction place <tile> <layout> <transform>`: rcon `junction_place`; print the cost or errors
- `ottd rail build <path>`, `ottd signal <tile> <dir> <type>`, `ottd depot <tile> <dir>`: GS
- `ottd train spawn <depot> <engine> <wagons> <orders>` / `ottd train start <id>`: GS
- `ottd trains [--filter]`: one line per train (id, tile, speed/max, state)
- `ottd tile <tile|rect>`: tracks, signals, owner, slope
- `ottd watch --until jam|crash|<secs> [--tiles ...]`: per-merge min/avg speed, stuck trains (speed 0 and running for more than N days), crashes; ends with a one-line verdict
- `ottd company`: money, recent costs

**Size:** core about 450 LOC, CLI about 120 LOC, Squirrel about 500 LOC, C++ console command about 100 LOC. A later MCP front end is about 120 LOC. Roughly 3-5 agent steps. *(estimate)*

**First step:** a spike with no CLI wrapper yet.
1. Start the fork as a dedicated server, with admin password set and `allow_insecure_admin_login=true`, plus a hello-world GS.
2. From a 50-line Python script: connect, rcon `getdate` and `screenshot minimap`, and round-trip one JSON ping through the GS.
3. Run it twice, from two separate connections, to prove per-call connect works.

This settles auth, GS selection on a loaded save, and whether screenshots work on a dedicated server.

**Open questions:**
- Does `screenshot normal` / `giant` render on `-D`, and does `_game_speed` speed up a server? If not, a GUI listen server (real viewport, host company) may be the better rig.
- How should the GS be attached to an existing save? Via a `newgame`-time setting, or via `start_ai`-like loading of the GS? The GS is chosen at game creation, so a scenario save must be made with the GS already selected.
- Which company should `junction_place` act as on a dedicated server?
- Is tile-level vehicle position enough to measure braking at merges, or does that need speed sampling per tick?
