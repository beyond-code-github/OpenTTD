# Audit of DECISIONS.md: which rows are dead, partly dead or in conflict

Read-only audit; DECISIONS.md was not edited. Rule applied: a later decision wins over an earlier one. Rows D1 to D349 (363 rows including letter suffixes) were read once; supersession was confirmed by grep on the cited rows.

What the architecture calls rely on:
- STATUS "RESUME HERE (2026-11-15 night)": "start the new model" (planar line/route model, D305-D309); "Old-pipeline experiment dropped"; "client setting switches the tool".
- D308 (2026-11-15): "replace the current model with the planarized line/route graph"; D310 (2026-11-16) replaces DESIGN-planar section 2a; D326 (2026-11-16): per-hub compositions are "fixtures ... never the product"; D330 (2026-11-17): "Atomic primitives only".
- `.claude/ng1/src/junction/planar/CLAUDE.md`: "The planar model stands beside the old pipeline and shares none of its graph, order, structure, programs or placement concepts."
- `.claude/ng1/src/junction/legacy/CLAUDE.md`: generator code is "superseded by the graph pipeline and is not extended". The root CLAUDE.md still documents the old graph pipeline as present-tense facts, and `line/`, `terrain/`, `station/`, `signals/` stay live.
- Caveat: the old graph/placement pipeline code is still in the tree and selectable by client setting. "Obsolete" in Table 4 means no new work is planned on it, not that the code is gone. Rows that state a coop or game rule (D24, D80, D82, D83, D107, D113, D117 and similar) are kept live even when their implementing mechanism is obsolete.
- Early rows (D1 to about D60) carry no dates; dating there is by id order.

## Table 1: superseded (the whole ruling is dead)

| old id | superseded by | how | evidence |
|---|---|---|---|
| D28 | D70 | explicit | D28 "far side of the mainline track from the other direction" / D70 "It can go wherever space and cost permits." |
| D30 | D76 | explicit | D30 "Fewest tiles, then cost." / D76 "Score: cost first (from the spec's unit prices)." |
| D76 | D114 | implicit | D76 "Score: cost first (from the spec's unit prices)" / D114 "compares first the count of tiles the junction occupies" |
| D37 | D66 | explicit | D37 "a left/right option in the build dialogue" / D66 "no dialogue option" |
| D59 | D79 | explicit | D59 "carries on past the merge point by one short segment" / D79 "The prio no longer extends past the merge" |
| D33 | D74 | implicit | D33 "pre-signal prio for block, path-signal prio" / D74 "Pre-signal prio always (Recommended)" |
| D73 | D74 | implicit | D73 "Research path prios first." / D74 "Pre-signal prio always (Recommended)" |
| D99 | D99a, then D108 | explicit | D99 "is exempt from the "one join per covered block" rule" / D108 "D99/D99a become unreachable" |
| D99a | D108 | explicit | D99a "a covered block is exempt only if it has no tile where a join can go" / D108 "D99/D99a become unreachable" |
| D102 | D233 | implicit | D102 "The joiner starts from rest at the merge signal" / D233 "is `JunctionPrioLength`: the joiner starts from rest at the merge" |
| D137a | D233 | implicit | D137a "`SizeJunctionPrio(spec)` runs on flat ground once per train and spec" / D233 "M, its top and start, is `JunctionPrioLength`" (legacy/CLAUDE.md: "`SizeJunctionPrio` has no live caller") |
| D152a | D152b | explicit | D152a "the router offers both as successors and the cost decides" / D152b "replaces D152a's choice" |
| D163 | D164 | explicit | D163 "REVOKED by D164." / D164 "D163 revoked" |
| D181, D181a | D182 | explicit | id cells "(SUPERSEDED by D182)" / D182 "Supersedes D181/D181a" |
| D184 | D201 | implicit | D184 "read with ExtractJunctionChains (N0b)" / D201 "not traced from a constructed flat JunctionLayout" |
| D185 | D201, D202 | implicit | D185 "make today's flat generator build on flat land for every spec" / D201 "the tracer and ConstructJunctionTMap's role in the node path retire" |
| D243, D246, D249 | D254 | explicit | D254 "D243, D246 and D249 (retired)" |
| D247 | D251 | explicit | D247 "150 s Release (was 90 s)." / D251 "was 150 s, D247" |
| D262 | D264 | explicit | D262 "Before the commit: `jt gate` once." / D264 "Revises D262" |
| D292, D293, D294, D295 | D296 | explicit | D292 "Loose: the outer Corner is the inner one shifted" / D296 "D292-D295 are NOT confirmed: treat them as open." |
| D304 | D305 | explicit | D304 title "Non-crossing band edges keep their S order on every row" / D305 "Supersedes D304 (held, not built)." |
| D347a | D347b | explicit | D347a "What "bridge the prio line" off means" / D347b "Supersedes D347a's global toggle reading." |

## Table 2: partly superseded or amended

