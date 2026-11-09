# RECON: organic junction pieces and constraints (for the CSP redesign)

Method: 27 images viewed (list at the end) plus ABR06/ABR10 text and Merging_Tracks. Every count is read off a screenshot by eye, so it is "estimated" unless an article states it. Zoom varies between images, so counts are tiles of the isometric grid and carry about +-1. "Not visible" means the image does not show it. Image names are bare file names (abr/images/ or coop/files/). Abbreviations: ABR06 = abr/06 hubs text, ABR10 = abr/10 invisible hubs text, MT = Merging_Tracks.

Overall finding up front: no image shows a fixed template. Every hub is a set of repeated small pieces (portal rows, short flyovers, hairpin-free diagonal runs) linked by long straight diagonal or axis-aligned lines, and the land is modified only locally (mounds around portals, plateaus). The line shapes follow a few direction rules, not the terrain.

## 1. Piece catalogue

### P1 Tunnel flyunder (a line dips under one or more other lines)
- Does: separates one flow from crossing flows without a bridge. The tunnel passes below the crossed lines and may pass below several at once (hubs_173Mark1 centre: the portal row sits on both sides of the 2 x 2 diagonal mainline bundle and the tunnels run beneath it).
- Footprint: tunnel length equals the crossed width plus both portal ramps, typically 6-12 tiles in the diagonal rows of hubs_173bbh02 (7 tunnels, longest estimated 10) and hubs_bbh04. Longest tunnels are in the invisible hubs: they cover the whole hub, roughly 15-25 tiles (ABR10_tunnelhub_signs, ABR10_TL5static; the article states "we basically tunnel the whole hub through from one end to another", abr/10 text).
- Constraints seen: portal faces along the track direction; track entering a portal is straight for at least 1 tile (in every portal row); tunnels cannot be crossed by a bridge over the portal tile but a rail line can run on the land above them (ABR10_SLH03: lines above tunnels); tunnel count per direction depends on train length: "shorter trains require more tunnels in general" (abr/10 text).
- Images: hubs_173Mark1, hubs_173bbh02, hubs_slh01, hubs_slh06, hubs_bbh04, hubs_msh03, hubs_msh01shift, ABR10_tunnelhub_signs, ABR10_TL2static, ABR10_TL5static, 206_BBH03merger, Psg206_SLH03, ABR10_SLH03.

### P2 Portal row (parallel tunnels with 1-tile lane pitch)
- Does: n parallel lines of one direction (or several split outputs) each dive under the same crossing in one go. This is the commonest element in the set.
- Footprint: rows of 3-8 portals. hubs_173Mark1: two rows of about 8 and 7 in the centre; hubs_173bbh02: 7; hubs_slh06: 4; hubs_bbh04: 4+4; 206_BBH03merger: 3 (single-mouth stack of three side by side near the top) and 4 in a column; hubs_msh03: 4 and 5. Adjacent portals are 1 tile apart across the track direction (every row above), so lane pitch is 1 tile. Portals form a stepped diagonal because each lane starts 1 tile further along than its neighbour (hubs_173bbh02 left row, hubs_bbh04).
- Constraints: portals in a row are staggered so each lane's ramp ends before the next lane's mound begins (the stagger step is 1-2 tiles, hubs_slh06, hubs_bbh04); all lanes in a row have the same tunnel direction; row sits beside, not on, the crossed line.
- Images: as P1; best are hubs_173bbh02, hubs_slh06, hubs_bbh04.

### P3 Mound or embankment for a portal
- Does: gives a portal something to dive into when the land is flat. Visible as a darker green raised area with sloped rim around each portal.
- Footprint: about 2-3 tiles of rising ground in front of each portal, a 1-tile wide shoulder each side. In rows (P2) the mounds merge into one continuous ridge (hubs_slh06: one ridge for 4 portals; hubs_173bbh02 left row: a ridge along 7 portals). Height of the mound: one level (not countable exactly; the shading in hubs_slh01 and hubs_slh06 suggests 1-2 steps, estimated).
- Constraints: a mound is local; surrounding land is untouched (hubs_slh01 plateau near the left shows the flat unmodified land at one level beside the mounds). Track approaching the portal climbs on the ramp (slope tiles under the track) for 1-2 tiles before the portal (estimated; hubs_slh06 portals at row ends).
- Images: hubs_slh01, hubs_slh06, hubs_bbh04, hubs_173Mark1, hubs_173bbh02, ABR10_TL2static (white ramps and cut faces visible with bridges transparent).

