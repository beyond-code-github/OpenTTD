# INVENTORY-gadgets: local shapes the sources name or build (read-only recon, 2026-11-21)

Facts and citations only. Granularity is D398 (per node, per structure, per prio row, per port). Paths: `DEC` = docs/junction-tool/DECISIONS.md, `COMP` = DESIGN-components.md, `SSS` = DESIGN-sss-v2.md, `RG` = wip/RECON-gadgets.md (section numbers are its own), `OD` = /mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad. Units: ht = half tile. Sketches head east; `=` track, `/` `\` diagonal pieces, `H` bridge head, `P` prio row, `T` tunnel portal. Sketches marked (text) are drawn from a ruling's words, not from a source picture.

"Needed by" is taken from RG §4 (T1 = 6 nodes, 3 prio-gated; T2/T3 = n-lane fray and join sites, prio B/E lanes; LL2 = 6 entry frays, 6 lane splits, 12 merges, prio on 6 lanes; PSG 197 = 62 nodes: 31 splits (8 fray, 23 lane split), 31 merges (9 / 10 / 12 by entry sets), no prio routes, 50 tunnels in 18 groups, ports N 2/2, E 3/3, S 3/3). Where a topology's need is not stated in those sources it says "unknown".

Sources not swept in full: DECISIONS.md (read by grep for the listed terms, then the rows in full for D79, D81, D110, D113, D118, D145, D203, D299, D312, D314, D342, D346, D347, D349, D351, D353, D355, D364, D372, D375, D376, D380, D382-D384, D388, D390, D398; the other ~230 rows were not read); RECON-corpus-census*, graph1/1b, RECON-corpus-ll2 (heads only), RECON-psg-hubs, RECON-psg-saves, RECON-bundle-copies, RECON-prio-footprint, RECON-llrr*, DESIGN-modules/-nodes/-pieces/-tracks/-planar, SPEC-* files in wip; BBH_building_* and hubs_* images (not opened); wip/psg197-gaps (cited by SSS but no such file found under docs/junction-tool or wip).

---

## Group A: nodes (splits, merges, tridents) - 9 entries

### A1. One-lane split (coop: exit / "line split" / "direction split" by use; our: Fork, C2 lane split)
- **Purpose:** one track divides into two legs.
- **Nodes and tracks:** one split node, 1 lane in, 2 out. Legs are the optional diagonals h-1 and h+1 from the in line's end point (RG §1.1); which leg runs straight is a Slots/Shape matter (D360, D380). Coop names the same shape a *direction split* (legs to different out ports, fray) or a *line (choice) split* (both legs to the same out ports); the shape is the same, the site kind differs (RG §1; ANATOMY-psg227 block table, ANATOMY-psg197 block table).
```
0 =====\=====         (text, COMP C2: the diverging leg steps one line out)
1      \====
```
- **Tile sharing:** the switch tile holds the trunk axis piece and both leg halves (join of the strands at one point: `Node({in,left,right})`, RG §1.1; `at_shared_node` exemption RG §2). No other sharing.
- **Fixed vs free:** legs lb 2 ht each, may vanish (optional, cost 1); fork gap between leg ends >= 2 ht (RG §1.1). D355: gaps are lower bounds. D113: between a split and the next bridge head at least one signal-legal piece; D117: one signal may serve several jobs. COMP C2 says signal room before and after (R2, R6); not in code (RG §5).
- **Variants:** diverge left or right; axis or diagonal trunk (D380: a diagonal trunk with two axis legs cannot share one point; each leg gets a 0-or-1 ht stub, "a coop build puts the two switches in adjacent tiles"); straight leg chosen by the slot solve (D360).
- **Evidence:** D24, D110, D113, D117, D204, D330 ("split = fray of one lane"), D360, D380, D398; COMP C2; RULES-coop R8-R11; ANATOMY-psg227/-psg197 (every direction and line split is a 1-in/2-out block of 1x1 to 3x2 tiles).
- **Status:** built today (RG §1.1 `Fork`, per node for LaneSplit sites; fails for a straight leg inside a mixed site, RG §1.1 and §5); signal-room rows not built.
- **Needed by:** T1, T2, T3 (as the per-lane splits of a fray), LL2 (6 lane splits), PSG 197 (31 splits).

### A2. One-lane merge (coop: join / merger node; our: lane merge, one-lane Join, C4 element)
- **Purpose:** two tracks become one.
- **Nodes and tracks:** one merge node, 2 in, 1 out: straight S (h), joiner J (run h, then landing diagonal toward S), node = `Tie(End(J), End(S))`, out O (h) with optional exit step then 4 ht stub (RG §1.3). With or without a prio row (see B1).
- **Tile sharing:** the merge tile holds the two ends and the out axis piece (`Node({S,J,O})`). Exempted by `at_shared_node` (RG §2).
- **Fixed vs free:** S lb 2 (lb 2P+2 with a prio); J run lb 2; landing diagonal lb 2 (per node) or lb 6 / 4 (n-lane site); O lb 2, cost 1.5; stub lb 4 (RG §1.3). R6: one plain axis tile between a merge tile and the next bend (DESIGN-blocks §3). Bay R5: a non-prio route runs >= 2TL + 4 ht from its last split to each 2-in merge (RG §1.9).
- **Variants:** joining side left/right (sigma); with prio (B1) or without ("plain merge"); gated by an entry signal (B7); 3-input and cascades are A7/A8.
- **Evidence:** D24 (split before merge), D224 (legs of one lane merge first, one joiner), D230-D232, D302 (a doubling's rejoin is not a D24 merge), D330 ("a site with more than two inputs is a chain of two-input merges"), D398; COMP C4; RECON-mergers §1-§3.
- **Status:** built today (RG §1.3 `Join` n = 1, via `build_nodes`); exit step to the left with n > 1 fails (RG §1.4).
- **Needed by:** T1, T2, T3, LL2 (12 merges per node), PSG 197 (31 merges; 12 same-port-set "lane merges", RG §4).

### A3. Split or merge on a diagonal trunk with two axis legs (stub variant of A1/A2)
- **Purpose:** places the two legs of a diagonal-trunk node on the tile grid.
- **Nodes and tracks:** one split (or merge); each leg gets a stub of the trunk heading of 0 or 1 ht before its first piece (after its last at a merge) so the two switches fall on adjacent tiles.
- **Tile sharing:** none beyond A1/A2; the two switch tiles are adjacent, not shared.
- **Fixed vs free:** stub 0 or 1 ht, chosen by the grid rows (D380; parities D382).
- **Variants:** split or merge; mirror.
- **Evidence:** D380 (from LL2 node 18, A2 SW forking to A2a S and A2b W); D382.
- **Status:** ruled (orchestrator default, toggle candidate); not built as a catalogue shape.
- **Needed by:** LL2 (node 18 named); T2/T3/197 unknown.

### A4. Trident (D110 (a): clean three-way split; D398: catalogue shape)
- **Purpose:** two consecutive split nodes on one trunk sharing one tile, with no plain tile between them.
- **Nodes and tracks:** two split nodes (one trunk in, three legs out). D398: "axis trunk; straight leg and both diagonals on one tile". D110's original case: SL, lane A and lane B leave one tile ("clean three-way split"); the alternative (b) is exactly one plain tile carrying a signal between the two splits. Sketch (text):
```
        /=== leg 1 (diagonal)
 ======X=== leg 2 (straight)
        \=== leg 3 (diagonal)        X = the one trident tile