| id | which part is dead | by |
|---|---|---|
| D13 | "Transparent real sprites"; the red tint for invalid stays | D44, D57 |
| D16 | "C++ generators (one per family, parameterised e.g. by TL)"; per-family generation | D202 ("Generic, not the T"), D326 |
| D18 | CL as a set parameter (the definition of CL stands) | D67 |
| D21 | default signal distance of 2 and the leg-signalling default | D65, D97 |
| D27 | "Mainline merges only." | D159, then D282, D298, D301 |
| D29 | "somewhere between 1 and 2 TL" cap | D34, D102, D233 |
| D36 | the two-thirds piece-time figure | D98 (a) |
| D45 | fastest-buildable-engine rule, when a train is picked | D141 |
| D62 | the 200 ms per generation target (the 64x64 limit is untouched) | D151 (a), D253, D297 |
| D64 | the CL setting | D67 |
| D80, D188 | "Always doubled" for every main-line bridge; now a policy on a layout choice, and doubling follows flow share | D309, D331, D332 |
| D85 | lane A/B geometry (second lane on the outer side) | D100 |
| D95 | "terraforming is a priced fallback" as the default stance | D190, D218 (terraform allowed by default) |
| D96 | the "library of clean flat candidates" instant tier | D242, D253 (seed per preset, cached, background) |
| D100 | the symmetric style as the chosen style (equal-cost balance rule stays) | D105, D151 (b) |
| D103 | `bridge_margin` sweep parameter (the throughput point stays) | D228a |
| D106 | "no entry balancing and no entry check" | D120 (explicit: "wrong for the engineer's standard") |
| D107 | the thinning rule, now only a preference | D116 (b) |
| D111 | prio runs "parallel to the ML ... offset to the side" through curves | D342 |
| D112 | the drag interaction | D133 (click waypoints) |
| D139 | M defined as `SizeJunctionPrio` (slider itself stays) | D233 |
| D140 | `scratchpad/ptest.sh` runner and `[slow]` tags | D264 (`jt`), D262 |
| D144 | Q2 (symmetric style), Q3 (library); S2d solver semantics | D151 (b), D253, D183, D237 |
| D152 | O4 "junction generator is rebuilt on" the line router | D170, D182, D201 |
| D157 | the anchor layer | D160 (explicit) |
| D159 | "Main-line merges always have one"; throughput-estimate rule | D282, D298, D301 (pattern and prio list decide) |
| D166 | part (c), starting conflicts at a bridge's price | D167 (explicit) |
| D168 | masks and emergent splits and merges | D170 (explicit) |
| D170 | negotiation as the projection technique | D182, D213, D216 |
| D170a | "terraforming last" ordering | D190 (explicit), D218 |
| D183 | "per-spec precompile"; monotone propagation as the seed's solver | D184, D235, D237 |
| D189 | "doubling (from train length and throughput)" derived by rule; the ports-and-movements topology form | D309, D305, D306, D308 |
| D207 | "the structure (row stack, node placement D204/D205, crossing choice, doubling form, stub) is fixed by rule" (crossing form, doubling form, variants) | D260, D284, D318 |
| D209 | "a bridge's crossing set is capped" | D228 (explicit), D228a |
| D217 | flat-order reading of order | D305 (explicit) |
| D225 | CLAUDE.md may cite decision ids | D232 (explicit "Amends D225") |
| D235 | (2) joint-solve seed and (3) objective "flat build cost" in the old placement | D254, D315 |
| D236, D240, D250 | the recording/rounds/joint-loop mechanism | D254 (explicit "Revises D236 ..., D240 ..., D250") |
| D248 | choice default "before" | D254 ("D248 re-derived") |
| D251 | the 300 s sweep budget | D264 ("D251's 300 s retired") |
| D256 | the square-form rule for every single-bridge band crossing | D260 (explicit) |
| D298 | the movement list as the way to say which paths hold prio | D301 (explicit) |
| D299 | "doubling by prio flag" criterion | D331 ("replaces doubling by prio flag") |
| D302 | route-set test for a rejoin | D309 ("replaces D302's route-set test in the new model") |
| D318 | fray forms as model variants | D320, D321 (variants "are discovery, not the model") |
| D325 | the fixed-lane choice at three lanes | D326 ("the user's option") |
| D335 | "no jog switches" | D338 (explicit) |
| D343 | "A topology fully fixes the hub's planar order" | D344 (explicit "Amends D343") |
| D347 | the "bridge the prio line" user toggle | D347b |

## Table 3: conflicts "later wins" may not settle (to the engineer)