### P4 Cutting and plateau
- Does: levels a patch so a hub body sits flat; the slopes appear as tan vertical faces at the edge. Seen in the desert maps (ABR10_TL2static, ABR10_tunnelhub_signs: stepped cliffs at left and right) and as flat green rectangular fields in hubs_PZ05 and hubs_A2A.
- Footprint: whole hub width, 20+ tiles (ABR10_tunnelhub_signs), estimated.
- Constraints: lines on the plateau are level; ends of the plateau meet existing land with a 1-tile slope face. Not visible how much earth moved.
- Images: ABR10_tunnelhub_signs, ABR10_TL2static, ABR10_TL5static, SnakeNest (terraced land with tan retaining walls, many levels).

### P5 Bridge flyover (short and doubled)
- Does: lifts a line over crossing lines. In the TL rail-with-bridges games the flyovers are short (3-6 tiles) and numerous; Psg197_bbh01 shows about 30 flat grey ramp-bridge decks, each 3-5 tiles, scattered through the hub, usually 1 tile pitch for doubled pairs.
- Constraints: bridge at least 4 tiles (RULES-coop R47); "Bridges can not go under anything" (ABR06); pairs are equal length (R53); bridge heads need a straight 1-tile approach (Psg197_bbh01); in ABR10 text bridges are preferred for placement flexibility but too-long ones are cut in half or paired ("Too long bridges? Why don't I just cut them in half!").
- Images: Psg197_bbh01, Psg186_LLLLmerger01 (curved wide white-concrete bridges in the lower left, long about 6-8), Mergers_bridges_basic, ABR10_TL2static (green girders).

### P6 Slope ramp on open land (no tunnel)
- Does: a line climbs one level to change height without portal. Visible as a single track on grey ramp tiles on a mound edge (hubs_slh01 right side, 219_bbh04 not resolvable at its scale).
- Footprint: 1-3 slope tiles per level (hubs_slh01, Psg206_SLH03: track on brown earth embankment ramps). Height steps: 1 level most places. Heights beyond 1 level are not visible except in SnakeNest.
- Constraint: slope under track follows track direction, straight (no curve on slope tiles, hubs_slh06; consistent with OpenTTD rules, not in the articles).
- Images: hubs_slh01, Psg206_SLH03, SnakeNest.

### P7 Embankment line (line on raised ground over a gap)
- Does: keeps a line level where land falls. Brown embankment sides under the track with a fence (Psg206_SLH03, ABR10_SLH03: lines on brown raised bed across green land; SnakeNest: many lines on walls, tan faces).
- Constraint: 1 tile wide base per track pair; walls appear where the land is lower than the line (SnakeNest).
- Images: Psg206_SLH03, ABR10_SLH03, SnakeNest, Psg197_bbh01.

### P8 Split (one line into n lanes)
- Does: fans a direction's lines out to feed several branches. Shape: lines diverge diagonally, each lane heading to its own portal (P2). hubs_slh04, hubs_173bbh14 (orchestrator-viewed) and hubs_173bbh02 show the split as a clean fan of 1-tile-pitch lines opening to a row of portals. ABR06: "you just split lines from each direction ... you can play with the actual order".
- Footprint: a fan of n lanes takes about n tiles along the track per lane step (diagonal 1 tile per lane) so a 4-way fan needs about 4-6 tiles before the portals (hubs_bbh04, estimated).
- Constraints: split point is straight for several tiles beforehand (all examples); signals of a split sit before the fan (not visible on most images).
- Images: hubs_173bbh02, hubs_bbh04, hubs_slh04, hubs_split (not viewed), ABR10_tunnelhub_signs (labelled "Split").

