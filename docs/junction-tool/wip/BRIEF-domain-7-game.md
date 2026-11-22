# BRIEF domain-7: pin the domain's hand cases in the game itself

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 90 minutes wall. No rerun loops. Never run git stash. Never start a game client or touch the rig servers.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- Engineer: "If we can actually tie these tests/code to the real game mechanics in some way then even better." `CheckPlanarDomain` (src/junction/planar/planar_domain.h) is checked by hand cases and a source reading (`docs/junction-tool/wip/RECON-domain-game-rules.md`, main checkout: read it first). This step builds the cases with the game's own commands on a flat test map and asserts the game's answer next to the domain's.

## 2. Read first
- The RECON file above (the rule table).
- `src/tests/junction/game/junction_game.cpp` lines 900-960 (`JunctionGameWorld`, `MakeBareGrass`) and 1182-1240 ("a planned tunnel builds": world, heights with `SetTileHeight`, `max_tunnel_length`, `Command<Commands::BuildTunnel>`, `BuildRail`, `BuildBridge` calls at ~1013 and ~1255).
- `src/tests/junction/planar/planar_domain.cpp` (the hand cases and `TrackFromRoute`).
- `src/junction/planar/planar_domain.h`.

## 3. Where
New TEST_CASEs at the end of `src/tests/junction/game/junction_game.cpp` (it owns `JunctionGameWorld`), tag `[junction][junctiongame][planardomaingame]`. Include `planar_domain.h`. Domain tile (tx, ty) is game tile `TileXY(96 + tx, 96 + ty)` on bare grass at height 4 (`MakeBareGrass(TileXY(88, 88), 40)` and heights as the tunnel test sets them). A small static helper `BuildPieces(points)` rasters a domain track (`RasterPlanarDomainTrack`) and runs `Command<Commands::BuildRail>` per piece (Execute), returning the first failure.
Game connectivity helper `GameConnects(a, b)`: pieces on edge-adjacent tiles connect when the game's track status of tile a has a trackdir exiting through the shared edge (`TrackdirToExitdir`) and tile b's track status has a trackdir in `DiagdirReachesTrackdirs(that edge)`; use `GetTileTrackStatus(tile, TransportType::Rail, 0)` and `TrackStatusToTrackdirBits`.

## 4. The pins (one TEST_CASE each; each builds the geometry in a fresh world and asserts the game's answer AND the domain's verdict on the same points)
1. **Connection (rule 1):** domain case 10 (track B's corner ends on A's edge midpoint, no node): the game connects a piece of B to a piece of A (`GameConnects` true for some pair); the domain gives `{"connection"}`. And domain case 1 (two parallel X tracks on neighbouring rows): no pair connects; domain legal.
2. **Switch (rule 10):** domain case 3 for one axis start: every leg builds; the trunk's last piece connects to each leg's first piece; domain legal.
3. **Diamond (rule 2b, design):** domain case 9: the game builds both tracks (both succeed), and no X piece connects to a Y piece; the domain gives `{"diamond"}`. Comment: a design ruling (D182), the game allows it.
4. **Bridge head (rule 3):** a bridge (as domain case 7, `BuildBridge` between its head tiles) then `BuildRail` of a Y piece on a head tile: the game refuses; domain case 18 gives `{"head"}`.
5. **Under a bridge (rule 4):** domain case 7's crosser built under the bridge middle (before or after the bridge): succeeds; domain legal. Domain case 15 (unlisted): the game also succeeds; the domain gives `{"under"}`. Comment: a planar-model rule (a crossing the topology lacks), not a game rule.
6. **Two decks (rule 5):** domain case 17: the second bridge is refused; domain `{"decks"}`.
7. **Tunnel end runs (rule 6):** flat at h = 4; separator X along row y, crosser Y at column c. Lower the two outer corners of the portal tiles at c - 1 and c + 1 to 3 (`SetTileHeight` on those corners, as the tunnel test does), so the portals incline toward the crosser and the tiles at c - 2 and c + 2 become the ramps. Assert: `BuildTunnel` from c - 1 succeeds and ends at c + 1; `BuildRail` X on c - 2 succeeds (the ramp carries the axis track); `BuildRail` Y on c - 2 is refused (no other track fits the ramp at that level); the crosser Y on c builds. Domain: the same layout as a tunnel structure with `ends = 2`, `first` = c - 2, `last` = c + 2 (piece indices on the separator) is legal, and with a Y track through c - 2 gives `{"head"}`.
If a pin's game answer differs from the RECON table: stop and report it (that is a finding, not a test to bend).

## 5. Gates and output
- `tools/junction/jt fast build '[planardomaingame]' 5` and `tools/junction/jt fast build '[junctiongame]' 5` (Debug); `tools/junction/jt gate build-rel`.
- Update RECON-domain-game-rules.md in the worktree copy (docs/junction-tool/wip/): a column "pinned in game" naming the TEST_CASE per rule.
- `src/junction/planar/CLAUDE.md` **Domain** bullet: add "its hand cases are pinned in the game (`[planardomaingame]`: the game's commands and track status agree, the X/Y diamond and the under rule are marked design rulings)".
- One commit: `Add: Junction: the domain's hand cases pinned in the game (D428)`, Co-Authored-By Claude Sonnet 5.5.

## 6. Report (short)
Commit; per pin: game answer, domain verdict, agree or not; red evidence (one pin's assertion inverted, seen failing); suites with times; stops; state. When you have reported, stop.
