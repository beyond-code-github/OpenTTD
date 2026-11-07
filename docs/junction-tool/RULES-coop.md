# #openttdcoop building rules for the junction generator

Date: 2026-11-07. Scope: the first junction, an L_R (one track per direction) full 3-way T-junction. Decisions already made are in `DECISIONS.md`; this file only adds sourced rules and flags where sources agree or disagree with those decisions.

Sources (short names used in the Source column):

- **ABR02..ABR13**: V453000, "Advanced Building Revue" blog posts (plain-text copies in the scratchpad `abr/` folder). ABR02 (splits) is about logic-gate train splitters only and gives no track-layout rules. ABR04/08/12 (overflows), 05, 09, 11, 13 were grepped and gave nothing relevant to junction geometry.
- **MT**: coop wiki "Merging Tracks" (V453000), archived page.
- **ML**: coop wiki "Mainline". **RS**: coop wiki "Ruleset". **BBH**: coop wiki "Backbone Hub". **CS**: coop wiki "Max Curve Speed". **SML**: coop wiki "Shift Mainlines". **GL**: coop wiki "Glossary".
- **PM**: OpenTTD wiki Junctionary "Priority Merge". **JC**: "Junction Criteria". **JMP**: Junctionary "Main Page". **LM**: "Line Merge". **BJ**: "Braided Junction". **C3BM**: "Compact 3-Way Branch-Merge Junction".

Important limit: the coop sources are written for LL_RR and wider mainlines with train-length-3 style rules of thumb. They contain no formula for prio length, no formula for CL from speed and TL, and no statement of which side a prio goes on. Those gaps are listed under "Conflicts and gaps". Rules marked "(implied)" are inferred from a source, not stated.

## Curves

| ID | Rule | Why | Source | Generator impact |
|---|---|---|---|---|
| R1 | No 90-degree turns anywhere in the junction. | Trains cannot take them with coop patch settings. | RS: "No 90 degree turns! Trains can't take them due to our patch-settings." | **check** |
| R2 | Two consecutive 45-degree turns in the same direction need at least one tile of straight track between them (CL >= 1). The 2x45 case (CL 0) is forbidden. | Too high a speed penalty. | RS: "No 2*45 degree curves, due to too high speed penalties." JMP: "Use at least a one-tile track segment between 45° turns" | **check** |
| R3 | An S-bend (one left, one right 45) carries no penalty, so no minimum CL applies between opposite-direction turns. | Game only penalises same-direction bends. | JMP: "S-shape bends do not slow down trains." Agrees with D18. | **check** (exempt from CL test) |
| R4 | CL is counted using the in-game measurement tooltip. Half values exist: if one of the two curves counts as "in between", the result is CL1.5. | Convention so builders can read a curve at a glance. | CS: "when, for example between 1 and 2, we say CL1.5" | **param** (CL granularity 0.5; see gaps) |
| R5 | CL1 is acceptable only for trains slower than about 88 km/h on rail. Faster trains need larger CL. | CL1 slows fast trains. | ABR03: "Do not use CL1 unless your trains are slower than 88kmh (on rail)" | **param** (warn if CL too small for the train) |
| R6 | OpenTTD's real calculation counts vehicles between curves, so shorter wagons pass curves faster (about x1.2 for half-length wagons). Coop still builds by CL convention, not by vehicle count. | CL is a proxy for the game's vehicle-count rule. | CS: "The real calculation OpenTTD does is counting wagons (vehicles) between curves" | **design** (matches D18) |
| R7 | A game uses one train length and speed so one curve size fits all. All curves in a junction use the same CL (implied). | Different lengths or speeds need different radii. | RS: "we try to avoid different lengths or train speeds." | **param** (single CL for the whole junction; implied) |

## Splits and merges