### P9 Merge (with prio / tunnels as the merger)
- Does: joins lanes into one; in invisible hubs the tunnel ends are the merger and the tunnels get the prio role (ABR10: "The tunnels just end. That is all that makes the merger. We also assign the priority role to the tunnels"). Mergers are shown as a bundle of 3 portals side by side in ABR10_tunnelhub_signs ("Tunnel End").
- Footprint: a merge of 3 inputs has portals in one row (3 portals, 1-tile pitch, ABR10_tunnelhub_signs), then lanes funnel diagonally for about 4-6 tiles (estimated). LLLL merges are large: Psg186_LLLLmerger01 shows a 4-lane merge across about 25 tiles with five bridges over each other.
- Constraints: prio lanes are long, "Because priorities have to be quite long due to the tunnels" (ABR10); only one tunnel prioritized can be fine (ABR06, hubs_173Mark2: "a prio that would normally be considered broken"); downhill merge helps acceleration (MT section "Downhill merge", Mergers_join_downhill: "GRAVITY" annotations, track descending to the merge).
- Images: ABR10_tunnelhub_signs, Psg186_LLLLmerger01, 206_BBH03merger, Mergers_join_downhill, hubs_msh01shift, hubs_msh03.

### P10 Mainline shift
- Does: moves the mainline sideways (changes its lateral position or swaps sides) so a hub fits between. Shown as a long diagonal run with a 45 degree jog of 2-4 tiles sideways, then straight on (hubs_msh01shift line "A"/"B"/"C"; hubs_msh03; Psg216shiftmerger: a field of repeated shift-merge units).
- Footprint: jog 2-4 tiles across, 4-8 along (estimated, hubs_msh01shift). Psg216shiftmerger: one unit repeats at about 8 tile pitch across a 20 x 10 grid.
- Constraints: the shifted line stays a straight run on both sides; unit is repeated with identical shape (Psg216shiftmerger).
- Images: hubs_msh01shift, hubs_msh03, Psg216shiftmerger.

### P11 Parallel lane bundle (long diagonal connector)
- Does: carries 2-4 lines for a long distance between pieces. Bundles of 2 x 2 (LL_RR) run for 30+ tiles diagonally with gaps of 1-2 tiles between directions (hubs_173Mark1: central bundle; hubs_A2A: three parallel pairs running 25+ tiles; Psg115_bbh04: five parallel bundles on a long diagonal).
- Constraints: bundle keeps constant gap; lines turn together with 45 degree bends so the pitch stays 1 (hubs_173Mark1 corners). Corner "stepping" at bends: the outer line is one tile longer at each bend (hubs_A2A, hubs_173bbh02).
- Images: hubs_173Mark1, hubs_A2A, hubs_PZ05, Psg115_bbh04, 219_bbh04.

### P12 Hairpin stub / pocket
- Does: a short pair of portals facing the same way joined by a U (e.g. sidings or a return loop). Visible as a pair of portals very close with a U-turn between them (hubs_slh01 right: two portals 2 tiles apart; 206_BBH03merger centre: two portals side by side with return). Function in hub not stated in the screenshots.
- Images: hubs_slh01, 206_BBH03merger, hubs_173Mark1 (several pairs at the right edge).

### P13 Crossing under shared mainline (BBH / three-way arrangement)
- Does: branch lines pass under or over the mainline bundle and come out on the other side (BBH). 3-way and BBH use the same pieces rearranged (hubs_3way, ABR06: "3way hubs look basically all like an ultimate 3way"; Junctionary BBH images: Psg197_bbh01, Psg115_bbh04, 219_bbh04, SnakeNest).
- Footprint: the whole BBH 02-04 fits about 25-40 tiles along the mainline (hubs_bbh04, hubs_173bbh02), estimated.
- Variation: see section 3.

## 2. Height use

