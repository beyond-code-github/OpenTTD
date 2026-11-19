# Audit of DECISIONS.md against the SSS architecture

Date 2026-11-20. Read-only; DECISIONS.md not edited, nothing deleted. Yardstick read: src/junction/planar/CLAUDE.md "Architecture" and "Terms" (lines 1-50), DESIGN-tracks.md section 17 (17.1-17.6), the 2026-11-18 audit header. All 291 rows (D1 to D368) read once, in file order. Rows with long cells were read to about 400 characters; D170, D171, D189, D190, D194, D195, D203, D208, D213, D216, D235, D311, D312, D334, D335, D338, D346 were read further. Rows culled on 2026-11-18 are not in the file and not listed.

Counts: K 215, T 31, O 29, S 11, Q 5 (total 291).

Layer names: Structure, Shape, Slots, Lengths, Heights, Signals, Product/UI (Product, UI), Process, Corpus. Table is in file order.

| Id | class | layer(s) or superseded-by | one-line reason (T: part to keep) |
|---|---|---|---|
| D1 | K | Product | fork, core C++ |
| D2 | K | Product | player flow |
| D3 | O | - | constrain multi-track search; old router |
| D4 | K | Process | test-first, outside-in |
| D5 | K | Process | doc location |
| D6 | K | Heights, Process | terrain read through interface with mock |
| D7 | K | Product | post ordinary build commands, validate up front |
| D8 | K | Product | rail type is an input |
| D9 | K | Product | exact cost before build |
| D10 | S | by D218 | M1 no terraform; terraform allowed (D218) |
| D11 | O | - | M1 build-order milestone history |
| D12 | O | - | deferred rail-type join rule, milestone note |
| D13 | K | UI | red invalid ghost |
| D14 | K | UI | price next to cursor |
| D15 | K | UI | rotate/mirror hotkeys |
| D16 | T | Product | keep "junctions produced procedurally into an in-memory description; placement, ghost, pricing, building consume only that" |
| D17 | O | - | first family L_R T, milestone |
| D18 | K | Lengths | CL definition, curve speed |
| D19 | K | Structure | doubled bridges; vanilla cannot signal bridges |
| D20 | K | Signals | one-way block default |
| D22 | K | Structure | movements, line spacing parameter |
| D23 | Q | Structure | "mainline bridged" vs D32/D364 separator + kind per structure: does any residue remain? |
| D24 | K | Structure, Signals | splits before merges |
| D25 | K | Signals | prio definition |
| D26 | K | Process | math designs the layout |
| D29 | K | Signals | shortest mathematical prio |
| D31 | K | Product | player picks a train |
| D32 | K | Structure | bridge arrangement options by traffic level |
| D34 | K | Signals | math prio length, warn |
| D35 | K | Heights | bridge type tables |
| D36 | K | Lengths | diagonal = half tile for CL |
| D38 | K | Product, Structure | line notation |
| D39 | Q | Product | traffic level "open"; still open or settled later? |
| D40 | K | UI | cursor anchor, red outline |
| D41 | O | - | 1a agent grouping |
| D42 | K | Process | rebase-friendly seams |
| D43 | K | Process | BRIDGE_Z_START shared constant |
| D44 | K | UI | opaque ghost |
| D45 | K | Heights | bridge type for unlimited rail |
| D46 | O | - | 1a bridge culling note |
| D47 | K | Product | train drives bridge type |
| D48 | K | UI, Process | house style |
| D49 | K | UI | cost in window |
| D50 | K | Product | clearing cost under bridges counted once |
| D51 | K | UI | shift-click estimate |
| D52 | K | Process | agent overhead |
| D53 | K | UI | ghost pinned footprint |
| D54 | K | UI | blue to-build |
| D55 | K | UI | generic invalid message |
| D56 | K | UI | outline draw order |
| D57 | K | UI | translucent blitter parked |
| D58 | K | Shape, Lengths | footprint ends one tile beyond outermost split/merge/prio signal |
| D60 | K | Product | notation per line |
| D61 | K | Shape | diagonal track under bridge allowed |
| D62 | T | Product | keep "no layout found rather than hanging" |
| D63 | K | Product | per-hover estimate cached |
| D64 | K | Product | settings vs dialogue |
| D65 | K | Product | SD uses drag signal setting |
| D66 | K | Product | driving side follows road side |
| D67 | K | Lengths | CL derived from train |
| D68 | K | Product, Signals | junction signal setting |
| D69 | K | Process | no savegame change |
| D70 | T | Signals | keep "prio can go wherever space and cost permits"; drop generator try-each-side/score mechanism |
| D71 | K | Structure, Signals | joiners never block ML; waiting bays; no deadlocks |
| D72 | O | - | 1b single-lane milestone limit; planar multi-lane replaces |
| D74 | K | Signals | pre-signal prio always |
| D75 | K | Signals, Shape | streamlined backward 45-degree prio joins |
| D77 | O | - | 1b flat-clear assumption, milestone |
| D78 | K | Product, Heights | place-time terrain-aware hover generation |
| D79 | K | Signals | first prio join on merge tile |
| D80 | K | Structure | doubled ML bridge, equal path lengths, plain merge |
| D81 | K | Signals | no stub on merge-tile join |
| D82 | K | Signals | signal before/after every bridge |
| D83 | K | Signals, Lengths | whole-train waiting bays |
| D84 | K | Product, Structure | Low/Med/High presets drive bridge arrangement |
| D85 | K | Structure, Signals | lane merge upstream of joiner merge; no prio joins a bridge block |
| D86 | K | Process | hand layouts only checker fixtures |
| D88 | K | Signals, Lengths | bay boundary first legal piece at least TL back |
| D91 | K | Structure | early branch swap as a skeleton variant |
| D93 | T | Heights | keep terrain snapshot, blocked classes, "water only under spans"; "one plane per junction" may clash with Heights ramps (see Q) |
| D94 | K | Heights | bridge slower than design speed allowed |
| D96 | T | Product | keep hover refine runs on worker thread after rest; replaces ghost |
| D97 | K | Signals | signal distance = drag semantics |
| D98 | K | Lengths, Signals | real step counts, headway model |
| D100 | K | Structure | symmetric doubled bridges (YAPF curve penalty) |
| D101 | K | Product | loaded mass, min(train, rail), real train pick |
| D103 | K | Lengths, Structure | bridge length vs throughput; triples |
| D104 | K | Heights | bridges may span existing track/road |
| D105 | K | Structure | parallelogram doubling, shift back when safe |
| D106 | K | Product | station generator scope |
| D107 | K | Signals | prio holds under real pre-signal logic |
| D108 | K | Signals | no ML split inside prio coverage |
| D110 | K | Shape, Signals | tight split/bridge entry geometry |
| D111 | K | Signals, Slots | prio may bend with ML |
| D112 | K | Product | line painter |
| D113 | K | Signals, Structure | no split in a bridge's block |
| D115 | K | Lengths | branch stub one tile |
| D116 | T | Signals | keep "signal, one plain tile, signal allowed when prio reach holds"; judged-by-simulation mechanism retired by D363 |
| D117 | K | Signals | one signal may do several jobs |
| D118 | K | Structure, Lengths | staggered doubled bridges across diagonal |
| D119 | Q | Process | "constraints from the simulation, not hard rules" vs D361/D363 (checker oracle only); keep ML delay zero? |
| D120 | K | Product | station v2 balanced, waiting bays |
| D121 | K | Product | station throughput vs footprint |
| D122 | K | Product | NewGRF station classes |
| D123 | K | Product, Corpus | main station hub type, scope note |
| D124 | K | Product | goal: human and LLM building at scale |
| D125 | K | Product | upgrade hubs roadmap |
| D126 | K | Product, Structure | multi-lane L_R to LLLL_RRRR |
| D127 | K | Product | station modules |
| D128 | K | Process | map reading in C++ |
| D129 | K | Product | modular balance |
| D130 | K | Product | incentivise shared infrastructure |
| D131 | K | Product | profit per game time metric |
| D132 | K | Structure | pathfinder traps as balancing primitive |
| D133 | K | UI | line painter waypoint clicks |
| D134 | K | UI | line painter batch 1 |
| D135 | K | UI | line painter batch 2 |
| D136 | K | UI | line painter batch 3 |
| D137 | K | Product | design train required, sizes prio |
| D138 | K | Product, Corpus | design train = Buborough main-line train |
| D139 | K | Signals, UI | prio length slider |
| D140 | T | Process | keep "ccache; no baseline rebuilds; hand-made worktrees from the feature branch" |
| D141 | K | Product, Heights | design speed = min(rail, train) |
| D142 | K | Structure | notation groups adjacent, gap at _ only |
| D143 | K | Product | notation gap preferred, router may narrow diagonal at a cost |
| D115a | T | Lengths | keep "two tiles below edge-most split/merge: signal tile then boundary tile (G10)"; drop STUB=2 generator constant |
| D145 | T | Structure, Lengths | keep parallelogram style, one signal-legal piece per leg (D113/D117/D118); drop S2 solver constants |
| D147 | K | Product | line router LP5 scope (router is live) |
| D148 | T | Signals | keep "E->S split outside N->E join's prio coverage" (same as D108); drop G9c mechanism |
| D149 | K | Product | line bridges |
| D150 | K | Product | line waypoints |
| D151 | T | Product | keep hover generation under 100 ms; symmetric style note |
| D152 | O | - | PLAN-organic one-router plan; replaced by planar (D305+) |
| D153 | T | Product | keep "leave open the option to make better hubs than coop"; drop organic compactness-term mechanism |
| D154 | T | Structure, Heights | keep tunnel is a wormhole like a bridge: one block, no signals inside, surface track may cross; drop JunctionBridge flag mechanism |
| D155 | T | Heights | keep CmdBuildTunnel portal/exit facts, all tracks tunnel together equal length; drop router mechanism |
| D152b | K | Heights | axis piece along incline is a ramp (game fact) |
| D156 | K | Structure | money/compactness decide; ML bridging is a dominant strategy, no extra penalty |
| D157 | O | - | O4 token routing and negotiation, old search |
| D158 | K | Structure, Signals | prio bundles may bend and cross by bridge/tunnel |
| D159 | K | Signals, Product | which merges carry a prio from traffic mix, not a search choice |
| D164 | K | Structure | main-line bridging stays allowed |
| D166 | O | - | O4 hugging/clean-up of old router |
| D167 | O | - | O4 forced crossings, search mechanism |
| D168 | O | - | DESIGN-O4 revision 4 router rules |
| D169 | O | - | CP-SAT single model measured not viable; old pipeline (note: informs why MILP stack) |
| D170 | T | Structure | keep "map layer chooses the structure ... projection realises it"; drop "projection failure returns to the map layer" (conflict, see list) |
| D170a | K | Structure | "MADE to fit" by routing or terraforming; but see conflict list |
| D171 | T | Structure | keep "map records a crossing with a tile envelope any realisation fits"; "who changes level decided in projection" superseded by D364/D365 |
| D172 | O | - | crossing node envelope, node-model mechanism |
| D174 | K | Structure | ML bridges always doubled; SL bridge single at basic throughput |
| D175 | K | Heights, Structure | around or under towns/industries, never bridged |
| D176 | K | Signals, Lengths | what a full queue may block |
| D180 | K | Heights | each port at its own height |
| D182 | O | - | splits/merges/crossings as map nodes, node model |
| D183 | O | - | node model performance direction |
| D186 | O | - | first LL_RR scope milestone |
| D187 | K | Structure | all ML lanes split to S, whole-train legs merge before port |
| D185a | O | - | sequencing of old N3 steps |
| D189 | T | Structure | keep "derived by rule: node order (outgoing splits before incoming merges), sides, curve hands, rows"; drop "situational choices searched per site" |
| D190 | T | Structure | keep "main line taking the grade separation tried first (bridge or tunnel)"; terraform-as-repair search conflicts with D218 |
| D191 | O | - | no shifters in first LL_RR step |
| D192 | K | Signals | signal after SL split, line-choice bay |
| D193 | K | Process | correct by construction; checker oracle only (= D361) |
| D194 | S | by D365 | height facts entering the node solve; Heights is a later layer after Lengths (no-trial-and-error principle kept in D361) |
| D195 | Q | Heights, Slots | blocked area inside footprint "must give a layout" vs D365 "hard facts fail by tile": which wins? |
| D197 | K | Product | solve at the cursor spot |
| D201 | K | Process | graph built directly from topology, JunctionLayout output only |
| D202 | K | Product, Process | generic any topology, not the T |
| D203 | K | Structure, Slots | rows opened by S-bends; one bridge/tunnel straddles several crossings |
| D204 | K | Structure | branch-in split order |
| D205 | K | Structure, Lengths | exit lane legs merge by the port, bays use free depth |
| D206 | Q | Lengths | branch-port stub 1 or 2 tiles is OPEN; still open? |
| D207 | O | - | alternative rulesets as offline R&D |
| D208 | T | Shape | keep "nmax derived from the game: longest middle of any bridge type with speed >= line speed, capped by max_bridge_length"; flow limit replaced by D228a |
| D209 | K | Structure | tunnel fans / short bridges in series as coop pattern; cost decides |
| D210 | K | Structure | keep D205 exit-merge as is |
| D211 | S | by D361 / 17.5 | signals as graph tokens; Signals is a walk over built tracks |
| D212 | K | Slots | lane runs on its port row as long as it can; one generic rule |
| D213 | T | Process | keep "projection works on the graph, never on a rendered layout"; repair-by-graph-edit loop conflicts with D366 |
| D215 | K | Slots | every row change next to the feature it serves |
| D216 | S | by D365, D366 | projection solves graph against terrain with graph-edit repairs; replaced by Lengths then Heights |
| D218 | K | Heights | height as an axis; terraform allowed by default |
| D219 | K | Heights | ramp rule k_max, realistic acceleration required |
| D220 | K | Structure | tunnels as crossings, no span-height issue |
| D221 | K | Product, Structure | lane pairing a user option |
| D222 | K | UI | topology picker window |
| D223 | K | Structure | over/under at main x main and joiner x joiner |
| D224 | K | Structure | two branch legs bound for one main lane merge first, one joiner |
| D225 | K | Process | CLAUDE.md facts only, update per commit |
| D226 | O | - | folder layout of old topology/placement split |
| D227 | K | Structure, Signals | crossing defaults C1-C3, prio sizing across prios |
| D228 | K | Structure, Shape | cascades of splits; no hard 2-track rule under a bridge |
| D228a | K | Shape | doubled bridge flow bound B <= 2d + tl |
| D229 | K | Structure | merge before or after a crossing both legal, cost decides |
| D230 | K | Slots | S-side branch ports fan, sized by placement |
| D231 | K | Product | merge generic, merger = coop pattern, user option |
| D232 | K | Process | CLAUDE.md stands alone |
| D233 | K | Signals | prio length from game rules alone |
| D235 | T | Lengths | keep "main goal is overall cost; speed never traded; curve and ramp rules hard"; node-shape data/re-pin is old placement |
| D236 | O | - | G3.3a old joint-solve rules as length sums |
| D237 | K | Lengths | HiGHS behind a thin wrapper |
| D240 | O | - | old joint-solve rule source |
| D242 | K | Product | seed cache per preset |
| D244 | O | - | old [hoverplan] test budget |
| D245 | O | - | old wide-gate sweep tag |
| D248 | O | - | which S-bend a bridge passes over (old joint model); Slots now owns S-bends |
| D253 | K | Product | seed may take seconds in background, cached; hover under 100 ms |
| D257 | K | Product | expansion of existing hubs, rehydrate graph |
| D264 | K | Process | parallel tests, budgets |
| D270 | K | Corpus | PSG corpus as growth path |
| D276 | K | Process | do not over-prescribe rules; legal LL_RR exists |
| D277 | K | Structure | missing balancers/mergers layer |
| D278 | O | - | LL-squared probe in a 200x200 box with lazy separation |
| D279 | K | Structure | balancing is a family of patterns |
| D280 | K | Product | benchmark train double-headed NUTS |
| D282 | K | Structure, Signals | prio chosen by balancing pattern |
| D283 | K | Structure, Corpus | first LL-squared pattern is PSG 227 |
| D284 | K | Product, Structure | balancing/prio toggles, every combination solves |
| D285 | K | Structure | PSG 227 first; D24 strict |
| D286 | K | Structure | split early, cross in core, merge late |
| D287 | K | Corpus | copy PSG 227 lossily, keep parallelograms |
| D288 | K | Process | 15 s probe cap (superseded in practice by solve-budget rule, same) |
| D296 | K | Process | do not over-prescribe width-2 geometry; D292-D295 unconfirmed |
| D297 | K | Product, Lengths | seed < 5 s, hover < 100 ms |
| D299 | K | Structure | choice legs not doubled |
| D300 | K | Process | checker boundary = port sides (oracle) |
| D301 | K | Structure, Signals | prio per lane path, explicit lane paths |
| D302 | K | Structure, Signals | doubled lane's rejoin is not a D24 merge |
| D303 | T | Slots, Shape | keep "single bridge relates its deck to every track of the crossed edge, own column per track"; drop joint-model/fixed-path mechanism |
| D305 | K | Structure | nodes where a line leaves, joins or crosses a channel |
| D306 | K | Structure | edges carry ordered lines |
| D307 | K | Structure | graph fixes order not distance; crossings are level relations |
| D308 | K | Structure | coop rules live below the topology |
| D309 | K | Structure | doubling is a layout concern; ML bridges doubled by policy |
| D310 | T | Structure | keep "coop components run inside bundles; width varies"; "fixed-shape stretches" is composer mechanism (see Q) |
| D311 | T | Structure | keep one Join and one Fray type from primitives; prio row away from joining track; order swaps bridge-over-neighbour only |
| D312 | K | Structure, Slots | doubled pairs parallelogram, chevron in stacks, no staircase |
| D313 | K | Slots | ports at notation spacing, fan at last responsible moment |
| D314 | K | Structure, Lengths | doubled pair far end mirrors near end |
| D315 | K | Lengths | objective: area inside the outline |
| D316 | K | Slots | main-line ports need not align |
| D319 | K | Process | checker main line under lane-path prio (oracle) |
| D320 | O | - | D318 variants as discovery, find the levers |
| D321 | S | by D340, D350 | row itself says optional-bend direction superseded |
| D322 | K | Structure, Signals | doubled stretches in series |
| D326 | K | Product, Process | any hub at scale; per-hub compositions are fixtures |
| D327 | T | Structure | keep "lanes run in channels: ordered sets of lanes between topology events"; width-profile step mechanism now Slots |
| D328 | K | Structure | prio channel over non-prio; ML straight and doubles |
| D329 | K | Lengths, Product | area linearised, scores as user options |
| D330 | T | Structure | keep atomic vocabulary: fray, lane split, two-input join chain, prio row, crossing with deck, bay rules; composer mechanism retires (17.6 step 4) |
| D331 | K | Structure | doubling by throughput, after direction splits |
| D332 | K | Structure | SL traffic single bridges |
| D333 | K | Slots, Structure | doubled lane counts as two lanes in channel width |
| D334 | T | Shape, Lengths | keep "straight track on tile-row/column middles, whole-tile lengths, bridge heads on whole tiles"; drop composer/pieces audit |
| D335 | S | by D363 | gap/jog steps replaced by port-side corridors; "no zero-length pieces, no OPTMAX" still holds |
| D336 | K | Lengths | outline area stays the objective and ranking |
| D337 | K | Slots | straight main line stays offset, no return step |
| D338 | S | by D340, D362 | solver-chosen bends; Shape has no sideways moves, Slots owns steps |
| D339 | T | Lengths | keep corridor ladders from one-shot rough solve (17.4); lazy clash role retired |
| D340 | K | Shape | headings from a shape program, no optional bends |
| D341 | S | by D362 | per-variant default vs shape-program scoring |
| D342 | K | Signals | prio-protected track need not be straight |
| D343 | K | Structure, Product | topology complete artefact, shipped as NewGRF |
| D344 | K | Structure | crossing order pinned or open by default |
| D345 | S | by D363 | checker-judged prio search |
| D346 | K | Shape, Structure | wide crossings break up or triple per flow bound |
| D347 | K | Signals, Structure | prio coverage may contain a main-line bridge |
| D347a | S | by D347b | toggle reading replaced |
| D347b | T | Structure | keep "over/under per crossing of a prio main line is a searched choice"; granularity now per joiner (D364) |
| D348 | T | Lengths | keep "translation fixed: first port's first line point pinned"; mapped-composer mechanism retires |
| D349 | K | Structure | prio row is a real line of the bundle owned by its lane |
| D350 | K | Slots | slot layer owns sideways position, min-cost tension LP |
| D351 | K | Structure, Slots | parallelogram only, no diamond |
| D352 | K | Slots | lane width from tracks only |
| D353 | K | Signals, Shape | prio row may share diagonal tiles with main line |
| D354 | K | Lengths | linear compactness term for enclosed empty space |
| D355 | T | Slots, Lengths | keep "core's sideways move may come after bridges; prio fray need not climb first; fray split gaps are lower bounds"; drop probe order |
| D356 | K | Structure | joiner crossing nearer lanes on one diagonal is not a rule |
| D357 | K | Process | track-map step lands when every hub builds end to end, checker clean, within 15 s (17.6 gates) |
| D358 | K | Process, Lengths | each variant as compact as its own decisions allow, within tolerance |
| D359 | S | by D363 | withdrawn gap steps |
| D360 | K | Slots | node-group frames adopted (15.5) |
| D361 | K | Process | layers valid by construction |
| D362 | K | Structure, Shape, Slots, Lengths, Signals | SSS layers |
| D363 | K | Slots, Process | D359 withdrawn, checker oracle, D345 retired |
| D364 | K | Structure | separator and kind bits per joiner |
| D365 | K | Shape, Heights, Structure | tunnels, terrain, hover under SSS |
| D366 | K | Process | layer dependencies, information flows down |
| D367 | K | Process | refine, own invariants |
| D368 | K | Process | layers designed outside-in; no layer on guessed inputs |

