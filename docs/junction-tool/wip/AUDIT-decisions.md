# AUDIT: DECISIONS.md live rulings (draft, read-only audit)

Scope: rows D1..D330 (342 rows incl. lettered). Current architecture = planar path (`src/junction/planar/`, CLAUDE.md read). Old pipeline (negotiation, G1-G4, node templates, rows/bands, projection) still ships behind the planar switch; rows that only govern it are 'not applicable' unless a later row explicitly replaces them. Each id is counted once. Evidence is from the log itself, planar/CLAUDE.md and src/junction/CLAUDE.md; code lines cited are as read.

## 1. LIVE rulings by area (D-id: ruling)

### A. Targets and budgets
- D69: savegame format never changes; tool state in client settings (openttd.cfg) only
- D151: hover generation under 100 ms (part a); symmetric-style part (b) is old generator
- D253: hover stays well under 100 ms, the hard limit; seed may take seconds, background, cached
- D288: every probe or experiment run capped at 15 s wall; cap hit counts as infeasible
- D297: each seed solves in under 5 s; extra free variables weighed against it

### B. Planar solver architecture
- D193: correct by construction; CheckJunctionRules is a test oracle, never acceptance on the hover path
- D201: no interim layout; graph straight from topology; JunctionLayout is output only
- D202: generic, not the T; second topology in tests so nothing T-only passes
- D237: HiGHS (MIT) is the solver, behind one thin wrapper (JunctionMilp)
- D257: expansion of existing hubs not built now; keep solves able to freeze or move existing pieces
- D296: do not over-prescribe geometry; generalise instead of adding rules; cards in plain coop terms
- D305: graph nodes are every place a line leaves, joins or crosses a channel; edges are channels of n lines
- D306: DAG; an edge is an ordered vector of lines, each line carries a set of routes; lines share no track
- D307: graph fixes order and level relations, not distance; crossings are relations, not nodes
- D309: doubling is a layout concern; topology never holds copies; main-line doubling is a policy rule
- D310: bundle skeleton with fixed-shape coop components running inside bundles; width varies along a bundle
- D311: C1-C9 stand; one Join and one Fray type composed from primitives; prio row away from joiner; order swaps by bridge only
- D313: ports at the notation's spacing; fans as late as possible; curve gaps along whole routes
- D316: main-line port alignment not enforced (about 5% area)
- D317: 227 join block: inside out lane straight to its port, outside lane closes up straight after its merge (must emerge from primitives per D330)
- D319: checker main line under lane-path prio; fixes go into component shapes, never a solve-check loop
- D320: find the levers (free lengths, MIP choices), not more forms; one component code path
- D321: optional bend pairs (binary z) and free lengths replace fixed fan/step choices; one generator for T2 and LL2
- D323: LLL_RRR first steps: lane keeps lane at three lanes, then pairing any; 227 join generalised; no tunnels first
- D324: T3 shapes: prio join arrival lines 0,1,3; joiners cross nearer lane before its prio; deck middle cap; optional piece cap OPTMAX*(n-1)
- D326: architecture bar: solver takes any topology, lane counts and pairing in hover budget; hand compositions are fixtures only
- D327: channels with width profiles: sideways steps emerge where width changes; solver places transitions
- D328: prio channel over non-prio at a crossing; between two prio or two non-prio the over channel is a variant
- D330: generic composer uses atomic primitives only (fray, lane split, two-input join, prio row, crossing deck, bay rules); no per-hub blocks; Block227 retires once mapped LL2 matches

### C. Objective and scoring
- D130: incentivise shared infrastructure; added track must pay for itself in throughput
- D131: network success metric: profit per elapsed game time, capital included
- D153: coop builds are a benchmark, never a ceiling; objective is absolute (clause 1 compactness term superseded by D315)
- D199: user weight between organic and artificial (terraform vs shape); default plain money
- D315: minimise area inside the outline (neighbouring-arm outer routes, shoelace); track length tie-break
- D329: outline area optimised approximately (linearised, choices open); user-selectable scores (compact, cheapest, thinnest, most square) later