- Number of levels: the hub body is one level; crossings add one level up (bridge) or down (tunnel). Three levels in one footprint "tunnel - rail - bridge" is named in ABR06 ("A super-powerful move ... 3level tracks") and RULES-coop R57. On screenshots, a bridge over a rail over a tunnel is not visible in a single spot; the third level is implied. Count in hubs_173Mark1 and hubs_173bbh02: tunnels only (2 levels). Psg197_bbh01: bridges only (2 levels).
- Main line level: in nearly all images the long diagonal bundle stays level on the original land and the branches change height around it (hubs_173Mark1, hubs_173bbh02, hubs_bbh04, hubs_msh03). Exceptions: Psg197_bbh01 and Psg186_LLLLmerger01 where branches stay level and lines are bridged over each other.
- Mounds are made rather than found: portal rows sit on a ridge that is raised on flat land (hubs_slh06, hubs_slh01, hubs_bbh04). On steep land, SnakeNest and Mergers_join_downhill show lines following the slope, with retaining walls and descending runs; for the merge, downhill is helpful ("abuse gravity", MT).
- Plateau: invisible-hub screenshots (ABR10_TL5static, ABR10_tunnelhub_signs) show flat plateaus with cliff edges (desert terraforming). Only in those images is levelling large.
- Heights per hub: 2 levels typical; 3 occasionally (stated in ABR06, not isolated in an image).
- Not visible: exact height values, how much earth was moved, cost.

## 3. Composition

- Order along a flow: split (P8) -> optional shift (P10) -> crossing in tunnels or bridges (P1/P5, portal rows P2) -> merge (P9). In invisible hubs it is explicitly "just split, tunnel, and minimal merge" (abr/10 text) and traffic ends up swapped sides: "it will create some left hand driving" (ABR10 text; RULES-coop R45).
- Distances: portal rows start 4-8 tiles after the split fan (hubs_173bbh02, hubs_bbh04, estimated); pieces are joined by straight runs of 3-10 tiles; the gap between rows of portals is the width of what they pass under (hubs_173Mark1: the central bundle).
- Follow or reshape land: on flat green land the hub is built straight onto the land and the only reshaping is mounds at portals and the occasional plateau (hubs_*). On terraced land (SnakeNest, Mergers_join_downhill) the layout follows the land with embankments. In desert (ABR10) terrain is reshaped to a plateau. So the same hub shape is placed on flat or sloped land with only the portal and ramp parts changing.
- Variation for the same job: Psg197_bbh01 (bridge style, scattered flyovers), 219_bbh04 (huge multi-bundle network, tunnels), Psg115_bbh04 (long parallel bundles, along coast, with tunnels and bridge across), SnakeNest (winding lines on terraces, very different outline) are all BBH. Differences: bridge versus tunnel preference, orientation (diagonal versus axis), number of lanes in a row (3-8), and whether lines hug the land. The same piece repertoire in all.
- Hub sizes: hubs_msh01shift shows three adjacent hubs (A, B, C) of different shapes in 40 x 25 tiles; sizes vary from 15 x 15 to 40 x 30 tiles (estimated).
- Repetition: parts that look the same are copied with offset (Psg216shiftmerger 20 identical units; hubs_173Mark1 two portal rows mirrored).
- Tunnel sharing: "use as long tunnels as possible, and stuff there as much things as possible" (ABR06 Joining bridges together), so pieces are merged under one tunnel and the order of pieces is compressed.

## 4. CSP reading

Candidate variables (domain) and evidence:
- Piece type per crossing, bridge or tunnel or level (3 values): seen in 13+ images (hubs_*, Psg197_bbh01, 206_BBH03merger); ABR06: "Bridge vs. Tunnel".
- Number of parallel lanes n per direction (2-8): hubs_173bbh02 (7), hubs_173Mark1 (8), hubs_slh06 (4), 206_BBH03merger (3).
- Portal position along each lane (integer stagger, pitch 1): seen in hubs_173bbh02, hubs_bbh04, hubs_slh06, hubs_173Mark1.
- Tunnel or bridge length per lane (4..25): Psg197_bbh01, ABR10_tunnelhub_signs.
- Height of each track segment (level, +1, -1, rarely more): hubs_slh01, SnakeNest, Psg206_SLH03.
- Mound or ramp tiles in front of each portal (0-3): hubs_slh06, hubs_bbh04.
- Mainline shift offset (0-4 tiles): hubs_msh01shift, Psg216shiftmerger.
- Lane order at split (which line first): ABR06 "you can play with the actual order".
- Which tunnels carry the prio (all, one, none): hubs_173Mark2 text, ABR10.

