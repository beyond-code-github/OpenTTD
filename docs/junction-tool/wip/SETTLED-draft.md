# Settled design questions (draft index of DECISIONS.md)

Source: docs/junction-tool/DECISIONS.md, rows D1 to D456 (later rows win). Entries are keyed by question and ordered by pipeline layer. "Open?" marks a row whose status the rows leave unclear. "SSS-superseded" is the DECISIONS note "mechanism parts superseded" on a row; the mechanism there is not current, the principle may be.

# Product, building and UI

## Where does the feature live, and what may it change in the game?
Now: core C++ in a fork; new code in `junction_*` files, upstream edits only minimal seams (D1, D42, D43); the savegame format never changes, tool state lives in client settings (D69).
Dropped: NewGRF or Game Script (D1: cannot do it / lacks the UI).

## What does the player do, and how is the plan built and priced?
Now: pick from a junctionary, tag connection points on a mainline, get the smallest organic footprint (D2); plan posted as ordinary build commands, validated up front, stop and report on failure (D7); exact test-run cost shown and re-checked before building (D9, D14, D49, D50); Shift-click only shows the estimate (D51).
Dropped: generation from a prompt (D2).

## How does the ghost look and behave?
Now: solid, full-colour, opaque ghost (D44, D53); red outline on invalid tiles, blue for pieces still to build, footprint pinned at the build origin (D40, D53, D54); draw order ghost, selection, outlines (D56); one generic "tiles shown in red can't be used" message (D55); rotate/mirror hotkeys (D15); follow house UI style (D48).
Dropped: red-tinted transparent ghost (D13 -> D44/D53: no reduced-opacity blitter); translucent ghost blitter (D44 -> D57: parked, too much work).