### D. Coop and hub rules (curve speed, prio, signals, bridges, bays)
- D18: CL = minimum straight between same-direction 45-degree turns; derived per D67
- D19: doubled bridges: split, two bridges, merge, because vanilla cannot signal bridges
- D20: default one-way block signals; type is a parameter
- D22: T movements: mainline straight through, branch joins or leaves either direction; line gap configurable
- D23: mainline carries the bridge so joins pass underneath
- D24: splits before merges on every path (a rejoin is not a merge, D302)
- D25: prios are signal loopbacks letting trains merge only when mainline has space
- D26: let the maths design the junction from topology, rules and parameters
- D28: prio on the far side of the mainline from the joiner
- D31: player picks a train; prio sized from the acceleration model
- D32: bridge arrangement chosen by traffic level; single, doubled or bridged connections
- D33: prio signals follow the signal-type parameter unless space and cost differ
- D34: build the prio length the maths needs even beyond 2 TL and warn
- D35: bridge type: cheapest at least as fast as the rail or design speed; fast types need two or more middle tiles
- D36: diagonal piece is half a tile for CL (travel-time two-thirds figure replaced by D98a)
- D38: line notation: each underscore is one empty tile; spacing is a guideline
- D65: SD is the game's drag_signals_density
- D67: CL derived from train via Train::GetCurveSpeedLimit; no CL setting
- D68: junction-only signal type setting, client side
- D71: traffic joining an SL never blocks the ML; whole-train waiting room; no deadlocks
- D74: prios are always pre-signal entry/combo/exit
- D75: streamlined backward 45-degree prio joins; not a 90-degree rung
- D79: first prio join sits on the merge tile
- D80: main-line bridges always doubled (D299 exempts choice legs)
- D81: no axis stub on the merge-tile join
- D82: a signal before entering and after leaving every bridge
- D86: hand layouts are checker fixtures only; the generator designs
- D88: bay boundary: first legal piece at least 2 TL units back; bays may hold a bridge
- D94: unlimited rail with no fast-enough bridge falls back to the fastest bridge
- D97: signal distance = drag-fill rule (CmdSignalTrackHelper weights 192/128), default drag density
- D100: doubled copies equal pathfinder cost (symmetry) so YAPF balances them
- D101: pick a train like the depot clone tool; loaded mass; (support-both acceleration clause replaced by D219)
- D103: bridge length matters for throughput; long spans need triples (measure on rig)
- D104: bridges may span existing rail and road (Spannable class)
- D105: doubled-lane shape is free; the rule is balance (equal cost)
- D107: prios must hold under OpenTTD's real pre-signal logic (G9b)
- D108: no ML split inside a prio's coverage plus one segment (G9c)
- D110: no two adjacent splits on an ML path: three-way split or one signal tile between
- D111: a prio may bend and S-bend with the ML
- D113: at least one signal piece between a split and the next bridge head
- D116: any prio shape that the prio-reach check proves holds is allowed
- D117: one signal may do several jobs; remove back-to-back signals where one suffices
- D118: staggered parallelogram: one signal tile per leg, equal bridges, one-tile stagger
- D137: a picked design train is required; no made-up default train
- D138: design train is Buborough's main-line train (2x Mastermind + 6 wagons)
- D139: prio slider maxes at the computed M; minimum one TL; below M accepts occasional ML holds
- D141: design speed = min(rail limit, picked train max speed); replaces D45 when a train is picked
- D142: in LL_RR directions are adjacent; underscores set the gap between direction groups
- D145: near-end lane constants x_ls - x_heA = 2, x_ls - x_heB = 1 (far end replaced by D314)
- D154: a tunnel is a JunctionBridge with tunnel = true, a wormhole for blocks and signals
- D156: main-line lift first; money and compactness decide, no extra penalty
- D164: main-line bridging stays allowed; a shortcut that changes layout looks goes to the engineer first
- D174: main-line bridges and tunnels always doubled; SL single at basic throughput
- D175: towns and industries passed by going around or tunnelling under, never bridged
- D176: queue exactly full must not block the other direction; multi-lane legs whole bays
- D188: each lane keeps its own doubled bridge: four bridges for an LL_RR crossing
- D190: main line takes grade separation first; terraform is a repair ranked by cost
- D192: signal straight after every line-choice split; provisional until rig test
- D208: nmax derived from game bridge types; flow limit past some length measured on rig
- D219: ramp rule: zero speed loss at line speed; junction tool requires realistic acceleration
- D220: a crossing may be bridge or tunnel, costed; tunnels avoid span-height limits (queued)
- D224: two branch legs bound for one main lane merge first, join as one joiner
- D228: cascades split-before-merge approved; no cap on tracks under one bridge
- D228a: doubled bridge flow bound B <= 2d + tl (derivation to confirm on rig)
- D231: merge is topological; merger is a coop pattern; merger style is a user option
- D233: prio length M from the game's acceleration model alone: JunctionPrioLength
- D279: balancing is a family of patterns; solver should trade footprint against throughput
- D282: a prio is not added by default; the balancing pattern decides who has priority
- D283: first balanced LL2 is PSG 227's async-prio pattern
- D284: balancing and prio patterns are toggles and every combination must solve
- D285: PSG 227 first as it exercises prio toggle; D24 kept strict in first build
- D286: split early, merge late, cross in the core; through bundles cross in the middle, choice tracks swap in the outer core
- D287: represent PSG 227's graph not his tiles; his prio styles map to our one prio; parallelogram bridges stay ours
- D298: topology may list prio movements; unlisted merges are plain; empty list keeps old rule
- D299: choice legs' bridges are single; checker exempts unlisted prio movements
- D300: checker footprint boundary is the box edges that hold ports
- D301: topology may list lane paths with prio flags; the flag decides the checker's prio rules
- D302: a rejoin of equal lane paths is not a merge for D24 (but see D309 in section 3)
- D312: tight near heads; pairs are parallelograms with equal bridges; chevron in stacks; no one-way lean
- D314: far end mirrors near end: one plain piece per leg at both ends
- D322: a lane may double at several crossings in series; G1 copies defined by shared-piece order and equal stretches