| ID | Rule | Why | Source | Generator impact |
|---|---|---|---|---|
| R8 | Along every route, a train meets any split before any merge. | Merges before exits can gridlock; split-first is deadlock free and backs up less. | JMP: "Gridlock can happen in junctions that have merges before exits." JC: "Split before merge ... leads to less backup" BJ: "No deadlocks since tracks split before they merge." | **check** (agrees with D24) |
| R9 | Each exit branch (leg) is longer than the train. | A train waiting to merge on a short leg backs onto the junction. | JMP: "Try to make each exit branch longer than your trains." | **check** (leg length > TL; also covers D21 leg length) |
| R10 | A full 3-way hub decomposes into one ML crossing, one crossover, 3 exits (splits) and 3 joins (merges). For a full L_R T this means 3 splits and 3 merges (implied by the 6 movements). | Gives a countable skeleton. | BBH: "one ML crossing, one crossover, 3 exits and 3 joins" | **check** (counts; the L_R reading is implied) |
| R11 | Exits (splits) need no balancing. Splitting is just "get the lines away"; orders do the choosing. | Only merges can jam. | BBH: "exits don't require balancing". ABR06: "just use anything to get those lines away" | **design** |
| R12 | Trains should join a given output line at only one spot, possibly prioritised. | All trains get the same chance; the line is disturbed only once. | MT: "join at only one (possibly prioritized) spot" | **check** (each merge has a single join point) |
| R13 | Avoid a chain of prioritised joins onto the same line. The downstream joins get starved because upstream joins are checked first. | Starvation of later joiners. | MT: "trains downstream will have a lot harder time joining" | **check** (at most one prio merge per output track; implied) |
| R14 | Do not merge with PBS signals. | Slow and jam-prone; V453000 lists it as his main early mistake. | ABR03: "PBS merges are very bad"; "merging with PBS is B.A.D." | **check** (no path signal on the merge) |
| R15 | Every place trains might wait needs a safe waiting spot that does not block other routes. A merging train waits at its merge signal, so that block must not reach back into a split or crossing (implied). | A waiting train blocking a switch can deadlock. | MT: "waiting bays are necessary everywhere" | **check** (implied; see R9) |
| R16 | A merge with no choice (a "joiner") gives one direction priority, SLH style. Choices and all-to-all balancing apply only where a train has several outputs. With L_R there is one output lane per direction, so there is no choice to build (implied). | Balancing logic is for multi-lane directions. | MT: "Joiner is a specific case of merging where one direction gets priority" | **design** |
| R17 | The ML is not sacred geometry: it may be moved or spread apart inside the junction to make room. | Builders resist moving the ML and make worse hubs. | MT: "do not be afraid to move with them!" | **design** |

## Priorities (prios)

| ID | Rule | Why | Source | Generator impact |
|---|---|---|---|---|
| R18 | The mainline has highest priority; anything that joins it must yield. | ML flow is paramount. | ML: "anything joining (say a Sideline), must yield track space, to ML traffic." | **check** (every merge onto the ML has a prio; D27) |
| R19 | A prio is a check ("which track is free?"), not a value ranking. | Sets design intent: detect traffic, do not hoard track. | MT: "the priority is like a check, or a question" | **design** |
| R20 | Prio length depends on the merger type. A joiner that must not slow the ML can have a longer prio; a "line is 100% full" check in a BBH merger needs a very short one. | Trade between detecting traffic and letting trains join. | MT: "If we have a SLH joiner which expects that the ML will not be slowed down, prio can be longer." | **design** (SLH-style joiners get the long end; see D29) |
| R21 | A prio must be long enough to detect traffic and not disrupt the chosen line if a train joins. It must be short enough that trains join often. | Both too short and too long hurt. | MT: "long enough to be able to detect if there is traffic ... short enough to let trains join frequently" | **design** |
| R22 | The basic prio, two-way pre-signals with nothing extra, can reach no further than the length of two signals. | Signal reach limit. | PM: "you can't extend the priority further than the length of two signals" | **design** (so longer prios need extra track) |
| R23 | A longer prio is made by joining an extra track to the merge signal upstream, so a train on the prio track holds the merge signal red. | The prio track reaches back along the ML. | PM: "connecting track to activate the main merging signal upstream" | **check** (extra parallel track, connected to the ML) |
| R24 | The extra-track prio works with path signals or block signals. The path version needs one block signal on the merging track. The block version needs pre-signals on the unused track so trains do not signal themselves to stop. | Different reservation behaviour. | PM: "The path signal construction needs only a single block signal on the merging track" | **check** (signal types on prio track depend on D20; see gaps) |
| R25 | Split a long prio into segments so the shortest train always sits on a connection. Otherwise a train in the middle of an unlinked stretch leaves the merge green. | A gap in the prio lets a train merge into a passing train. | PM: "accommodate your shortest train's length"; "red no matter where a 5-tile-long train is on the priority" | **check** (connection spacing vs TL; D25) |
| R26 | A prio needs a stretch of prioritised track in front of the merge point as well as behind it. | Stops a joining train arriving too close to the train ahead of it. | PM: "need a bit of prioritized track in front of the merge as well as behind" | **check** (prio extends past the merge point; implied length) |
| R27 | To avoid slowing the ML at all with an ordinary prio, the prio behind the merge must cover the joining train's whole acceleration run from standstill to top speed. | The follower would otherwise catch the accelerating joiner and brake. | PM: "accommodate the joining train's entire acceleration room needed to reach maximum speed from stationary" | **param** (prio length depends on engine; D29, D31) |
| R28 | Alternative "high-speed merge": a two-way entry signal makes a train skip the merge and use another track if the prio is occupied, so no one brakes. Prio length then depends only on train length, not acceleration. The cost is an alternate track (optionally looped back). | Joiner never starts from standstill. | PM: "only train length determines it." | **design** (shortest-prio option; conflicts with block-signal default, see gaps) |
| R29 | Long prios reduce the number of trains that can join and leave long gaps on the merged line. | Capacity cost of long prios. | PM: "Priorities can get extremely long ... long gaps between trains on the merged track" | **design** |
| R30 | Prios can be added after the rest of the layout is built, in any place. | They are small appended constructs. | BBH: "you can hack in priorities in any place" | **design** (prio construction may be a late pass) |
| R31 | Prios on bridged lines risk "evil mode" (see Bridges). A prio on a bridge cannot use an entry check; "full priority" on doubled bridges needs a long prio and can still jam. | Bridges are slow and unsignalled. | MT: "cant use PBS, nor entry signal to stop trains when both bridges are full" | **design** (see D23 gap) |

