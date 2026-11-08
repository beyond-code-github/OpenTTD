# Station design patterns in the openttdcoop archive images

Method: 53 images viewed once each (list at end), plus ABR07/09/11/13 blog text. "(?)" = uncertain reading.
Iso view: tracks run along two diagonal axes; "platform end" = the end where trains enter/leave a platform.
Image filenames: ABR07_* / PZ13_* / abr11_* are in `abr/images/`; the rest in `coop/files/`.
Quotes are from V453000's ABR07 (stations) unless stated.

## 1. Pattern catalogue

### P1. Staggered platform ends (one platform per step) - "stairstep" / chevron
- Structure: parallel platforms of equal length, each shifted 1 tile along the track axis from its neighbour, so the platform heads lie on a straight diagonal line (and tails on another). Each platform gets its own entry spur off a main entry bundle and its own exit spur; because the ends are offset, entry spurs peel off at successive positions and exit spurs rejoin at successive positions with no two spurs sharing a tile. Entry feeders A..E on the lower/open side, exits F..K labelled one per platform.
- Seen: RORO1.png, RORO3.png (6 labelled platforms plus 5 entry letters, fan wraps round the platform tail; platforms run in a V/chevron), Pre_balanced_roro.PNG (two groups of 6 and 5, each internally stair-stepped), Roro_dedicated_plats.PNG (two banks of 8, stair-stepped, each platform with a dedicated line from a vertical spine both sides), Roro_all-to-all.PNG (3 roofed blocks stair-stepped), Roro_all_to_all_pz05.PNG (stair-stepped roofed blocks bottom-left), PSG192.png (tessellation of stair-stepped roofed blocks).
- Why: not stated as a rule in text; the image captions are the wiki's RORO variants. Rationale visible in the pictures: the fan/exit spurs stay parallel and short, so the whole station is a parallelogram that packs tightly into a diagonal band.
- Character: footprint minimal for the platform count (no bays; width = platforms, length = platform + 2 fans). Throughput = whatever the entry tree gives; all-platform capacity usable.

### P2. Two (or more) groups staggered against each other - "block stagger"
- Structure: platforms split into 2 banks (e.g. 3+3, 5+5, 6+5, 8+8), second bank offset along the line, each bank fed by one branch of a first-level split. Entry and exit spines are vertical (run perpendicular to platforms) and each bank takes its own full-length run of parallel feeder lines.
- Seen: ABR07_braking_03.png (3+3, second bank offset ~platform length), ABR07_gap_02.png (6 platforms: first split in half, then each half to 3), Roro_dedicated_plats.PNG, Pre_balanced_roro.PNG, ABR07_logic_01.png (PSG192 early form: 5+5 with the second bank offset and roofed), Psg149_pbs.png (Nickman "compact version", "10 platforms on 15 tiles"; two banks of 5, interleaved in opposite directions (?)), Psg186_towndrop03.png (Town Drop 03: 2 banks stacked with offset, big U-shaped entry and exit loops).
- Quotes: "if you need to split one line into 6 platforms, you can first split in half, then each line goes to 3 ... making the first split gaps so long that [the later gaps do not slow trains]" (gap_02); "if we split the station into two, ... the first array has longer braking spaces than the second one" (braking_03).
- Character: the stagger is what lets the second bank's entry and braking space sit beside the first bank rather than in front of it. Cheap in length, wider than P1.

### P3. Fan entry / "pre-balanced" distribution
- Structure: N entry lines each fan into per-platform spurs; spurs of different lines cross by diamonds or under each other, so any line reaches any platform ("every incoming line can join any platform"). A "pre-balanced" version splits the incoming pair into per-bank bundles BEFORE the platforms, so each bank is fixed to one subset of the lines (Pre_balanced_roro.PNG: two incoming lines, entry tunnel stub, then two bundles of 6 and 5 lines to the two banks).
- Seen: ABR07_balanced_01.png, balanced_02.png (entry/exit of a balanced station: a split with a priority tunnel stub and a bypass loop), ABR07_balanced_03.png (PSG190 oil drop, "XeryusTC style": staggered platforms with a tangle of crossings and tunnels on the entry), Station_entry_load-balancer.png (A/B/C crossings diagonally, a tunnel loop at the start, "2 tiles for braking"), Roroentrance_1.PNG (entry fan across red bridges), Twoentry.PNG (two entries A/B merging into D..H bundle through a cross/diamond).
- Quotes: "Balanced entrances are quite essential for pickups"; "Note: There is one considerable advantage for unbalanced stations: easy expandability by just cloning them."
- Character: best platform use (fewer platforms for the same load) but the complex bit is the entry, and it is NOT clone-extendable (see section 2).