## Where do junction parameters live (settings vs window)?
Now: per-game values are settings (junction signal type D68; SD reuses `drag_signals_density` D65); the window keeps per-placement choices: line notations (D60, D64), train, traffic level (D84); driving side follows road side (D66); window shows topology as a presets dropdown plus a Topology... window, steppers not typing (D222).
Dropped: leg length as a control (D64: fixed margin D58); CL as a setting (D64 -> D67); presets with state of their own (D222: presets only set the Topology window's choices).

## Which train drives the maths, and what is the design speed?
Now: the player picks an actual train (clone-style); tool refuses to generate without one (D101, D137); loaded mass, both acceleration models supported (D101), but ramp rule needs realistic acceleration (D219); design speed = min(rail type, train max speed) (D141); benchmark train is Buborough's double-headed NUTS consist (D138, D280).
Dropped: made-up default train (D137); fastest-buildable-engine speed once a train is picked (D45 -> D141); the prio_tl=1 bug that ignored the train (D137).

## What are the time budgets for hover and seed?
Now: hover well under 100 ms and the first frame drawn is what gets built (D151(a), D253, D370); a seed (planar solve per topology and settings) under 5 s, background, cached (D253, D297); seeds cached per preset, last 8 LRU (D242); probe runs capped at 15 s, a hit counts as infeasible (D288); at most 64x64 tiles then "no layout found" (D62, SSS-superseded).
Dropped: 200 ms hover target (D62 -> D151(a)).

## How do traffic level and the throughput model enter?
Now: traffic level is Low/Medium/High presets mapped to throughput brackets, values to be pinned by measurement (D84); 1c model defaults (D98); traffic mix default `MakeDefaultJunctionMix()` until a GUI setting (D159); long bridges and prios sized from the model (D103, D139).
Open?: D119 (constraints from simulation not hard rules, "OPEN: engineer to rule"); D84 bracket values.

## How is the player's line built (line painter)?
Now: rail-toolbar tool; click a start and waypoints, router lays ghost track, nothing built until Confirm, undo free (D133, D134, D136); waypoint fixes position only, router picks heading (D134); first/last click snap to own track ends (D135); tracks of one direction adjacent, gap only at `_` (D142); diagonal spacing aim 3, 2 or 1 allowed at cost (D136, D143); bridges and tunnels allowed (D149, D155); one plane, existing track an obstacle (D147).
Dropped: drag interaction (D112 -> D133).

## How does the tool read existing maps and grow beyond sideline hubs?
Now: map export is C++ in the fork, a console command writing JSON (D128); roadmap items, none scheduled: main station hubs (D123), upgrading and expanding live hubs with frozen/movable/new pieces and one-line-at-a-time sequencing (D125, D257), multi-lane to LLLL_RRRR with line choice, balancing and shifters (D126), network planning scored by profit per elapsed game time (D124, D130, D131), PSG corpus widening after G4 (D270).

## What do stations (ro-ro drop) look like?
Now: balanced, buffered entry bays, exit bays, short prio at the exit merge, bay 1 TL (D120); judged by throughput then footprint (D121); modules, balance per incoming line (D127, D129); dressing with the player's station class later (D122).
Dropped: v1 "no balancing, direct entry" (D106 -> D120).

# Topology

## What is a topology, and what does it fix?
Now: a planarised DAG: edges are ordered vectors of lines (a line is one physical track), each labelled with the routes it carries; crossings are level relations, not flow nodes (D305, D306, D307); the graph fixes order, not distance, rows or spans (D307); built directly from topology by maths, no interim layout (D201, D202); projection works on the graph never a rendered layout (D213); a topology is a complete authored artefact, shippable as a NewGRF (D343).
Dropped: topology as flat-order/box model (D305 supersedes D217/D304); interim JunctionLayout (D201: output only).

## Does the topology fix crossing order?
Now: crossing order may be pinned per triangle by the author, open by default; the solver resolves open orders cheaply (D344); a topology may author every crossing with `cross L:` lists, pinned, used to write down PSG 197 (D389).
Dropped: topology fully fixes the planar order (D343 -> D344: best order depends on geometry the author cannot see).

## Where do coop rules and doubling live, if not in the topology?
Now: coop rules (roles, prio owner, signal style, D24) are a layer below topology (D308); doubling is a layout concern, topology never holds copies, "main-line bridges doubled" is a policy rule on the layout choice (D309).
Dropped: graph rewrite with a doubling node (D306/D308 -> D309).

## What notation describes lines, lanes and pairing?
Now: `L`/`R` with `_` as one empty tile, guideline not constraint (D38); own notation per line (D60); same-direction tracks adjacent (D142); lane pairing A any-to-any (default), B lane keeps lane, C lane per direction, chosen with the topology (D221).

## Which balancing and merger styles are supported?
Now: merge = topological node, merger = coop pattern; merger style is a user option per hub type (D231); balancing is a family of patterns the solver trades against footprint (D279); balancing and prios are toggles and every combination must solve (D282, D284); first LL^2 built is PSG 227's async-prio pattern, copied lossily (D283, D287).
Dropped: prio at every join by default (D282: the balancing pattern decides).

## Must the solver take any hub, or hand-composed ones?
Now: the solver takes any user topology, lane counts and pairing inside the hover budget; hand compositions and per-hub tables are fixtures only (D26, D202, D326); litmus test: represent and build PSG 197 BBH01 (D387).
Dropped: per-hub hand compositions keyed by name (D326); T-only generator (D202); hand-designed layouts as product (D86: acceptance fixtures only).

## Is "split early, merge late, cross in the core" the hub philosophy, and is D24 strict?
Now: split by direction early, cross in the core (through bundles in the middle, choice tracks swap in the outer core), choose line and merge late (D286); D24 splits-before-merges is hard for everything generated (D24, D285); a doubled lane's rejoin is not a merge (D302, D309) and doubling may repeat in series (D322); a choice split after a merge exists only in a pinned written-down hub (D391).
Dropped: merge-then-split compaction in the first build (D285: later option).

## Are pathfinder traps used?
Now: traps are a solver element to equalise YAPF costs, costs from YAPF defaults (D132); the extractor ignores V's unknown prio/trap rectangles with 90-degree moves (D411).
Open?: D132 (catalogue of trap types "still to be filled in"; overflow tree inferred, not rig-checked).

# Architecture

## What is the solver's architecture?
Now: stack of layers, each valid by construction, never repaired by a later one (D361); Structure, Shape, Slots (which owns lengths), then Heights and Signals (D362, D379); each layer reads only the model before it, refines it and owns its invariants (D366, D367); bundle-level SSS with components inside bundles (D388, D310).
Dropped: Lengths as a layer (D362 -> D379); track composer plan (D362); free track placement (D310, SSS-superseded); per-hub cell/dummy refinement in Shape (D385/D386 -> D388, retired D431).

## In what order are layers designed?
Now: outside-in, in stack order; a layer's design starts only when the layer before it has a settled hand-off spec (D368); PSG 197's Structure blockers first (D388(c)).
Dropped: probing or building a layer on guessed inputs (D368).

## Is the rule checker an acceptance step?
Now: no, a test oracle only; a violation is a missing layer promise (D193 settled by D363); a checker fix goes into component shapes, never a solve-check loop (D319).
Dropped: checker in acceptance on the hover path (D193 -> D363); prio search judged by checker functions (D345 per D363, retires with real prio rows).

## Are there fixed-shape components (gadgets)?
Now: no gadget catalogue for now; node, structure, corridor rules own the shapes, "verified once" is a proof test per rule (D401); atomic primitives only, no composite blocks (D330); one Join and one Fray type composed from primitives (D311); nodes (including the trident) are rules (D400).
Dropped: gadget per node/structure (D398 -> D401, retired D431); Block227 as composite (D330); catalogue components C1-C9 as fixed programs (D311: kept as oracles only).

# Structure

## When is a bridge doubled?
Now: doubled ML bridge, split, two equal-length parallel bridges, merge, block-signalled, plain merge (D19, D80); doubling by flow share: in-lane 1, each leg after a direction split full, after a lane choice 1/k; full share doubled, half or less single (D331); sideline traffic counts half share (D332); a doubled lane counts as two lanes in its channel (D333).
Dropped: single ML bridge deferral (D32 -> D80); choice legs single by rule (D299 -> D331); doubling by prio flag (D299 -> D331); "always doubled" (D80 note -> D309, D331, D332).

## What shape is a doubled pair?
Now: parallelogram, equal bridges, one-tile stagger, one signal tile per leg both ends (D105, D118, D312, D314); line ends on its copy's track, no diamond (D351); pairs in a stack mirror (D312); balance for equal pathfinder cost is the rule (D100, D105).
Dropped: symmetric lane style as a chosen style (D100 -> D105/D151(b); style 0 ON HOLD in D151 then SSS-superseded); one-way staircase lean (D312); D145's far-end constants (D314).

## How long may a bridge be, and what breaks it up?
Now: nmax derived from game (longest type at least as fast as the line, capped by max_bridge_length) (D208); doubled bridge B <= 2d + tl, N parallel copies B <= N(d+tl) - tl, longer spans broken by a signalled landing or tripled, by cost (D228a, D346); no crosser-count cap (D228).
Dropped: hard-coded nmax 10 (D208); 2-crosser cap (D209 -> D228).
Open?: rig confirmation of the flow bound (D208, D228a, D346).

## Which bridge type is built?
Now: cheapest type at least as fast as the design speed (D35, D141); when none is that fast, the fastest bridge, trains slow on it (D94); bridges may span existing plain rail and road (D104).
Dropped: fixed R47 length rule (D35).

## Which crossing goes over, and who is the separator?
Now: a joiner has two Structure bits, separator (joiner or main) and kind (bridge or tunnel), shared by all its crossings (D364); every mover vs keeper at node groups likewise, starting from the straight/through line as separator (D372); main line over is the search's start; searched, not a toggle (D347b, D156); defaults C1-C3, turning leg under, far-side leg over (D223, D227, D328).
Dropped: global "bridge the prio line" toggle (D347a -> D347b); one bit per join or per crossing (D364); fixed-only rule D223 (D347b makes it the start).
Open?: D372 (no corpus check for frays).

## May the main line bridge?
Now: yes; lifting the ML onto a bridge is the dominant compact strategy, money and compactness decide, try it first (D156, D164).
Dropped: D163 ban on ML bridging (D164: orchestrator shortcut around missing doubled-bridge move, contradicted D156).

## How are crossings grouped into structures, and what does Structure cost?
Now: Structure groups crossings, one structure per maximal run, split only where the kind limit or D228a forces (D369, D371); cost is count of structures plus copies, nothing about which line separates (D371a); one bridge or tunnel may straddle several lines (D203); variants rank Structure, then Shape, then Slots (D373, D377).
Dropped: counting ML crossings in the cost (D371(1) -> D371a); deck groups an L2 output (D369).

## How are tunnels used?
Now: a crossing is a bridge or a tunnel, per structure (D220, D365); around or under blocked tiles, never bridged over towns (D175); tunnels main-line doubled like bridges (D174); tunnel under tunnel allowed, Heights picks which is deeper (D390); main-line tunnel entrance is the portal alone, Heights makes the ground lower outside (D437); a crosser may run along a tunnel's line (D438, D439).
Dropped: per-crossing bridge-or-tunnel choice (D220 -> D365 per structure, per joiner D364).

## Where do exit lane legs merge and where does a branch split in?
Now: exit lanes of a direction merge by the branch port, outer band (D205, kept by D210); branch-in splits by direction first, then lane (D204); legs for one main lane merge first and join as ONE joiner (D224); joiner merge before or after a crossing, cost decides (D229).
Dropped: merge between bundles (D205 option, ruled out by D210).

## How long is a branch port's stub?
Now: 2 tiles (signal tile plus boundary tile), one rule constant (D58, D115a, D206).
Dropped: 3-tile then 1-tile stubs (D90 -> D115 -> D115a: literal 1 tile puts the signal on the boundary).
Open?: D206 (1 or 2 tiles depending on signal placement, "record as open").

## Is there a footprint margin, and where are the ports?
Now: footprint ends one tile beyond the outermost split, merge or prio signal, all tracks of a leg flush to the edge (D58); each port at its own height (D180); ports at notation spacing (D313); a port line may carry positions along its side (D410) and may have length 0 before its first node (D412).
Dropped: TL-length exit legs (D58); single hub plane (D180 retires D177).

# Shape

## What does Shape decide?
Now: headings from a shape program over the planar map, no lengths, no sideways moves (D340, D362, D373); bends are not optional pieces in a length solve (D340); variants rank Structure then Shape (D373); no heading rule at prio merges (D374).
Dropped: composer rules for headings (D340); Shape owning cells and dummy edges (D385, D386, D405 to D409: retired D431).

## How do crossers meet a deck?
Now: crossers under one span run parallel (headings equal mod 4), per span, so a doubled pair's two decks may differ (D376); a crosser may bend at its crossing (D414, D420(2)); along-run over a tunnel (D438); deck gap 2 half tiles parallel, 1 otherwise (D415); crossings at different levels not spaced against each other (D440).
Dropped: mirrored-crossing rule sharing one angle (D376 amends SPEC-shape §2.1).

## What heading does a node's leg take?
Now: a split/merge leg that is not straight starts one eighth off the trunk (D404), topology nodes only; copy and row splits keep the parent's heading (D404); a prio cusp joiner is forced to turn flush at the node (D451); on a diagonal trunk two axis legs get a 0 or 1 half-unit stub (D380); node-to-node tracks with no event travel 0 or at least node_leg (D424); trident is 0-distance between two splits (D400).
Dropped: node gadgets (D398, D400); d as free binary (D404).

## Is the trident allowed against D113?
Now: yes, the trident is the split itself; each leg onto a bridge still gets a signal-legal tile with a signal before the head (D110, D399, D400).
Dropped: two adjacent splits with no tile (D110: wrong).

# Slots and the drawing program

## What does Slots decide?
Now: every track's sideways position, every piece length and every crosser's place on its deck, on Shape's headings (D379), keep-apart lazy in stage B (D447; see Keep-apart); widths come from tracks only, objective counts steps, width last tie-break (D350, D352); rows written over contracted lines (D432); first target Slots v2 starts as the track-level drawing program, judged by the domain suite (D441).
Dropped: Lengths layer (D362 -> D379); width weight / length-weighted widths (D352).

## How does Slots reach the tile grid?
Now: grid bit per point, parities by GF(2) elimination, step rows linear (D382); whole-tile grid integrality is Slots' problem, not E5's (D423); the pipeline's integer final writes grid rows with suite-cleared rules on (D435).
Dropped: whole-tile integer final in the E5 pipeline (D422 -> D423); big-M parity rows (D382); tile grid owned by geometry pieces (D334, SSS-superseded).

## Which zero-length pieces exist in Slots?
Now: none, except a node-to-node track with no event (a trident) (D432, D433); an in-port line is length 0 or ordinary (D434, `port_zero` off until the suite judges it).
Dropped: zero contraction and its skip rules (D432; `strict_zero` retired D431).
Open?: D434 (default off, awaiting suite).

## How are the Slots v2 stages organised?
Now: stage A picks S-bend offers by the length objective with no keep-apart rows; stage B contracted, lazy keep-apart, minimise movement; flank turn rows in stage A; length only in a warm-started final polish (D445, D446, D447, D448); tight coord_max, integer not LP (D446).
Dropped: stage A lazy loop and LP stage (D446 -> D447); one-axis-at-a-time compaction (D445, parked); movement-free zero objective (D448).
Open?: D447/D448 after D449 (D449 says it supersedes them as the path for S-bends).

## What is the layout objective (compactness)?
Now: area inside the outline, approximated by linearisation, track length tie-break (D315, D329, D336); score selectable later: most compact, cheapest, thinnest, most square (D329); a step lands on architecture and scorecard, not per-step compaction (D357, D358); working drawing for every hub before V-level compactness (D427); coop builds are a benchmark never a ceiling (D153).
Dropped: box term and "tuck" cost (D315); box semi-perimeter plus track (D336).
Open?: D354 (linear enclosed-empty-space measure, probes did not land one).

## Where do port fans and row changes sit?
Now: in-bound fan as late as possible next to its node, out-bound close-up as early as possible; a lane runs on its row until the feature that needs the other row (D212, D215, D313, D378); decided in projection, not in the seed (D212); S-side branch lanes fan by a vertical S-bend, west half west (D230).
Dropped: slide pass in the seed (D212, FN1 dropped unrun).

## How is a prio row represented in Slots?
Now: a real line of its lane's bundle (D349), exactly its coverage long (D384), exactly 1 lane from its lane for now, free between joins is target (D377 Q4), offset curve with exact opening/closing steps (D421); neighbouring lines bend around it (D349).
Dropped: prio at least coverage long (D158 -> D384); prio pushed aside for its own row (D349).

## Which solver, and which solvers are measured only?
Now: HiGHS behind one wrapper (D237); CP-SAT offline benchmark only, never linked (Apache-2.0 vs GPLv2) (D418, D450); MIT CP solvers (Chuffed, Gecode) benchmarked offline, nothing enters the build (D450).
Open?: D450 outcome not recorded in the rows.

## What does Slots hand to the back end?
Now: one typed hand-off `PlanarDrawnHub` (tracks, nodes, ports, structures, routes); Heights, Signals and rig read only it (D452).
Dropped: reading v1 route programs or solver internals (D452).

# Keep-apart

## How are tracks kept apart in Slots?
Now: pairwise keep-apart rows per face, added lazily in stage B only (D444 trial, adopted D447, engineer's pick); sides fixed by rule in bundle faces (corridor faces rigid: one lane for beside tracks and sibling lanes, at least one lane elsewhere, D426, D421); every pair and skip rule must be exact against the domain (D428, D436, D456).
Dropped: all rows before the solve (D381 -> D444/D447: size, T2 never converged with all pairs); ladders from a rough solve (D339 -> D381); convex cells (D385, D386 -> retired D431; D444: cells fix the half-plane, not the axis carrying the spacing); one-axis-at-a-time compaction (named in D444 -> parked D445: with 45-degree pieces fixing one axis freezes the diagonals); pairwise directions read from earlier drawings (D418: MEASUREMENT ONLY D431); K-E5 hybrid and monotone-walk probes (D416, D417: retired D431); cross-face keep-apart rows (2026-11-24, withdrawn before D456: per-face simplicity + orientation already implies planarity, the gaps were skips and representation).

## Must the program represent only legal layouts?
Now: yes, the feasible set is exactly the domain-legal drawings; a stage B Infeasible means no legal drawing for its S-bend set, never a bug (D456, D428, D436); node-leg lead rule only for a leg stepping toward its sibling (D429, D430).
Dropped: extra rows that only paper over a too-generous skip rule (D456); broad node-lead form (D430 narrows D429).

# S-bends

## Who decides S-bends?
Now: a bend pre-program decides which S-bends exist, direction and size in lanes, from the planar order, port pins, copies and rows, with no lengths; Slots sizes a fixed set, some steps may stay optional (D449).
Dropped: optional bend pieces in the length solve (D340 -> D350: slot layer owns sideways position); Shape placing steps by choice (D395: retired D431); Slots choosing which offers are used (D442 -> D449); Slots choosing per D447/D448 turn rows alone (D449 supersedes).
Open?: D449 (pre-program only just ruled, nothing built in the rows).

## Which stretches carry flex points?
Now: flex C, on every one-piece free stretch and the end pieces of every multi-piece free stretch, both bend pairs each side within 3 eighths, Slots sizes >= 0 (D397, D402, D420); lattice clause only between crossings of one structure (D443); on the suite's retest list (D431(2)); S-bends between crossings of different structures allowed (D443).
Dropped: Shape places a step by choice (D395 -> D397); D402-only eligibility (D420); flex on every piece (D413, NOT ADOPTED, retired D431).

## How do S-bends meet curve spacing (CL)?
Now: CL is derived from the train (D67), a diagonal piece counts half a tile (D36); built-in curve handling for optional flex: D422/D425 measurement only (D431); curve rows for flex inserts not enforced by a continuous rule yet (D383).
Open?: D383 (curve oracle counts breaches until a continuous curve rule exists).

## Where do sideways steps on spines come from?
Now: by rule on every free one-piece stretch, compactness decides where they appear (D397); legs of one node are exempt from the single-spine rule (D403, V builds it); S-bends offered k = 3 alternating each way per place (D442, parameter).
Dropped: Shape-chosen steps (D395 -> D397).

# Heights and terrain

## How are heights solved, and what fails a placement?
Now: height is a graph axis like rows; only hard facts fail a solve (houses kept, water, map edge, unpurchasable), heights are costs, terraform allowed by default (D218); a Heights layer after Slots decides levels and terraform on the fixed footprint, always feasible with terraform (D365).
Dropped: single hub plane (D180 retires D177); lazy terrain loop on the hover (D365).

## What is the ramp rule?
Now: z(t + tl) - z(t) <= k_max levels along every route, zero speed loss at line speed, requires realistic acceleration (D219); an axis piece along an incline is a ramp only (D152b).
Dropped: level-at-z_max reading of along-incline pieces (D152b: old router left as is).

## How does terrain enter the hover?
Now: first frame within 100 ms is exactly what gets built; Heights runs on cached seeds at the cursor and nearby offsets, shows the cheapest, picks kind within Structure's allowed set; terrain is never an input to Structure (D370, D366).
Dropped: terrain-repair loop and refinement while the cursor rests (D370 supersedes D78, D96, D170, D170a, D190, D197, D213's repair loop); obstacle repair inside footprint (D195 -> D370).

# Signals and prios

## Which signal types and spacing?
Now: junction signal type is its own setting, default one-way block (D20, D68); prios use pre-signals entry/combo/exit, one segment per ML block (D74); signal distance follows drag-signalling semantics, walked upstream from the downstream fixed signal, minimise gaps (D97).
Dropped: path-signal junction default (D20); path-prio fallback only if in-game checks fail (D74); even-spread signal pass (D97).
Open?: fixed-distance mode deferred (D97).

## Where must signals sit?
Now: a signal facing along every path before and after each bridge (D82); a signal-legal piece with a signal between a split and the next bridge head, and a merge after a far head (D113, D399); one signal may satisfy several rules, no back-to-back signals (D117); a signal right after every line-choice split, provisional until the LL_RR rig (D192).
Dropped: min length split to bridge head after a line-choice split (D192 relaxes D113/D176 interplay).

## How are waiting bays sized?
Now: whole-train bays: first legal signal piece at least 2 x TL units back from the previous boundary, never on a bridge or split, at least one bay (D83, D88); more bays by throughput, L_R bays matter less (D71, D176); multi-line legs after a line-choice split are whole bays or carry no signal (D176); traffic joining a SL never blocks the ML and no layout deadlocks (D71).
Dropped: exact k x TL positions (D83 -> D88).

## Which merges carry a prio, and how is it chosen?
Now: the balancing pattern decides which track has priority (D282); explicit lane paths with a prio flag (D301); `prio <merge out line>: <holding leg>` notation (D390(4)); one prio element on the track the pattern gives priority (D287).
Dropped: main-line merges always carry one (D159 -> D282, D298, D301); prio chosen by search (D159: search would always drop it).

## How long is a prio?
Now: exactly its coverage long (D384); coverage from game rules alone: joiner from rest, main train follows, M = largest lead in tiles rounded up plus TL plus SD (D233); one length for all prios, the slider sets it, min one TL (D139, D227).
Dropped: 1 to 2 TL cap (D29 -> D34 -> D233); SizeJunctionPrio sweep (D233); "math, warn" beyond 2 TL stays with the tooltip (D34).

## What shape may a prio take?
Now: backward-facing 45-degree trailing pair joins, first join on the merge tile (D75, D79); the coverage may bend, S-bend, run diagonal, leave the ML between joins and contain a main-line bridge (D111, D342, D347, D353); a prio join may start from a diagonal ML (D374); the prio itself stays track not special (D384).
Dropped: ladder rung (D25 -> D75); spurious tail past the merge (D59 -> D79); straight-prio-row requirement (D111).

## Does a prio hold under the game's real pre-signal logic?
Now: prio reach (G9b) ports OpenTTD segment logic and is the proof (D107); no split (any ML diverge/join, incl. lane split/merge) inside coverage plus one segment (D108), amended for a main-line bridge in coverage (D347).
Dropped: vacuous G9 pass on plain-less segments (D107).

## Is a prio row ever drivable, and how does Signals draw it?
Now: never drivable; Signals draws each join, both ends, as a chevron pointing against travel, from the hub's beside run only (D455); signal pass builds the row, adds signals, and `CheckPrio`/`CheckPrioReach` judge every leaf (D454 as amended).
Dropped: row as drawn with end joins (D454 -> D455).

# Domain suite and testing

## How are geometry rules found and changed?
Now: a domain predicate of legal track geometry independent of the solver; an encoding suite of local configurations against it, every disagreement named too strict or too loose; a rule lands only with the suite clean (D428); suite is exact on X1, X2, M1, K1, L1, Y1, P1, BR1 (D435).
Dropped: tuning rules on hubs (D428); `strict_zero`, strict_span, crossing_flanks probes (D428 off by default, D431 retires strict_zero).

## What does the suite assert?
Now: a behaviour of Slots through one seam (`RowsJudge`), two-sided pinned tests, exactness asserted in the gate, fixtures isolate cases, realism not required; implementation tests kept apart (D436).

## Which rules are defaults after the suite?
Now: `node_lead`, `end_node`, `cross_piece` and `final_grid` on by default (D435); node gaps on (D424); not defaults: `port_zero` (D434), the F6 node-turn rule (D435); flex C on the retest list (D431).

## How is PSG 197 used?
Now: pinned at the post-Structure level to prove Shape onward and expose model gaps (D389); gaps are hand-off gaps, differences from Structure's rules are information (D393, D394); D24 exception, doubled crosser left out (D391, D392); extractor ignores unknown constructs (D411); next corpus hubs 186, 227, 248, 188 (D401).
Dropped: litmus-test status of generated 197 (D389: generalising Structure's defaults is later work E7).

## What does the decision audit retire?
Now: the E5 drawing pipeline is a measurement tool (D431); retired D385, D386, D395, D398, D405 to D409, D413, D416, D417; MEASUREMENT ONLY D418, D422, D425 (D431); strict_zero retired (D431).

# Process

## How are tests run?
Now: `jt` runs each test case in its own process; budgets gate 30 s, sweep 60 s, identity 45 s (D264); a step runs the tags of the code it touches, the wide gate once per merge by the orchestrator (D453).
Dropped: ptest.sh and [slow] tags (D140 -> D264/D262); 300 s identity budget (D251 -> D264); wide gate per step (D453).

## When are agents used, and how is work driven?
Now: test-first, outside-in (D4, D368); small fully-specified fixes inline, agents for substantive work, Sonnet for small fixes needing new reads (D52); ccache, parallel agents on hand-made worktrees (D140, SSS-superseded in part); engineer rules once, recorded rulings are not re-asked (header of DECISIONS).

## How are reversible defaults and the CLAUDE.md files handled?
Now: orchestrator defaults are recorded reversible with toggle candidates (D227, D372, D430, D435); corpus authority: if V builds it, allow it (D403, D412, D414, D415); junction CLAUDE.md holds decided facts only, updated in every promoting commit, standing alone with no references to decisions (D225, D232).
Dropped: an Open section in CLAUDE.md (D225); references to DECISIONS or briefs from CLAUDE.md (D232).