Constraints:
- Local: portal faces along track, 1+ straight tile before and after portal (all rows); no curve on slope tiles; bridge >= 4 tiles; lane pitch >= 1; bridge head straight.
- Between pieces: tunnels may stack under bridges/rails (3 levels); a bridge cannot pass under another (ABR06); crossing pairs have equal length (R53); split before merge; mounds of neighbouring portals merge into a ridge (hubs_slh06).
- Global: mainline level fixed, flows preserve direction handling (drive on right, left-hand crossing after split-merge); total footprint within the allowed rectangle; tunnel count per direction depends on train length (ABR10).
- Terrain: tunnel depth requires land at least 1 level above the tunnel floor over the whole tunnel, else a mound (P3) is added (hubs_slh01, hubs_slh06). Not visible whether there is a minimum cover.

Cost terms:
- Terraform tiles (mounds, plateaus): seen in all desert and flat images; the compactness preference "Great way ... keep compact" (ABR06).
- Tunnel or bridge tiles (bridges "more compact", tunnels "look clean", ABR10).
- Footprint area / length of hub ("makes the hub short", ABR10).
- Curve count: "curves eat the space back" (ABR06).
- Prio length (tunnel length): "priorities ... quite long due to the tunnels" (ABR10).
- Slope tiles (acceleration loss for weak trains; MT Downhill merge for the benefit side).

## 5. Gaps vs our rules

- RULES-coop R47-R57 cover bridge and tunnel length/doubling but nothing on portals, mounds or ramps (portal facing, straight approach tiles, mound size).
- No rule for portal rows with 1-tile pitch and stagger (P2).
- No rule for terraforming (mounds, plateaus, embankments) or height steps (P3, P4, P6, P7).
- Terrain-dependent choice between bridge and tunnel ("depends on the landscape", ABR10) is not in RULES-hubs.
- Tunnel sharing (many pieces under one long tunnel, ABR06) not covered.
- "One tunnel prioritized only" prio pattern (hubs_173Mark2 text) not covered.
- Mainline shifts as repeated units (Psg216shiftmerger) not covered.
- Downhill merge as acceleration aid (MT, Mergers_join_downhill) not covered beyond a note.
- Lane bundles that bend together at 45 degrees with the outer line one tile longer (hubs_A2A, hubs_173Mark1) not covered.
- Hairpin pockets (P12) not covered; their function is not visible.

## Images viewed (27)
hubs_173bbh14, hubs_slh04, hubs_3way (orchestrator, not re-viewed), hubs_173Mark1, hubs_173bbh02, hubs_slh01, hubs_slh06, hubs_msh01shift, hubs_msh03, hubs_bbh04, hubs_A2A, hubs_PZ05, ABR10_intro, ABR10_SLH03, ABR10_TL2static, ABR10_TL5static, ABR10_tunnelhub_signs, Psg197_bbh01, 219_bbh04, Psg115_bbh04, Psg186_LLLLmerger01, Psg216shiftmerger, 206_BBH03merger, Psg206_SLH03, Mergers_join_downhill, Mergers_bridges_basic, SnakeNest.

## 6. Game rules checked in code (orchestrator, 2026-11-09)
- Bridges never cross: `CmdBuildBridge` refuses any span tile that already has a bridge above it ("Disallow crossing bridges for the time being", `tunnelbridge_cmd.cpp` ~507). A bridge ramp may sit under a perpendicular bridge unless the upper deck is at the ramp's deck height (~486-491).
- Tunnels cross at different depths: `CmdBuildTunnel` refuses only a perpendicular tunnel at exactly the same z (`IsTunnelInWayDir`, `tunnel_map.cpp` 48-60; skipped with the crossing_tunnels cheat). So per tile: surface track, plus one tunnel per depth, plus at most one bridge span above.
- Land can't be lowered onto a tunnel (`terraform_cmd.cpp` ~250): a cutting dug over a tunnel must stay above it.
- Tunnel price grows with length (`cost += cost >> tiles_coef`, coefficient steps every 25, 50, ... tiles).