| ids | question |
|---|---|
| D116 (a) vs D342 | D116 says "The first join need not sit on the merge tile"; D342 says "The first join stays on the merge tile (D79/D81)". Which holds for bent prios? |
| D108 vs D347 | D108 forbids any split or lane merge in coverage; D347 lets coverage contain "a doubled bridge". Is the doubled bridge's copy split and rejoin exempt from D108? |
| D193 vs D345 (and D211) | D193: checker is "a test oracle only, never an acceptance step on the hover path". D345 places prio joins and signals by search "judged by the checker"; root CLAUDE.md says the search keeps it in acceptance. Does D345 override D193 for the signal pass? |
| D119 vs D193, D308 | D119: constraints "should really come from the simulation rather than hard rules". Later rows encode coop rules as hard constraints. Is D119's direction still in force? |
| D254 vs D276/D277 and root CLAUDE.md | D254 deletes the joint loop and records "Arrangement first, one static MILP, no loop"; D276 stops the G4 queue; root CLAUDE.md still describes the joint loop on the bridge-cut path. Is D254 cancelled, or just not yet executed on the old pipeline? |
| D281 vs D305 | D281: "The single joint search is not the core for any". The planar layout is one integer program. Does D281 apply to the planar model? |
| D321, D338 vs D340 | D321/D338 make bends optional pieces with a 0/1 existence; D340: "Bends are not optional pieces in the length solve". planar CLAUDE.md still lists optional segments. Is the optional-bend model retired now or at the end of D340's steps? |
| D204/D205 vs D283/D285 | D204 (split by direction, then lane) and D205 (merge by the S port) vs the PSG 227 pattern first (D283). Are D204/D205 the "full any" pattern of D284, or replaced? |
| D115a vs D206 | D206 records the stub length as OPEN against D115a's two tiles. Does D115a stand until D206 is closed? |
| D114 vs D315 (and D235 (3)) | D114 ranks by tile count then cost; D315 ranks variants by outline area. Does D315 replace D114 only for planar, or for the old pipeline too? |
| D171 vs D189, D307 | D171: the map says "crosses", not above or below. D307 fixes "who is above at each crossing". Which governs the topology's crossing data? |
| D196-D200, D214, D217 | The graph-edit repair search (best-first, A*) vs planar CLAUDE.md, which describes a lazy terrain re-solve. Does the edit search apply to the planar path? |
| D263 vs D297 | D263 accepts a 15.6 s seed walk "for now"; D297: each seed "solves in under 5 s". Is D263's exception void? |
| D89 | Roadmap order (1b finish, D78, endpoint solver, 1c) vs the STATUS queue and the planar pivot. Is D89 still the order? |
| D291 vs D306, D333 | D291: a bundle track is "copy k of the existing copy machinery". D306: "lines ARE tracks"; D333: a doubled lane counts as two lanes. D296 did not list D291 among the open rows. Is D291 still in force? |
| D158 vs D349 | D158: a prio is "a two-track routed token". D349: each prio row is a real line in the channel. Does D349 replace D158's token model? |

## Table 4: obsolete by architecture

| family (ids) | reason | evidence |
|---|---|---|
| Fixed-template generator and its sweep: D87, D90, D91, D92, D103 (sweep part), D105, D109, D110, D117b, D146, D148, D148a | The template generator, skeleton windows and sweep were replaced by the topology-to-graph pipeline, then by planar. Their coop rules live on as checker rules, not as these mechanisms. | legacy/CLAUDE.md "superseded by the graph pipeline and is not extended"; D201 "then the tracer and ConstructJunctionTMap's role in the node path retire"; D86 "the generator designs" |
| Router negotiation and flat-T projection: D160, D161, D162, D165, D173, D177, D178, D179, D179a | Routing movements with emergent splits and merges, and projecting from a flat T, were replaced by nodes placed first (D182), a graph-only projection (D213) and the planar solver. | D182 "nodes are placed on the terrain first, then every edge is routed between fixed node footprints"; D213 "Projection works on the graph, never on a rendered layout" |
| Old placement, joint loop and G4 static-family steps: D234, D238, D239, D241, D252, D255, D258, D259, D260, D261, D265, D266, D267, D268, D269, D271, D272, D273, D274, D275 | Rows, bands, item rows and the joint loop belong to the pre-planar placement. The G4 plan was stopped, and planar "shares none" of these concepts. | D276/D277 "G4's static-family plan stays stopped"; D305 "rows and bands are a shape-stage strategy, not a graph concern"; planar CLAUDE.md "shares none of its graph, order, structure, programs or placement concepts"; STATUS 2026-11-15 "Old-pipeline experiment dropped" |
| Block hubs: D289, D290 | Exit and join block templates were replaced by components inside bundles and atomic primitives. | D305 "Blocks become named groups of these nodes"; D310 "Components run inside bundles"; D330 "No composite blocks" |
| Hand compositions and probe forms: D317, D323, D324 | Per-hub compositions are fixtures only; the generic mapped composer replaces them. | D326 "fixtures to measure a generic builder against, never the product"; D330 "retires with the hand compositions" |

## Counts

Each row is counted once, in the first class that applies: superseded, then partly superseded, then conflicting, then obsolete.

Total rows 363. Live 198; superseded 28; partly superseded 52; conflicting 40; obsolete by architecture 45. (Table 3 range D196-D200 counts D196, D197, D198, D199, D200.)