### F. Projection and terrain
- D78: place-time generation with local terrain and obstacles as inputs
- D95: fit the landscape first; terraform is a priced candidate (order clause replaced by D196)
- D152b: track along an incline is a ramp; level foundation only across an incline and for diagonals
- D161: line router pruning stays approximate; tests check informed <= uninformed cost
- D162: multi-track line router heuristic: rail floor x track 0 Manhattan plus turns
- D195: a blocked area inside the footprint must still give a layout when ports are clear
- D196: height failure repaired by terraform or ramp structure; cheaper wins, neither is last
- D197: solve AT the cursor; offsets only when the cursor spot has no layout
- D203: a single bridge or tunnel may straddle several lines (clause 2); clause 1 rows are old generator
- D218: height as a graph axis; terraform allowed by default; only hard facts fail a solve

### G. Hover, seed, caching, pricing
- D7: build by ordinary commands in sequence; validate up front; stop on failure and report
- D9: exact cost (test-only runs of every build command) shown before building; re-checked at build
- D50: bridge span tiles that also hold track are cleared once in the estimate
- D63: hover pricing is the per-hover test-run estimate

### H. Process and tooling
- D1: core C++ fork; upstream acceptance not a goal
- D2: player tags connection points on an existing mainline; not generation from a prompt
- D4: test-first, outside-in, run through the agent-orchestration skill
- D5: plan files in docs/junction-tool on feature/junction-tool
- D6: solver reads terrain only through a small interface with game and test implementations
- D42: new code in junction_* files; upstream edits minimal seams, each its own Codechange commit
- D43: shared upstream constants live in headers as static constexpr
- D48: follow OpenTTD house UI conventions
- D52: tiny fully-specified fixes inline; agents only when worth their fixed cost; Sonnet for small delegated fixes
- D124: goal: fast human and LLM-driven building at network scale; structured results, deterministic generation
- D125: upgrading existing hubs: read hub from map, evaluate, generate smallest change (roadmap)
- D128: map reading in C++ (console export), not a separate savegame parser
- D225: CLAUDE.md files hold decided facts; updated in every promoting commit (but see D232 in section 3)
- D226: src/junction folder layout: topology, placement, signals, root driver (planar sits beside it)
- D264: jt runs tests per process in parallel; gate, sweep, identity before commit; budgets 30/60/45 s; never raise a budget for a slow test