## Signals

| ID | Rule | Why | Source | Generator impact |
|---|---|---|---|---|
| R32 | On the ML, signal distance is every other tile (SD 2). | Packs trains densely. | ML: "Signalling distance is every other tile" | **param** (default SD2; agrees with D21) |
| R33 | Where an ML section is split into several tracks (doubling), line sync must be kept: parallel paths have equal length. | Trains emerge in the same formation they entered. | ML: "important to maintain Line sync in it" | **check** (doubled paths equal length; ties to D19) |
| R34 | Signal gaps of 1 are a bad idea. (Meaning unclear: see gaps.) | Quote is from V453000's early-games critique. | ABR03: "signal gaps of 1 are not a good idea" | **param** (open: forbids SD1?) |
| R35 | A longer signal gap caps throughput of that stretch. Dense signalling is wanted wherever throughput matters. | Signal spacing sets minimum train spacing. | ABR07: "by making a longer signal gap, you can cap the maximum throughput" | **design** |
| R36 | Where trains are choosing, one-way pre-signals are best; PBS into one-way signals chooses badly; PBS needs two-way signals; two-way signals can lose trains. Prefer one-way so missing connections show up. | Exit-signal red penalty is huge for pre-signals (about 40000); path choice is reliable. | MT: "1 way pre-signals are standard"; "PBS into 1-way signals is bad" | **design** (the T has no train choices; mainly useful for the prio) |
| R37 | Block signals leading to the same place without waiting bays are unacceptable: trains stop at wrong spots. | Combo-signal choices got stuck. | MT: "absolutely unacceptable to use block signals which lead to the same place without waiting bays" | **check** (implied for any node with two outputs; none in an L_R T) |
| R38 | Bridges and tunnels cannot hold signals, so signal distance across them equals their length. | Game rule; underlies D19. | LM: "mainlines have tunnels where you cannot place signals." BJ: "Signal distance is 7 tiles on straight track over the bridge." | **check** (no signals on bridge or tunnel tiles) |
| R39 | Basic (block) signals on slowed bridges can recover from evil mode; pre-signals and PBS on slowed bridges keep it. | Block signals let a train pick the other bridge and reset. | MT: "Basic signals on slowed bridges can deal with evil mode." | **design** (supports D20) |
| R40 | Pre-signals are "highly friendly" with split bridges, unless the bridges are to be prioritised. | Pre-signals pick a free path before the bridge. | ABR06: "highly friendly with presignals which are just best unless you want to prio the bridges" | **design** |

## Spacing and line layout