```
- **Tile sharing it relies on:** the trident tile (trunk axis piece, straight leg and both diagonal halves on one tile). Ruled as the only cross-node sharing between splits (D398).
- **Fixed vs free:** shape fixed; D110's rule picks per pair: gap 0 tiles -> trident, else at least the signal tile (D398). Exact piece list is not given in any source beyond the sentence above.
- **Variants:** applied by rule to any two consecutive split nodes on one trunk of any kind: a topology split, a doubling's copy split, a prio row's split (D398, engineer: "a split that immediately runs into a doubled bridge can be a trident, with one leg splitting and the other two starting on the bridge", A5). Mirror (two merges on one tile): ruled "only once the corpus or PSG 197 shows it" (D398). Axis trunk only is stated; diagonal trunk not stated.
- **Evidence:** D110, D398 (the ruling), D113, D117, D118, D145 (shared split tile, one plain piece, head); CARD-E3-catalogue.md. Corpus: a 1-in/3-out line split N90 to S, block 1x2 at 92,157-158 in PSG 197 (ANATOMY-psg197-bbh block table "Line split N90 to S (3-way)"), the three legs entering three tunnels.
- **Status:** ruled, not built. "trident" has no function or constant (RG §2); D110's tight geometry is in the legacy generator (junction_generator.cpp:214, :387, :483) and built in the composer only via exact deck heads (RG §1.7, §2).
- **Needed by:** unknown for T2/T3/LL2 (T family doubles B1, E2 in series, RG §1.7 / D322); PSG 197 has the N90 three-way line split.

### A5. Trident into a doubled bridge (split node followed by a doubling's copy split)
- **Purpose:** the D110 original case: the sideline split and the lane (copy) split of a doubled bridge entry on one tile.
- **Nodes and tracks:** a topology split plus a copy split on one trunk; one leg splits, the other two start on the bridge (D398 engineer quote). Same shape as A4 with the copy split as the second node.
- **Tile sharing:** the trident tile (A4); deck heads of the two copies follow (D118: one signal tile per leg between split and head, stagger one tile).
- **Fixed vs free:** near-end constants D145 (x_ls - x_heA = 2, x_ls - x_heB = 1; "shared split tile, one plain piece, head"), the far end mirrors it (D314).
- **Variants:** as A4 and E1.
- **Evidence:** D110, D118, D145, D314, D398.
- **Status:** ruled, not built (RG §1.7 builds the doubled entry by exact heads 4/0 without a trident node).
- **Needed by:** unknown.

### A6. Three-input join on one tile or adjacent tiles (coop 227 "3-into-1")
- **Purpose:** fixed lane plus two choice lanes meet at one point.
- **Nodes and tracks:** three merges' worth of inputs, 3 in / 1 out; engineer: "either all convene on a single tile, or are on adjacent tiles such that there is no space for a signal between them - effectively the same thing". Corpus: PSG 227 joins N353, SW362, SW363, each a 1x2 block (ANATOMY-psg227-bbh block table).
- **Tile sharing:** one tile or adjacent tiles for three inputs (no signal between); not defined at piece level in the sources.
- **Fixed vs free:** none stated.
- **Variants:** with pre-signals: fixed input gated by a two-way combo, both choice inputs by entry signals (ANATOMY-psg227 prio style 1).
- **Evidence:** ANATOMY-psg227-bbh (engineer's reading 3, trace "Join N353 (3 inputs)"); COMP C4 "3-into-1"; D311, D330 (generic path composes chains of two-input merges, no composite blocks).
- **Status:** seen in the corpus only; today's composer builds a chain of one-lane joins (RG §5, C4 row). 
- **Needed by:** PSG 197 uses binary cascades, not 3-input joins (ANATOMY-psg197 findings (2)); PSG 227 only; T2/T3/LL2: unknown (LL2's exit join takes three channels, composed per node, DESIGN-components §9 via RG §4).

### A7. Two-stage (2+2) cascade join (coop 227 N350, E342, E343; 197 joins)
- **Purpose:** the two choice lanes merge first, then the merged track meets the fixed lane.
- **Nodes and tracks:** two merge nodes in series on one out line; an entry signal on the joint piece, one tile or one signal gap before the second join (ANATOMY-psg227 findings (3)). 197: binary and cascaded, block signals with a few entry-gated inputs.
- **Tile sharing:** none stated.
- **Fixed vs free:** D224 says the legs of one main lane merge first and join as one joiner; the sources give no lengths.
- **Variants:** with/without prio on the second stage.
- **Evidence:** ANATOMY-psg227-bbh, ANATOMY-psg197-bbh findings (2); COMP C4 "2+2 cascade"; D224, D228 (cascades of two-way nodes, D24 holds), D330.
- **Status:** built today as a chain of per-node joins (RG §1.3, §5); ruled.
- **Needed by:** LL2 (12 merges incl. exit joins), PSG 197 (joins to N/S/E, 1x2 to 9x1 boxes), T2/T3 (D224 band merges).

### A8. Peel splits (coop 227 E345)
- **Purpose:** a track peels off several legs in a row along one stretch.
- **Nodes and tracks:** a cascade of 1-in/2-out splits on one trunk, 6x1 and 5x1 tile blocks (ANATOMY-psg227 "Peel splits E345 374-379,345 and 374-378,346"). No picture or lane table in the source beyond the block box.
- **Evidence:** ANATOMY-psg227-bbh block table; D228 (cascades approved), D398 (several nodes with free faces between).
- **Status:** seen in the corpus only (cascade of A1 nodes by D398).
- **Needed by:** unknown.

### A9. n-lane fray and n-lane join (composition of A1/A2 plus order swaps; not a gadget under D398)
- **Purpose:** a bundle divides by destination (fray) or converges (join).
- **Nodes and tracks:** n splits (one per in lane) plus order swaps (C8) so each destination's lanes are adjacent; join: n merges plus prio rows plus crossing of joiners (RG §1.2, §1.4). Today the composer builds them as per-site shapes: fray lanes' split points exactly `2 * slot gap` apart, `S_j` crossing the farther `V_i` on a deck; join with `J_k` landings lb 6 (fixed when n == 2).
- **Tile sharing:** fray/join node points; crossings via decks (RG §1.2, §1.4).
- **Fixed vs free:** D355: fray split gaps are lower bounds (code has exact equalities).
- **Evidence:** D311 (2) (one Fray and one Join type composed from primitives), D330, D398 (per site was option A, ruled out; B: several node gadgets plus crossing gadgets, space between is free faces); CARD-E3-catalogue.md.
- **Status:** built today per site (RG §1.2, §1.4); under D398 to become per-node gadgets, not built.
- **Needed by:** T2, T3 (3 sites of 2/3-lane frays and joins each), LL2 (entry frays are 2-lane sites); PSG 197: none (all 1-lane, RG §4).

---

## Group B: prio - 6 entries

### B1. Prio row on a straight main line (coop "standard prio" / extra-track prio; our C5)
- **Purpose:** a parallel unused track joined to the main line at intervals upstream of a merge, so a main-line train holds the joiner's merge signal red.
- **Nodes and tracks:** the merge (A2) plus a row one lane beside the main line on the side opposite the joiner's approach (D75, D87, D311 (3)); joins are two 45-degree pieces to the row. Pattern in RECON-prio-element §3 (p = prio tiles, n = p rounded up to even): ML joins at offsets 0, 2, ..., n; ML signals at odd offsets; row tiles steps 0..n (step 0 holds h2 only, even steps axis+h2, odd steps axis); row signals at odd steps facing upstream, farthest Exit, others Combo; joiner's last signal before M becomes Entry.
```
0     PPPPPPPP        row (adds a line for P tiles)       (COMP C5)
1 ==============X==   main line, merge at X
2 =============/      joiner
```
- **Tile sharing it relies on:** join tiles hold the main-line axis piece plus the half pieces h1/h2; the join on the merge tile holds h2 only on the row side, no axis stub (D79, D81); ML tile at M holds the ML axis, joiner's half piece and h1 (D79). Allowed by `at_shared_node`, `ok_pair` and the checker's CheckPrio shape clauses (RG §2).
- **Fixed vs free:** length exactly the coverage P tiles along its path, never longer (D384, amends D158 and SPEC-slots-lengths §8.1: an equality); no node or bend on the main line within the coverage plus one segment (D108, R9 in DESIGN-blocks); the row is a line of its lane's bundle (D349). One join per signal block; one signal between each pair of joins (RECON-prio-element §1 rules 7, 9).
- **Variants:** side (left/right); coverage P from the slider (D233 sizing, D280 benchmark prio 14 tiles); odd P adds a piece (RECON-prio-element §3).
- **Evidence:** D25, D70, D75, D79, D81, D107-D108, D111, D158, D282, D287, D342, D349, D384; COMP C5; RULES-coop R18-R30, "Prio construction"; wiki Prio ("Standard prios: ... can be made infinitely long without causing any delays", trains within 9 tiles); RECON-prio-element, RECON-prio-footprint.
- **Status:** built today as a fixed-length unused strand beside S (RG §1.5); D75's trailing 45-degree pair is placed by the signals pass (PlacePrio, junction_signal_pass.cpp:310), not by the composer (RG §1.3, §1.5); the row does not bend with its lane in the composer (D349/D384 say it follows its lane).
- **Needed by:** T1 (B1, E1), T2 (B1, B2, E1, E2), T3 (B1-3, E1-3), LL2 (E1, F2, A1, B2, C1, D2); PSG 197: none (no prio routes, RG §4).

### B2. Prio row on a bend or diagonal main line, with diagonal halves
- **Purpose:** a prio whose protected track is not straight.
- **Nodes and tracks:** as B1; the row may run on the opposite diagonal half of a main-line coverage tile (two parallel diagonal pieces, no switch); a join on a diagonal main line is an axis connection piece (D374).
- **Tile sharing:** D353 (the opposite diagonal half of a main-line tile); `JunctionTracksMayShareTile` (only Upper|Lower or Left|Right, never X|Y; D182, D217), `Clashes.ok_pair` (RG §2); the checker narrows to tiles where the prio track touches the main-line piece (CheckPrio, RG §2).
- **Fixed vs free:** coverage rule as B1; between joins the row may leave the main line (tunnel, bridge, gap for another track, D342).
- **Variants:** bendy prio on a curve; straight; diagonal main line.
- **Evidence:** D111, D342, D349 ("the row follows its lane as an offset copy through bends"), D353, D374; RG §1.5.
- **Status:** ruled; not built (no row for a prio on a diagonal main line, row does not bend, RG §5 C5 row); checker loosening to-do (D374 note).
- **Needed by:** T3 (prio stagger probe, D347 context); T2/LL2/197: unknown.

### B3. Prio row with n-lane staggered join (rows of several lanes)
- **Purpose:** several lanes of one join each carry their own prio row.
- **Nodes and tracks:** for each lane a row; the bundle widens by that line over the join and coverage; neighbouring lines bend around the prio, outer lanes shift out by the rows inside them (D349); in code: `Double(S_j..)` for each joiner crossed before the lane's coverage, 2 ht fixed step back, joiners crossed P tiles apart (RG §1.4, D322).
- **Tile sharing:** none beyond B1/B2.
- **Fixed vs free:** O_j walk-out step lo 2, hi 2j; rows exact across-travel gaps from slots (RG §1.4).
- **Evidence:** D322, D349, D350, D355, D356 (the diagonal js[k] is construction, not a rule).
- **Status:** built today for n-lane Join (RG §1.4), D349 widening only partly (RG §5).
- **Needed by:** T2, T3.

### B4. Prio over a bridge (coop "prio over a bridge": standard prio, presignal prio, PBS prio over bridge)
- **Purpose:** protect a main-line track whose coverage contains a bridge.
- **Nodes and tracks:** a main-line bridge (a doubled bridge where a joiner crosses) inside a coverage; "its own join/row/signal arrangement, to be recorded from the corpus" (D347). Wiki Prio: "Standard prio over a bridge", "Presignal prio over a bridge", "PBS prio over bridge" (needs no parallel track, two-way signals at the bridge exit).
- **Tile sharing:** not stated; D158: a bridge block in a coverage is seen through a join on a plain piece at its end; joins never sit on bridge pieces.
- **Fixed vs free:** none stated.
- **Variants:** doubled bridge; PBS form (no parallel track).
- **Evidence:** D347, D347b, D158; RULES-coop R31, R51; ANATOMY-psg227 (path signals "prio-over-doubled-bridge", INFERRED); wiki Prio sections 4, 5, 8.
- **Status:** ruled, structure not recorded; "already built today" for a bridge inside coverage (D342 note); no dedicated shape.
- **Needed by:** T3 (D347 driver); others unknown.

### B5. Non-row prio styles (two-way signal prio; limited-space same-side prio; PBS + presignal prio)
- **Purpose:** prio without a long extra row.
- **Nodes and tracks:** wiki: two-way combo + exit (reach limited to about two signals, R22); prio built on the same side as the joining track using combo and exit pre-signals on the main track; prio built over a used tunnel using PBS and a combo; PSG 227 uses three styles (ANATOMY-psg227 "Prio styles"): single 3-input join with pre-signals, 2-stage join with an entry signal on the merged track, path and two-way signals at doublings and rejoins.
- **Tile sharing:** unstated (signal-level).
- **Evidence:** wiki Prio (OD/coop-site/site/Prio.html), RULES-coop R22-R28, ANATOMY-psg227-bbh; D287 (his three prio styles map onto our one prio element on the track the pattern gives priority; no two-way or path signals).
- **Status:** seen in the corpus only; D287 excludes them.
- **Needed by:** unknown.

### B6. Entry-gated merge with no prio row (PSG 197 style)
- **Purpose:** a merge where one input is gated by an entry signal; no parallel row.
- **Nodes and tracks:** an A2 merge; six inputs entry-gated in 197 (E173 at 98-101,171; 107-109,172-173; 100-108,174; 110-111,176; E169/E173 at 86,151-156; N90 at 94-96,193-195), the rest block/block (ANATOMY-psg197 "Prio styles").
- **Tile sharing:** none.
- **Evidence:** ANATOMY-psg197-bbh; D390 (4) (prio per merge: `prio <merge out line>: <holding leg>`); psg197.txt header (no prio routes: "six entry-gated merges are plain merges", RG §4).
- **Status:** modelled as plain merges today (RG §4); BRIEF-E1j covers it (not read).
- **Needed by:** PSG 197.

---

## Group C: crossings (bridges, tunnels, doubling, stacks, order swaps) - 12 entries

### C1. Crossing stack: bridge (C7)
- **Purpose:** one over bundle crosses an under bundle, one deck per over track spanning all the tracks it crosses.
- **Nodes and tracks:** a channel crossing (D307) replacing |A| x |B| line crossings; decks side by side with aligned heads (staggered when doubled).
```
        B (under)
        | |