### P4. Direct (unbalanced, per-line) entry; "dedicated platforms"
- Structure: each incoming line owns a bank of platforms; no crossings; spurs fall off the line in sequence ("every Xth platform joins one of the X lines" on the exit).
- Seen: ABR07_direct_01.png (one split, 2 spurs, signal gaps highlighted), ABR07_Xth_01.png (3 exit lines labelled 1,2,3 weaving: each platform's exit goes one of three ways, lines cross by tunnels, platforms at the lower left), Roro_dedicated_plats.PNG, TerminusPrimary.png, Terminal1.png, Twoentry.PNG.
- Quote: "Drop stations ... Balancing the entrance isn't that necessary either! ... (PZG13 for example)".
- Character: tightest, fastest; no waits inside; clone to extend.

### P5. Buffered (waiting bays) vs direct entry; fast bays
- Buffered: waiting bays between a split and the platforms, trains stop to wait for a free platform (ABR07_buffer_01.png: 3 trains in two bays beside the platform approach; ABR07_buffer_02.png: bays directly in front of each platform as parallel full-length lines in front of the platform bank, trains released as soon as a platform frees).
- Fast bays: very short signal gaps behind the pre-signal so the line stays full-speed through whichever branch is free (ABR07_fastbay_01.png, platform bank top right, highlighted short gap in a bypass loop at the bottom-left).
- Quotes: "Making waiting bays usually applies to stations with balanced entrance and trains with good acceleration"; "bays right in front of the platforms ... trains get into the platforms right after they get free".
- Character: bays cost length (1 TL each) but buy platform utilisation; direct costs nothing but needs unbalanced/fail-safe design.

### P6. Synced station (all platform paths equal length)
- Structure: platforms staggered so entry + exit path length through each platform is the same; trains emerge in the same formation they entered. Shown as 2 platforms offset on a diagonal (ABR07_sync_01.png: 2 platforms, parallelogram), and as doubled banks of 3 with the second bank offset by a full bank length (ABR07_sync_02.png has single platforms in stair-step along a long diagonal (?) and ABR07_braking_03.png shows 3+3).
- Quote: "all platforms take equally long path to go into and out of in total ... you have to use the direct entrance. Any balancing could break it". PZG13 drop "worked on the same pattern ... but it was further doubled because of extreme train inflow."
- Character: maximum throughput for packed fast lines; stagger offset is a hard constraint (equal path length), length cost is high.

### P7. Terminus with X / X-less
- X: entry and exit share one side; two lines cross in front of the platforms in a double diamond (ABR07_X_01.png: 2 circled crossings of 2 and 3 lines; Terminal1.png; Thraxian_Terminus-Short.png: 2 platforms, one X; Dedicated_terminus_2p.PNG: roofed 2-platform terminus off a loop with one bypass; TerminusPrimary.png).
- X-less: the in/out lines never meet: entry diverges to one side, the exit from the other (ABR07_Xless.png: platform pair at top, a diverging entry and parallel exit).
- Quote: "the only real difference is that you use the same side for both entrance and exit"; "station throughput can be virtually endless, provided you just make them large enough"; X-less "platforms can be used as frequently as of a ro-ro station".
- Character: half the length of a ro-ro; platform usage worse; one-sided so rebuild touches both entry and exit.

### P8. Roro (roll-in roll-out) fan, 1-sided and both-sided
- Structure: entry bundle one side, exit bundle opposite, trains never block leavers (RORO1-3, Pre_balanced_roro, Roro_all-to-all, Roro_dedicated_plats). Roro_all-to-all.PNG: 3 modules of 2-3 roofed platforms stacked vertically beside 3 tunnel stubs at left.
- Station_diagramm_layout.png: 4 schematics of a platform block with stubs fed from top/bottom and exits both sides (single-bank arrangements; reading is a diagram only, (?)).
- Character: the standard; larger than terminus by the exit fan.

### P9. Tiled "unit cell" stations
- Structure: a compact module (roofed 3-4 platform terminus-like block plus a small triangle/diamond feeder and its own tunnel mouth) repeated in a diagonal lattice (PSG192.png: ~30 modules over the whole image, each with its own entry spur, depots at the left (?)). abr11_psg225_north.png: one refit pickup made of ~7 identical unit rows, each row = depot + tunnel entry + platform group, tiled diagonally along a long pickup band. abr11_psg205_drop3.png: ~9 depots in diagonal sequence, each with its own bay + tunnel, feeding the same drop band.
- Character: horizontal scale by repetition. ABR11 admits the limit: "The layout remained unchanged as it is not too modifiable."

### P10. Entry via "ladder of diamonds" (crossing loops)
- Structure: a block of diamond/loop cells (each a small ring of track with 4 crossings) laid in a lattice in front of the platforms; trains choose path through the lattice. Station_layout_with_tunnels.png (Koln Valley; 4+ diamond rings both entry and exit, three roofed blocks centre) and Station_goods_drop.png (Town Drop; 6 diagonal platform blocks, diamond rings top left and bottom left, tunnels at both ends), Station_coal_ore_drop_ottdcoop.png (hexagon/diamond rings sprawled across the whole entry; "do not remove these" comments; platforms are single tunnel-fed rows).
- Quote (on those comments, not an explanation): "do not remove these / what do they do? / improve presignal logic".
- Character: balanced-equivalent through repeating one cell; footprint large.

### P11. Logic splits (flip-flop) and others
- ABR07_hypno.png (PSG179 Hypnotizer: bridge/tunnel tangle); ABR07_logic_01.png/logic_02.png (PSG192's flipflop version versus "a proper station entrance": a plain 5-platform entry from one side and roofed bank on the right).
- Quote: "logic should be used when needed, not whenever possible."
- Character: not a design pattern for modularity; skip in a generator.

### P12. Transfer / SRNW / refit (special cases)
- Srnwstationbig.png: 8 near-identical rows (4 + 4) of long platforms, each row a dummy-loader unit with a tank/loader, each fed by its own tunnel and feeder at left; Psg200_SRNWtransfer, PSG239_new_SRNW_station.png: 6 compact transfer cells with 3 bays each; Psg149_pbs.png: 10-platform sync SRNW. abr11 refit images (psg213_rorodepot, psg214_1, psg200_goodsyard, psg205): depots as the "bay" ahead of platforms.
- Not general station geometry; mentioned for the repeated row unit (section 2).

## 2. Modularity

| Pattern | Repeating unit | Extend by | What must be rebuilt | Evidence |
|---|---|---|---|---|
| P1 stair-step | 1 platform + 1 entry spur + 1 exit spur, offset 1 tile from the previous | Add a row at the open end | Entry fan and exit fan each grow by one spur; the main entry bundle must have a spare lane. Existing rows untouched | RORO1/2/3 (identical but different counts, 5-6 platforms) |
| P2 bank stagger | Bank of k platforms + its spine segment (k = 3, 5, 6, 8 seen) | Add a bank offset by one bank length | The first-level split (1 to n banks) | Roro_dedicated_plats (identical two banks of 8), braking_03 |
| P3 fan/balanced | Not clean; per-line fan plus crossings | Rebuild the whole fan | Entry (all of it) and sometimes exit | Quote "unbalanced ... easy expandability by just cloning" |
| P4 direct | One line + its bank | Clone the bank | Nothing; just a new line | Quote above |
| P9 tiled | Whole cell (3-4 platforms + feeder + tunnel) | Add cell on the lattice | Only the trunk feeder | PSG192, abr11_psg225_north |
| P10 diamond ladder | Diamond cell | Add cells | Ladder dimension grows both ways | Station_goods_drop, Station_layout_with_tunnels |
| P12 SRNW rows | Row = loader + platforms + own tunnel feeder | Add row | Trunk feeder | Srnwstationbig |

- Module sizes readable: P2 bank = 3 / 5 / 6 / 8 platforms; per-platform pitch is 1 tile of platform + 1 tile of track gap (stair-step offset 1 tile along the axis, from RORO3 / Pre_balanced_roro).
- Text on extension: "There is one considerable advantage for unbalanced stations: easy expandability by just cloning them."; "Balancing station entrance ... should mean that you need less platforms too." (so balanced = fewer platforms, harder to extend).
- A bank is the unit of extension; the entry split above the banks is the only thing that changes (n-way split, with signal gaps lengthened at the first splits: ABR07_gap_02).
- Doubling: "the station worked on the same pattern ... but it was further doubled" (ABR07 sync_02).

## 3. Layering (multi-level)

What the images show (not what the text says; the text barely discusses layers):
- Tunnels as the crossing method: in ABR07_balanced_04.png (PZG05 factory drop, "XeryusTC styled exit") a stack of many parallel feeder lines run past each other and dive into tunnels every few tiles, crossing underneath neighbouring lines, so ~8+ lines weave with no at-grade diamonds. In ABR07_Xth_01.png the exit lines 1/2/3 cross under each other through short tunnels beside the platform bank; the "every Xth platform joins line X" structure is made by tunnels, not by junction crossings.
- Entry tree layers: Station_layout_with_tunnels.png: entry is on the upper level (diamonds + feeders), then three tunnel portals dive under the track from the first rows to reach the later platform blocks; exits mirrored at lower right. This is the nearest to the engineer's "lines pass under the first layer's platforms".
- Roro_all-to-all.PNG: 3 tunnel portals at the left take feeders under the vertical entry spine, then come up in front of each platform block (platform blocks are at one level; the feeders for the later block pass under the first block's feeders).
- Roro_all_to_all_pz05.PNG: ~10 tunnel portals and ~6 bridges; bridges carry entry lines over the earlier ones, tunnels take them under; the stack of 2-3 levels sits in front of the roofed blocks at lower-left.
- Roroentrance_1.PNG: the entry fan is built on red truss bridges (3 levels visible), lines peel off across the others on the bridge deck, then descend to platform level (the bridges end at the platform fan).
- Psg193_BRIDGES.png: whole "bridge country" lattice of long bridges crossing perpendicular (not a station).
- Exits crossing under later layers: ABR07_balanced_01/02 show exit lines going through a tunnel stub under their own bypass; no image clearly shows "exit of layer 1 passing under layer 2 platforms" as a deliberate pattern. Closest: Station_goods_drop.png (tunnel portals at the bottom-left right of the diamond ring: lines for the later blocks pass under the platform bank (?)).
- Buys (from images): crossings without signal blocks shared (no blocking, ABR07 "Blocking"), shorter fans (lines reach later banks without detours), narrower footprint than diamond laddering. Costs: tunnel portals need approach length (braking/signal blocks inside the tunnel are shorter than a TL unless signals sit at portals).
- Not seen: platforms themselves on two levels (bridge-over platforms). All platform banks are at one level in every image viewed.

## 4. Generator implications
- Platform bank: k platforms, platform length L, pitch (platform + gap), stagger offset s (0 = aligned, 1 = stair-step, bank length = block stagger).
- Entry tree: n-way split built from bank spurs; split gaps lengthening with depth ("first split gaps long, later short"); direct (clone) or fan-with-crossings flag.
- Exit tree: mirror of entry; "every Xth platform joins line X" generator for the exit side (Xth_01).
- Station shape: roro vs terminus (X or X-less), flag for same-side exit.
- Bank count and bank offset (equal path = synced; unequal = free).
- Waiting bays: none / before-bank / before-each-platform; fast-bay gap rule.
- Crossing method per crossing: diamond / tunnel / bridge, with a level counter (max 2-3 seen) and portal-approach length.
- Module = bank + its spine; extension = clone module at offset and widen the root split; if balanced, regenerate the entry.
- Roof/platform decoration is independent of routing (ignore).

## 5. Gaps
- The blog text does not describe layered stations or stagger at all; both are inferred from images, and stagger is only named by the engineer.
- Sizes (platform length, spacing, number of tiles) are read from sprites only; no ruler or tile counts except "2 tiles for braking" (Station_entry_load-balancer) and "10 platforms on 15 tiles" (Psg149_pbs).
- Signal placement is not legible in the large images; only highlighted gaps in ABR07 small images.
- Which of Roro_dedicated_plats vs Pre_balanced_roro has crossings is judged from the picture (none visible in Roro_dedicated; 6 of 7 fans) - unverified in-game.
- No image shows an exit that passes under a later platform bank unambiguously.
- Direction of travel (which end is entry) is inferred from labels A/B etc., not stated.

## Images viewed (53)
ABR07: balanced_01, balanced_02, balanced_03, balanced_04, Xth_01, MAZE, X_01, Xless, direct_01, buffer_01, buffer_02, fastbay_01, gap_02, hypno, logic_01, logic_02, sync_01, sync_02, braking_03.
coop: Pre_balanced_roro, Roro_all-to-all, Roro_all_to_all_pz05, Roro_dedicated_plats, Roroentrance_1, RORO1, RORO3, Dedicated_terminus_2p, Dedicated_terminus_japan, Station_entry_load-balancer, TerminusPrimary, Terminal1, Thraxian_Terminus-Short, Twoentry, BasicStation1, Srnwstationbig, PSG239_new_SRNW_station, Station_layout_with_tunnels, Station_goods_drop, Station_coal_ore_drop_ottdcoop, Station_diagramm_layout, Psg147_coaldrop, Psg149_pbs, Psg175_drop, Psg186_towndrop03, Psg193_BRIDGES, Psg216_coaldrop, PSG186, PSG189, PSG192, PSG260.
abr: PZ13_dropS (title card only), PZ13_joiners_station, abr11_psg205_drop3, abr11_psg213_rorodepot, abr11_psg225_north, abr11_psg200_goodsyard, abr11_rorodepot_1 (depot demo, no station), abr11_psg214_1.
Junctionary*Stations images are 75x75 icons: not viewed.