## Q rows (engineer must decide)
- D23: main line bridged so joins pass under; is any of it left now that Structure decides separator and kind per structure (D364, D365)?
- D39: traffic level "open"; D84 settled it as presets, so likely just a stale row; confirm purge.
- D119: "constraints should come from the simulation rather than hard rules": keep only ML delay "strictly zero" and the objective, or drop the direction as contrary to D361/D363?
- D195: "a blocked area inside the footprint must give a layout" vs D365 "only hard facts fail, named by tile": which layer owns an obstacle inside the footprint, and does the row survive?
- D206: branch-port stub 1 or 2 tiles is still marked OPEN; open or decided (D115a gives 2)?

Also check before purging: D310/D311 (components as "fixed-shape stretches", catalogue C1-C9) are classed T. Pete's element-composition rule keeps the coop components inside bundles; the engineer should confirm which of their shapes still stand once the track composer (D357/D358) and MappedComposer retire.

## K or T rows that conflict with the SSS layer table
These make a later layer repair, retry or re-decide an earlier layer's work (D361, D366), or move a Structure decision downstream.
- D170 (T): "projection failure returns to the map layer" is a later layer sending work back; also D170a (K): projection "MADE to fit ... growing the box when routing needs room" and "terraforming" is a later layer adapting the earlier one's output.
- D213 (T): a failing terrain solve names a graph element and a repair edits the graph, then re-places and re-solves: the retry loop D366 forbids. Only "projection works on the graph, never a rendered layout" is kept.
- D171 (T): "who changes level and how (bridge, tunnel, doubling, terraform) is decided in projection": Structure now decides separator and kind (D364, D365); the hover may only pick within Structure's set (D366).
- D190 (T): "a failing terrain fact may be repaired by terraforming ... the search ranks repairs" makes a terrain stage repair Structure; D218 and D365 make Heights always feasible and fail only on hard facts.
- D189 (T): "situational choices searched per site: S-bends, ramps, bridge vs tunnel" lets Structure's search choose S-bends (Slots owns them) and ramps (Heights owns them). Keep only the derived-by-rule part.
- D78 (K), D96 (T), D197 (K): the hover generates at the cursor, "adapts the layout's variables, side choices and grade separation", and ranks "structure changes and terraforming" there. D365/D366 say the hover runs Heights only on the flat seed footprint and picks kind only within Structure's set. Reconcile the wording.
- D93 (T): "one plane per junction (flat, or leveled foundations up to H)" against Heights deciding every tile's level with ramps; keep only the snapshot and blocked classes.
- D116 (T), D345 (S): acceptance or prio shape judged by checker/simulation. D116's permissive part is kept; the judging mechanism conflicts with D363 (checker is an oracle).
- D211 (S): signals as graph tokens enriching the model puts Signals inside Structure; the Signals layer is a walk over built tracks.
- D303 (T) and D327 (T): the "joint model variable/binary" and width-profile step mechanism let Lengths or the frame model decide S-bends; Slots is the one owner. Kept parts only.