| ID | Rule | Why | Source | Generator impact |
|---|---|---|---|---|
| R41 | LL_RR means two left tracks, a gap, two right tracks. The underscore is the gap between directions, literally one tile, usually more. | Ease expansion and hub building. | ML: "Taken literally, it is only one tile, but usually more tiles are used" | **param** (gap >= 1 tile) |
| R42 | On 2+/2+ mainlines leave some spaces between the two directions; the more the better, subject to room. | Leaves room for hubs and expansion. | RS: "be sure to leave some spaces in between the two lanes (the more the better" | **param** (gap; agrees with D22) |
| R43 | Trains drive on the right. | House convention. | RS: "We (well most of us) drive on the right side of the road, the trains should too" | **param** (handedness, mirroring per D15) |
| R44 | Sidelines (L_R or LL_RR) reach the mainline only through a sideline hub. Never connect a station directly to the mainline. | Keeps the ML fast. | RS: "Never ever connect a station directly to the mainline." | **design** (the branch is a sideline joining via an SLH-style joiner) |
| R45 | Because trains split first and then merge, traffic ends up crossing as if driving on the left. Cross the lines later, to save space on top of tunnels. | Consequence of split-before-merge. | ABR10: "Because trains first split and then merge, it will create some left hand driving." | **design** |
| R46 | Treat all tracks of a direction equally; traffic on any line may be random. | Capacity is per direction. | MT: "essential to treat all tracks of the same direction equally" | **n/a** for L_R (one lane) |

## Bridges and tunnels

| ID | Rule | Why | Source | Generator impact |
|---|---|---|---|---|
| R47 | Bridges shorter than 4 track segments have lower speed limits; make bridges at least 4 tiles long. Tunnels have no speed limit. | Game rule. | JMP: "make your bridges at least 4 tiles long" | **check** (bridge length >= 4) |
| R48 | Doubled bridges (and tunnels) are standard coop building: "doubled bridges, non-slowing down curves and balancing". | Throughput of the choice. | BBH: "including doubled bridges, non-slowing down curves and balancing" | **design** (agrees with D19) |
| R49 | Bridges add throughput to a choice only if they are enough in number; "increasing throughput by bridges can be helpful when necessary". | A single bridge is one long block. | MT: "improve the choice throughput by doubling bridges" | **design** |
| R50 | Evil mode: on doubled bridges, trains endlessly slow each other in a cycle once something slows them; merges can start it. | Destructive interference. | MT: "trains endlessly slow each other down in a cycle on doubled bridges" | **design** |
| R51 | Remedies for evil mode: (a) block signals so bridges unblock themselves; (b) put the prio on lines that are not bridged; (c) give each bridge its own destination line instead of doubling. "Not bridging the prioritized line overcomes this problem entirely." | Removes the cycle. | MT: "Not bridging the prioritized line overcomes this problem entirely." | **design** (tension with D23, D19; see gaps) |
| R52 | Doubled bridges have drawbacks: they make evil mode possible and add waiting space, so trains take longer to try other choices. | Cost of doubling. | MT: "makes evil mode possible to happen, and gives more waiting spaces" | **design** |
| R53 | The two bridges of a pair should have equal length so trains keep formation (implied). | Same reason as line sync. | ABR07: "any path through any platform is the same length ... Just like bridges." | **check** (implied) |
| R54 | Bridges cannot pass under anything; tunnels can. Bridges are the more compact and flexible choice. | Placement flexibility. | ABR06: "bridges ... can not go under anything." | **design** |
| R55 | Shorter CL gives shorter bridges and tunnels; the number of parallel tunnels depends on TL (shorter trains need more). | Bridge length is set by what must pass underneath. | ABR10: "the shorter the curve lengths, the shorter the tunnels"; "shorter trains require more tunnels" | **design** (bridge length from CL and TL) |
| R56 | A tunnel (or single long bridge) raises minimum train spacing; use two or more. | One long unsignalled block. | LM: "Tunnel increases minimum train spacing. (Can be fixed by using two or more tunnels)" | **design** |
| R57 | Bridge-over-track-over-tunnel gives three levels in one footprint; bridge the straight line, as the straight-line direction stays still. | Compactness. | ABR06: "you decide you will bridge the straight line" | **design** (supports D23) |

## Hubs and T-junctions (BBH/SLH)

| ID | Rule | Why | Source | Generator impact |
|---|---|---|---|---|
| R58 | A backbone hub must be balanced (each entering lane can choose any leaving lane), efficient (no slowing) and sized for the longest train. | Core BBH requirements. | BBH: "Efficient - Trains should not have to slow down" | **design** (the "no slowing" part is the D29 goal) |
| R59 | Plan or build the hub centre first, where trains turn left across traffic (the "sweet spot"). Start with the ML crossing, not an exit or a join. | Prevents forgotten connections and rework. | ABR06: "beginning the building of a hub with this spot"; BBH: "common mistake to start with an exit or a join" | **design** (centre crossing first) |
| R60 | The standard 3-way is a single "ultimate 3way" design; coop now chooses components over layouts. | Unified shape. | ABR06: "3way hubs look basically all like 'an ultimate 3way'" | **design** |
| R61 | SLH-style merge: one line joins all others with full priority to the joined lines. | No slowdown on the ML. | ABR06: "prevent any slowdowns on ML by the joining trains." | **design** (matches D27) |
| R62 | The Junctionary compact 3-way with splits before merges is listed as deadlock free and handling three trains at a time. | Reference design for splits-first T. | C3BM: "Split before merge. Deadlock free." | **design** |