### I. Stations and line painter
- D106: station v1: ro-ro drop, user-set platform count, platform length exactly TL
- D120: default ro-ro drop is balanced with entry and exit waiting bays
- D121: stations trade throughput against footprint using the evaluator
- D122: stations use the chosen station class and type, with a dressing pass
- D123: main station hubs are a hub type of their own (roadmap)
- D127: station generator works in modules; extensibility is a design criterion
- D129: balance is per incoming line; each line adds one unit of balance
- D132: pathfinder traps are a balancing primitive with YAPF-default costs
- D133: line painter is waypoint/click driven; nothing built until confirm; undo free
- D134: waypoint fixes position only; re-route last two segments; router yields only buildable track
- D135: line ends snap to own track ends; own track and roads crossed by bridge only
- D136: painter window, worker-thread routing, client-local ghost; diagonal spacing aim 3, allow fewer at a cost
- D143: router may narrow a diagonal run to 2 or 1 at a cost
- D147: line router works at one plane; existing track an obstacle unless exact parallel pair
- D149: line bridges move the whole line; heads side by side
- D150: waypoints and frozen prefix in the line request
- D155: line-router tunnels mirror CmdBuildTunnel; all tracks tunnel together

### J. UI, game integration, throughput
- D8: rail type is an input to every solve; rules component separate from terrain interface
- D14: live price next to cursor, red when unaffordable or invalid
- D15: rotate and mirror by hotkey
- D16: junctions come from C++ generators into an in-memory description
- D40: cursor at the layout's north corner; red outline on invalid tiles
- D44: ghost drawn in full colour, opaque (D13 replaced)
- D49: persistent cost line in the Junction window
- D51: Shift-click shows the estimate for the whole junction, never builds
- D53: ghost always opaque; footprint outline pinned at the build origin while building or failed
- D54: blue outline for pieces still to build; red means invalid
- D55: generic tooltip 'tiles shown in red can't be used'
- D56: draw order: ghost sprites, selection, ghost outlines
- D57: translucent ghost blitter parked
- D60: mainline and branch take separate notation strings
- D61: connections may pass under a bridge on diagonal track
- D66: driving side follows the road-side setting; manual mirror flips
- D84: traffic level as Low/Medium/High presets mapped to throughput brackets (not yet built)
- D98: 1c model defaults: real step counts, block-occupancy headway, YAPF only (clause a replaces D36's two-thirds)
- D119: constraints should come from simulation; ML delay strictly zero by default (D139 softens)
- D126: multi-lane L_R up to LLLL_RRRR and beyond with line choice, balancing, shifters, bays
- D221: lane pairing is a user option: any lane to any, lane keeps lane, lane per direction
- D222: compact topology row: presets dropdown plus Topology window; presets only set the choices

### E. Topology language
- D308: topology = ports with lines in boundary order, routes, lines labelled by routes (CB1 notation), 2-in/1-out, 1-in/2-out or crossing nodes

## 2. SUPERSEDED (old -> replacing; evidence old -> new)

- D13 -> D44 / D53: D13 'semi-transparent' -> D44 'Full colour, opaque'; D53 'the ghost is always drawn opaque'
- D21 -> D65 / D97: D21 'default 2' -> D65 'SD is the game's existing signal spacing when dragging'
- D27 -> D282: D27 'Mainline merges only' -> D282 'A prio is not added at every join by default: the balancing pattern decides'
- D29 -> D34 / D233: D29 'between 1 and 2 TL' -> D34 'build the length the acceleration math requires, even beyond 2 TL'; D233 'Replaces the D137/D185a sweep'
- D30, D76, D114 -> D315 / D329 (planar code): D114 'count of tiles the junction occupies... then build cost' -> planar CLAUDE.md:61 'objective is track length plus one linearisation of the outline area (D329)'
- D37 -> D66: D37 'option in the build dialogue' -> D66 'no dialogue option' (follows road-side setting)
- D39 -> D84: D39 'Open' -> D84 'settles D39's form': Low/Medium/High presets
- D45 -> D141 / D137: D45 'fastest engine the company can build' -> D141 'replaces D45's fastest-buildable-engine rule when a train is picked'
- D59 -> D79: D59 'continues past the merge' -> D79 'Overrides D59'
- D62 -> D151 / D253; planar_plan.cpp:95: D62 'at most 64x64 tiles and 200 ms' -> D151 'under 100 ms'; planar_plan.cpp:95 'for (int bound : {64, 128})' (T3 is 129x45)
- D64 -> D69 (and D67, D68): D64 'persistent settings go into the advanced game settings window' -> D69 'no new game settings... client settings (openttd.cfg) only' ('Settles the persistence question')
- D70 -> D311(3): D70 'either side of its ML track' -> D311 'The prio row sits on the side away from the joining track'
- D72 -> D126 / D186: D72 'one lane per direction... multi-lane not supported yet' -> D126 'expand the existing L_H only models to LLLL_RRRR'
- D77 -> D78 / D218: D77 'flat and clear' -> D78 'take terrain into account'; D218 'terraforming allowed by default'
- D83 -> D88: D83 'exactly k x TL units back' -> D88 'not exactly k x TL'
- D85 -> D100: D85 'second lane on the outer side' -> D100 'revises D85's lane geometry' (symmetric Y)
- D92 -> D253 / D297: D92 'about 0.5 s... over D62's 200 ms target' -> D297 'solves in under 5 s; the hover stays under 100 ms'
- D99 -> D99a: D99 'exempts single-tile blocks' -> D99a 'D99 was wrong; narrowed'
- D102 -> D233: D102 'L = ceil(max over t ...)' -> D233 'M = the main train's largest lead... SizeJunctionPrio is retired'
- D112 -> D133: D112 'drags from a start' -> D133 'Revises D112's drag'
- D115 -> D115a (still open: D206): D115 'STUB = 1' -> D115a 'STUB = 2'
- D137a -> D233: D137a 'SizeJunctionPrio(spec)' -> D233 '`SizeJunctionPrio` is retired'
- D140 -> D264: D140 'parallel test runner scratchpad/ptest.sh' -> D264 '`jt` runs each test case in its own process, 14 at a time' (ccache and hand-made worktrees still stand)
- D152a -> D152b: D152a 'router offers both' -> D152b 'replaces D152a's choice'
- D157 -> D160: D157 'anchors' -> D160 'Replaces D157's anchor layer'
- D159 -> D282: D159 'Main-line merges always have one... a branch merge when the throughput estimate says' -> D282 'Each pattern... states its prios'
- D160 -> D182 / D305: D160 'splits and merges emerge' -> D182 'The splits and merges absolutely have to be nodes'
- D163 -> D164: D163 header 'REVOKED by D164'
- D168 -> D170: D168 'Rules move into the routing' -> D170 'Supersedes D168's masks and emergent splits/merges'
- D169 -> D237; planar CLAUDE.md:56-61: D169 'negotiation stays the core (D168); a solver only as an offline tool' -> D237 'HiGHS (MIT) enters the fork build' and CLAUDE.md 'composes ... as a MILP per variant' (the CP-SAT measurement itself concerned a different model; see section 3)
- D177 -> D180: D177 'single hub plane' -> D180 'D177's single hub plane is retired'
- D181, D181a -> D182: D181 header '(SUPERSEDED by D182)'
- D184, D185 -> D185a / D201: D184 'flat structure built on first use from today's flat generator' -> D185a 'Amends D185: no stopgap on today's flat generator'; D201 'No interim layout'
- D189 -> D305 / D306 / D308: D189 'Topology = ports... + movements' -> D308 'topology: ports with their lines in boundary order, routes... lines labelled with the routes'
- D209 -> D228: D209 'a bridge's crossing set is capped' -> D228 'replaces D209's cap'
- D212 -> D313: D212 'the seed is not changed; decided when projected' -> D313 'in-bound fans are as late as possible' in the layout solver
- D223, D227, D239 -> D328 (planar code: CLAUDE.md:27): D227 C1 'the one from the far side... goes over' -> D328 'Between two prio or two non-prio channels the over channel is a variant'; D227 PS3 prio sizing stands via D233
- D234 -> D235: D235 'supersedes D234(1)(2) and S4'
- D236, D240, D243, D246, D249, D250 -> D254: D254 'Revises D236..., D240 (pairs from the arrangement), D243, D246 and D249 (retired), D250'
- D244, D245, D247, D251, D262 -> D264: D251 'was 150 s, D247'; D264 'D251's 300 s retired', 'Revises D262'. (D244, D245 tag-specific to the old placement suites)
- D248 -> D256: D256 'G3.4a-2's t and D248's before/after retire with it'
- D254, D255, D256, D258, D260, D261, D263, D265, D266, D267, D268, D269, D271, D272, D273, D274, D275 -> D276 / D277 / D281: D277 'G4's static-family plan stays stopped (D276)'; D276 'The G4 queue stops after 2b2b1b'; D281 'The single joint search is not the core for "any"'
- D277 -> D308 / D310: D277 'topology = core type + per-leg options' -> D308 'topology: ports with their lines... Any specific features of co-op rules don't belong in the topology'
- D289 -> D311(2): D289 'Exit and join blocks are produced by a template generator' -> D311 'one Join and one Fray component type, each composed inside its stretch from the primitives'
- D292, D293, D294, D295 -> D296: D296 'D292-D295 are NOT confirmed: treat them as open'
- D304 -> D305: D305 'Supersedes D304 (held, not built)'
- D318 -> D320: D320 'The D318 variants are discovery, not the model'

## 3. UNCLEAR / CONFLICTS

### 3a. Counted unclear (no explicit evidence either way)
- D235: clause (3) 'objective: the flat build cost' and 'width + height is not a great measure' vs CLAUDE.md:61 objective = track length + outline area (D329); DESIGN-planar.md:24 still says 'flat build cost... plus compactness'. Clauses (1),(2) are old placement. SETTLE: engineer retires D235(3) in favour of D315/D329, and DESIGN-planar objective line edited; or D329's 'cheapest' score is declared the D235 mode.
- D216, D213: D216 'You don't place on flat ground if the target ground is not flat'; D213 'Projection works on the graph, never on a rendered layout' vs CLAUDE.md:47 'RenderPlanarRoutePrograms... (flat placement even on terrain, levels solved after...)' for component plans (T2, LL2); non-component plans re-solve on terrain (line 49) SETTLE: engineer rules whether flat-placement-then-levels is accepted for component plans, or D213/D216 are retired for the planar path.
- D242: D242 'at least one seed per preset... last 8, LRU, made off the main thread' vs planar_plan.cpp:80 unbounded static std::map keyed by topology text and spec fields; no LRU or thread SETTLE: decide whether the tool/ layer implements D242 on top of the planar cache, or D242 is restated for the planar cache.
- D232: D232 'CLAUDE.md should not know about or reference the decision log' vs planar/CLAUDE.md (headings cite D305, D306, D327, D328, D329, DESIGN-planar) and src/junction/CLAUDE.md (cites D298-D302, D319, D322; names docs/junction-tool/tools/sigsim.py) SETTLE: engineer confirms D232 still applies, then strip the ids; or amends D232.
- D325: D325 'every lane of the joining bundle is a choice lane' vs D326 'Hub semantics such as D325's choice of fixed lanes are the user's option, input to the solver'; probe 10 parked; CLAUDE.md:61 lane splits/merges refused SETTLE: engineer confirms D325 is parked fixture only; add lane-pairing options to PlanarTopology.
- D58, D115a, D206: D58 'one tile beyond the outermost split' -> D115a 'STUB = 2' -> D206 'OPEN... could be 1 or 2 tiles'; planar CLAUDE.md:36 port end segment 'at least 4 units' (2 tiles) SETTLE: engineer rules D206; planar constant named in planar_layout.
- D93, D96: D93 'worker thread after the hover rests 300 ms, cached per (anchor, orientation, params, terrain hash)' and D96 'instant tier: library of clean flat candidates' vs DESIGN-planar §3 'Hover: the seed's binaries fixed, positions solved on the terrain'; neither marked SETTLE: engineer or DESIGN-planar §3 names the hover design; retire D93/D96 if so.
- D270: D270 'once G4 lands, widening the model is the next phase' but G4 was stopped by D276 and replaced by planar SETTLE: engineer says the corpus phase now follows the projection push; memory note says 'next phase after G4'.
- D207: D207 'Alternatives are an offline R&D path... not candidates at hover time' vs planar variants (D327/D328 triangle flips, tie sites, up to 6 bits, 16 variants) solved at seed time SETTLE: none if variants count as seed-time only; engineer confirms.
- D179: D179 'every port's approach usable before any routing' (cheap move-the-cursor check) is not in planar CLAUDE.md; unknown if the planar hover has it SETTLE: grep planar_plan for the port-approach test.

### 3b. Counted live (or superseded) but in tension with the planar path or a later row
- D253, D297: D253 'seed may take seconds' (looser) vs D297 'each seed < 5 s' (later, tighter; D297 refines D253, no conflict between them) but planar T3 takes 10 s wall for 16 variants (CLAUDE.md:62) and each variant has a 15 s budget (planar_components.cpp:45, CLAUDE.md:65); hover time of the planar path is unmeasured against 100 ms (STATUS: 82 ms only 'with everything fixed') SETTLE: engineer says whether D297's 5 s is per seed or per variant on a thread pool; time the planar T3/T2 seed and hover in Release.
- D225: D225 'updates CLAUDE.md per commit' is live but its 'removes facts it supersedes' is not applied: src/junction/CLAUDE.md still lists the old pipeline, G9 rules and Order/Structure/Programs stages beside planar/CLAUDE.md line 3 'shares none of' SETTLE: audit pass of src/junction/CLAUDE.md for pipeline facts that only the old path uses.
- D169: (second reading) the CP-SAT infeasibility measurement (44x16 no layout in 120 s) is evidence against 'one constraint model at runtime' but planar runs HiGHS MILP at 129x45 in seconds with width-decided switches; D326 asks for generic hover budget SETTLE: none needed beyond retiring D169's ruling (section 2); keep the measurement as history.
- D302: checker still counts a rejoin by D302's 'carry the same (from_slot, to_slot) paths' (src/junction/CLAUDE.md) while D309 says the checker 'recognises the rejoin as the end of one (replaces D302's route-set test in the new model)' SETTLE: grep CheckJunctionRules for D302's route-set test; if still there, D309 is not yet implemented.
- D194, D319: D194 'We're not supposed to be doing anything by trial and error'; D319 'never into a solve-check loop' vs CLAUDE.md:42,66 lazy clash loop re-solving until none found SETTLE: engineer confirms lazy MILP row generation (clash separation) is allowed, trial-and-error repair of geometry is not.
- D326: D326 'hand-written compositions per hub... never the product' vs CLAUDE.md:46,56 SolvePlanarComponents has hand compositions for `T2` and `LL2`; D330 (2026-11-17) says Block227 'stays only until mapped LL2 matches it, then retires'; D284 'every combination solves' vs CLAUDE.md:61 'Lane splits and lane merges... are refused for now' SETTLE: in progress by D330: close when LL2 runs through SolvePlanarMapped and SolvePlanarComponents' name table is deleted.

## 4. NOT APPLICABLE (pure history or old-pipeline only; no later row or planar code uses it)

D3, D10, D11, D12, D17, D41, D46, D47, D73, D87, D89, D90, D91, D99a, D109, D144, D117b, D146, D148, D148a, D152, D158, D165, D166, D167, D170, D170a, D171, D172, D173, D178, D179a, D180, D182, D183, D186, D187, D185a, D191, D198, D200, D204, D205, D210, D211, D214, D215, D217, D229, D230, D238, D241, D252, D259, D276, D278, D280, D281, D290, D291, D303

## 5. Count

live 185 / superseded 81 / unclear 15 / not applicable 61 (total 342); 3b rows are counted under live or superseded.