A  =====H=H=====      every A track bridges B's width with one deck   (COMP C7)
A  =====H=H=====
```
- **Tile sharing:** deck middle over ground is free (`KindOf`); heads exclusive; two deck middles clash on one tile (RG §2). Under track at ground; when a piece is both over and under, the crossing lies outside its own deck by >= 3 ht (RG §1.8).
- **Fixed vs free:** crossing parameter b >= 1, heads 4 ht (3 if doubled) along the deck from the crossing, n, f even, `f - n <= 2*(maxmid+2)` (RG §1.8); consecutive decks of a lane >= 2 ht apart; D228: no cap on tracks per bridge; D376: crossers parallel at the deck per span, one angle per span (width k at 90 degrees or k + 1 on a diagonal); D383: no flex point in a span; coop spans mostly 2 middle tiles (67% of bridges, RECON-corpus-crossings §1); R47 bridge at least 4 tiles including heads.
- **Variants:** square or S-bend form (C3 below); which bundle is over (D156, D164, D328: prio channel over a non-prio one; D347b searched choice; D364 separator bit per joiner, D372 per mover); lane counts; stacks of 2-4 (PSG 227: 4-bridge stacks "B1", "B2", "A"; 19 bridges, RECON-mfb-core Q2).
- **Evidence:** D156, D164, D203 (2), D228, D303, D305-D307, D328, D364, D372, D376, D388; COMP C7; RECON-mfb-core, RECON-corpus-crossings, RECON-hub-bridges.
- **Status:** built today for whole channels (RG §1.8 `deck_groups`, `wire_decks`); partial crossings and square/S-bend forms not distinguished (RG §5 C7).
- **Needed by:** T1, T2, T3, LL2 (all crossings), PSG 197 (crossings by tunnel, below), and per-lane crosser sets for 197 (CARD-E3-catalogue order item 3, SSS §3.1 item 5).

### C2. Tunnel crossing (separator = the under track)
- **Purpose:** the crossing structure is a tunnel; the carrier is the under edge (flip from the bridge rule, RECON-tunnel-crossings §2).
- **Nodes and tracks:** portals (inclined, track at the low edge), flat tunnel middle; for signals a wormhole like a bridge (D154: one block, no signals).
- **Tile sharing:** a node may sit above a tunnel's middle tiles (D390 (1): crossing at a split/merge on a tunnel's middle tile is recorded once, on the node's trunk); two tunnel separators may cross at different levels (D390 (2)); a portal on another tunnel's middle tile (SSS §3.1 item 3).
- **Fixed vs free:** tunnel length = span from crossers plus end runs (SSS §5 table); max length 64 setting (RECON-tunnel-crossings §1); coop tunnel lengths: median 3, 8% length 1, 16% 6+ (RECON-corpus-crossings §1); the portal needs an axis piece run before it ('T' move, RECON-tunnel-crossings §2.3).
- **Variants:** doubled (two parallel tunnels, R56, wiki "Dual tunnels"); over/under per D364 (separator bit and kind bit per joiner).
- **Evidence:** D154, D155, D175, D220, D364, D390, D394; RECON-tunnel-crossings; PROPOSAL-tunnels-sss.md (not read).
- **Status:** ruled; composer has no tunnel segment type (`Deck` only, RG §5); PSG 197's 50 tunnels exist only in structure.txt.
- **Needed by:** PSG 197 (50 tunnels); T2/T3/LL2: unknown (LLL_RRR motivation, D220).

### C3. Parallel tunnel group (PSG 197 "tunnel bundle")
- **Purpose:** one tunnel per output lane, side by side, exits at different lengths landing at their joins; throughput as a tripled bridge.
- **Nodes and tracks:** per-output-lane splits (cascaded A1) before the group; 15 groups of 2, 3 of 3 (G02: T2-T4; G05: T8-T10; G19: T33-T35), 11 tunnels not in a group (RG §4); head-to-head distances 2 to 14 (13-14 in the longest groups, 15 tiles counting heads, ANATOMY-psg197 (d)). Choice by pre-signals at the entrances at the first stage only (25 of 50 entrances).
- **Tile sharing:** unstated; groups "parsed, ids checked, not stored" (planar_structure.h:141 via RG §4).
- **Fixed vs free:** tunnel lengths free; exit-to-join stretch free in Slots (SSS §5 table).
- **Variants:** stages in series (a lane crosses 2-3 tunnels, 13 + 8 + 3); "two of a three-way tunnel group exit and both feed the outermost output lane" is a suspected builder error (ANATOMY-psg197 (e)).
- **Evidence:** D387 (the litmus test), D388, SSS §5 table; ANATOMY-psg197-bbh; RECON-corpus-anatomy Q3 (49 small clusters of 2x2 to 5x4 tiles inside the 197 box, "tunnel-pair element").
- **Status:** ruled as "crossing-stack gadget per group with per-lane crosser sets" (SSS §5); not built.
- **Needed by:** PSG 197.

### C4. Doubling: parallelogram doubled bridge (C6; "doubled bridges", D19, D80)
- **Purpose:** an over track splits into two parallel decks so trains alternate (a bridge is one signal block).
- **Nodes and tracks:** lane split (copy starts), lane deck, step, copy step, copy deck, copy rejoin on the copy's line (RG §1.7).
```
0 ===+==H====H\            copy 0: straight through its bridge, S-bend after it     (COMP C6)
1     \H====H==+===\       copy 1: S-bend before its bridge, heads one tile upstream
```
- **Tile sharing:** none inside (the two decks are one lane apart); the split tile of copy 0 may be a trident tile (A5).
- **Fixed vs free:** equal bridge lengths, equal bends (D80, R33 line sync); each copy bends once; near heads tight (D145, D312: copy 1's head straight after its S-bend, copy 0's after the split tile and one plain piece); far end mirrors (D314); copy 1 one tile upstream (stagger 1 in 62% of coop twins, 0 in 31%, RECON-corpus-crossings §2); steps fixed 2 ht in the composer (RG §1.7); parallelogram only, no diamond (D351); counts as two lanes of width (D333).
- **Variants:** lean direction fixed by the crossing (D118, D312); in series with its own parallelogram per crossing (D322, T2's B1 three times); doubled by flow share >= 1.0 (D331; half-share lanes single, D332; choice legs single, D299); staggered across a diagonal (D118); doubled tunnels (R56); tripled (D346, C6 below).
- **Evidence:** D19, D80, D100, D105, D118, D145, D174, D188, D299, D302, D309, D312, D314, D322, D331-D333, D351, D352; COMP C6; DESIGN-blocks §2.2 (exit block, "D pair").
- **Status:** built today on axis pieces only; bundle form for turning channels only with two full-share lanes; mixed doubled/single core not composed (RG §1.7, §5).
- **Needed by:** T1/T2/T3 (main-line B, E lanes), LL2; PSG 197: none (D392: the doubled crosser D3 left out).

### C5. Doubling rejoin / doubling followed by a split ("diagonal cross at doubled-bridge ends")
- **Purpose:** the two bridges converge and allow an immediate split.
- **Nodes and tracks:** a doubling's rejoin is not a D24 merge (D302) so a split may follow; coop 227 has "diagonal crosses that allow the end of a doubled bridge to take either direction" (engineer reading 7).
- **Evidence:** D302 (prior art: mfb splits lines after his doubled stacks, RECON-mfb-core Q5), ANATOMY-psg227 reading 7 and block table ("Rejoin of E344 pair 362-363,330-331 3/2").
- **Status:** D302 ruled; the diagonal-cross shape seen in the corpus only (cf. C9 below).
- **Needed by:** unknown.

### C6. Structures in series with a landing; tripled bridge
- **Purpose:** a long (doubled) bridge or tunnel is broken into spans with a signal block between, or tripled.
- **Nodes and tracks:** two or more crossing structures on one lane with a landing between (>= 1 tile, the signal tile; odd lengths 1, 3, 5 preferred; separator and copy may step sideways on the landing, a Slots S-bend, never inside a deck, tunnel or ramp, D375). Tripled: N parallel copies with `B <= N(d + tl) - tl` (D346).
- **Fixed vs free:** landing length derived from the signal spacing rule (D375); coop series gap k = 1 in 73% (signal in 97% of links), k = 0 never in hubs (RECON-corpus-crossings §3); copies between two series bridges stay parallel, each with its own signal (D209).
- **Evidence:** D209, D228a, D346, D375; RECON-corpus-crossings §3; wip/RESEARCH-bridge-length.md (cited by D346, not read).
- **Status:** ruled; not built. Lane-splitting landing step (landing and "port fan" contexts) in coop steps: RECON-corpus-steps (landing 2% of steps, 76% bundles).
- **Needed by:** 197 (tunnels in series, 13 + 8 + 3); wide LLL_RRR bundles (D346); T2/T3/LL2 unknown.

### C7. Bridge over an S-bend of the crossed track (coop under-span shapes)
- **Purpose:** the local shape of how a crossed track passes under a deck.
- **Nodes and tracks:** S-bend of size L (2L half pieces; staircase of L columns, vertical dominoes), deck parallel to the crossed heading over one column's domino, heads one tile beyond, span exactly 2 middle tiles in the old placement rule (RECON-corpus-sbend-crossings Part A).
```
-->.....
..<>....
..B<>B..        (corpus psg167_33, RECON-corpus-sbend-crossings shape 2)
....<>..
.....<->
```
- **Tile sharing:** parallel diagonals Lower|Upper or Left|Right on one tile (the "doubled pair" class, 13%).
- **Fixed vs free:** coop: 620 S-bends (42%), 670 Corner diagonals (45%: the crossed track turns 90 degrees under the bridge), 191 doubled pairs sharing tiles (13%); 2-tile spans 91%; S-bend step L median 9.
- **Evidence:** RECON-corpus-sbend-crossings, RECON-corpus-crossings §1c, §4; D217, D248, D256, D376, D383 (no flex in a span).
- **Status:** the old placement had the S-bend family only (S-bend 35% of coop groups accepted); in the planar composer the crosser's angle is a Slots/Shape matter (D376); no catalogue shape for the Corner diagonal under a bridge or the doubled pair.
- **Needed by:** unknown (T2/T3/LL2 crossings are perpendicular or diagonal per Shape; 197 uses tunnels).

### C8. Order swap, bridge over neighbour (C8)
- **Purpose:** two adjacent tracks of one bundle exchange lines.
- **Nodes and tracks:** one track bridges its neighbour while stepping across (crossing between two tracks of the same channel, e.g. `F1 x E2` in the T and LL2 frays).
```
0 =====\  H=====       (COMP C8)
1 =====H\=======
```
- **Fixed vs free:** deck heads 4 ht along the deck (RG §1.2).
- **Variants:** bridge-over-neighbour only for now (D311 (4)); the 2x2 diagonal cross (below) later.
- **Evidence:** D311 (4), D327, D372 (a fray's other leg is a "mover" crossing a "keeper").
- **Status:** exists only inside the n-lane fray (V crossing nearer S under a deck, RG §1.2, §5 C8); no stand-alone order swap.
- **Needed by:** T2, T3, LL2 (frays); PSG 197: up to 13 candidate same-port-set crossings, "to confirm with the channel builder" (RG §4, CARD order item 5).

### C9. Direction cross (coop 2x2 diagonal cross, "4 in / 2 out")
- **Purpose:** a split and a track swap share the tiles of one 2x2 block.
- **Nodes and tracks:** 4 in / 2 out on 2x2 tiles; corpus: PSG 227 "Direction cross E345 372-373,345-346", "Line cross N355 to SW 367-368,346-347", the 4-in-2-out cross in bundle b (RECON-mfb-core Q1).
- **Tile sharing:** diagonal pieces of two tracks on shared tiles (parallel diagonals; 227's "a Left and f Right pieces share tiles 371-374,335-341, parallel diagonals, no crossing", RECON-mfb-core Q2).
- **Evidence:** COMP C8 (forms), D285 (first 227 build leaves mfb's 2x2 diagonal crosses out; D24 strict), D311 (4).
- **Status:** seen in the corpus only; ruled "later".
- **Needed by:** unknown (227 only).

### C10. Crossing at a node on a tunnel middle tile / tunnel over tunnel
- **Purpose:** PSG 197 blockers: tunnels crossing tunnels at different levels; a node (split/merge) or portal over another tunnel's middle tiles.
- **Evidence:** D390 (1)-(3), D388 (c), SSS §3.1 items 2-3, D394 (P4 allows two tunnel structures to cross).
- **Status:** ruled; not built.
- **Needed by:** PSG 197.

### C11. Three-level stack (tunnel / rail / bridge pair)
- **Purpose:** compactness from stacked levels.
- **Evidence:** RULES-coop R57 ("bridge-over-track-over-tunnel gives three levels in one footprint"; ABR06), RECON-hub-bridges (hubs_split: two stacked bridge decks over one track).
- **Status:** seen in the corpus/wiki only; not defined as a shape.
- **Needed by:** unknown.

### C12. Level crossing (grade crossing, X+Y tile)
- **Evidence:** RECON-corpus-crossings §5: 541 tiles in 18% of hubs. D182/D217: a diamond X|Y is never legal for the tool (`JunctionTracksMayShareTile`, RG §2); G2/G10 forbids X|Y level crossing (RG §3 `CheckGeometry`).
- **Status:** coop seen only; the tool never builds them.
- **Needed by:** none.

---

## Group D: ports and arm ends - 3 entries

### D1. Port stub / arm end (C9)
- **Purpose:** where a bundle meets the box edge.
- **Nodes and tracks:** port in: start pinned to the box edge; port out: end pinned; lanes of one port exactly 2 ht apart, ports 4 ht apart (RG §1.9). Stub lb 4 ht (STUB) before the first node or bend (RG §1.2-1.4); D115a: the signal tile and the boundary tile (2 tiles below an edge-most split or merge; D206 open: could be 1).
- **Tile sharing:** none.
- **Fixed vs free:** exact pins per D313 (ports at the leg notation's spacing, LL_RR D38); R1: first node or bend >= 2 columns inside the in edge (DESIGN-blocks §3); D300 (checker boundary only on port sides). In SSS: port pins and side gaps are exact quantities (SSS §2 line 36).
- **Variants:** side of the box; line profile notation (LL_RR, LLL_RRR; unequal counts allowed, D126, D135b).
- **Evidence:** D38, D58, D115, D115a, D206, D300, D313, D363; COMP C9; BRIEF-X0a-side-gaps.md (not read).
- **Status:** built as pin and stub only (RG §1.9, §5 C9: no leg line-profile notation).
- **Needed by:** T1, T2, T3, LL2, PSG 197 (N 2/2, E 3/3, S 3/3).

### D2. Port fan and close-up
- **Purpose:** lanes open up (in-bound fan) or close up (out-bound) between a port and the node it serves.
- **Nodes and tracks:** a vertical or sideways S-bend between the port and the edge's node end; in-bound fans as late as possible next to the fray, out-bound close-ups as early as possible next to the join (D313, D378).
- **Fixed vs free:** spread stretch costs more than the tight one; sized by Slots; 0 columns allowed (D230).
- **Evidence:** D230, D313, D359 (withdrawn), D362-D363, D378; RECON-corpus-steps: port-fan steps are 52% of coop steps (inflated by long diagonals; sizes 1-3 = 54%).
- **Status:** only the close-up is composed in the model (RG §1.9); D363: port-side frames in Slots own it under SSS.
- **Needed by:** T2, T3, LL2 (D313 context); 197: unknown.

### D3. Waiting bay (coop "waiting bay"; our R5 / D83 / D88 / D176)
- **Purpose:** a safe stop for each choice before a merge, long enough for a whole train.
- **Nodes and tracks:** non-prio route runs >= 2TL (+4 ht) from its last split (or port) to each 2-in merge; signal at each bay boundary; after a line-choice split a signal straight after the split (D113, D192); first legal piece at least TL back (D88).
- **Fixed vs free:** length rule only (RG §1.9 "bay length" row); D176: how many whole bays a waiting leg gets is a throughput question.
- **Evidence:** D71, D83, D88, D113, D176, D192, D205; RULES-coop R15, R37; wiki Merging_Tracks "Waiting bays / spots / spaces".
- **Status:** built as a row in the composer (RG §1.9), not a shape.
- **Needed by:** all (every merge with choice).

---

## Group E: steps and shifters - 7 entries

### E1. Sideways step / S-bend (slot step)
- **Purpose:** the sideways move between two slots; the only source of S-bends (D350).
- **Nodes and tracks:** fixed-length 45-degree pieces: `SlotBend` = `AddSeg(l,h,0)` then Turn(h,+-1) of fixed `2*|delta|` ht (RG §1.6); fails on a diagonal heading ("a slot step on a diagonal piece is not composed").
- **Fixed vs free:** curve gaps D93, D217 (`JunctionCurveGaps`: opposite hands >= 2, same hand >= CL); coop corpus 935 steps in 156 of 294 hubs: size 1: 31%, 2: 17%, 3: 15%, 4+: 36%; contexts port-fan 52%, at-switch 19% (distance 2-6, never adjacent, 52% size 1), mid-run 19%, under-span 7%, landing 2% (RECON-corpus-steps); steps next to a switch 19% (SSS §3.2).
- **Variants:** left/right; size in lanes; bundle of parallel steps.
- **Evidence:** D18, D350, D352, D395 (steps on spines are Shape's, flex), D397 (flex by rule), D203 (1) (rows opened by S-bends).
- **Status:** built (RG §1.6); D395/D397 flex lives in the Shape layer, not in a gadget (RG §5 last bullet).
- **Needed by:** all topologies (slot layer).

### E2. Flex point (D383 / D397)
- **Purpose:** optional bends: a pair of axis inserts (h+1, odd diagonal part, h-1) of >= 0 lanes each on a free diagonal piece; length 0 = no bend.
- **Fixed vs free:** insert lengths decided by Slots, priced as length; no flex in a span or on a crosser's stretch between two crossings (D383); on every free one-piece stretch by rule (D397, two-sided).
- **Evidence:** D383, D395, D397; evidence T2 top timeout -> 2.8 s (D383).
- **Status:** built in the Shape layer (planar_shape_layer.cpp), not in the composer (RG §1.6).
- **Needed by:** T2, T3, LL2 (D383 probes); 197 (V's 197: 381 dummies with flex, CARD-E3-catalogue "Later E3 steps").

### E3. Exit step / merge exit step
- **Purpose:** an optional step on a join's out line (`PlanarMergeExitStep`, `PlanarInnerMergeExitStep`).
- **Fixed vs free:** Turn(h, exit_step) lb 2 ht; left exit with n > 1 fails (RG §1.4).
- **Evidence:** D313; RG §1.3.
- **Status:** built (RG §1.3, §5 "Code with no catalogue entry").
- **Needed by:** unknown.

### E4. Coop shifter (SML shifter; "shift", coop wording)
- **Purpose:** trains on a mainline shift to the neighbouring lane so the merging lane is free (Shift Mainline); "shifters sit before every SLH"; in a hub, trains from a give-way line are shifted onto the already-joined line and then ignored in further balancing (MSH 01, ABR06).
- **Nodes and tracks:** "a choice to the neighbouring line supported by a priority, a penalty and, in case the 2-way entry signal gets red just when a train starts heading towards it, an alternative path around it" (wiki Shift_Mainline, "Original Shifter"); fail-safe shifters (entry always green once reached; PSG 172 picture); shift gaps; timed SML.
- **Tile sharing:** unstated.
- **Fixed vs free:** none stated.
- **Variants:** original, fail-safe, shift gaps, timed (wiki headings only); shifts always one direction (left/right) across a given ML.
- **Evidence:** wiki Shift_Mainline and Unbalancers (OD/coop-site/site/); RULES-hubs H9, H19, H32; RECON-llrr §2; D126 (3) ("shifters = moving trains between lanes before a split or after a merge so later balancing can ignore lanes"; in scope, not scheduled); D187 ("no shifter"); D277 (a hub is composed of blocks including shifter).
- **Status:** seen in the corpus/wiki only. RECON-llrr: nothing makes shifters mandatory for LL_RR. This is distinct from an S-bend (E1).
- **Needed by:** none stated (card 5 recommended not in step one, RECON-llrr-facts).

### E5. Unbalancer (simple, with bypass)
- **Purpose:** keep traffic on one lane by a crossover between two tracks with a prio and a penalty on the lane to be emptied; with a bypass if the switching train jams.
- **Evidence:** wiki Unbalancers (obsolete / outdated, "this page is not final"); principle "SML".
- **Status:** wiki only.
- **Needed by:** none.

### E6. Right-of-way lane change
- **Purpose:** trains switch lanes, waiting for an opening in the 2nd lane before merging; useful before large stations.
- **Evidence:** Junctionary page "Right-of-way Lane Change" (OD/junctionary-mirror/pages/).
- **Status:** wiki only (general-community design).
- **Needed by:** none.

### E7. Diagonal run with a straight piece ("dform") and curve-axis-curve jog (corpus step shapes)
- **Purpose:** step shapes counted outside the S3 definition of a step.
- **Evidence:** RECON-corpus-steps: dform 652 (size by axis-run tiles 1/2/3/4+: 60/51/59/482), jog 13 (size 1/2/3/4+: 0/11/1/1) in groups 1+2; "Slots may care".
- **Status:** corpus only; no catalogue entry.
- **Needed by:** unknown.

---

## Group F: other (composites, corpus patterns) - 6 entries

### F1. PSG 227 join block (two bundles in, two lanes out) - composite, oracle only
- **Purpose:** the block program for mfb's join: choice tracks split and cross, two lanes out each with a prio row.
- **Nodes and tracks:** splits sA, sB; merges mC0, mC1, mL0, mL1 (prio on the fixed tracks fA, fB); one crossing cA1 over cB0; lines 0-7 table in DESIGN-blocks §2 (prio rows on lines 0 and 7, bundle gap 2 lines).
- **Tile sharing:** by the block program; no explicit allowance beyond prio and node ties.
- **Evidence:** DESIGN-blocks §2, §2.1; D298, D299 (single bridge), D311 (2), D330 ("no composite blocks in the generic path"; blocks stay as oracles).
- **Status:** built as a block program (B2b); not a catalogue gadget under D330/D398.
- **Needed by:** PSG 227 only.

### F2. PSG 227 exit block (one input arm, two bundles out) - composite
- **Purpose:** each input lane splits by destination and the branches regroup, with a doubled bridge on a main-line lane.
- **Nodes and tracks:** DESIGN-blocks §2.2 table (lines 0-4; l0b doubled; l1 steps by S-bend(+3), l1a returns by S-bend(-3)).
- **Evidence:** DESIGN-blocks §2.2; RECON-mfb-core ("D pair" in the E arm, 2 bridges span 2).
- **Status:** block program; not a gadget (D330).
- **Needed by:** PSG 227 only.

### F3. Crossover ladder; ring; tunnel merger; tunnel-pair element (corpus patterns)
- **Purpose:** compact all-to-all or merging patterns found in the corpus.
- **Nodes and tracks:** 5-to-5 crossover ladder 6x5 tiles (PSG137, once); 8-to-8 ring 8x8 (PSG231, once); 6-to-4 tunnel merger 7x48 in 15 tunnels (PSG145, once); tunnel-pair element 2x2 to 5x4 tiles, 49 in PSG 197 (RECON-corpus-anatomy Q3, Q4); 2 to 2 all-to-all blocks 1-3 tiles (1216 clusters), 3 to 3 up to 7 tiles.
- **Evidence:** RECON-corpus-anatomy Q3-Q4 (no lane-by-lane trace).
- **Status:** corpus only.
- **Needed by:** unknown.

### F4. Merger styles (ABR06: not-balancing, fully prioritized 1-N, multi-to-multi, fully prioritized some lines, half prioritized, all-to-all)
- **Purpose:** compositions of merges, prios and signals chosen per merger.
- **Evidence:** D231 ("merge" topological, "merger" coop pattern; style a user option), RECON-mergers §3 (each style in our terms), RULES-hubs H23-H26, wiki Merging_Tracks (unified split; high-throughput choice; one spot; sequence with/without prio; tree structure, "one spot can only take three rails maximum"; downhill merge).
- **Status:** ruled as a user option, not built (RECON-mergers §3 says style 3 needs MJ1 to accept crossed legs, which fails today).
- **Needed by:** LL2/T family: unknown.

### F5. Pre-signal choice at a split before a bundle ("choice by free tunnel")
- **Purpose:** an entry signal before the fork and exit signals on every tunnel entrance, so a train takes whichever tunnel is free.
- **Evidence:** ANATOMY-psg197-bbh (b), findings (3); wiki Dual tunnels (signals so trains wait for the first available tunnel); RULES-coop R36, R40.
- **Status:** signals-level; seen in the corpus only.
- **Needed by:** PSG 197.

### F6. Not swept as gadgets
- Station generator (RO-RO drop, modules, bays, ro-ro; D106, D120, D127), overflows (wiki Overflow: station-side), line painter (D112), and the throughput model are separate workstreams; no entries made. General-community Junctionary designs (Compact 3-Way, Braided, Crossover-Junction, Line Merge, Priority Merge page = same content as the coop prio) are whole junctions, not local gadgets, except as cited above.

---

## Counts

Group A nodes 9; B prio 6; C crossings 12; D ports and arm ends 3; E steps and shifters 7; F other 6. Total 43.

## Open: shapes the sources mention but do not define well enough to be a gadget

1. Trident exact piece list: D398 says "straight leg and both diagonals on one tile"; D110 (a) says "SL, lane A and lane B leave one tile". No source gives tile-level pieces, whether a diagonal trunk may host it, or which node pairs (topology split + copy split, copy split + prio row split) have been laid out.
2. Mirror trident (two merges on one tile): "only once the corpus or PSG 197 shows it" (D398); not searched in the corpus in any source read.
3. Any other tile sharing across node gadgets: "measured on T2 and 197 first" (D398).
4. Three-input join "on one tile or adjacent tiles" (A6): no tile-level shape, only block boxes (1x2).
5. Prio over a bridge (B4): "to be recorded from the corpus" (D347).
6. Peel splits and 2x2 direction crosses (A8, C9): block boxes and tile coordinates exist (ANATOMY-psg227), no lane tables or pieces.
7. Corner diagonal under a bridge (45% of coop under-span groups) and the doubled pair under one bridge (13%): no catalogue shape (RECON-corpus-sbend-crossings Part C); D376 loosens the angle rule.
8. Partial crossings of a bundle (per-lane crosser sets, SSS §3.1 item 5, §7 question 2): the crossing-stack gadget's per-lane form is open.
9. Which gadget parameters are free (SSS §7 question 1): D355 split gaps; CARD-E3-catalogue's "parameters are cuts" argument; non-cut freedoms are to be sampled.
10. The hull description of a gadget for free-face cells (SSS §7 question 1).
11. Prio on a diagonal main line: row shape and the checker loosening (D342, D353, D374) not built.
12. Tunnel gadgets: portals, tunnel groups (planar_structure.h:141 ids "not stored"), portal on a middle tile, tunnel over tunnel (C2, C3, C10).
13. Shifter variants (E4): only wiki headings; no geometry.
14. Port leg line-profile notation (D1) and the stub length (D206 open).
15. Landing steps between structures (C6, D375): shape ruled, no piece-level definition.
16. Branch-in "doubled split then merge" distance rules (D229 merge-then-bridge or bridge-then-merge): cost choice, not a shape.

## Conflicts: where two sources disagree

1. **Gadget granularity.** CARD-E3-catalogue.md (option A recommended, per site and per lane count), DESIGN-sss-v2 §2 line 32 and §3.2 item 2 ("verified once per lane count", fray and join as gadgets), D388 (a) ("verified once per lane count"), D311 (2) (one Fray and one Join type) versus D398 (per node; "amends DESIGN-sss-v2 §3.2's 'verified once per lane count' for nodes"). SSS text and the card still carry the older wording.
2. **Adjacent splits.** D110 originally: "Two adjacent splits on an ML path (no tile between) are wrong" (apart from the clean three-way split); the checker `CheckOrder` (G4) enforces "D110 adjacent splits" (RG §3). D398 allows a trident of 0 tiles "by rule" across node kinds. Whether the checker clause was widened is not stated in the sources read.
3. **D113 versus D398 trident into a bridge.** D113: between a split and the next bridge head at least one signal-legal piece; D398's example has two legs starting on the bridge directly from the trident; D118/D145 keep "exactly one signal tile per leg between the split and its head". Reconciliation is not written.
4. **Fray split gaps.** D355: lower bounds; RG §1.2/§5: the composer has exact equalities; the prio climb diagonal is required in code, optional per D355.
5. **Prio row bending.** D349/D384: the row follows its lane through bends; RG §1.5: the composer's row does not bend. D75 trailing pair versus RG §1.3: the composer lays the joiner's diagonal onto S and the row as an unused strand; the pair is placed by the signals pass.
6. **Prio length.** D158 "at least the coverage" versus D384 "exactly the coverage" (D384 amends D158; older text in other docs may still say "at least").
7. **Doubling criteria.** D80, D174, D188 (main-line bridges always doubled, "whatever they cross") versus D331/D332 (doubled by flow share; half-share and sideline lines single) and D299 (choice legs single); D174 and D331 are both active in DEC, with D331 stated as the throughput-based replacement for "doubling by prio flag" in D299's trim note.
8. **D145 far-end constants versus D314.** D314 supersedes D145's far end (x_hwA - x_lm = 2, x_hwB - x_lm = 3).
9. **D285 versus 227 as a corpus anatomy.** The first 227 build keeps D24 strict (no merge then split) and leaves out mfb's 2x2 diagonal crosses; D391 accepts a choice split after a merge only in a pinned written-down hub (PSG 197); ANATOMY-psg227 counts D24-breaking diagonal crosses as present.
10. **C7 forms.** COMP C7 names square and S-bend forms; RG §5: not distinguished in code; RECON-corpus-sbend-crossings reads the older placement rule (span exactly 2 middle tiles) while D376/D383 are newer.
11. **Where doubled bridges appear in the corpus.** RECON-junctionary: "doubled bridges appear mainly in mergers and balancers ... not in simple crossings" and "rare in junctions"; RECON-corpus-crossings §2: 92% of hub bridges have a parallel twin. Different corpora (Junctionary pages versus 294 saved hubs).
12. **Doc locations.** SSS cites `psg197-gaps` and "RULES-coop" is in docs/junction-tool (found); `psg197-gaps` was not found in docs/junction-tool or wip, so §3.1's per-item gaps could not be checked.