## Other

| ID | Rule | Why | Source | Generator impact |
|---|---|---|---|---|
| R63 | Avoid ramps on the most important lines. | Climbs slow trains greatly. | JMP: "Avoid ramps on the most important lines in the junction." | **check** (ML flat; consistent with D11) |
| R64 | Keep terraforming as low as possible. | House rule. | RS: "we keep terraforming as low as possible" | **n/a** (D10) |
| R65 | Avoid extra directions and unused movements. A full T has all six movements, so the generator follows D22 regardless. | Space and curve cost. | JMP: "Build only those directions that are actually going to be used." | **n/a** |

## Prio construction

What coop actually describes, in order of authority:

1. **Basic prio (no extra track).** Two-way pre-signals at the merge. A merging train is held if a train sits directly in front of the combo or exit signal. Reach is at most two signal lengths (PM).
2. **Extra-track prio (the standard long prio).** A parallel unused track runs alongside the mainline track upstream of the merge. Short connection pieces join it to the mainline at intervals, as diagonal X-crossings (viewed in `Priority merges.png`, `Fixed priority.png` and `Full-length priority.png`). When a mainline train sits on a tile linked to the unused track, it occupies a block on that track, and that holds the merge signal red (PM, R23).
3. **Signals.** Path-signal version: one block signal on the merging track, path signals on the extra track. Block-signal version: the unused track needs pre-signals so trains do not signal themselves to stop (PM, R24). Exact signal type and tile for each pre-signal is only visible in the images; text does not give it.
4. **Segment length versus TL.** The text rule is "accommodate your shortest train's length". The caption on the fixed prio says the signal reads red wherever a 5-tile train sits. In `Fixed priority.png` and `Full-length priority.png` each segment (the open cell between two X-crossings) is visibly no longer than the train drawn beside it (viewed; estimated by eye, not measured). So: the gap between consecutive connection points must not exceed the shortest TL (R25). Whether "not exceed" is strict or inclusive is a boundary the tests need to settle by simulation (see gaps).
5. **Length versus speed and braking.** PM gives the only physical statement: for an ordinary prio to avoid every disruption, the prio behind the merge must cover the joining train's full acceleration run from standstill to top speed, with a little more in front of the merge. In `Full-length priority.png` (3-tile trains) the chain is visibly several train lengths long, so the full-speed length can exceed 2 TL (eyeballed, about four cells). No formula is given. MT answers "How long should a priority be?" only as depending on train type and merger type (R20).
6. **High-speed alternative (R28).** Two-way entry signal plus an alternate "failed to join" track. Prio length is then set by train length alone, which is the shortest answer in the sources. Added needs: an extra exit signal and the entry signal moved half a tile upstream (PM, "High-speed merge fix").
7. **Braking spaces in ABR07** (1x TL between a main line and a platform) are for stations, not prios. No source ties prio length to braking distance.

For D29, the usable computed bound is therefore R27 (acceleration run), with R25 as a lower bound on segmentation. Sources do not support a "1 to 2 TL" window.

## Splits before merges and line balancing

- **What the sources say.** The Junctionary lists "Split before merge" as a junction criterion (JC) and tells builders to "make the merges after the exits" (JMP). C3BM and BJ call split-first designs "deadlock free". These are the only direct statements. The coop blog and wiki do not state it as a named rule, but the BBH build order does exits first and joins last (BBH), and ABR10 describes the geometry consequence (R45).
- **Why.** If a merge is hit before a split, a train waiting to merge can sit in the path of a train heading elsewhere, which gridlocks (JMP). With splits first, no wait can block a different route (R8, R9). Coop's second reason is balance: exits need none, joins do (BBH).
- **Balancing.** In coop, "balancing" means every entering lane can choose any leaving lane; V453000 dislikes the word because people read it as even loads (MT). It matters for 2+ lanes per direction. For L_R with one lane per direction there is nothing to choose between: each merge has two inputs and one output, so it is a joiner (R16). Line balancing is not needed for the first junction (implied).
- **Single join point and no chained prios** (R12, R13) are the only merge-structure rules that bite on L_R.

