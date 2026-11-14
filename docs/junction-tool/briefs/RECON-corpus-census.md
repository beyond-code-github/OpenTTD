# Recon C1: classify the PSG hub corpus into the tool's hub types (read-only, scripts only)

You are a read-only recon. You change no file in the repo except the one output file named below. You run no game and build nothing. Keep your context small: scripts write to files, you read summaries, and the report gives numbers, not dumps.

## Goal
We hold 529 candidate hubs exported from 177 #openttdcoop public-server saves, but they have only been counted (switches, bridge heads, signals), never classified. Later research steps will measure coop practice per hub type (crossing forms, bridge widths, spacing, footprint, crossing order) against the tool's rules. They need, first, every candidate mapped to the tool's own hub vocabulary, with a flag for whether it is a clean example. This step builds that census.

## Data (on the engineer's OneDrive; read-only, never re-fetch anything)
- `D = /mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad/psg`
- `D/hubs/hubs.tsv`: the 529 candidates (save, k, window, box, bundles, shape, sides, switches, bridges, signals, score).
- `D/hubs/psg<N>_<k>.json`: the box's tiles: `{box, map_size, orientation, tiles: [{x, y, height, tile_type, kind, tracks, signals: [{track, trackdir, exit_edge, type, variant, state}], owner, rail_type, ...}]}`. Bridges and tunnels: `tile_type "tunnelbridge"`, `kind "bridge"|"tunnel"`, `direction`, `far_end`. `D/hubs/psg<N>_<k>.txt` is an ASCII picture of the same box with a legend.
- `D/maps/<save>/<x>_<y>.json`: whole-map exports in 256 x 256 boxes, for widening a clipped box.
- `D/hubs/find_hubs.py`: how the candidates were found. Read it for the tile format and the border-crossing logic and reuse it.
- `/home/pete/OpenTTD/docs/junction-tool/RECON-psg-hubs.md`: the finder's method and its caveats.

## Read first (once each)
- `RECON-psg-hubs.md` (Method section);
- `D/hubs/find_hubs.py`;
- `/home/pete/OpenTTD/src/junction/topology/CLAUDE.md`, sections "Notation and topology" and "Graph, order and structure" (the tool's vocabulary: lanes L/R, the notation `LL_RR`, main line W-E with a branch on S, movements, splits, merges, prio, doubled bridges);
- `/home/pete/OpenTTD/docs/junction-tool/RULES-coop.md` (headings only, then any section you need).

## Method
Scripts go in `D/census/` and run with `python3 -I`. Outputs go there too.
1. **Lanes and directions per bundle.** For each candidate, take the border bundles (reuse find_hubs' crossings). Give each track crossing the border a direction (into or out of the hub) from the signals on it or next to it inside the box: a signal's trackdir is the direction trains pass it. Where a lane has no signal within a few tiles, mark it unknown.
2. **Movements.** Follow the track graph inside the box from each in-lane (following track connectivity, bridges and tunnels by `far_end`, only along the legal travel direction where signals fix it) and record which out-lanes it reaches. Group the lanes into lines: a line is a bundle's in-lanes plus the out-lanes that pair with them. Use the bundle on the opposite side if one exists, else the one that most lanes go to.
3. **Hub type.** Express each candidate in the tool's terms where possible:
   - the main line as the pair of opposite bundles with the most through traffic, written in the notation (lanes per direction and the gap: e.g. `LL_RR`; read the gap from the tiles between the direction groups at the border);
   - each other bundle as a branch, with its side relative to the main line (left or right of the main line's direction, i.e. the tool's S or N) and its notation;
   - whether every in-lane reaches every out-lane of another line (AnyToAny), and which movements are missing;
   - axis-aligned or diagonal: is the main line on map axes at the border (X/Y tracks) or diagonal (Upper/Lower/Left/Right)?
   - type labels: `T` (one L_R branch), `<main> x <branch>` for one branch (e.g. `LL_RR x L_R`), `+` for two branches on one side, `both-sides`, `mesh` (more than one through line), `unclassified` with the reason.
4. **Cleanliness flag.** A candidate is *clean* when: all bundles are classified, every lane has a direction, the box is not clipped (no track runs off the box except through the border bundles; widen from `D/maps` once if clipped), and no station or depot lies inside it. Record the reasons for every unclean one.
5. **Spot check.** Pick 10 classified hubs (spread over types) and check each label by eye against its `.txt` picture. Record agree or disagree, with the reason.

## Output
`/home/pete/OpenTTD/docs/junction-tool/RECON-corpus-census.md`, short:
- the method as built, in about ten lines, with the deviations from this brief;
- **Table A:** hub types, with count, clean count, how many are axis-aligned and how many diagonal, and two example ids each;
- **Table B:** for types the tool supports or plans (`T`, `L_R x L_R`, `LL_RR x L_R`, `L_R x LL_RR`, `LL_RR x LL_RR`, `LLL_RRR x *`), the clean axis-aligned examples by id;
- how often branches sit on both sides, on N only, as several branches, or as 3+ lanes per direction (the tool refuses or has not ruled some of these);
- the spot check's results;
- where the scripts and the per-hub `census.tsv` are.

## Do not
- Fetch anything from the network.
- Write anywhere but `D/census/`, the output file and your scratchpad `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/census/`.
- Touch any git worktree, run any game binary, or use `pkill` or a broad `kill`.
- Spend more than about 90 minutes. If classification is unreliable (the spot check disagrees on more than 3 of 10), stop, report why and what would fix it.

## Report (short)
- the output file's path, plus Table A's top rows and the Table B counts in five lines;
- the spot-check score;
- stops and anything left undone.

When you have reported, stop.