## Conflicts and gaps

Conflicts or refinements to DECISIONS.md:

1. **D23 and D19 versus evil mode (MT).** Coop says bridging the prioritised line risks evil mode: "Not bridging the prioritized line overcomes this problem entirely." D23 bridges the mainline, which is the prioritised line (D27). Coop still bridges the straight line in hubs (ABR06, "Directional Tunnels"). Mitigation in coop: block signals so bridges unblock themselves, no entry check on bridges, and a long prio if needed. Engineer should confirm: keep the ML bridged and accept this, or put the prio where the ML is not on a bridge.
2. **D20 and D25, block signals versus prio construction (PM).** Block-signal prios need pre-signals on the unused track (R24). "Block signals by default" must be read as block signals on the junction, with pre-signals on the prio loop, or the generator needs path signals there. Needs a ruling.
3. **D29, prio between 1 and 2 TL (PM).** Not supported. A full-speed ordinary prio can be longer than 2 TL; the high-speed merge needs only train length (R27, R28). The sources give no formula, so the derivation in D29/D31 is the project's own. The high-speed merge also needs a two-way entry signal and an alternate track, which conflicts with a pure one-way block-signal layout (D20).
4. **D18, CL definition (CS, JMP).** Coop counts CL from the measurement tooltip, with half steps (CL1.5), and builds by CL rather than game vehicle counts. D18 says CL3 is 3 tiles of straight track. Diagonal counting is still open (also noted in D18).
5. **D21, signal distance.** SD2 matches ML (R32). But signals cannot go on bridges (R38), so SD across a bridge equals bridge length. ABR03's "signal gaps of 1 are not a good idea" (R34) may mean SD1 is discouraged; the wording is ambiguous.
6. **D24, split before merge.** Confirmed by JC, JMP, BJ and C3BM. Not stated as a rule in the coop blog.
7. **D27, which merges get prios.** Consistent with ML-highest-priority (R18). In a full L_R T, two of the three merges feed mainline lanes and one feeds the branch. Coop does not say whether the branch-bound merge, which has no ML to protect, should have a prio; D27 says no ("mainline merges only"). BBH-style hubs do use "half prioritized" merges between equals (ABR06); coop does not prescribe either way.
8. **D28, prio side.** Not stated in any source. Implied support: SML merges enter the outermost lane (SML: "Merging trains will attempt to enter lane C", the outermost). Treat D28 as the project's own call.
9. **Doubled bridges (D19) and line sync.** ML doubling must keep line sync, so paired bridges should be equal length (R33, R53).
10. **Handedness.** Coop drives on the right (R43). Combined with D15 mirroring, tests should state which handedness the layout assumes.
11. **Gap in the rules.** No coop source gives: a formula for CL from speed and TL; a number for the minimum gap between directions beyond "one tile literally" and "the more the better"; the exact signal positions on a prio; or a rule for how far a prio must extend past the merge point (R26 gives no number).
12. **L_R specifics.** Almost every coop rule is written for LL_RR or wider. Balancing and "choice" rules (R16) mostly do not apply. The engineer should confirm that the full T is a pure joiner layout with no balancing.

## Images worth viewing

Viewed (4 of 8 allowed): `Priority merges.png`, `Fixed priority.png`, `Full-length priority.png` (all in `junctionary-mirror/images/`), and `6x8 3 Way Split before Merge Junction.png` (same folder; too small to read detail, it shows a compact split-first 3-way with a bridge).

1. `junctionary-mirror/images/Priority merges.png`: the extra-track prio, connection pieces and the merge signal (R23, R24).
2. `junctionary-mirror/images/Fixed priority.png`: segment length relative to train length (R25).
3. `junctionary-mirror/images/Full-length priority.png`: length of a full-speed prio for 3-tile trains (R27).
4. `junctionary-mirror/images/Flawed priority.png`: the failure case when the prio has a gap (R25).
5. `junctionary-mirror/images/Basic priority.png`: the two-signal prio (R22).
6. `coop/files/Used_tunnel_prio.png`: tunnels acting as the prio (R31, ABR10).
7. `coop/files/Pbs_presig_prio.png`: PBS and pre-signal prio mix (R24).
8. `coop/files/Twoway_prio.PNG`: two-way entry signal prio (R28).
9. `junctionary-mirror/images/6x8 3 Way Split before Merge Junction.png`: reference split-first 3-way (R8, R62).
10. `abr/images/186_bridges.png`: bridges in an actual coop game (R48, R54).
