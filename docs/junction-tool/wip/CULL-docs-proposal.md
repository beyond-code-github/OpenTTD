# CULL-docs proposal (2026-11-18, read-only classification)

Scope: 916 files: top-level `*.md` and `*.patch` (173), `briefs/` (414), `tools/`, `layouts/`, `wip/` incl. subfolders. Nothing was deleted or edited.

## Read this first

- **STATUS.md and the roadmap changed while I worked.** Commit ba936db880 (2026-11-18) rewrote STATUS.md to a 45-line current-state page and turned `wip/ROADMAP-draft.md` into top-level `ROADMAP.md`. I therefore took references from: current STATUS.md, ROADMAP.md, and the pre-rewrite STATUS first 40 lines (`git show d18117a214:docs/junction-tool/STATUS.md`). A "pre-rewrite STATUS" reference is weak: the current STATUS no longer names those files. Files cited only that way and in nothing else are classed REMOVE-done unless a DECISIONS row cites them as evidence (then KEEP).
- **43 REMOVE files are untracked in git** (they have no history; deleting them is irreversible). They are marked with a `U` after the date. Most are under `wip/`: see the `U` marks.
- **Dangling citations after removal.** Many kept files cite files proposed for removal: DECISIONS.md rows (about 70 cite a removed file), TESTS.md (RECON-stretch), RECON-foundations (DESIGN-N3, DESIGN-nodes), RECON-mfb-core (RECON-bundle-copies), REPRESENT-psg227 (DESIGN-modules), ROADMAP.md legacy list (DESIGN-G1, DESIGN-width, DESIGN-modules, DESIGN-blocks). The list is in the last section. The `referenced by` column shows what I found; DECISIONS cites come from the current DECISIONS.md and are noted `DECISIONS Dnnn`.
- `src/junction/**/CLAUDE.md` were not scanned for duplicated facts, only spot-checked (curve-spacing rule is there).
- Date is the last commit date of the file (`git log -1`); `U` = untracked, date is mtime.
- Judgement calls: briefs of the parked painter workstream (LP*) are UNSURE, not KEEP; reproduction patches of kept probe reports are UNSURE (one decision: keep doc and patch together or remove both); 2026-11-18 BRIEF-* files are REMOVE-done except BRIEF-spike-slots (named KEEP) and BRIEF-deadline (cited by ROADMAP).

## Counts

| class | files |
|---|---|
| KEEP-live | 129 |
| REMOVE-legacy | 486 |
| REMOVE-done | 217 |
| UNSURE | 84 |
| total | 916 |

| area | KEEP-live | REMOVE-legacy | REMOVE-done | UNSURE |
|---|---|---|---|---|
| top-level md/patch | 45 | 96 | 14 | 18 |
| briefs | 5 | 306 | 94 | 9 |
| tools | 5 | 7 | 0 | 0 |
| layouts | 4 | 0 | 0 | 0 |
| wip | 70 | 77 | 109 | 57 |

## KEEP-live (129)

| file | date | reason | referenced by |
|---|---|---|---|
| ANATOMY-psg197-bbh.md | 2026-11-14 | corpus anatomy of a real coop hub |  |
| ANATOMY-psg227-bbh.md | 2026-11-14 | corpus anatomy of a real coop hub | ORACLE-psg227.md, REPRESENT-psg227.md |
| DECISIONS.md | 2026-11-18 | core live doc | RULES-coop.md, DESIGN-planar.md, AUDIT-decisions.md, BRIEF-spike-slots.md +3; DECISIONS ?/D232 |
| DESIGN-components.md | 2026-11-17 | design DESIGN-tracks builds on, still true | DECISIONS D310/D311/D312 |
| DESIGN-pieces.md | 2026-11-17 | design DESIGN-tracks builds on, still true | DESIGN-tracks.md, ROADMAP.md; DECISIONS D335/D338 |
| DESIGN-planar.md | 2026-11-17 | design DESIGN-tracks builds on, still true | AUDIT-decisions.md, DESIGN-components.md, ROADMAP.md; DECISIONS D310 |
| DESIGN-tracks.md | 2026-11-18 | current design (solve on tracks) | BRIEF-spike-slots.md, ROADMAP.md, REVIEW-sideways-modelling.md, RECON-prio-under.md +3; DECISIONS D321/D338/D339 |
| IMAGE-INDEX.md | 2026-11-07 | index of coop/game reference images | RULES-stations.md; DECISIONS D192 |
| ORACLE-psg227.md | 2026-11-14 | rule checker run on the real PSG 227 hub (coop corpus) |  |
| PLAN-stations.md | 2026-11-08 | station / line painter workstream doc (parked, live) | DECISIONS D106 |
| RECON-1c.md | 2026-11-08 | facts about the game (rail on slopes, train physics, pathfinder, path signals, prio merge) | DECISIONS D98 |
| RECON-corpus-anatomy.md | 2026-11-14 | coop hub corpus recon | DECISIONS D277 |
| RECON-corpus-census.md | 2026-11-14 | coop hub corpus recon | RECON-corpus-census2.md |
| RECON-corpus-census2.md | 2026-11-14 | coop hub corpus recon | RECON-corpus-anatomy.md |
| RECON-corpus-crossings.md | 2026-11-14 | coop hub corpus recon | RECON-corpus-sbend-crossings.md |
| RECON-corpus-graph1.md | 2026-11-14 | coop hub corpus recon |  |
| RECON-corpus-graph1b.md | 2026-11-14 | coop hub corpus recon |  |
| RECON-corpus-ll2.md | 2026-11-14 | coop hub corpus recon |  |
| RECON-corpus-oneway.md | 2026-11-14 | coop hub corpus recon | RECON-corpus-ll2.md |
| RECON-corpus-sbend-crossings.md | 2026-11-14 | coop hub corpus recon |  |
| RECON-exit-merge.md | 2026-11-12 | coop hub practice survey (bridges, merges, mfb core) | DECISIONS D210 |
| RECON-foundations.md | 2026-11-11 | facts about the game (rail on slopes, train physics, pathfinder, path signals, prio merge) |  |
| RECON-hub-bridges.md | 2026-11-12 | coop hub practice survey (bridges, merges, mfb core) | DECISIONS D209 |
| RECON-junctionary-crossings.md | 2026-11-13 | Junctionary / coop wiki research (coop practice) |  |
| RECON-junctionary.md | 2026-11-07 | Junctionary / coop wiki research (coop practice) |  |
| RECON-linepainter.md | 2026-11-08 | station / line painter workstream doc (parked, live) | DECISIONS D133/D150 |
| RECON-mfb-core.md | 2026-11-15 | coop hub practice survey (bridges, merges, mfb core) | DECISIONS D302 |
| RECON-path-prio.md | 2026-11-07 | facts about the game (rail on slopes, train physics, pathfinder, path signals, prio merge) | DECISIONS D74 |
| RECON-pbs.md | 2026-11-08 | facts about the game (rail on slopes, train physics, pathfinder, path signals, prio merge) |  |
| RECON-psg-hubs.md | 2026-11-13 | PSG save corpus manifest and hub census |  |
| RECON-psg-saves.md | 2026-11-13 | PSG save corpus manifest and hub census |  |
| RECON-slope-physics.md | 2026-11-12 | facts about the game (rail on slopes, train physics, pathfinder, path signals, prio merge) | DECISIONS D219 |
| RECON-tool-icons.md | 2026-11-13 | in-game UI icon facts for the new tools |  |
| RECON-yapf-compute.md | 2026-11-13 | facts about the game (rail on slopes, train physics, pathfinder, path signals, prio merge) |  |
| RECON-yapf.md | 2026-11-08 | facts about the game (rail on slopes, train physics, pathfinder, path signals, prio merge) | DECISIONS D132 |
| REF-stations-archive.md | 2026-11-08 | station / line painter workstream doc (parked, live) | DECISIONS D127 |
| REF-stations-engineer.md | 2026-11-08 | station / line painter workstream doc (parked, live) | DECISIONS D121/D123/D124 |
| REF-stations-measured.md | 2026-11-08 | station / line painter workstream doc (parked, live) | REF-stations-engineer.md |
| REPRESENT-psg227.md | 2026-11-14 | what the model must represent of a real coop hub (named KEEP class) | DECISIONS D287 |
| ROADMAP.md | 2026-11-18 | core live doc | DESIGN-tracks.md, STATUS.md |
| RULES-coop.md | 2026-11-07 | coop rules reference | RECON-prio-bridge.md, RULES-hubs.md, RECON-prio-bridge-rule.md, RULES-stations.md; DECISIONS D35 |
| RULES-hubs.md | 2026-11-08 | coop rules reference | DECISIONS D126/D231 |
| RULES-stations.md | 2026-11-08 | coop rules reference | REF-stations-engineer.md, RULES-hubs.md, PLAN-stations.md; DECISIONS D123 |
| STATUS.md | 2026-11-18 | core live doc | AUDIT-decisions.md, ROADMAP.md, CULL-decisions-log.md, RECON-prio-under.md +1 |
| TESTS.md | 2026-11-17 | core live doc | RECON-prio-straight.md |
| briefs/ST-step1-rules.md | 2026-11-08 | station workstream brief (parked, live by instruction) |  |
| briefs/ST-step2-rules.md | 2026-11-08 | station workstream brief (parked, live by instruction) |  |
| briefs/ST-step3-generator.md | 2026-11-08 | station workstream brief (parked, live by instruction) |  |
| briefs/ST-step4-rig.md | 2026-11-08 | station workstream brief (parked, live by instruction) |  |
| briefs/ST-step5-window.md | 2026-11-08 | station workstream brief (parked, live by instruction) |  |
| layouts/reference-LR-TL4-doubled.design | 2026-11-08 | test fixture read by src/tests/junction/signals/junction_signal_pass.cpp | DECISIONS D88 |
| layouts/reference-LR-TL4-doubled.txt | 2026-11-08 | test fixture read by src/tests/junction/signals/junction_signal_pass.cpp |  |
| layouts/reference-LR-TL4.design | 2026-11-07 | test fixture read by src/tests/junction/signals/junction_signal_pass.cpp |  |
| layouts/reference-LR-TL4.txt | 2026-11-07 | test fixture read by src/tests/junction/signals/junction_signal_pass.cpp |  |
| tools/fetch_abr.py | 2026-11-07 | tool code (corpus fetch / index / signal sim) |  |
| tools/fetch_coop_archive.py | 2026-11-07 | tool code (corpus fetch / index / signal sim) |  |
| tools/fetch_junctionary.py | 2026-11-07 | tool code (corpus fetch / index / signal sim) |  |
| tools/index_images.py | 2026-11-07 | tool code (corpus fetch / index / signal sim) |  |
| tools/sigsim.py | 2026-11-08 | tool code (corpus fetch / index / signal sim) | RECON-prio-bridge.md; DECISIONS D107 |
| wip/ANALYSIS-merge-steps.md | 2026-11-17 | cited by DESIGN-tracks, ROADMAP, DESIGN-pieces/components or DECISIONS (planar-era evidence) | DESIGN-tracks.md, REVIEW-sideways-modelling.md |
| wip/AUDIT-decisions.md | 2026-11-18 | today: decisions audit/cull (parallel agent) | DECISIONS ? |
| wip/BRIEF-deadline.md | 2026-11-18 | cited by DESIGN-tracks, ROADMAP, DESIGN-pieces/components or DECISIONS (planar-era evidence) | ROADMAP.md, pre-rewrite STATUS 1-40 |
| wip/BRIEF-spike-slots.md | 2026-11-18 | today: slot spike |  |
| wip/CHECK-rungorder.md | 2026-11-17 | cited by DESIGN-tracks, ROADMAP, DESIGN-pieces/components or DECISIONS (planar-era evidence) | DESIGN-tracks.md, pre-rewrite STATUS 1-40 |
| wip/CULL-decisions-log.md | 2026-11-18 | today: decisions audit/cull (parallel agent) |  |
| wip/DIAG-ladders.md | 2026-11-17 | cited by DESIGN-tracks, ROADMAP, DESIGN-pieces/components or DECISIONS (planar-era evidence) | DESIGN-tracks.md |
| wip/PROBE-anchor.md | 2026-11-18 | cited by DESIGN-tracks, ROADMAP, DESIGN-pieces/components or DECISIONS (planar-era evidence) | pre-rewrite STATUS 1-40; DECISIONS D348 |
| wip/PROBE-corridors.md | 2026-11-17 | cited by DESIGN-tracks, ROADMAP, DESIGN-pieces/components or DECISIONS (planar-era evidence) | DESIGN-tracks.md, pre-rewrite STATUS 1-40 |
| wip/PROBE-flowturns.md | 2026-11-18 | cited by DESIGN-tracks, ROADMAP, DESIGN-pieces/components or DECISIONS (planar-era evidence) | DESIGN-tracks.md, ROADMAP.md, pre-rewrite STATUS 1-40 |
| wip/PROBE-joiner-over.md | 2026-11-18 | cited by DESIGN-tracks, ROADMAP, DESIGN-pieces/components or DECISIONS (planar-era evidence) | DESIGN-tracks.md, ROADMAP.md, pre-rewrite STATUS 1-40 |
| wip/PROBE-ladders.md | 2026-11-17 | cited by DESIGN-tracks, ROADMAP, DESIGN-pieces/components or DECISIONS (planar-era evidence) | DESIGN-tracks.md, pre-rewrite STATUS 1-40 |
| wip/PROBE-ladders2.md | 2026-11-17 | cited by DESIGN-tracks, ROADMAP, DESIGN-pieces/components or DECISIONS (planar-era evidence) |  |
| wip/PROBE-ladders3.md | 2026-11-17 | cited by DESIGN-tracks, ROADMAP, DESIGN-pieces/components or DECISIONS (planar-era evidence) |  |
| wip/PROBE-ladders4.md | 2026-11-17 | cited by DESIGN-tracks, ROADMAP, DESIGN-pieces/components or DECISIONS (planar-era evidence) | pre-rewrite STATUS 1-40 |
| wip/PROBE-ladders5.md | 2026-11-18 | cited by DESIGN-tracks, ROADMAP, DESIGN-pieces/components or DECISIONS (planar-era evidence) | DESIGN-tracks.md, pre-rewrite STATUS 1-40 |
| wip/PROBE-noclash.md | 2026-11-18 | cited by DESIGN-tracks, ROADMAP, DESIGN-pieces/components or DECISIONS (planar-era evidence) | DESIGN-tracks.md, pre-rewrite STATUS 1-40 |
| wip/PROBE-prio-bridge.md | 2026-11-18 | today: prio work (named KEEP / reproduction patch) | pre-rewrite STATUS 1-40 |
| wip/PROBE-prio-bridge2.md | 2026-11-18 | today: prio work (named KEEP / reproduction patch) | DESIGN-tracks.md, ROADMAP.md, pre-rewrite STATUS 1-40 |
| wip/PROBE-prio-open.md | 2026-11-18 | today: prio work (named KEEP / reproduction patch) | ROADMAP.md, pre-rewrite STATUS 1-40 |
| wip/PROBE-prio-row.md | 2026-11-18 | today: prio work (named KEEP / reproduction patch) | ROADMAP.md, pre-rewrite STATUS 1-40 |
| wip/PROBE-prio-spread.md | 2026-11-18 | today: prio work (named KEEP / reproduction patch) | ROADMAP.md |
| wip/PROBE-prio-stagger.md | 2026-11-18 | today: prio work (named KEEP / reproduction patch) | RECON-prio-bridge.md, ROADMAP.md, RECON-prio-under.md, pre-rewrite STATUS 1-40; DECISIONS D347 |
| wip/RECON-prio-bridge-rule.md | 2026-11-18 | today: prio work (named KEEP / reproduction patch) | pre-rewrite STATUS 1-40; DECISIONS D347a |
| wip/RECON-prio-bridge.md | 2026-11-18 | today: prio work (named KEEP / reproduction patch) | RECON-prio-bridge-rule.md, pre-rewrite STATUS 1-40 |
| wip/RECON-prio-straight.md | 2026-11-18 | today: prio work (named KEEP / reproduction patch) | DESIGN-tracks.md, pre-rewrite STATUS 1-40 |
| wip/RECON-prio-under.md | 2026-11-18 | today: prio work (named KEEP / reproduction patch) | pre-rewrite STATUS 1-40 |
| wip/RECON-shape-seam.md | 2026-11-18 | cited by DESIGN-tracks, ROADMAP, DESIGN-pieces/components or DECISIONS (planar-era evidence) | DESIGN-tracks.md, ROADMAP.md |
| wip/RECON-ties-steps.md | 2026-11-18 | cited by DESIGN-tracks, ROADMAP, DESIGN-pieces/components or DECISIONS (planar-era evidence) | DESIGN-tracks.md, ROADMAP.md |
| wip/RESEARCH-bridge-length.md | 2026-11-18 | cited by DESIGN-tracks, ROADMAP, DESIGN-pieces/components or DECISIONS (planar-era evidence) | DESIGN-tracks.md, ROADMAP.md; DECISIONS D346 |
| wip/RESEARCH-prio-freedom.md | 2026-11-18 | cited by DESIGN-tracks, ROADMAP, DESIGN-pieces/components or DECISIONS (planar-era evidence) | RECON-prio-bridge.md, DESIGN-tracks.md, RECON-prio-bridge-rule.md |
| wip/REVIEW-channels.md | 2026-11-17 | cited by DESIGN-tracks, ROADMAP, DESIGN-pieces/components or DECISIONS (planar-era evidence) | DESIGN-pieces.md; DECISIONS D335 |
| wip/REVIEW-sideways-modelling.md | 2026-11-18 | today: slot spike | DESIGN-tracks.md, BRIEF-spike-slots.md, ROADMAP.md, STATUS.md |
| wip/SPIKE-LL3.md | 2026-11-18 | cited by DESIGN-tracks, ROADMAP, DESIGN-pieces/components or DECISIONS (planar-era evidence) | ROADMAP.md, pre-rewrite STATUS 1-40 |
| wip/SPIKE-freebend.md | 2026-11-17 | cited by DESIGN-tracks, ROADMAP, DESIGN-pieces/components or DECISIONS (planar-era evidence) | DESIGN-tracks.md, pre-rewrite STATUS 1-40 |
| wip/SPIKE-regions2.md | 2026-11-17 | cited by DESIGN-tracks, ROADMAP, DESIGN-pieces/components or DECISIONS (planar-era evidence) | ROADMAP.md |
| wip/SPIKE-shapeflow.md | 2026-11-18 | cited by DESIGN-tracks, ROADMAP, DESIGN-pieces/components or DECISIONS (planar-era evidence) | DESIGN-tracks.md, ROADMAP.md, pre-rewrite STATUS 1-40 |
| wip/SPIKE-shapeflow2.md | 2026-11-18 | cited by DESIGN-tracks, ROADMAP, DESIGN-pieces/components or DECISIONS (planar-era evidence) | pre-rewrite STATUS 1-40; DECISIONS D340 |
| wip/SPIKE-shapeflow3.md | 2026-11-18 | cited by DESIGN-tracks, ROADMAP, DESIGN-pieces/components or DECISIONS (planar-era evidence) | pre-rewrite STATUS 1-40 |
| wip/SPIKE-slots.md | 2026-11-18 | today: slot spike | DESIGN-tracks.md, BRIEF-spike-slots.md, ROADMAP.md, STATUS.md |
| wip/ascii-T3-o1t3f1s0-base.txt | 2026-11-18 | cited by PROBE-prio-row | PROBE-prio-row.md |
| wip/checker-d301-mainline.patch | 2026-11-16 | cited by DESIGN-tracks, ROADMAP, DESIGN-pieces/components or DECISIONS (planar-era evidence) | ROADMAP.md; DECISIONS D319 |
| wip/facedump.patch | 2026-11-17 | cited by DESIGN-tracks, ROADMAP, DESIGN-pieces/components or DECISIONS (planar-era evidence) | DESIGN-tracks.md |
| wip/ll2-tags.txt | 2026-11-18 | cited by DESIGN-tracks, ROADMAP, DESIGN-pieces/components or DECISIONS (planar-era evidence) | DESIGN-tracks.md, ROADMAP.md, pre-rewrite STATUS 1-40 |
| wip/localsearch/ls.py | 2026-11-18 | cited by DESIGN-tracks, ROADMAP, DESIGN-pieces/components or DECISIONS (planar-era evidence) | DESIGN-tracks.md, ROADMAP.md, pre-rewrite STATUS 1-40 |
| wip/planar-probe8.py | 2026-11-16 | cited by DESIGN-tracks, ROADMAP, DESIGN-pieces/components or DECISIONS (planar-era evidence) | DESIGN-components.md |
| wip/planar-probe9.py | 2026-11-16 | cited by DESIGN-tracks, ROADMAP, DESIGN-pieces/components or DECISIONS (planar-era evidence) |  |
| wip/probe-noclash-iis-LL2-o3t7f0s2.txt | 2026-11-18U | planar-era baseline/fixture/probe cited by ROADMAP, TESTS or DESIGN-components |  |
| wip/probe-noclash-iis-T2-o1t0f0s1.txt | 2026-11-18U | planar-era baseline/fixture/probe cited by ROADMAP, TESTS or DESIGN-components |  |
| wip/probe-prio-bridge.patch | 2026-11-18 | today: prio work (named KEEP / reproduction patch) | PROBE-prio-bridge.md |
| wip/probe-prio-bridge2.patch | 2026-11-18 | today: prio work (named KEEP / reproduction patch) | PROBE-prio-bridge2.md, PROBE-prio-row.md |
| wip/probe-prio-open.patch | 2026-11-18 | today: prio work (named KEEP / reproduction patch) | PROBE-prio-open.md |
| wip/probe-prio-row.patch | 2026-11-18 | today: prio work (named KEEP / reproduction patch) | PROBE-prio-spread.md, PROBE-prio-row.md |
| wip/probe-prio-spread.patch | 2026-11-18 | today: prio work (named KEEP / reproduction patch) | PROBE-prio-spread.md |
| wip/probe10/README.md | 2026-11-16 | planar-era baseline/fixture/probe cited by ROADMAP, TESTS or DESIGN-components |  |
| wip/probe10/known.py | 2026-11-16 | planar-era baseline/fixture/probe cited by ROADMAP, TESTS or DESIGN-components |  |
| wip/probe10/lll2.topo | 2026-11-16 | planar-era baseline/fixture/probe cited by ROADMAP, TESTS or DESIGN-components |  |
| wip/probe10/probe10.py | 2026-11-16 | planar-era baseline/fixture/probe cited by ROADMAP, TESTS or DESIGN-components |  |
| wip/probe10/rules9.py | 2026-11-16 | planar-era baseline/fixture/probe cited by ROADMAP, TESTS or DESIGN-components |  |
| wip/probe10/sides.py | 2026-11-16 | planar-era baseline/fixture/probe cited by ROADMAP, TESTS or DESIGN-components |  |
| wip/probe10/sides9.log | 2026-11-16 | planar-era baseline/fixture/probe cited by ROADMAP, TESTS or DESIGN-components |  |
| wip/probe10/sides9.py | 2026-11-16 | planar-era baseline/fixture/probe cited by ROADMAP, TESTS or DESIGN-components |  |
| wip/probe10/steps9.log | 2026-11-16 | planar-era baseline/fixture/probe cited by ROADMAP, TESTS or DESIGN-components |  |
| wip/pv-baselines/LL2.txt | 2026-11-18 | planar-era baseline/fixture/probe cited by ROADMAP, TESTS or DESIGN-components |  |
| wip/pv-baselines/README | 2026-11-18 | planar-era baseline/fixture/probe cited by ROADMAP, TESTS or DESIGN-components |  |
| wip/pv-baselines/T2.txt | 2026-11-18 | planar-era baseline/fixture/probe cited by ROADMAP, TESTS or DESIGN-components |  |
| wip/pv-baselines/T3.txt | 2026-11-18 | planar-era baseline/fixture/probe cited by ROADMAP, TESTS or DESIGN-components |  |
| wip/recon-band-lines-occupancy.md | 2026-11-15 | cited by DESIGN-planar header | DESIGN-planar.md |
| wip/slot-spike/gap.py | 2026-11-18 | today: slot spike | SPIKE-slots.md |
| wip/topologies/LL3a.txt | 2026-11-18 | planar-era baseline/fixture/probe cited by ROADMAP, TESTS or DESIGN-components |  |

## REMOVE-legacy (486)

| file | date | reason | referenced by |
|---|---|---|---|
| AUDIT-assumptions.md | 2026-11-08 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: Audit: hand-coded assumptions in the generator and checker |  |
| DESIGN-G1.md | 2026-11-13 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: DESIGN-G1: the node graph from a topology (D189, D201, D202) — DRAFT | ROADMAP.md; DECISIONS D216/D218/D219 |
| DESIGN-G2-general-graph.md | 2026-11-13 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: DESIGN-G2: the graph generalised so any lane counts build (draft for the enginee |  |
| DESIGN-G3-vectors.md | 2026-11-13 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: DESIGN-G3: placement as vectors (D235) |  |
| DESIGN-G4-static.md | 2026-11-14 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: DESIGN-G4: the static placement model over the arrangement (at ea79529c24) |  |
| DESIGN-N3.md | 2026-11-12 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: DESIGN-N3: node template, render, structure search (D182-D184) | RECON-foundations.md, RESEARCH-prio-freedom.md; DECISIONS D342 |
| DESIGN-O4.md | 2026-11-11 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: O4: a junction from the constraints | DECISIONS D156/D157/D158 |
| DESIGN-modules.md | 2026-11-15 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: DESIGN-modules: hubs built from blocks (D277, D279, D281) | ROADMAP.md, REPRESENT-psg227.md; DECISIONS D277/D282/D283 |
| DESIGN-nodes.md | 2026-11-11 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: DESIGN-nodes: the map as a chain of typed edges, placed before projection (D182) | RECON-foundations.md; DECISIONS D182/D183/D185a |
| DESIGN-width.md | 2026-11-15 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: DESIGN-width: the bundle T at width 2 (steps 2-5 of RECON-width-placement) | ROADMAP.md; DECISIONS D296/D303 |
| DIAG-fits-row2.md | 2026-11-11 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: DIAG-fits-row2: four "fits" rejections in the [nodeoracle] gate (read-only diagn |  |
| DIAG-flat-speed.md | 2026-11-11 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: DIAG-flat-speed: why the flat T fails at some design speeds (FX1, D185 part 1) |  |
| DIAG-house.md | 2026-11-11 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: DIAG: house at (xI-1,0), first height (13) only |  |
| DIAG-near.md | 2026-11-11 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: DIAG-near: why the in-game hover search returns NoLayout (B0b) |  |
| DIAG-node-shortfalls.md | 2026-11-11 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: DIAG-node-shortfalls (dg1): what the node model lacks, measured (not committed) |  |
| DIAG-ph1-limit.md | 2026-11-11 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: DIAG ph1-limit: N->B fails on the "W end height change" terrain (height 13) |  |
| FABLE-G1.md | 2026-11-11 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: FABLE-G1: independent review of DESIGN-G1 (draft at 3a51984978) |  |
| FABLE-G1c.md | 2026-11-12 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: FABLE-G1c: review of DESIGN-G1 §10 (the seed as path programs) | DECISIONS D205 |
| FABLE-G1c2.md | 2026-11-12 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: FABLE-G1c2: review of DESIGN-G1 §11 (G1c(ii): placement and render of the seed)  | DECISIONS D208 |
| FABLE-nodes-perf.md | 2026-11-11 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: FABLE-nodes-perf: performance review of DESIGN-nodes revision 2 (2026-11-11) | DECISIONS D183 |
| FABLE-projection.md | 2026-11-12 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: FABLE-projection: review of DESIGN-G1 §13 "Projection on the graph" (at 19bdc4ba |  |
| FABLE-terrain-placement.md | 2026-11-12 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: FABLE-terrain-placement: review of DESIGN-G1 §14 and of Pete's steers D213-D216  | DECISIONS D216 |
| FABLE-topology-graph.md | 2026-11-11 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: FABLE: independent review of D201 (topology -> graph -> solve) and RS1 as a grap | DECISIONS D202 |
| FINDINGS-O4x.md | 2026-11-10 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: O4x findings: why the uninformed line search returns a costlier route |  |
| FLAT-T-CHAINS.md | 2026-11-11 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: The flat T of the test spec as chains (N0b, ExtractJunctionChains, layout coordi |  |
| GENERATOR-1b.md | 2026-11-08 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: 1b generator: route search instead of templates (D87) |  |
| PLAN-organic.md | 2026-11-09 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: Plan: organic junctions on any terrain (draft for the engineer) | DECISIONS D152 |
| PROBE-choice-pairing.md | 2026-11-14 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: PROBE: LL_RR x LL_RR with PSG 227's choice pairing (2026-11-14) |  |
| PROBE-ll2-elastic.md | 2026-11-14 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: PROBE: the elastic joint on LL_RR x LL_RR (Probe E, REVIEW-G4-overprescribed Q3) |  |
| RECON-A6-crossers-fans.md | 2026-11-14 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: Recon A6: crossers' pieces at item rows, and other rows' port fans (read-only; D |  |
| RECON-F8-doubled.md | 2026-11-13 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: F8 recon: doubled bridges off the leg (commit 29c9be26bf, read-only) |  |
| RECON-F8-run.md | 2026-11-14 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: F8 recon, run (read-only, worktree .claude/ng1 at 87b5e522e8, release build, thr |  |
| RECON-G1c-seed.md | 2026-11-12 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON G1c: how the node template depends on the seed's positions |  |
| RECON-G1d-roles.md | 2026-11-12 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON G1d: where the checker and signal pass assume the T junction |  |
| RECON-G2c.md | 2026-11-14 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON G2c: band-edge bridge over several crossings (at d1d35c0ece, read-only) |  |
| RECON-O1-item-rows.md | 2026-11-14 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: Recon O1: what stands on each stack item row today (read-only; D268) |  |
| RECON-O3-bands.md | 2026-11-14 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: Recon O3: band occupancy, segments (A4), event order and clearance before 2b2b |  |
| RECON-R2-families.md | 2026-11-13 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: R2 recon: relation families of build() (junction_placement.cpp, commit 29c9be26b |  |
| RECON-R3-loop.md | 2026-11-14 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON R3: the joint loop's rule source today (b7fffd8f19) |  |
| RECON-bundle-copies.md | 2026-11-15 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON: can the lane+copy machinery carry a bundle? | RECON-mfb-core.md |
| RECON-corner-join-reject.md | 2026-11-13 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON P6: "corners ... cannot be joined by one terraform" in the LL_RR near sear |  |
| RECON-crossed-in-edge.md | 2026-11-13 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON: a crossed lane in edge (LLL_RRR x LLL_RRR and wider) |  |
| RECON-dense-crossers.md | 2026-11-13 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: OC3 recon: dense crossers on a doubled lane (LL_RR x LL_RR, pairing per directio |  |
| RECON-detour.md | 2026-11-12 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON-detour: the local S-bend as a graph edit (read-only recon, 2026-11-12) |  |
| RECON-doubled-row-edges.md | 2026-11-13 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON: doubled bridges on a row's in and out edges (D229) |  |
| RECON-edge-rules.md | 2026-11-11 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON edge rules: the exact rule table for DESIGN-nodes step N0 (2026-11-11, rea |  |
| RECON-exact-corners.md | 2026-11-13 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON-exact-corners (HC0): the level solve's objective from exact corners |  |
| RECON-fidelity.md | 2026-11-14 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON fidelity: recording vs joint model, L_R x L_RR, L_R x LL_RR, LL_RR x LL_RR |  |
| RECON-four-decks.md | 2026-11-15 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON: four decks in the width-2 core (H1c) |  |
| RECON-frame.md | 2026-11-08 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON-frame: the generator's fixed frame constants |  |
| RECON-generator.md | 2026-11-07 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: Recon: junction generator (milestone 1b) |  |
| RECON-graph-input.md | 2026-11-12 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON-graph-input (RP2): the node template's input from the graph (D201, D213, D |  |
| RECON-hover-switch.md | 2026-11-12 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON: the new projection pipeline behind the in-game hover (read-only; 2026-11- |  |
| RECON-in-game-red-flat.md | 2026-11-13 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON IG1: why in game every hover has red tiles, and why the terraform flattens |  |
| RECON-incremental-cost.md | 2026-11-13 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON P4: an incremental exact_cost for the level solve's descent |  |
| RECON-joint-levels.md | 2026-11-12 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON joint levels (Z2d) - a joint level solve over the whole junction |  |
| RECON-lane-geometry.md | 2026-11-09 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: Doubled-lane geometry recon (worktree .claude/geo @ 6ef853bf27, instrumented, re | DECISIONS D145 |
| RECON-level-solve-cost.md | 2026-11-13 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON: why the level solve costs ~7 ms, and how to cut the level solves per hove |  |
| RECON-llrr-facts.md | 2026-11-11 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON-llrr-facts: what is ruled, built and measured for multi-lane (LL_RR) T jun |  |
| RECON-llrr-placement-contradiction.md | 2026-11-13 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON: why placement refuses LL_RR x LL_RR and L_R x LL_RR (any) | DECISIONS D230 |
| RECON-llrr-placement-fail.md | 2026-11-13 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON P5: LL_RR near search, offsets that fail in the facts placement |  |
| RECON-llrr-round0.md | 2026-11-14 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON: LL_RR x LL_RR round 0 infeasible (daec34b101, all-floor choices) | DECISIONS D248 |
| RECON-llrr-round0b.md | 2026-11-14 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON: LL_RR x LL_RR round 0 infeasible again (8a63a4ae2e), after R3b port fans |  |
| RECON-llrr-time.md | 2026-11-13 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON P3: why an LL_RR placement on real terrain costs 70-150 ms per offset |  |
| RECON-llrr.md | 2026-11-11 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON: LL_RR main line, T junction (L_R branch, and LL_RR branch) |  |
| RECON-nodes.md | 2026-11-11 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON nodes: splits, merges and crossings of the line-router T (2026-11-11, read |  |
| RECON-nonadjacent-crossing.md | 2026-11-13 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON OC1: LL_RR x LL_RR, the non-adjacent crossing and the per-direction placem |  |
| RECON-notation-matrix.md | 2026-11-13 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON: notation matrix (NM1), which notation pairs the pipeline builds (2026-11- |  |
| RECON-per-row-code.md | 2026-11-13 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON: per-row and per-kind code that keeps DESIGN-G1 §16.1 unmet (RC16) |  |
| RECON-placement.md | 2026-11-07 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: Recon: junction placement ghost |  |
| RECON-prio-stretch.md | 2026-11-12 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON: the prio element under a row stretch (C5a; RECON-stretch C5, D217) |  |
| RECON-real-terrain.md | 2026-11-12 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON: real game terrain for junction tests (read-only, untracked) |  |
| RECON-row-chain.md | 2026-11-12 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON: a general row chain for placement (DESIGN-G1 §14.6, §14.7 T4a, §15) |  |
| RECON-s2-cycle.md | 2026-11-13 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON: why placement still refuses L_R x L_RR and L_R x LL_RR with the S2 change |  |
| RECON-s3-probe.md | 2026-11-13 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON: why placement refuses L_R x L_RR, L_R x LL_RR and LL_RR x LL_RR after S2  |  |
| RECON-seed-floors.md | 2026-11-12 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON: the seed's out-fan and doubled-leg floors (SF1a, D212, D215) |  |
| RECON-seed-projection.md | 2026-11-12 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON-seed-projection (RP1): the G1 seed through the node template and solve |  |
| RECON-snap-negotiate.md | 2026-11-12 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON N1: how big is snap-and-negotiate on real terrain (DESIGN-G1 §17) |  |
| RECON-solver-vars.md | 2026-11-08 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON: solver variables and constraints (S2) |  |
| RECON-stretch.md | 2026-11-12 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON: which relations placement is missing, found by stretching every relation  | TESTS.md |
| RECON-template-graph.md | 2026-11-11 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON-template-graph: what a graph-level ramp insert must supply (RC2) |  |
| RECON-template-seam.md | 2026-11-15 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON: where a template block generator meets the pipeline (HEAD 13f7073742) |  |
| RECON-topology-placement-seam.md | 2026-11-13 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON: the seam between topology and placement in junction_topology.cpp (RS1) |  |
| RECON-train-length.md | 2026-11-12 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON: which seed floors do not scale with train length and speed (TL1a) |  |
| RECON-turn-programs.md | 2026-11-11 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON turn programs: today's flat T as per-movement programs, and which lengths  |  |
| RECON-why-flat.md | 2026-11-13 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON IG5: why half the sloped straights still end flat (launch HEAD 0b1d7ba12c, |  |
| RECON-wide-hubs.md | 2026-11-13 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON: wider hubs stop at two refusals |  |
| RECON-width-placement.md | 2026-11-15 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: RECON: placing and rendering the T at width 2 |  |
| RECONCILE-O4.md | 2026-11-10 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: O4 reconciliation (draft for the engineer, 2026-11-10) |  |
| REVIEW-graph-model.md | 2026-11-13 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: REVIEW-graph-model: review of the graph pipeline's representation and placement  | DECISIONS D235 |
| REVIEW-joint-loop.md | 2026-11-14 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: REVIEW-joint-loop: review of the joint loop and its rule source (at aad3c5f03e) | DECISIONS D253 |
| REVIEW-opus-since-move.md | 2026-11-13 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: REVIEW-opus-since-move: review of d59a4a488e..295164422f (161 commits, 44 code)  | DECISIONS D227 |
| REVIEW-opus-since-terrain-placement.md | 2026-11-12 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: REVIEW-opus-since-terrain-placement: review of D213-D219, DESIGN-G1 §14-§17 and  |  |
| REVIEW-port-fans.md | 2026-11-13 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: REVIEW-port-fans: review of D230, briefs/SF1-south-fans.md and RECON-llrr-placem | DECISIONS D230 |
| REVIEW-rule-source.md | 2026-11-13 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: REVIEW-rule-source: review of DESIGN-G3-vectors.md §G3.6 "One rule source (D240) |  |
| REVIEW-width-overprescribed.md | 2026-11-15 | old pipeline (graph/placement/joint loop/rows/bands) or design planar replaced: REVIEW-width-overprescribed: review of DESIGN-width steps 2-5c and the block int |  |
| briefs/ARR0-arrangement-recon.md | 2026-11-14 | brief for an old-pipeline step: ARR0: which arrangement facts the seeds obey (recon for REVIEW-joint-loop Q6 ste |  |
| briefs/ARR1-arrangement.md | 2026-11-14 | brief for an old-pipeline step: ARR1: the track arrangement as data (REVIEW-joint-loop Q6 step 1, D254, D255) |  |
| briefs/ARR2-band-line-recon.md | 2026-11-14 | brief for an old-pipeline step: ARR2: do the seeds obey the sweep's west-to-east line inside the bands? (recon f |  |
| briefs/B0-baseline-bench.md | 2026-11-11 | brief for an old-pipeline step: B0: baseline hover timings of today's generator on the Buborough save (D183 §9) |  |
| briefs/B0b-near-diag.md | 2026-11-11 | brief for an old-pipeline step: B0b: why the in-game hover search returns NoLayout (read-only diagnosis) |  |
| briefs/BL1-bridge-limit.md | 2026-11-12 | brief for an old-pipeline step: BL1: the bridge length limit comes from the game (D208(1)) |  |
| briefs/C1-compactness-score.md | 2026-11-09 | brief for an old-pipeline step: C1: the compactness score (D153) | DECISIONS D154 |
| briefs/C5a-recon-prio-row-stretch.md | 2026-11-12 | brief for an old-pipeline step: C5a (recon by experiment): why the prio element breaks under a row stretch on LL |  |
| briefs/C5b-prio-footprint.md | 2026-11-12 | brief for an old-pipeline step: C5b: the prio element's footprint as an occupancy family; the signal pass fails  |  |
| briefs/CF1-claude-md-corrections.md | 2026-11-13 | brief for an old-pipeline step: CF1: correct the CLAUDE.md facts the review found wrong |  |
| briefs/CK1-checker-oracle.md | 2026-11-11 | brief for an old-pipeline step: CK1: the checker leaves search acceptance; an oracle sweep keeps it honest (D193 |  |
| briefs/CN1-hover-cancel.md | 2026-11-13 | brief for an old-pipeline step: CN1: a superseded hover search stops (LL_RR near searches run for seconds on hil |  |
| briefs/CR1-multilane-branch-crash.md | 2026-11-13 | brief for an old-pipeline step: CR1: the seed crashes on an L_R main with a multi-lane branch (NM1); make it ref |  |
| briefs/CS1-curve-state-across-edges.md | 2026-11-11 | brief for an old-pipeline step: CS1: an edge that continues routed track inherits its curve state (D178) |  |
| briefs/CS1-fan-split-curve-gap.md | 2026-11-12 | brief for an old-pipeline step: CS1: curve gap from an in fan to its exit split (G3; DESIGN-G1 §11.8 "Curve gaps |  |
| briefs/CS2-curve-state-after-goal.md | 2026-11-11 | brief for an old-pipeline step: CS2: an edge that ends on routed track keeps the curve rule over what continues  |  |
| briefs/CS3-boundary-by-rule3.md | 2026-11-11 | brief for an old-pipeline step: CS3: curves across an edge boundary are judged by the checker's own rule (rule 3 |  |
| briefs/CX1-crossings.md | 2026-11-13 | brief for an old-pipeline step: CX1: structure crossings without one joiner (D223, D227 C1 and C3) |  |
| briefs/CX1-main-main-crossings.md | 2026-11-13 | brief for an old-pipeline step: CX1: main x main and joiner x joiner crossings get their over/under from D223 (" |  |
| briefs/D104-spannable.md | 2026-11-08 | brief for an old-pipeline step: Brief D104: bridges may span existing rail and road |  |
| briefs/D137-design-train-prio.md | 2026-11-09 | brief for an old-pipeline step: D137: a design train is required; the picked train sizes the prio |  |
| briefs/D139-prio-slider.md | 2026-11-09 | brief for an old-pipeline step: D139: prio length slider in the junction window |  |
| briefs/DB1a-doubled-out-edge-programs.md | 2026-11-13 | brief for an old-pipeline step: DB1a: programs for a doubled bridge on a row's out edge (D229, DESIGN-G2 §G2.4) |  |
| briefs/DESIGN-G4-amend-form-choice.md | 2026-11-14 | brief for an old-pipeline step: Design amendment: DESIGN-G4 with the crossing form as a choice (D260, 2026-11-14 |  |
| briefs/DESIGN-G4-static.md | 2026-11-14 | brief for an old-pipeline step: Design pass: DESIGN-G4, the static placement model over the arrangement (2026-11 |  |
| briefs/DI1-doubled-in-edge.md | 2026-11-13 | brief for an old-pipeline step: DI1: a doubled bridge on a row's in edge, structure and programs (DESIGN-G2 §G2. |  |
| briefs/E1-main-line-as-edge.md | 2026-11-11 | brief for an old-pipeline step: E1: A's main line is an edge of the map, with port approaches checked first (D17 |  |
| briefs/E2-port-edges.md | 2026-11-11 | brief for an old-pipeline step: E2: the N and S stubs are port edges, routed like every other edge (D178) |  |
| briefs/E3-b-lanes-as-edges.md | 2026-11-11 | brief for an old-pipeline step: E3: B's lanes are edges between nodes, routed like every other edge (D178) |  |
| briefs/E3a-line-router-d110-bridge.md | 2026-11-11 | brief for an old-pipeline step: E3a: the line router may close an S-bend with a bridge head (D110) |  |
| briefs/E3b-checked-prefix.md | 2026-11-11 | brief for an old-pipeline step: E3b: span edges are checked against the terrain (D175, D178) |  |
| briefs/F5-band-run-probe.md | 2026-11-14 | brief for an old-pipeline step: F5 probe: does a horizontal band run fit LL_RR x LL_RR? (recon for REVIEW-joint- |  |
| briefs/FN1-slide-in-fans.md | 2026-11-12 | brief for an old-pipeline step: FN1: in-fans keep their port row as long as they can (D212) |  |
| briefs/FRAME-recon.md | 2026-11-08 | brief for an old-pipeline step: Brief FRAME-recon: inventory of the generator's fixed frame constants (read-only |  |
| briefs/FRAME-step1.md | 2026-11-08 | brief for an old-pipeline step: Brief FRAME step 1: one frame struct for every fixed frame constant (pure refact |  |
| briefs/FRAME-step2.md | 2026-11-08 | brief for an old-pipeline step: Brief FRAME step 2: the frame grows with the prio coverage |  |
| briefs/FX1-flat-speed-diag.md | 2026-11-11 | brief for an old-pipeline step: FX1: why the flat T fails at some design speeds (diagnosis, D185 part 1) |  |
| briefs/G0-template-comparator.md | 2026-11-11 | brief for an old-pipeline step: G0: an exact node-template comparator (DESIGN-G1 §5, §6) |  |
| briefs/G1-construct-t-map.md | 2026-11-10 | brief for an old-pipeline step: G1: construct the T's map, build once, check once (DESIGN-O4 "G1") |  |
| briefs/G1a-template-input.md | 2026-11-11 | brief for an old-pipeline step: G1a: the template builder takes paths and per-piece facts, not a layout (DESIGN- |  |
| briefs/G1b1-topology-lanes-crossings.md | 2026-11-11 | brief for an old-pipeline step: G1b(i): the topology type, two topologies as data, lane expansion and chord cros |  |
| briefs/G1b2a-track-graph.md | 2026-11-12 | brief for an old-pipeline step: G1b(ii-a): the track graph — split and merge trees, legs, crossings per edge (DE |  |
| briefs/G1b2b-track-order.md | 2026-11-12 | brief for an old-pipeline step: G1b(ii-b): the order of a track graph — node levels, row orders, crossings along |  |
| briefs/G1b2c-track-structure.md | 2026-11-12 | brief for an old-pipeline step: G1b(ii-c): side tracks, the row stack, bridges, turns (DESIGN-G1 §9; D75/D87, D8 |  |
| briefs/G1b2d-levels-first.md | 2026-11-12 | brief for an old-pipeline step: G1b(ii-d): node levels before crossings (D205), crossings filtered by level, one |  |
| briefs/G1c1a-track-programs.md | 2026-11-12 | brief for an old-pipeline step: G1c(i-a): track programs, the bends and crossings of every graph edge (DESIGN-G1 |  |
| briefs/G1c1b-band-layers.md | 2026-11-12 | brief for an old-pipeline step: G1c(i-b): band layers, the y order of nodes and band crossings inside each band  |  |
| briefs/G1c2a-placement.md | 2026-11-12 | brief for an old-pipeline step: G1c(ii-a): seed placement, the tile of every node, bend, crossing and bridge hea |  |
| briefs/G1c2b-render.md | 2026-11-12 | brief for an old-pipeline step: G1c(ii-b): render a seed placement to a track-only layout (DESIGN-G1 §11.10) |  |
| briefs/G1d-checker-topology.md | 2026-11-12 | brief for an old-pipeline step: G1d: the checker's ports and movements from a topology (RECON-G1d-roles §5; DESI |  |
| briefs/G2a-cascade-splits.md | 2026-11-13 | brief for an old-pipeline step: G2a: a split or merge with more than two keys becomes a cascade of two-way nodes |  |
| briefs/G2a-tmap-timing-line-router.md | 2026-11-10 | brief for an old-pipeline step: G2a: the T map's time by phase, and its edges on the line router (DESIGN-O4 "G1" |  |
| briefs/G2b-exit-side-d224.md | 2026-11-13 | brief for an old-pipeline step: G2b: exit-side D224, so a main lane's branch-only splits sit in the band below i |  |
| briefs/G2b-third-crossing-in-projection.md | 2026-11-10 | brief for an old-pipeline step: G2b: the third crossing decided in projection (D171), best of two projections |  |
| briefs/G2c-crossing-nodes-sovern.md | 2026-11-10 | brief for an old-pipeline step: G2c: crossing nodes with windows, and SoverN projected (D172, DESIGN-O4 "G1", cr |  |
| briefs/G2c1-band-series.md | 2026-11-14 | brief for an old-pipeline step: G2c-1: a band edge carries a series of single bridges, one per crossing (DESIGN- |  |
| briefs/G3-1a-node-shapes.md | 2026-11-13 | brief for an old-pipeline step: G3.1a: node shapes and edge ends as program data (DESIGN-G3 §G3.1), no behaviour |  |
| briefs/G3-1b-placement-reads-shapes.md | 2026-11-13 | brief for an old-pipeline step: G3.1b: band chains and row items read the programs' shapes (DESIGN-G3 §G3.1), id |  |
| briefs/G3-3P1-bend-rasters-affine.md | 2026-11-13 | brief for an old-pipeline step: G3.3 P1: are bend rasters affine in their length? (DESIGN-G3 §G3.3a, D236 premis |  |
| briefs/G3-3SP-scale-probe.md | 2026-11-13 | brief for an old-pipeline step: G3.3 SP: joint-solve scale probe on wider hubs (measurement only) |  |
| briefs/G3-3b1-milp-seam.md | 2026-11-13 | brief for an old-pipeline step: G3.3b-1: HiGHS in the build behind one wrapper, JunctionMilp (D237, DESIGN-G3 §G |  |
| briefs/G3-3b2-joint-model.md | 2026-11-13 | brief for an old-pipeline step: G3.3b-2: the joint model in C++, a port of the prototype (DESIGN-G3 §G3.3, D235- |  |
| briefs/G3-3b3-curve-gap-counters.md | 2026-11-13 | brief for an old-pipeline step: G3.3b-3: curve gaps as unit counters in the joint model (DESIGN-G3 §G3.3a item 1 |  |
| briefs/G3-3b4-hug-inequalities.md | 2026-11-13 | brief for an old-pipeline step: G3.3b-4: the bridge hug as inequalities and cost in the joint model (DESIGN-G3 § |  |
| briefs/G3-3b5-joint-loop.md | 2026-11-13 | brief for an old-pipeline step: G3.3b-5: zero-size bends in the counters, choices from the joint result, and the |  |
| briefs/G3-3b6-clash-rows.md | 2026-11-13 | brief for an old-pipeline step: G3.3b-6: occupancy as kept clash rows in the joint model (DESIGN-G3 §G3.3a item  |  |
| briefs/G3-3b7-order-from-joint.md | 2026-11-13 | brief for an old-pipeline step: G3.3b-7: occupancy order from the joint solution (DESIGN-G3 §G3.3a item 3 (b), D |  |
| briefs/G3-4a-bridge-over-sbend.md | 2026-11-13 | brief for an old-pipeline step: G3.4a: the column of a bridge over an S-bend as a choice (DESIGN-G3 §G3.4a) |  |
| briefs/G3-4a2-t-on-the-loop.md | 2026-11-14 | brief for an old-pipeline step: G3.4a-2: land the bridge-over-S-bend column t on the joint loop (DESIGN-G3 §G3.4 |  |
| briefs/G3-4b-flanking-sbend.md | 2026-11-14 | brief for an old-pipeline step: G3.4b: a single bridge may pass over the S-bend after the crossing as well as be |  |
| briefs/G3-R1-sizes-as-inputs.md | 2026-11-13 | brief for an old-pipeline step: G3 R1: bend sizes and fan hands are inputs of build() (DESIGN-G3 §G3.6 v2, D240) |  |
| briefs/G3-R2-detect.md | 2026-11-14 | brief for an old-pipeline step: G3 R2: build() emits only rows that need no solve; detect(x) adds the rest (DESI |  |
| briefs/G3-R3a-loop-at-a-point.md | 2026-11-14 | brief for an old-pipeline step: G3 R3a: the joint loop records at a point and detects there (DESIGN-G3 §G3.6, D2 |  |
| briefs/G3-R3b-port-fans-and-point.md | 2026-11-14 | brief for an old-pipeline step: G3 R3b: port fans follow the rows in the joint model; the loop detects only at a |  |
| briefs/G3-R3c-clash-order.md | 2026-11-14 | brief for an old-pipeline step: G3 R3c: a kept clash's order is a choice of the joint model (DESIGN-G3 §G3.6, D2 |  |
| briefs/G3-R3d-port-offset.md | 2026-11-14 | brief for an old-pipeline step: G3 R3d: the port offset is a variable of the joint model (DESIGN-G3 §G3.6 R1 inp |  |
| briefs/G3-R4a-seed-from-the-loop.md | 2026-11-14 | brief for an old-pipeline step: G3 R4a: the seed falls back to the joint loop when no cut places (DESIGN-G3 §G3. |  |
| briefs/G3P-joint-solve-prototype.md | 2026-11-13 | brief for an old-pipeline step: G3P: offline prototype of the joint solve (DESIGN-G3 §G3.3, §G3.5 step 1) |  |
| briefs/G3a-flat-height-line-router.md | 2026-11-10 | brief for an old-pipeline step: G3a: the line-router T starts at the flat map's height (D173) |  |
| briefs/G3b-tmap-obstacles.md | 2026-11-10 | brief for an old-pipeline step: G3b: projection onto terrain, step 1: obstacles (revision 5 projection, D170a, D |  |
| briefs/G3c-tmap-ridge.md | 2026-11-10 | brief for an old-pipeline step: G3c: projection onto terrain, step 2: a ridge climbed by ramps (revision 5 proje |  |
| briefs/G3c0-box-growth-crossing-window.md | 2026-11-10 | brief for an old-pipeline step: G3c0: the box grows within the terrain, and the crossing window on the line-rout |  |
| briefs/G4-2a1-square-crossing.md | 2026-11-14 | brief for an old-pipeline step: G4 2a1: a band edge passes under a single bridge square (D256, D258; DESIGN-G4 § |  |
| briefs/G4-2a1b-both-forms.md | 2026-11-14 | brief for an old-pipeline step: G4 2a1b: every single-bridge crossing carries both forms; the form is data (D260 |  |
| briefs/G4-2a2-joint-chooses-form.md | 2026-11-14 | brief for an old-pipeline step: G4 2a2: the joint chooses each crossing group's form (D260, D261, D266; DESIGN-G |  |
| briefs/G4-2b1a-band-lines.md | 2026-11-14 | brief for an old-pipeline step: G4 2b1a: band lines, neighbour pairs over every event order, neighbouring event  |  |
| briefs/G4-2b2a0-f8-floor.md | 2026-11-14 | brief for an old-pipeline step: G4 2b2a0: the run from the lane split to the heads is a floor (F8; DESIGN-G4 §2, |  |
| briefs/G4-2b2a1-gap-corner-form.md | 2026-11-14 | brief for an old-pipeline step: G4 2b2a1: the gap table and the Corner column form (DESIGN-G4 §3, §8 2b2a; D268) |  |
| briefs/G4-2b2a2-item-rows.md | 2026-11-14 | brief for an old-pipeline step: G4 2b2a2: item-row occupants in a static order, and O1 rows (DESIGN-G4 §1 A5, §3 |  |
| briefs/G4-2b2a3a-crosser-pieces.md | 2026-11-14 | brief for an old-pipeline step: G4 2b2a3a: a crosser's piece at each item row, with affine columns and guarded O |  |
| briefs/G4-2b2a3b-fans.md | 2026-11-14 | brief for an old-pipeline step: G4 2b2a3b: O2, adjacent W/E port fans separated at their end rows (DESIGN-G4 §3  |  |
| briefs/G4-2b2b0-item-rows-ends.md | 2026-11-14 | brief for an old-pipeline step: G4 2b2b0: item rows reach past a row's end nodes (D273; RECON-O3 finding 7) |  |
| briefs/G4-2b2b1a-band-segments.md | 2026-11-14 | brief for an old-pipeline step: G4 2b2b1a: A4, band edges' cuts, segments and bend roles (D265, D273; RECON-O3 Q |  |
| briefs/G4-2b2b1b-event-rows.md | 2026-11-14 | brief for an old-pipeline step: G4 2b2b1b: band event rows, crossing rows rho, layer order and event-order binar |  |
| briefs/G4-tmap-in-game.md | 2026-11-11 | brief for an old-pipeline step: G4: the projected T map in the in-game junction tool (D173, D177) |  |
| briefs/G5-tmap-tunnels.md | 2026-11-11 | brief for an old-pipeline step: G5: tunnels in the T map's projection (D155, D174, D175) |  |
| briefs/GR1-fetch-psg-newgrfs.md | 2026-11-13 | brief for an old-pipeline step: GR1: get the NewGRFs the PSG saves need, so they load and run |  |
| briefs/GS1-growth-stop.md | 2026-11-11 | brief for an old-pipeline step: GS1: the box grows only when the box stopped the search (D170a, D178, D179) |  |
| briefs/GS1-junction-spec-split.md | 2026-11-13 | brief for an old-pipeline step: GS1: move the shared types out of legacy/junction_generator.h into src/junction/ |  |
| briefs/GS1-sweep-tag.md | 2026-11-14 | brief for an old-pipeline step: GS1: the five slowest gate cases move to a hidden sweep tag run by `jt sweep` (D |  |
| briefs/GS1b-relaxed-reachability.md | 2026-11-11 | brief for an old-pipeline step: GS1b: a relaxed reachability test skips doomed searches and stops useless growth |  |
| briefs/GT1-gate-trim.md | 2026-11-13 | brief for an old-pipeline step: GT1: the wide gate back under budget: identity sweeps get their own tag |  |
| briefs/GT2-topology-tag-budget.md | 2026-11-13 | brief for an old-pipeline step: GT2: `[topology]` back under its 5 s Debug budget |  |
| briefs/H1-hover-node-search.md | 2026-11-11 | brief for an old-pipeline step: H1: hover generation runs the node search (D182, D185a, D189, D193; the in-game  |  |
| briefs/H1-hover-plan.md | 2026-11-12 | brief for an old-pipeline step: H1: the hover plan on the new pipeline, terraform as build steps, a terraformed  |  |
| briefs/H2-hover-gui.md | 2026-11-12 | brief for an old-pipeline step: H2: the in-game hover uses the projection pipeline (toggle), shows terraform, bu |  |
| briefs/H3-ghost-levels.md | 2026-11-12 | brief for an old-pipeline step: H3: the ghost draws track at its solved levels (slopes, ramps, bridge decks) (D2 |  |
| briefs/H4-ghost-signals-ground.md | 2026-11-12 | brief for an old-pipeline step: H4: ghost signals at their track's level; terraformed ground shown on every plan |  |
| briefs/HC0-recon-exact-corners.md | 2026-11-13 | brief for an old-pipeline step: HC0 (recon by experiment): the level solve's objective from exact corners instea |  |
| briefs/HC1-exact-corners.md | 2026-11-13 | brief for an old-pipeline step: HC1: the level solve's terraform from exact corners; ADJ and FLEX deleted (RECON |  |
| briefs/HF1-find-psg-hubs.md | 2026-11-13 | brief for an old-pipeline step: HF1 (recon): find the real junction hubs in the PSG saves and export them |  |
| briefs/HF1-height-facts.md | 2026-11-11 | brief for an old-pipeline step: HF1: height facts in the node solve (D194, DESIGN-N3 §10) |  |
| briefs/HF1a-foundation-facts.md | 2026-11-11 | brief for an old-pipeline step: HF1a: tile facts per track set from the game's foundation functions |  |
| briefs/HF1b-height-joins.md | 2026-11-11 | brief for an old-pipeline step: HF1b: the node solve meets the height joins (D194, DESIGN-N3 §10) |  |
| briefs/HH1-no-diamond-occupancy.md | 2026-11-13 | brief for an old-pipeline step: HH1: unrelated pieces may share a tile only as Upper/Lower or Left/Right (no X/Y |  |
| briefs/HH2-bridge-middles-tile-class.md | 2026-11-13 | brief for an old-pipeline step: HH2: a bridge middle never spans a Blocked tile (D175) |  |
| briefs/HH3-height-clamp.md | 2026-11-13 | brief for an old-pipeline step: HH3: levels and terraform stay inside the map's height range (REVIEW-opus-since- |  |
| briefs/HM1-foundation-as-game.md | 2026-11-13 | brief for an old-pipeline step: HM1: the level solve's levelled foundation is the game's (REVIEW-opus-since-terr |  |
| briefs/HN1-height-failure-names.md | 2026-11-11 | brief for an old-pipeline step: HN1: a Height failure names its part and its tiles (DESIGN-N3 §11, trigger) |  |
| briefs/IG1-recon-red-tiles-flat.md | 2026-11-13 | brief for an old-pipeline step: IG1 (recon by experiment): why in game every hover has red tiles, and why the te |  |
| briefs/IG2-estimate-on-planned-ground.md | 2026-11-13 | brief for an old-pipeline step: IG2: the in-game estimate judges terraformed tiles on the planned ground; the st |  |
| briefs/IG3-bridge-heads-as-game.md | 2026-11-13 | brief for an old-pipeline step: IG3: bridge heads take the game's slopes; the span's ground stays at or below th |  |
| briefs/IG4-ramp-rule-bridges.md | 2026-11-13 | brief for an old-pipeline step: IG4: the ramp rule counts the climbs the game's trains feel at bridge heads and  |  |
| briefs/IG5-recon-why-flat.md | 2026-11-13 | brief for an old-pipeline step: IG5 (recon by experiment): why half the sloped straights still end flat when ram |  |
| briefs/IG6-llrr-in-game-check.md | 2026-11-13 | brief for an old-pipeline step: IG6: the in-game estimate and build check covers LL_RR, not just the T (IG2/IG3  |  |
| briefs/J1-tighten.md | 2026-11-08 | brief for an old-pipeline step: Brief J1: make the requested W-side tightening actually happen |  |
| briefs/JN1-recon-junctionary-crossings.md | 2026-11-13 | brief for an old-pipeline step: JN1 (read-only recon): what the pulled Junctionary and coop hub images say about |  |
| briefs/L1-occupancy-legality.md | 2026-11-13 | brief for an old-pipeline step: L1: occupancy legality from the graph, not from a reference solve (REVIEW-opus-s |  |
| briefs/LP1-lane-pairing.md | 2026-11-13 | brief for LL_RR lane pairing in the old placement (D221) |  |
| briefs/LR0-llrr-recon.md | 2026-11-11 | brief for an old-pipeline step: LR0: LL_RR junctions, read-only recon for planning (side workstream) |  |
| briefs/LR1-llrr-in-game.md | 2026-11-13 | brief for an old-pipeline step: LR1: the LL_RR hub in the hover: topology from the notations, measured on real t |  |
| briefs/LS1-level-solve-speed.md | 2026-11-13 | brief for an old-pipeline step: LS1: level solve 4x faster with identical results, plus the sound cone reject (R |  |
| briefs/MJ1-legs-merge-before-join.md | 2026-11-13 | brief for an old-pipeline step: MJ1: branch legs bound for one main lane merge before joining it (D224; LL_RR x  |  |
| briefs/MV1-move-to-src-junction.md | 2026-11-13 | brief for an old-pipeline step: MV1: move the junction tool into src/junction/ feature folders (identity step) |  |
| briefs/MX1-map-export.md | 2026-11-08 | brief for an old-pipeline step: MX1: map area export and hotspot finder (console commands) |  |
| briefs/MX2-release-psg.md | 2026-11-08 | brief for an old-pipeline step: MX2: map commands in Release builds; PSG saves and the overflow export |  |
| briefs/N0a-curve-gaps.md | 2026-11-11 | brief for an old-pipeline step: N0a: curve speed as pairwise gap constraints, checked against the checker (D183, |  |
| briefs/N0b-chain-extraction.md | 2026-11-11 | brief for an old-pipeline step: N0b: the flat T read as chains of runs, curves and switches (D182, D183, DESIGN- |  |
| briefs/N0c1-y-free.md | 2026-11-11 | brief for an old-pipeline step: N0c1: vertical runs are free, depth is a variable (DESIGN-N3 §5; D185a, D189) |  |
| briefs/N0c2-free-diagonals.md | 2026-11-11 | brief for an old-pipeline step: N0c2: free diagonals and the two-phase solve (DESIGN-N3 §6; D185a, D189, D193) |  |
| briefs/N1-position-propagation.md | 2026-11-11 | brief for an old-pipeline step: N1: incremental position propagation (D183, DESIGN-nodes §9, FABLE-nodes-perf §1 |  |
| briefs/N1-recon-snap-negotiate.md | 2026-11-12 | brief for an old-pipeline step: N1 (recon by experiment): how big is snap-and-negotiate on real terrain (DESIGN- |  |
| briefs/N2-tile-facts.md | 2026-11-11 | brief for an old-pipeline step: N2: per-tile facts with O(1) run and next-bad queries (D183, DESIGN-nodes §2 hei |  |
| briefs/N3a-node-template.md | 2026-11-11 | brief for an old-pipeline step: N3a: the node template of the flat T, and its render (DESIGN-N3 §1-§3) |  |
| briefs/N3b-node-facts.md | 2026-11-11 | brief for an old-pipeline step: N3b: terrain facts in the node solve, no structure changes yet (DESIGN-N3 §4, fa |  |
| briefs/N3b2-heights-oracle.md | 2026-11-11 | brief for an old-pipeline step: N3b2: a height oracle for laid-out layouts (DESIGN-N3 §4; FABLE §2.1 "lay-out th |  |
| briefs/N3c-detour-search.md | 2026-11-11 | brief for an old-pipeline step: N3c: S-bend detours and the structure search (DESIGN-N3 §4; D182) |  |
| briefs/NG1-negotiate-a-for-n-to-a.md | 2026-11-11 | brief for an old-pipeline step: NG1: when N->A fails against A, A is routed again clear of N->A (D181) |  |
| briefs/NG1b-prio-window.md | 2026-11-11 | brief for an old-pipeline step: NG1b: the negotiated merge keeps its prio window (D181a) |  |
| briefs/NP1-node-input-from-topology.md | 2026-11-12 | brief for an old-pipeline step: NP1: the node template's input traced by a topology (projection of the G1 seed,  |  |
| briefs/NP2-link-directions.md | 2026-11-12 | brief for an old-pipeline step: NP2: directions per free link, not per path (projection of the G1 seed, step 2) |  |
| briefs/NP3-solve-by-topology.md | 2026-11-12 | brief for an old-pipeline step: NP3: the node solve signals by a topology; LL_RR solves on flat ground (projecti |  |
| briefs/NP4-search-by-topology.md | 2026-11-12 | brief for an old-pipeline step: NP4: the node search by a topology; LL_RR builds around a house (projection of t |  |
| briefs/NW1-shift-back-window.md | 2026-11-11 | brief for an old-pipeline step: NW1: node windows, first case: N->B's failure moves the shift-back west (D178, C |  |
| briefs/NW1c-finish.md | 2026-11-11 | brief for an old-pipeline step: NW1c: finish NW1 on top of CS3 |  |
| briefs/O1-router-heights.md | 2026-11-09 | brief for an old-pipeline step: O1: heights in the line router (PLAN-organic O1, D152) |  |
| briefs/O1b-descent-choice.md | 2026-11-09 | brief for an old-pipeline step: O1b: a ramp is a choice beside a foundation (D152 amended) |  |
| briefs/O2-router-tunnels.md | 2026-11-09 | brief for an old-pipeline step: O2: tunnels in the line router (PLAN-organic O2, D155) | DECISIONS D155 |
| briefs/O4a-router-starts.md | 2026-11-09 | brief for an old-pipeline step: O4a: the line router takes a set of start pieces (DESIGN-O4) |  |
| briefs/O4b1-negotiated-movements.md | 2026-11-10 | brief for an old-pipeline step: O4b1: the six movements of a T routed together by negotiation (DESIGN-O4 revisio |  |
| briefs/O4b2-rule-clean-organic.md | 2026-11-10 | brief for an old-pipeline step: O4b2: a rule-clean organic T: feedback, prios, signals, judge, score (DESIGN-O4  |  |
| briefs/O4b2b-ml-turns-convergence.md | 2026-11-10 | brief for an old-pipeline step: O4b2b: main-line turn cost, the non-converging specs, and the last feedback rule |  |
| briefs/O4b2c-hug-cleanup-convergence.md | 2026-11-10 | brief for an old-pipeline step: O4b2c: hugging, clean-up of needless bridges, faster convergence, last feedback  |  |
| briefs/O4b2d-forced-crossings.md | 2026-11-10 | brief for an old-pipeline step: O4b2d: forced crossings decided up front, clean-up fixed (continues O4b2c) |  |
| briefs/O4c1-router-rule-state.md | 2026-11-10 | brief for an old-pipeline step: O4c1: a rule state machine in the line router (DESIGN-O4 revision 4, D168) |  |
| briefs/O4c2-organic-rules-in-routing.md | 2026-11-10 | brief for an old-pipeline step: O4c2: the organic T with rules in the routing, feedback loop deleted (DESIGN-O4  |  |
| briefs/O4h-router-heuristic.md | 2026-11-10 | brief for an old-pipeline step: O4h: a tighter admissible heuristic for the line router (DESIGN-O4 revision 3, a |  |
| briefs/O4n-router-negotiation-hooks.md | 2026-11-10 | brief for an old-pipeline step: O4n: line router hooks for negotiated routing, minimum length, and the remaining |  |
| briefs/O4p-router-speed.md | 2026-11-10 | brief for an old-pipeline step: O4p: line router per-expansion speed and the D161 test oracle |  |
| briefs/O4r-route-cost.md | 2026-11-10 | brief for an old-pipeline step: O4r: measure one route's cost (DESIGN-O4 revision 3, step O4r) |  |
| briefs/O4x-router-pruning.md | 2026-11-10 | brief for an old-pipeline step: O4x: why the line router's uninformed search returns a costlier route (investiga |  |
| briefs/OC1-occupancy.md | 2026-11-11 | brief for an old-pipeline step: OC1: occupancy, unrelated tracks never share or touch a tile (DESIGN-N3 §7; D193 |  |
| briefs/OC1-recon-nonadjacent-crossing.md | 2026-11-13 | brief for an old-pipeline step: OC1 (recon): what LL_RR x LL_RR needs at the order stage ("a crossing between ed |  |
| briefs/OC2-vertical-order.md | 2026-11-11 | brief for an old-pipeline step: OC2: vertical order chain in the Y phase (DESIGN-N3 §9; D193) |  |
| briefs/OC3-recon-dense-crossers.md | 2026-11-13 | brief for an old-pipeline step: OC3 (recon): dense crossers on a doubled lane (LL_RR x LL_RR per direction: "bri |  |
| briefs/OF1-order-deferred-crossings.md | 2026-11-13 | brief for an old-pipeline step: OF1: the order sweep places crossings between non-adjacent edges (RECON-nonadjac |  |
| briefs/OF2-unmade-row-node.md | 2026-11-13 | brief for an old-pipeline step: OF2: a row node reached only by row edges is placed by the order (LL_RR x LL_RR, |  |
| briefs/P2-line-router-speed.md | 2026-11-10 | brief for an old-pipeline step: P2: the line router's per-expansion cost (FINDINGS-P1 F2, F3, F4), and the T map |  |
| briefs/P2-near-search-shared-work.md | 2026-11-13 | brief for an old-pipeline step: P2: the near search does offset-independent placement work once (hover headroom; |  |
| briefs/P2a-token-pass-from-graph.md | 2026-11-12 | brief for an old-pipeline step: P2a: the signal-token pass reads the graph and its routes, never a layout (DESIG |  |
| briefs/P2b-solve-signals-by-tokens.md | 2026-11-12 | brief for an old-pipeline step: P2b: the solve renders tagged routes and signals them by tokens; LL_RR solves on |  |
| briefs/P3-flat-map-once.md | 2026-11-11 | brief for an old-pipeline step: P3: the flat map is computed once per spec, not once per offset (D173, PERF2) |  |
| briefs/P3-port-fan-lean.md | 2026-11-12 | brief for an old-pipeline step: P3: port fans lean toward their node in the solve (D212; DESIGN-G1 §13.3, §13.8  |  |
| briefs/P3-recon-llrr-time.md | 2026-11-13 | brief for an old-pipeline step: P3 (recon by experiment): why an LL_RR placement on real terrain costs ~70-150 m |  |
| briefs/P3b-every-row-change-leans.md | 2026-11-12 | brief for an old-pipeline step: P3b: every row change leans toward the feature it serves (D215; generalises P3/D |  |
| briefs/P3b-flat-map-cached-in-window.md | 2026-11-11 | brief for an old-pipeline step: P3b: the junction window keeps the flat map between hovers (D173, P3) |  |
| briefs/P3b-llrr-level-speed.md | 2026-11-13 | brief for an old-pipeline step: P3b: P3's level-solve speed-ups and the steep-slope abort guard (RECON-llrr-time |  |
| briefs/P4-recon-incremental-cost.md | 2026-11-13 | brief for an old-pipeline step: P4 (recon by experiment): an incremental exact_cost for the level solve's descen |  |
| briefs/P4a-failures-name-graph-elements.md | 2026-11-12 | brief for an old-pipeline step: P4a: solve failures name the graph elements they hit (DESIGN-G1 §13.4, §13.8 ite |  |
| briefs/P4b-graph-search-detour.md | 2026-11-12 | brief for an old-pipeline step: P4b: best-first graph search with the local detour as a graph edit (DESIGN-G1 §1 |  |
| briefs/P4b-incremental-cost.md | 2026-11-13 | brief for an old-pipeline step: P4b: the level solve's descent uses the incremental exact cost (RECON-incrementa |  |
| briefs/P5-recon-llrr-placement-fail.md | 2026-11-13 | brief for an old-pipeline step: P5 (recon by experiment): the LL_RR near search on fixtures where every offset f |  |
| briefs/P5b-near-reject.md | 2026-11-13 | brief for an old-pipeline step: P5b: the near search rejects doomed offsets early (RECON-llrr-placement-fail.md) |  |
| briefs/P6-recon-corner-join-reject.md | 2026-11-13 | brief for an old-pipeline step: P6 (recon by experiment): the last slow LL_RR hovers: offsets that place but who |  |
| briefs/P6b-descent-in-cone.md | 2026-11-13 | brief for an old-pipeline step: P6b: the level descent stays inside the cone's level ranges (RECON-corner-join-r |  |
| briefs/PF1-prio-row-facts.md | 2026-11-11 | brief for an old-pipeline step: PF1: the reserved prio row meets the tile facts in the solve |  |
| briefs/PH1-port-heights.md | 2026-11-11 | brief for an old-pipeline step: PH1: every edge starts at its start piece's own height (D177 retired, D178, D179 |  |
| briefs/PH1c-finish.md | 2026-11-11 | brief for an old-pipeline step: PH1c: finish PH1 on top of CS1 |  |
| briefs/PR1-prio-row.md | 2026-11-11 | brief for an old-pipeline step: PR1: reserve the prio row in the occupancy chain (DESIGN-N3 §8; D193) |  |
| briefs/PRIO-d108.md | 2026-11-08 | brief for an old-pipeline step: Brief PRIO-d108: no ML switch inside a prio's coverage (D108) |  |
| briefs/PRIO-diag.md | 2026-11-08 | brief for an old-pipeline step: Brief PRIO-diag: why a joiner gets a green while an ML train is inside the prio | DECISIONS D107 |
| briefs/PRIO-reach.md | 2026-11-08 | brief for an old-pipeline step: Brief PRIO-reach: prio reach rule (G9b) and adjacent-signal fix (D107) |  |
| briefs/PROBE-choice-pairing.md | 2026-11-14 | brief for an old-pipeline step: PROBE: place LL² with PSG 227's lane pairing (REPRESENT-psg227 changes 1 and 5;  |  |
| briefs/PROBE-ll2-elastic.md | 2026-11-14 | brief for an old-pipeline step: PROBE: the elastic joint on LL_RR x LL_RR (REVIEW-G4-overprescribed Q3 "Probe E" |  |
| briefs/PROBE-ll2-site.md | 2026-11-14 | brief for an old-pipeline step: PROBE: LL² keep's last conflict (bridge over S-bend, edge 13) and the tunnel cas |  |
| briefs/PS1-prio-sizing-nodes.md | 2026-11-11 | brief for an old-pipeline step: PS1: size the prio coverage on the node model, not the old generator (D119, D137 |  |
| briefs/PS2-prio-tiles-from-rules.md | 2026-11-13 | brief for an old-pipeline step: PS2: the prio length M comes from the game rules, not the legacy sizing |  |
| briefs/QL1-compact-window.md | 2026-11-13 | brief for an old-pipeline step: QL1: a compact junction window that remembers its train and prio (engineer, 2026 |  |
| briefs/QL2-train-across-sessions.md | 2026-11-13 | brief for an old-pipeline step: QL2: the design train and prio remembered across sessions; "spent vs quoted" aft |  |
| briefs/R1-bay-hangover.md | 2026-11-10 | brief for an old-pipeline step: R1: no train hangs back over the upstream split (D83, D88; engineer 2026-11-10) |  |
| briefs/R1-floor-table.md | 2026-11-12 | brief for an old-pipeline step: R1: one floor table for every run constant in placement (DESIGN-G1 §16.1-§16.3 R |  |
| briefs/R1-rig-design-train.md | 2026-11-09 | brief for an old-pipeline step: R1: the rig builds the design train (NUTS Mastermind consist) |  |
| briefs/R2-accel-calibration.md | 2026-11-09 | brief for an old-pipeline step: R2: does StepJunctionTrain accelerate like the real design train? |  |
| briefs/R2-row-chains-parallel.md | 2026-11-12 | brief for an old-pipeline step: R2: row edges as Elem chains, built beside row_pts and checked equal (DESIGN-G1  |  |
| briefs/R3-fan-chains-live.md | 2026-11-12 | brief for an old-pipeline step: R3: in and out edges placed by their chains (Fan elements live), their row_pts r |  |
| briefs/R4-legs-prio-floor.md | 2026-11-12 | brief for an old-pipeline step: R4: undoubled legs on their chains, the prio element as a node floor, and the cu |  |
| briefs/R5-doubled-legs-chains.md | 2026-11-12 | brief for an old-pipeline step: R5: doubled legs placed by their chains (lane pair, HeadsPair); the row blocks g |  |
| briefs/R6-program-roles.md | 2026-11-12 | brief for an old-pipeline step: R6: programs carry roles: no in/leg/out shape check; S-bends tagged lane split,  |  |
| briefs/R7-shift-edit.md | 2026-11-12 | brief for an old-pipeline step: R7: the Shift edit: an S-bend pair inserted into a row edge, its shifted stretch |  |
| briefs/R7b-shift-adjacent.md | 2026-11-13 | brief for an old-pipeline step: R7b: the Shift edit, redone: Shift item adjacent to the edge's row, parent's cut |  |
| briefs/RC0-rule-coverage.md | 2026-11-11 | brief for an old-pipeline step: RC0: which checker rules the node template already guarantees (read-only audit,  |  |
| briefs/RC16-recon-per-row-code.md | 2026-11-13 | brief for an old-pipeline step: RC16 (read-only recon): the per-row and per-kind code that keeps DESIGN-G1 §16.1 |  |
| briefs/RECON-A6-crossers-fans.md | 2026-11-14 | brief for an old-pipeline step: Recon A6: crossers' pieces at item rows, and other rows' port fans (read-only; D |  |
| briefs/RECON-O1-item-rows.md | 2026-11-14 | brief for an old-pipeline step: Recon O1: what stands on each stack item row today, and how it maps to the arran |  |
| briefs/RECON-O3-bands.md | 2026-11-14 | brief for an old-pipeline step: Recon O3: band occupancy, segments (A4), event order and clearance before 2b2b ( |  |
| briefs/RECON-psg197-trace.md | 2026-11-14 | brief for an old-pipeline step: RECON: lane-by-lane trace of the PSG 197 tunnel BBH (V453000) |  |
| briefs/RECON-psg227-trace.md | 2026-11-14 | brief for an old-pipeline step: RECON: lane-by-lane trace of the PSG 227 LLRR BBH |  |
| briefs/REPRESENT-psg227.md | 2026-11-14 | brief for an old-pipeline step: RECON: can our graph model represent mfb's PSG 227 hub exactly? (D283-D286) | DECISIONS D287 |
| briefs/REVIEW-G4-overprescribed.md | 2026-11-14 | brief for an old-pipeline step: REVIEW: why LL_RR x LL_RR does not place, and whether our rules over-prescribe ( | DECISIONS D276/D277/D278 |
| briefs/REVIEW-joint-loop.md | 2026-11-14 | brief for an old-pipeline step: Review: does the joint loop converge, and what should the rule source be? (holis | DECISIONS D253 |
| briefs/RG1-llrr-circuit.md | 2026-11-12 | brief for an old-pipeline step: RG1: the LL_RR seed as a rig test circuit (milestone: LL_RR on flat ground in th |  |
| briefs/RN0c2-turn-programs.md | 2026-11-11 | brief for an old-pipeline step: RN0c2: the flat T's paths as turn programs, and which lengths the rules fix (rea |  |
| briefs/RO1-recon-folder-layout.md | 2026-11-13 | brief for an old-pipeline step: RO1 (read-only recon): a feature folder layout for the junction tool, following  |  |
| briefs/RP0-prio-from-rules.md | 2026-11-11 | brief for an old-pipeline step: RP0: prio length from the rules, not from the old generator's sweep (read-only r |  |
| briefs/RP1-prio-footprint.md | 2026-11-11 | brief for an old-pipeline step: RP1: the prio footprint as tiles relative to the merge (read-only recon for the  |  |
| briefs/RP1-recon-seed-projection.md | 2026-11-12 | brief for an old-pipeline step: RP1 (recon): the G1 seed through the node template and solve (projection) |  |
| briefs/RP2-recon-graph-input.md | 2026-11-12 | brief for an old-pipeline step: RP2 (recon): the node template's input from the graph, not from a layout (D201,  |  |
| briefs/RS1-ramp-staircase.md | 2026-11-11 | brief for an old-pipeline step: RS1: a free diagonal that cannot climb gets a ramp staircase (D196, D200, DESIGN |  |
| briefs/RS1-recon-topology-placement-seam.md | 2026-11-13 | brief for an old-pipeline step: RS1 (read-only recon): the seam between topology and placement inside junction_t |  |
| briefs/RT1-real-terrain-fixtures.md | 2026-11-12 | brief for an old-pipeline step: RT1: real-terrain fixtures from generated maps, and a placement oracle on them ( |  |
| briefs/RT2-hillier-fixtures.md | 2026-11-12 | brief for an old-pipeline step: RT2: regenerate the two "hilly" real-terrain maps with real hills |  |
| briefs/RU1-waiting-room-slots.md | 2026-11-11 | brief for an old-pipeline step: RU1: waiting room and signal slots as template rules (D193; AUDIT-rule-coverage  |  |
| briefs/RW1-items-from-tokens.md | 2026-11-13 | brief for an old-pipeline step: RW1: row item lookups in one helper; tokens carry their items (DESIGN-G1 §16.4 R |  |
| briefs/RW2-approach-span.md | 2026-11-13 | brief for an old-pipeline step: RW2: the joiner's arrival S-bend is sized by item rows like every other S-bend ( |  |
| briefs/RW3-roles-from-graph.md | 2026-11-13 | brief for an old-pipeline step: RW3: edge roles from the graph and tokens, not per-row arrays (DESIGN-G1 §16.4 R |  |
| briefs/RW4a-one-chain-builder.md | 2026-11-13 | brief for an old-pipeline step: RW4a: one token-driven chain builder for every row edge (DESIGN-G1 §16.4 RW4, fi |  |
| briefs/RW4b-row-pts-out.md | 2026-11-13 | brief for an old-pipeline step: RW4b: row_pts removed; every chain point from new_pt in chain order (DESIGN-G1 § |  |
| briefs/RW5-stack-from-edges.md | 2026-11-13 | brief for an old-pipeline step: RW5: the stack built from per-edge needs, not a per-row if-chain (DESIGN-G1 §16. |  |
| briefs/RW6-prio-elem.md | 2026-11-13 | brief for an old-pipeline step: RW6: the prio element is a chain element on the Prio item's row (DESIGN-G1 §16.4 |  |
| briefs/RW6b-prio-one-source.md | 2026-11-13 | brief for an old-pipeline step: RW6b: the signal pass and the level solve read the prio element from the placeme |  |
| briefs/S0-band-headings.md | 2026-11-13 | brief for an old-pipeline step: S0: band chains start on the edge's real heading; row-node relations get their o |  |
| briefs/S0b-band-headings-land.md | 2026-11-13 | brief for an old-pipeline step: S0b: land the band chain heading fix (REVIEW-port-fans F1, F4) with tests that f |  |
| briefs/S1a-occupation.md | 2026-11-08 | brief for an old-pipeline step: S1a: throughput evaluator, part 1: running times, block occupation and the bottl |  |
| briefs/S1b-spillback.md | 2026-11-08 | brief for an old-pipeline step: S1b: throughput evaluator, part 2: queue spillback conflicts |  |
| briefs/S1b2-holdpoints.md | 2026-11-08 | brief for an old-pipeline step: S1b2: queue spillback: held trains only at hold points |  |
| briefs/S1c-prio-timing.md | 2026-11-08 | brief for an old-pipeline step: S1c: throughput evaluator, part 3: prio timing on the real layout (ML delay) |  |
| briefs/S1c2-prio-sweep.md | 2026-11-08 | brief for an old-pipeline step: S1c2: prio timing: every ML arrival time, not just the coverage edge |  |
| briefs/S2-south-fans.md | 2026-11-13 | brief for an old-pipeline step: S2: S branch fans, with "S edge" per port (D230; REVIEW-port-fans F2, F3, F7; Q4 |  |
| briefs/S2a-stub-one-tile.md | 2026-11-09 | brief for an old-pipeline step: S2a: branch stubs one tile (D115) |  |
| briefs/S2b-sbend-roles.md | 2026-11-13 | brief for an old-pipeline step: S2b: commit S2 as far as it goes, then S-bend tokens name the node they serve (D |  |
| briefs/S2b-signal-dedupe.md | 2026-11-09 | brief for an old-pipeline step: S2b: one signal may do several jobs (D117 a, b) |  |
| briefs/S2c-row-roles-main-port.md | 2026-11-13 | brief for an old-pipeline step: S2c: a row's in and out edge meet a main port (REVIEW-port-fans F8), then S2b |  |
| briefs/S2d-skeleton-solver.md | 2026-11-09 | brief for an old-pipeline step: S2d: skeleton solver seam (difference constraints) |  |
| briefs/S2e-solver-lanes.md | 2026-11-09 | brief for an old-pipeline step: S2e: parallelogram lanes from solver positions (D118 stagger), wired into the ca |  |
| briefs/S2f-m-east-gap.md | 2026-11-09 | brief for an old-pipeline step: S2f-m: measure which rule sets the E-side gap (no code change, no commit) |  |
| briefs/S2f-m2-low-prio.md | 2026-11-09 | brief for an old-pipeline step: S2f-m2: measure what rejects the D148 E-side window at short coverage (no code c |  |
| briefs/S2f-sweep-removal.md | 2026-11-09 | brief for an old-pipeline step: S2f: the parallelogram style is built once per height, crossing cost and e_slack |  |
| briefs/S2f2-east-window.md | 2026-11-09 | brief for an old-pipeline step: S2f2: the parallelogram style's E-side window from G9c (D148) |  |
| briefs/S2f2b-east-phase.md | 2026-11-09 | brief for an old-pipeline step: S2f2b: the parallelogram style's E-side window phase (D148a), finishing S2f2 |  |
| briefs/S3a-nodes-served-by-legs.md | 2026-11-13 | brief for an old-pipeline step: S3a: nodes are served by their legs (D234) |  |
| briefs/SB1-series-bridges.md | 2026-11-12 | brief for an old-pipeline step: SB1: series bridges, a lane's crossings cut into bridges of at most two crossers |  |
| briefs/SB1b-bridge-cuts.md | 2026-11-12 | brief for an old-pipeline step: SB1b: series bridges with the cut chosen by placement (D209; DESIGN-G1 §11.9, §1 |  |
| briefs/SC1-terrain-cost-in-score.md | 2026-11-11 | brief for an old-pipeline step: SC1: the tool's score includes the terrain cost of the projected T map |  |
| briefs/SF1-south-fans.md | 2026-11-13 | brief for an old-pipeline step: SF1: fans on S-side branch port edges (D230, D203(1), D38) |  |
| briefs/SF1a-recon-seed-floors.md | 2026-11-12 | brief for an old-pipeline step: SF1a (recon by experiment): the seed's out-fan and doubled-leg floors next to th |  |
| briefs/SF1b-seed-floors.md | 2026-11-12 | brief for an old-pipeline step: SF1b: seed floors from the rules: out-fan gap derived, doubled lane split next t |  |
| briefs/SP1-levels-off-topology.md | 2026-11-13 | brief for an old-pipeline step: SP1: the level solve stops depending on topology.h (identity step) |  |
| briefs/SP2-split-topology-cpp.md | 2026-11-13 | brief for an old-pipeline step: SP2: split junction_topology.{h,cpp} into topology, placement, signals and the p |  |
| briefs/ST1-signal-tokens.md | 2026-11-12 | brief for an old-pipeline step: ST1: signal tokens and run rules on the rendered routes (D211; DESIGN-G1 §12.1-1 |  |
| briefs/ST1a-recon-stretch.md | 2026-11-12 | brief for an old-pipeline step: ST1a (recon by experiment): which relations placement is missing, found by stret |  |
| briefs/SV1-fetch-psg-saves.md | 2026-11-13 | brief for an old-pipeline step: SV1: pull the #openttdcoop showcase save games (PSG) that the Junctionary links  |  |
| briefs/T1-map-driven-organic-t.md | 2026-11-10 | brief for an old-pipeline step: T1: the map-driven organic T, end to end (DESIGN-O4 revision 5, D170) |  |
| briefs/T1-placement-on-positions.md | 2026-11-12 | brief for an old-pipeline step: T1: placement on JunctionPositions with reasons; the hidden choices become fixed |  |
| briefs/T1-tunnels-in-layouts.md | 2026-11-09 | brief for an old-pipeline step: T1: tunnels in layouts, build steps, pricing, checker and rig (D154) | DECISIONS D154 |
| briefs/T1b1-occupancy-relations.md | 2026-11-12 | brief for an old-pipeline step: T1b-1: the two checked missing relations (nested in-fans, bridge head clear of r |  |
| briefs/T1c1-curve-gap-family.md | 2026-11-12 | brief for an old-pipeline step: T1c-1: the curve-gap relation family on every route replaces the site floors (D2 |  |
| briefs/T1c2-occupancy-family.md | 2026-11-12 | brief for an old-pipeline step: T1c-2: the occupancy relation family replaces C1/C3; bridge heads hug by order,  |  |
| briefs/T1d-d215-objective.md | 2026-11-12 | brief for an old-pipeline step: T1d: the D215 objective: every row change pushed toward the feature it serves, a |  |
| briefs/T2-blocked-tiles.md | 2026-11-12 | brief for an old-pipeline step: T2: blocked tiles as run bounds and group domains in the X phase (DESIGN-G1 §14. |  |
| briefs/T3-height-facts.md | 2026-11-12 | brief for an old-pipeline step: T3: height facts in the X phase: run joins, element fits and joins (DESIGN-G1 §1 |  |
| briefs/TL1a-recon-train-length.md | 2026-11-12 | brief for an old-pipeline step: TL1a (recon by experiment): which seed floors do not scale with train length and |  |
| briefs/TL1b-derived-floors.md | 2026-11-12 | brief for an old-pipeline step: TL1b: every placement floor derived from the rules, scaling with train length an |  |
| briefs/TM2-two-steps-per-tick.md | 2026-11-09 | brief for an old-pipeline step: TM2: the train model advances twice per game tick, as the game does |  |
| briefs/TS1-trunk-sbend.md | 2026-11-13 | brief for an old-pipeline step: TS1: a branch trunk may S-bend between its port and a doubled bridge it passes u |  |
| briefs/TT3-loop-seeds.md | 2026-11-14 | brief for an old-pipeline step: TT3: tests follow the hubs the joint loop now seeds (D250, D251) |  |
| briefs/UI1-train-speed-and-window.md | 2026-11-09 | brief for an old-pipeline step: UI1: design speed from the picked train; sizing errors; junction window fixes |  |
| briefs/W1-wide-hub-rules.md | 2026-11-13 | brief for an old-pipeline step: W1: the two wide-hub rules, D238 and D239 (RECON-wide-hubs) |  |
| briefs/X1-cpsat-prototype.md | 2026-11-10 | brief for an old-pipeline step: X1: does a constraint solver build our T in under 100 ms? (off-repo experiment) |  |
| briefs/XB1-cross-once.md | 2026-11-11 | brief for an old-pipeline step: XB1: an edge crosses a crossing node once (D171, D172) |  |
| briefs/YR1-row-move-repair.md | 2026-11-11 | brief for an old-pipeline step: YR1: a heights failure moves the failing piece's node row (Y pin repair) |  |
| briefs/Z2a-terraform-plan.md | 2026-11-12 | brief for an old-pipeline step: Z2a: a terraform plan on a terrain snapshot: corner targets, the game's propagat |  |
| briefs/Z2b-ramp-rule-hard-facts.md | 2026-11-12 | brief for an old-pipeline step: Z2b: the ramp rule from the train (D219), realistic acceleration required, and a |  |
| briefs/Z2c-level-solve.md | 2026-11-12 | brief for an old-pipeline step: Z2c: the height solve: a level per tile on every edge by a shortest path, produc |  |
| briefs/Z2d-recon-joint-levels.md | 2026-11-12 | brief for an old-pipeline step: Z2d (recon by experiment): a joint level solve over the whole junction (DESIGN-G |  |
| briefs/Z2e-joint-levels.md | 2026-11-12 | brief for an old-pipeline step: Z2e: the joint level solve replaces the per-edge DP (RECON-joint-levels; DESIGN- |  |
| briefs/Z2f-prio-tiles-ramp.md | 2026-11-12 | brief for an old-pipeline step: Z2f: prio element tiles may ramp with the main line where they carry only straig |  |
| tools/g3proto/LLRR_x_LLRR.json | 2026-11-13 | G3 joint-solve prototype (old pipeline); RESULT.md in DECISIONS |  |
| tools/g3proto/LLRR_x_LR.json | 2026-11-13 | G3 joint-solve prototype (old pipeline); RESULT.md in DECISIONS |  |
| tools/g3proto/LR_x_LLRR.json | 2026-11-13 | G3 joint-solve prototype (old pipeline); RESULT.md in DECISIONS |  |
| tools/g3proto/LR_x_LRR.json | 2026-11-13 | G3 joint-solve prototype (old pipeline); RESULT.md in DECISIONS |  |
| tools/g3proto/RESULT.md | 2026-11-13 | G3 joint-solve prototype (old pipeline); RESULT.md in DECISIONS |  |
| tools/g3proto/T.json | 2026-11-13 | G3 joint-solve prototype (old pipeline); RESULT.md in DECISIONS |  |
| tools/g3proto/solve.py | 2026-11-13 | G3 joint-solve prototype (old pipeline); RESULT.md in DECISIONS |  |
| wip/2a2-crash-wip.patch | 2026-11-14U | old-pipeline experiment patch/dump (G1-G4, rows, bands, item rows, joint loop) |  |
| wip/RECON-prio-stretch.proposal.patch | 2026-11-14 | old-pipeline experiment patch/dump (G1-G4, rows, bands, item rows, joint loop) |  |
| wip/a6-crossers-fans-dump.txt | 2026-11-14U | old-pipeline experiment patch/dump (G1-G4, rows, bands, item rows, joint loop) |  |
| wip/a6-crossers-fans-probe.patch | 2026-11-14U | old-pipeline experiment patch/dump (G1-G4, rows, bands, item rows, joint loop) |  |
| wip/choice-probe.patch | 2026-11-14 | old-pipeline experiment patch/dump (G1-G4, rows, bands, item rows, joint loop) |  |
| wip/diag-T-render-oldpass.txt | 2026-11-14 | old-pipeline experiment patch/dump (G1-G4, rows, bands, item rows, joint loop) |  |
| wip/f8-lane-split-floor.patch | 2026-11-14 | old-pipeline experiment patch/dump (G1-G4, rows, bands, item rows, joint loop) |  |
| wip/g1d-llrr-footprint-stop.txt | 2026-11-14 | old-pipeline experiment patch/dump (G1-G4, rows, bands, item rows, joint loop) |  |
| wip/g1d-llrr-violations.txt | 2026-11-14 | old-pipeline experiment patch/dump (G1-G4, rows, bands, item rows, joint loop) |  |
| wip/g2b-attempt.patch | 2026-11-13 | old-pipeline experiment patch/dump (G1-G4, rows, bands, item rows, joint loop) |  |
| wip/g34a-stopped.patch | 2026-11-13 | old-pipeline experiment patch/dump (G1-G4, rows, bands, item rows, joint loop) |  |
| wip/g3p-dump-hook.patch | 2026-11-14 | old-pipeline experiment patch/dump (G1-G4, rows, bands, item rows, joint loop) |  |
| wip/h1c4-d303-nesting-proto.patch | 2026-11-15 | old-pipeline experiment patch/dump (G1-G4, rows, bands, item rows, joint loop) |  |
| wip/n1-experiment.patch | 2026-11-14 | old-pipeline experiment patch/dump (G1-G4, rows, bands, item rows, joint loop) |  |
| wip/o1-item-rows-dump.txt | 2026-11-14U | old-pipeline experiment patch/dump (G1-G4, rows, bands, item rows, joint loop) |  |
| wip/o1-item-rows-probe.patch | 2026-11-14U | old-pipeline experiment patch/dump (G1-G4, rows, bands, item rows, joint loop) |  |
| wip/p3b-llrr.txt | 2026-11-14 | old-pipeline experiment patch/dump (G1-G4, rows, bands, item rows, joint loop) |  |
| wip/probe-ll2-elastic.patch | 2026-11-14 | old-pipeline experiment patch/dump (G1-G4, rows, bands, item rows, joint loop) |  |
| wip/probe-ll2/i1-any-r0.txt | 2026-11-14 | G4-era LL² probe output (REVIEW-G4 probes), old checker |  |
| wip/probe-ll2/i1-any-r1.txt | 2026-11-14 | G4-era LL² probe output (REVIEW-G4 probes), old checker |  |
| wip/probe-ll2/i2-keep-r0.txt | 2026-11-14 | G4-era LL² probe output (REVIEW-G4 probes), old checker |  |
| wip/probe-ll2/i2-keep-r1.txt | 2026-11-14 | G4-era LL² probe output (REVIEW-G4 probes), old checker |  |
| wip/probe-ll2/i3-keep-gap7-r0-layout.json | 2026-11-14 | G4-era LL² probe output (REVIEW-G4 probes), old checker |  |
| wip/probe-ll2/i3-keep-gap7-r0.txt | 2026-11-14 | G4-era LL² probe output (REVIEW-G4 probes), old checker |  |
| wip/probe-ll2/i3-keep-gap7-r1-layout.json | 2026-11-14 | G4-era LL² probe output (REVIEW-G4 probes), old checker |  |
| wip/probe-ll2/i3-keep-gap7-r1.txt | 2026-11-14 | G4-era LL² probe output (REVIEW-G4 probes), old checker |  |
| wip/probe-ll2/i4-keep-gap7-prio9-r0-layout.json | 2026-11-14 | G4-era LL² probe output (REVIEW-G4 probes), old checker |  |
| wip/probe-ll2/i4-keep-gap7-prio9-r0.txt | 2026-11-14 | G4-era LL² probe output (REVIEW-G4 probes), old checker |  |
| wip/probe-ll2/i4-keep-gap7-prio9-r1-layout.json | 2026-11-14 | G4-era LL² probe output (REVIEW-G4 probes), old checker |  |
| wip/probe-ll2/i4-keep-gap7-prio9-r1.txt | 2026-11-14 | G4-era LL² probe output (REVIEW-G4 probes), old checker |  |
| wip/probe-ll2/i5-keep-gap7-tunnels-r0-layout.json | 2026-11-14 | G4-era LL² probe output (REVIEW-G4 probes), old checker |  |
| wip/probe-ll2/i5-keep-gap7-tunnels-r0.txt | 2026-11-14 | G4-era LL² probe output (REVIEW-G4 probes), old checker |  |
| wip/probe-ll2/i5-keep-gap7-tunnels-r1-layout.json | 2026-11-14 | G4-era LL² probe output (REVIEW-G4 probes), old checker |  |
| wip/probe-ll2/i5-keep-gap7-tunnels-r1.txt | 2026-11-14 | G4-era LL² probe output (REVIEW-G4 probes), old checker |  |
| wip/probe-ll2/rerun-gap7/i3-keep-gap7-r0-layout.json | 2026-11-14 | G4-era LL² probe output (REVIEW-G4 probes), old checker |  |
| wip/probe-ll2/rerun-gap7/i3-keep-gap7-r0.txt | 2026-11-14 | G4-era LL² probe output (REVIEW-G4 probes), old checker |  |
| wip/probe-ll2/rerun-gap7/i3-keep-gap7-r1-layout.json | 2026-11-14 | G4-era LL² probe output (REVIEW-G4 probes), old checker |  |
| wip/probe-ll2/rerun-gap7/i3-keep-gap7-r1.txt | 2026-11-14 | G4-era LL² probe output (REVIEW-G4 probes), old checker |  |
| wip/probe-ll2/rerun-gap7/i4-keep-gap7-prio9-r0-layout.json | 2026-11-14 | G4-era LL² probe output (REVIEW-G4 probes), old checker |  |
| wip/probe-ll2/rerun-gap7/i4-keep-gap7-prio9-r0.txt | 2026-11-14 | G4-era LL² probe output (REVIEW-G4 probes), old checker |  |
| wip/probe-ll2/rerun-gap7/i4-keep-gap7-prio9-r1-layout.json | 2026-11-14 | G4-era LL² probe output (REVIEW-G4 probes), old checker |  |
| wip/probe-ll2/rerun-gap7/i4-keep-gap7-prio9-r1.txt | 2026-11-14 | G4-era LL² probe output (REVIEW-G4 probes), old checker |  |
| wip/probe-ll2/rerun-gap7/i5-keep-gap7-tunnels-r0-layout.json | 2026-11-14 | G4-era LL² probe output (REVIEW-G4 probes), old checker |  |
| wip/probe-ll2/rerun-gap7/i5-keep-gap7-tunnels-r0.txt | 2026-11-14 | G4-era LL² probe output (REVIEW-G4 probes), old checker |  |
| wip/probe-ll2/rerun-gap7/i5-keep-gap7-tunnels-r1-layout.json | 2026-11-14 | G4-era LL² probe output (REVIEW-G4 probes), old checker |  |
| wip/probe-ll2/rerun-gap7/i5-keep-gap7-tunnels-r1.txt | 2026-11-14 | G4-era LL² probe output (REVIEW-G4 probes), old checker |  |
| wip/probe-ll2/summary.txt | 2026-11-14 | G4-era LL² probe output (REVIEW-G4 probes), old checker | PROBE-ladders2.md, PROBE-ladders4.md, PROBE-ladders5.md |
| wip/r3d-stopped.patch | 2026-11-14 | old-pipeline experiment patch/dump (G1-G4, rows, bands, item rows, joint loop) |  |
| wip/recon-o3-probe.patch | 2026-11-14U | old-pipeline experiment patch/dump (G1-G4, rows, bands, item rows, joint loop) |  |
| wip/recon-o3/dump-LL2_100100000.txt | 2026-11-14U | O3 band-occupancy recon dumps/queries (old pipeline) |  |
| wip/recon-o3/dump-LLRR_LR.txt | 2026-11-14U | O3 band-occupancy recon dumps/queries (old pipeline) |  |
| wip/recon-o3/dump-LR_LLRR.txt | 2026-11-14U | O3 band-occupancy recon dumps/queries (old pipeline) |  |
| wip/recon-o3/dump-T.txt | 2026-11-14U | O3 band-occupancy recon dumps/queries (old pipeline) |  |
| wip/recon-o3/parse.py | 2026-11-14U | O3 band-occupancy recon dumps/queries (old pipeline) | PROBE-ladders4.md, PROBE-ladders5.md |
| wip/recon-o3/q1.py | 2026-11-14U | O3 band-occupancy recon dumps/queries (old pipeline) |  |
| wip/recon-o3/q1b.py | 2026-11-14U | O3 band-occupancy recon dumps/queries (old pipeline) |  |
| wip/recon-o3/q2.py | 2026-11-14U | O3 band-occupancy recon dumps/queries (old pipeline) |  |
| wip/recon-o3/q4.py | 2026-11-14U | O3 band-occupancy recon dumps/queries (old pipeline) |  |
| wip/recon-o3/q4b.py | 2026-11-14U | O3 band-occupancy recon dumps/queries (old pipeline) |  |
| wip/recon-o3/q4c.py | 2026-11-14U | O3 band-occupancy recon dumps/queries (old pipeline) |  |
| wip/recon-o3/q4d.py | 2026-11-14U | O3 band-occupancy recon dumps/queries (old pipeline) |  |
| wip/recon-o3/q4e.py | 2026-11-14U | O3 band-occupancy recon dumps/queries (old pipeline) |  |
| wip/recon-o3/q5.py | 2026-11-14U | O3 band-occupancy recon dumps/queries (old pipeline) |  |
| wip/recon-o3/q7.py | 2026-11-14U | O3 band-occupancy recon dumps/queries (old pipeline) |  |
| wip/recon-o3/q7b.py | 2026-11-14U | O3 band-occupancy recon dumps/queries (old pipeline) |  |
| wip/recon-o3/runjt.sh | 2026-11-14U | O3 band-occupancy recon dumps/queries (old pipeline) |  |
| wip/recon-o3/seg.py | 2026-11-14U | O3 band-occupancy recon dumps/queries (old pipeline) |  |
| wip/recon-o3/seg2.py | 2026-11-14U | O3 band-occupancy recon dumps/queries (old pipeline) |  |
| wip/recon-o3/seg3.py | 2026-11-14U | O3 band-occupancy recon dumps/queries (old pipeline) |  |
| wip/s3a-stopped.patch | 2026-11-14 | old-pipeline experiment patch/dump (G1-G4, rows, bands, item rows, joint loop) |  |
| wip/sf1-stopped.patch | 2026-11-14 | old-pipeline experiment patch/dump (G1-G4, rows, bands, item rows, joint loop) |  |
| wip/st1-llrr.txt | 2026-11-14 | old-pipeline experiment patch/dump (G1-G4, rows, bands, item rows, joint loop) |  |
| wip/st1-t.txt | 2026-11-14 | old-pipeline experiment patch/dump (G1-G4, rows, bands, item rows, joint loop) |  |
| wip/st1a-experiment.patch | 2026-11-14 | old-pipeline experiment patch/dump (G1-G4, rows, bands, item rows, joint loop) |  |
| wip/st2-llrr.txt | 2026-11-14 | old-pipeline experiment patch/dump (G1-G4, rows, bands, item rows, joint loop) |  |
| wip/tl1a-experiment.patch | 2026-11-14 | old-pipeline experiment patch/dump (G1-G4, rows, bands, item rows, joint loop) |  |
| wip/z2d-joint-levels.patch | 2026-11-14 | old-pipeline experiment patch/dump (G1-G4, rows, bands, item rows, joint loop) |  |

## REMOVE-done (217)

| file | date | reason | referenced by |
|---|---|---|---|
| BENCH-B0.md | 2026-11-11 | diag/bench/cards/plan for a finished step: B0: baseline hover timings (GenerateJunctionNear) on the Buborough save |  |
| CARDS-LLRR.md | 2026-11-11 | diag/bench/cards/plan for a finished step: LL_RR: decision cards for the engineer (2026-11-11) | DECISIONS D186 |
| CARDS-crossings.md | 2026-11-13 | diag/bench/cards/plan for a finished step: Cards for the engineer: crossings D223 does not cover (CX1 step 0, 2026-11-13) | DECISIONS D227 |
| CHECKS-1b.md | 2026-11-13 | diag/bench/cards/plan for a finished step: Manual checks for brief 1b step E (engineer, step F) |  |
| PERF2.md | 2026-11-11 | diag/bench/cards/plan for a finished step: PERF2: GenerateJunctionNear wall time (Release clang-18, head 7b853cf7e5, median |  |
| PLAN-1a.md | 2026-11-07 | diag/bench/cards/plan for a finished step: Milestone 1a: junction placement |  |
| PLAN-1b.md | 2026-11-07 | diag/bench/cards/plan for a finished step: Milestone 1b: T-junction generator |  |
| PLAN-D78.md | 2026-11-08 | diag/bench/cards/plan for a finished step: D78: place-time generation on real terrain | DECISIONS D93 |
| PLAN-D96.md | 2026-11-08 | diag/bench/cards/plan for a finished step: D96: near-real-time hover (plan) |  |
| PLAN-S2.md | 2026-11-09 | diag/bench/cards/plan for a finished step: PLAN S2: skeleton solver for the W-side doubled section |  |
| RECON-folder-layout.md | 2026-11-13 | diag/bench/cards/plan for a finished step: RECON: folder layout for the junction tool (RO1) |  |
| briefs/1a-step1.md | 2026-11-07 | brief for a finished step (result in code/DECISIONS): Brief: 1a step 1, API skeleton and outer test |  |
| briefs/1a-step2.md | 2026-11-07 | brief for a finished step (result in code/DECISIONS): Brief: 1a step 2, test junction fixture and build steps |  |
| briefs/1a-step3-4.md | 2026-11-07 | brief for a finished step (result in code/DECISIONS): Brief: 1a steps 3+4, estimate and transforms |  |
| briefs/1a-step5.md | 2026-11-07 | brief for a finished step (result in code/DECISIONS): Brief: 1a step 5, game-side junction pricing |  |
| briefs/1a-step6.md | 2026-11-07 | brief for a finished step (result in code/DECISIONS): Brief: 1a step 6, drawing seams for the ghost |  |
| briefs/1a-step7.md | 2026-11-07 | brief for a finished step (result in code/DECISIONS): Brief: 1a step 7, ghost store and viewport hook |  |
| briefs/1a-step8.md | 2026-11-07 | brief for a finished step (result in code/DECISIONS): Brief: 1a step 8, junction tool window, hover ghost and price tooltip |  |
| briefs/1a-step8b.md | 2026-11-07 | brief for a finished step (result in code/DECISIONS): Brief: 1a step 8b, opaque ghost and bridge type for unlimited rail |  |
| briefs/1a-step9.md | 2026-11-07 | brief for a finished step (result in code/DECISIONS): Brief: 1a step 9, building the junction (sequencer, callback, Retry and Cancel) |  |
| briefs/1a-step9b.md | 2026-11-07 | brief for a finished step (result in code/DECISIONS): Brief: 1a step 9b, cost line in the Junction window |  |
| briefs/1a-step9c.md | 2026-11-07 | brief for a finished step (result in code/DECISIONS): Brief: 1a step 9c, size the cost line for large amounts |  |
| briefs/1a-step9d.md | 2026-11-07 | brief for a finished step (result in code/DECISIONS): Brief: 1a step 9d, don't count clearing twice under bridges |  |
| briefs/1a-step9e.md | 2026-11-07 | brief for a finished step (result in code/DECISIONS): Brief: 1a step 9e, Shift-click shows the estimate instead of building |  |
| briefs/1a-step9g.md | 2026-11-07 | brief for a finished step (result in code/DECISIONS): Brief: 1a step 9g, opaque invalid ghost and pinned footprint |  |
| briefs/1b-stepA.md | 2026-11-07 | brief for a finished step (result in code/DECISIONS): Brief 1b step A: foundations |  |
| briefs/1b-stepB.md | 2026-11-07 | brief for a finished step (result in code/DECISIONS): Brief 1b step B: the rule checker |  |
| briefs/1b-stepB2.md | 2026-11-07 | brief for a finished step (result in code/DECISIONS): Brief 1b step B2: streamlined prio joins in the checker |  |
| briefs/1b-stepB3.md | 2026-11-07 | brief for a finished step (result in code/DECISIONS): Brief 1b step B3: the first prio join on the merge tile |  |
| briefs/1b-stepB4.md | 2026-11-07 | brief for a finished step (result in code/DECISIONS): Brief 1b step B4: bridge signals and the stub-free merge join |  |
| briefs/1b-stepB5.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief 1b step B5: doubled bridges and whole-train bays in the checker |  |
| briefs/1b-stepB5b.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief 1b step B5b: bay boundaries per D88, then the doubled acceptance layout |  |
| briefs/1b-stepBM-bridgemargin.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief 1b step BM: sweep the doubled-bridge head margin (D103) |  |
| briefs/1b-stepD1.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief 1b step D1: route search core (`FindJunctionRoute`) |  |
| briefs/1b-stepD109-clusters.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief 1b step D109: one doubled section per crossing cluster |  |
| briefs/1b-stepD2.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief 1b step D2: signal and prio pass (`PlaceJunctionSignals`) |  |
| briefs/1b-stepD3a.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief 1b step D3a: build one generator candidate (`BuildJunctionCandidate`) |  |
| briefs/1b-stepD3b.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief 1b step D3b: `GenerateJunction`, sweep and scoring |  |
| briefs/1b-stepD3c.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief 1b step D3c: early branch swap as a sweep option (D91) |  |
| briefs/1b-stepD3d-diag.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief 1b step D3d-diag: why ml_gap 1 with branch_gap 0 gives no layout at TL3 an |  |
| briefs/1b-stepD3d-diag2.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief 1b step D3d-diag2: why TL3 (ml_gap 1, branch_gap 0) gives no layout |  |
| briefs/1b-stepE.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief 1b step E: the Junction window uses the generator |  |
| briefs/1b-stepF1.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief 1b step F1: test circuit and `ottd junction-check` |  |
| briefs/1b-stepPG-parallelogram.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief 1b step PG: parallelogram doubled bridges as a second lane style (D105) |  |
| briefs/1b-stepR1.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief 1b step R1: turn penalty in the router |  |
| briefs/1b-stepS1.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief 1b step S1: signal distance as drag-signalling (D97) |  |
| briefs/1b-stepY-symmetric.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief 1b step Y: symmetric doubled ML bridges (D100) |  |
| briefs/1c-step1-recon.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief 1c step 1: recon of train acceleration, signals and the rig's traffic tool |  |
| briefs/1c-stepM1-model.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief 1c step M1: pure train performance model (realistic acceleration, flat tra |  |
| briefs/1c-stepM2-priosolver.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief 1c step M2: prio length solver (D102) |  |
| briefs/1c-stepM3-picktrain.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief 1c step M3: pick an actual train in the Junction window (D101) |  |
| briefs/1c-stepP-priolen.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief 1c step PL: prio length as a multiple of TL (stopgap experiment) |  |
| briefs/1c-stepP2-singletile.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief 1c step PL2: prio coverage skips single-tile blocks (D99) |  |
| briefs/1c-stepP3-d99a.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief 1c step PL3: prio coverage exempts only unjoinable blocks (D99a) |  |
| briefs/1c-stepR1-traffic.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief 1c step R1: rig traffic run with a waiting heatmap |  |
| briefs/1c-stepR2-routes.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief 1c step R2: traffic with fixed routes, and an "ML train stopped" measure |  |
| briefs/AUDIT-assumptions.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief AUDIT: every hand-coded assumption in the generator and checker (read-only |  |
| briefs/CM1-claude-md.md | 2026-11-13 | brief for a finished step (result in code/DECISIONS): CM1: src/CLAUDE.md: the junction tool's current rules as present-tense facts (en |  |
| briefs/CM1-standalone-claude-md.md | 2026-11-13 | brief for a finished step (result in code/DECISIONS): CM1: the junction CLAUDE.md files stand alone (D232) |  |
| briefs/CM2-verify-claude-md.md | 2026-11-13 | brief for a finished step (result in code/DECISIONS): CM2: verify and correct the CLAUDE.md draft line by line |  |
| briefs/CM3-verify-claude-md-rest.md | 2026-11-13 | brief for a finished step (result in code/DECISIONS): CM3: verify the CLAUDE.md draft lines CM2 did not check |  |
| briefs/CM4-split-claude-md.md | 2026-11-13 | brief for a finished step (result in code/DECISIONS): CM4: split the CLAUDE.md draft into per-folder CLAUDE.md files |  |
| briefs/D78-stepT1.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief D78 step T1: terrain snapshot and router support |  |
| briefs/D78-stepT2.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief D78 step T2: the generator on terrain |  |
| briefs/D78-stepT3a.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief D78 step T3a: orientation mapping, hub offset, prio terrain check |  |
| briefs/D78-stepT3b.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief D78 step T3b: map terrain snapshot and the plane rule in the estimate |  |
| briefs/D78-stepT4.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief D78 step T4: generate at the hovered spot on a worker thread |  |
| briefs/D78-stepT5.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief D78 step T5: rig check on hilly maps |  |
| briefs/D78-stepT6.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief D78 step T6: search near the cursor when the spot doesn't fit |  |
| briefs/D96-stepP1.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief D96 step P1: profile junction generation |  |
| briefs/D96-stepP2.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief D96 step P2: router data structures (same routes, faster) |  |
| briefs/D96-stepP3.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief D96 step P3: router inner loop (same routes, faster) |  |
| briefs/D96-stepP4.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief D96 step P4: parallel candidate sweep (same results) |  |
| briefs/D96-stepP5.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief D96 step P5: candidate library and per-hover fit test (instant tier) |  |
| briefs/D96-stepP6.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief D96 step P6: two-tier hover in the Junction window |  |
| briefs/DF1-diagonal-facts.md | 2026-11-11 | brief for a finished step (result in code/DECISIONS): DF1: a free diagonal's interior tiles meet the facts in the solve |  |
| briefs/ES1-edit-soundness-sweep.md | 2026-11-13 | brief for a finished step (result in code/DECISIONS): ES1: edit-soundness sweep on flat ground (REVIEW-opus-since-terrain-placement to |  |
| briefs/ES2-edit-holes.md | 2026-11-13 | brief for a finished step (result in code/DECISIONS): ES2: close the two placement holes ES1's edit sweep found, and commit the sweep  |  |
| briefs/F1-near-frame-search.md | 2026-11-13 | brief for a finished step (result in code/DECISIONS): F1: near-cursor frame search in the hover (D197; RECON-snap-negotiate E5; review |  |
| briefs/GEOM-d110.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief GEOM-d110: tight split and bridge geometry at the doubled-bridge entry |  |
| briefs/GEOM-d110b.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief GEOM-d110b: SL split first with a signal gap, and a tight shift-back (D110 |  |
| briefs/IC1-recon-tool-icons.md | 2026-11-13 | brief for a finished step (result in code/DECISIONS): IC1 (read-only recon): icons for the new rail-toolbar tools (junction, junction  |  |
| briefs/M1-template-as-map.md | 2026-11-10 | brief for a finished step (result in code/DECISIONS): M1: the template junction as a map, projected (DESIGN-O4 revision 5, D170, D170a |  |
| briefs/NM1-recon-notation-matrix.md | 2026-11-13 | brief for a finished step (result in code/DECISIONS): NM1 (recon by experiment): which notation pairs the pipeline builds today, and w |  |
| briefs/ORACLE-psg227.md | 2026-11-14 | brief for a finished step (result in code/DECISIONS): ORACLE: run our rule checker on the real PSG 227 BBH (REVIEW-G4-overprescribed Q |  |
| briefs/P1-graph-input.md | 2026-11-12 | brief for a finished step (result in code/DECISIONS): P1: the node template's input built from the graph, with tags (DESIGN-G1 §13, §1 |  |
| briefs/P1-line-router-overhead.md | 2026-11-10 | brief for a finished step (result in code/DECISIONS): P1: where the line router's per-request time goes (recon, no commit) |  |
| briefs/P1-recon-level-solve-cost.md | 2026-11-13 | brief for a finished step (result in code/DECISIONS): P1 (recon by experiment): why the level solve costs ~7 ms, and how to cut the le |  |
| briefs/P1b-tag-boundaries.md | 2026-11-12 | brief for a finished step (result in code/DECISIONS): P1b: route tags end each edge at its node, and copy names the program (DESIGN-G1 |  |
| briefs/RECON-corpus-anatomy.md | 2026-11-14 | brief for a finished step (result in code/DECISIONS): RECON corpus: the anatomy of real balanced all-way hubs (D277) | DECISIONS D277 |
| briefs/RECON-corpus-ll2-find.md | 2026-11-14 | brief for a finished step (result in code/DECISIONS): RECON corpus: find real LL_RR x LL_RR junctions in the PSG saves (engineer: "We  |  |
| briefs/SPEED-cache.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): SPEED-cache: memoise router searches within one generation |  |
| briefs/SPEED-pass.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief SPEED-pass: get generation fast again, with identical output |  |
| briefs/SPEED-router.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): SPEED-router: flat structures inside the router search |  |
| briefs/ST2-prio-element.md | 2026-11-12 | brief for a finished step (result in code/DECISIONS): ST2: the prio element (D211 exception; DESIGN-G1 §12.4; RECON-prio-element) |  |
| briefs/TC1-test-cleanup.md | 2026-11-11 | brief for a finished step (result in code/DECISIONS): TC1: junction test cleanup and the [junctionmap] budget |  |
| briefs/TP1-topology-window.md | 2026-11-13 | brief for a finished step (result in code/DECISIONS): TP1: the topology choices in their own window; a presets dropdown in the junctio |  |
| briefs/TT1-test-time.md | 2026-11-13 | brief for a finished step (result in code/DECISIONS): TT1: measure test times on a quiet machine and bring tags back under budget (TES |  |
| briefs/TU0-recon-tunnel-crossings.md | 2026-11-13 | brief for a finished step (result in code/DECISIONS): TU0 (recon): tunnels as crossings in the graph pipeline (D220) |  |
| briefs/TU1-tunnel-flag-tokens.md | 2026-11-13 | brief for a finished step (result in code/DECISIONS): TU1: a crossing can be a tunnel in the structure and programs (D220; RECON-tunne |  |
| briefs/TU2-tunnel-placement.md | 2026-11-13 | brief for a finished step (result in code/DECISIONS): TU2: tunnel separations are placed (portals as the separation's heads) (D220; RE |  |
| briefs/TU3-tunnel-render.md | 2026-11-13 | brief for a finished step (result in code/DECISIONS): TU3: tunnel separations render and signal (D220; RECON-tunnel-crossings.md step  |  |
| briefs/TU4-tunnel-levels.md | 2026-11-13 | brief for a finished step (result in code/DECISIONS): TU4: the level solve plans tunnel portals and the ground over a tunnel (D220; RE |  |
| briefs/TU5-tunnel-flip.md | 2026-11-13 | brief for a finished step (result in code/DECISIONS): TU5: a bridge whose span ground is too high is tried as a tunnel (D220; RECON-tu |  |
| briefs/TUNE-footprint.md | 2026-11-08 | brief for a finished step (result in code/DECISIONS): Brief TUNE-footprint: tightest safe joiner positions (drop the fixed margins) |  |
| d3c-branch-swap.patch | 2026-11-08 | old-pipeline WIP patch, never merged or finished |  |
| hf1-wip.patch | 2026-11-11 | old-pipeline WIP patch, never merged or finished |  |
| wip/BRIEF-2a-gaps.md | 2026-11-17 | brief for a landed or abandoned step: Brief 2a: gap steps from the channel graph (test-first) |  |
| wip/BRIEF-2b-steps.md | 2026-11-17 | brief for a landed or abandoned step: Brief 2b: the composer builds jogs from the gap steps (no jog switches) |  |
| wip/BRIEF-2c-pieces.md | 2026-11-17 | brief for a landed or abandoned step: Brief 2c: fans and bundle leads as required pieces (no '+'/'-' switches) |  |
| wip/BRIEF-2d-fray-lanes.md | 2026-11-17 | brief for a landed or abandoned step: WITHDRAWN 2026-11-17: contradicts D331 (doubling by flow share, not prio flag).  |  |
| wip/BRIEF-2e-merge-steps.md | 2026-11-17 | brief for a landed or abandoned step: Brief 2e: a one-lane merge's exit step from the gap rule (no merge switches) |  |
| wip/BRIEF-2e2-merge-steps.md | 2026-11-17 | brief for a landed or abandoned step: Brief 2e-2: one-lane merge exit steps, one per pinned gap (no merge switches) |  |
| wip/BRIEF-3a-region-sides.md | 2026-11-17 | brief for a landed or abandoned step: Brief 3a: clash sides from the topology (regions, ring, leg order) |  |
| wip/BRIEF-3b-face-seps.md | 2026-11-17 | brief for a landed or abandoned step: Brief 3b-1: region separations up front (experiment knob, measured) |  |
| wip/BRIEF-L1-facemap.md | 2026-11-18 | brief for a landed or abandoned step: L1: face map from the variant's channels, faces with runs (DESIGN-tracks §9 L1) |  |
| wip/BRIEF-L2-ladders.md | 2026-11-18 | brief for a landed or abandoned step: L2: ladders in the mapped solve (DESIGN-tracks §9 L2+L3, one step) |  |
| wip/BRIEF-L3-warmstart.md | 2026-11-18 | brief for a landed or abandoned step: L3: warm-start the ladder model from the accepted solution |  |
| wip/BRIEF-P1-prio-checker.md | 2026-11-18 | brief for a landed or abandoned step: P1: CheckPrio follows the ML piece by piece (D342) |  |
| wip/BRIEF-P2-prio-signals.md | 2026-11-18 | brief for a landed or abandoned step: P2: prio signals follow the main line piece by piece (D342) |  |
| wip/BRIEF-P2s-prio-search.md | 2026-11-18 | brief for a landed or abandoned step: P2s: prio joins and signals by search, judged by the checker (D345) |  |
| wip/BRIEF-S1-shape-input.md | 2026-11-18 | brief for a landed or abandoned step: S1: the shape as an input (DESIGN-tracks §12 S1, D340) |  |
| wip/BRIEF-S1-slots.md | 2026-11-18 | brief for a landed or abandoned step: Brief S1: the slot layer as production code (`BuildPlanarSlots`), no composer ch |  |
| wip/BRIEF-S2-shape-program.md | 2026-11-18 | brief for a landed or abandoned step: S2: the shape program (DESIGN-tracks §12 S2, D340) |  |
| wip/BRIEF-S2b-finish.md | 2026-11-18 | brief for a landed or abandoned step: S2b finish: measure and commit (the machine crashed mid-run) |  |
| wip/BRIEF-S2b-land.md | 2026-11-18 | brief for a landed or abandoned step: S2b land: side rule only (drop the angle cost), measure against saved before run |  |
| wip/BRIEF-S2b-shape-rules.md | 2026-11-18 | brief for a landed or abandoned step: S2b: shape program: crossing direction from the topology, prefer right-angle cro |  |
| wip/BRIEF-S3a-program-shape.md | 2026-11-18 | brief for a landed or abandoned step: S3a: solve with the program's shape (switch, measured) |  |
| wip/BRIEF-S3b-score-shapes.md | 2026-11-18 | brief for a landed or abandoned step: S3b: score both shapes per variant (D341) |  |
| wip/BRIEF-anchor.md | 2026-11-18 | brief for a landed or abandoned step: Step: hard anchor in the mapped composer (lands probe wip/PROBE-anchor.md) |  |
| wip/BRIEF-check-rungorder.md | 2026-11-17 | brief for a landed or abandoned step: Check: does the chord drawing predict the order of events across each corridor? |  |
| wip/BRIEF-diag-S2b-loss.md | 2026-11-18 | brief for a landed or abandoned step: Diagnosis: why S2b loses T2's 862.25 and T3's 1933.25 |  |
| wip/BRIEF-diag-ladders.md | 2026-11-17 | brief for a landed or abandoned step: Diagnosis: why rungs miss T2 f1's C/F clashes, and the slack -1 rows |  |
| wip/BRIEF-diag-ll2shape.md | 2026-11-18 | brief for a landed or abandoned step: Diagnosis: why the shape program's shapes fail or grow on LL² |  |
| wip/BRIEF-dry-segfault.md | 2026-11-18 | brief for a landed or abandoned step: Step: the PLANAR_DRY segfault on LL², pinned by AddressSanitizer, then fixed tes |  |
| wip/BRIEF-dry-threads.md | 2026-11-18 | brief for a landed or abandoned step: Step: PLANAR_DRY prints one line per variant, in order, with bounded threads |  |
| wip/BRIEF-probe-anchor.md | 2026-11-18 | brief for a landed or abandoned step: Probe: hard anchor (one port point pinned, box low edges free) in the mapped com |  |
| wip/BRIEF-probe-corridors.md | 2026-11-17 | brief for a landed or abandoned step: Probe: corridor walls as up-front non-overlap rows (validity, then effect) |  |
| wip/BRIEF-probe-flowturns.md | 2026-11-18 | brief for a landed or abandoned step: Probe: put a crossing's turn where the flow puts it (T2), solve, compare area |  |
| wip/BRIEF-probe-joiner-over.md | 2026-11-18 | brief for a landed or abandoned step: Probe B: joiner over, main-line lanes S-bend under, in the n-lane prio join (D34 |  |
| wip/BRIEF-probe-ladderbends.md | 2026-11-18 | brief for a landed or abandoned step: Probe: free bends in the ladder solve, clash loop only in the rough |  |
| wip/BRIEF-probe-ladders.md | 2026-11-17 | brief for a landed or abandoned step: Probe: ladders from a rough solve replace the clash loop (fixed shapes first) |  |
| wip/BRIEF-probe-ladders2.md | 2026-11-17 | brief for a landed or abandoned step: Probe: ladders everywhere (box-edge corridors, bridge copies as walls) |  |
| wip/BRIEF-probe-ladders3.md | 2026-11-17 | brief for a landed or abandoned step: Probe: ladders with the flipped-triangle face map, no copies as walls |  |
| wip/BRIEF-probe-ladders4.md | 2026-11-17 | brief for a landed or abandoned step: Probe: ladders on every face boundary run, rung spacing from the clash rule |  |
| wip/BRIEF-probe-ladders5.md | 2026-11-18 | brief for a landed or abandoned step: Probe: ladders 5: stretches within a run, bridge copies as walls with deck middl |  |
| wip/BRIEF-probe-noclash.md | 2026-11-18 | brief for a landed or abandoned step: Probe: retire the clash loop: rough solve without clash separation, ladders unti |  |
| wip/BRIEF-probe-prio-bridge.md | 2026-11-18 | brief for a landed or abandoned step: Probe: prio coverage over a doubled bridge (D347), T2/T3 |  |
| wip/BRIEF-probe-prio-bridge2.md | 2026-11-18 | brief for a landed or abandoned step: Probe A2: prio coverage over a doubled bridge, near lanes' row reservation dropp |  |
| wip/BRIEF-probe-prio-open.md | 2026-11-18 | brief for a landed or abandoned step: Probe C: prio channel crossings open to over/under, explored by the variant sear | pre-rewrite STATUS 1-40 |
| wip/BRIEF-probe-prio-row.md | 2026-11-18 | brief for a landed or abandoned step: Probe A3: the prio row as a gapped offset copy of the near lane (DESIGN-tracks 1 | pre-rewrite STATUS 1-40 |
| wip/BRIEF-probe-prio-spread.md | 2026-11-18 | brief for a landed or abandoned step: Probe A4: A3 with the prio join's lanes spread one line wider (option b of wip/P |  |
| wip/BRIEF-probe-roughuse.md | 2026-11-18 | brief for a landed or abandoned step: Probe: warm start and variant pruning by bound (DESIGN-tracks §10 items 1, 2) |  |
| wip/BRIEF-probe-zerostep.md | 2026-11-17 | brief for a landed or abandoned step: Probe: one-lane merge exit steps as always-present diagonals allowed zero length |  |
| wip/BRIEF-recon-prio-bridge-rule.md | 2026-11-18 | brief for a landed or abandoned step: Recon: what coop means by "not bridging the prioritised line" (read-only) |  |
| wip/BRIEF-recon-prio-bridge.md | 2026-11-18 | brief for a landed or abandoned step: Recon: the coop prio-over-a-bridge structure, in our terms (read-only) |  |
| wip/BRIEF-recon-prio-under.md | 2026-11-18 | brief for a landed or abandoned step: Recon: what it takes to put the other line over a prio main line (D347b), read-o |  |
| wip/BRIEF-spike-LL3.md | 2026-11-18 | brief for a landed or abandoned step: Spike: LL³ (three lanes each way) through the solver: does it build, what breaks |  |
| wip/BRIEF-spike-freebend.md | 2026-11-17 | brief for a landed or abandoned step: Spike: every free stretch may bend (optional S-bends, the solver decides) |  |
| wip/BRIEF-spike-regions.md | 2026-11-17 | brief for a landed or abandoned step: Spike: regions from the topology vs the lazy clash sides |  |
| wip/BRIEF-spike-shapeflow.md | 2026-11-18 | brief for a landed or abandoned step: Spike: shape as a flow (bends as turning carried by faces, no yes/no) |  |
| wip/BRIEF-spike-shapeflow2.md | 2026-11-18 | brief for a landed or abandoned step: Spike 2: shape flow: tied crossing corners, jogs by cause, flow shapes solved |  |
| wip/BRIEF-spike-shapeflow3.md | 2026-11-18 | brief for a landed or abandoned step: Spike 3: shape program with the crossing rules (absolute headings) |  |
| wip/BRIEF-variant-search.md | 2026-11-18 | brief for a landed or abandoned step: Step: variant local search past the enumeration cap (DESIGN-tracks §14) |  |
| wip/DIAG-S2b-loss.md | 2026-11-18 | probe/diag/spike finished, result in DESIGN-tracks/DECISIONS/code: Diagnosis: why S2b loses T2's 862.25 and T3's 1933.25 | pre-rewrite STATUS 1-40 |
| wip/DIAG-heap.md | 2026-11-18 | probe/diag/spike finished, result in DESIGN-tracks/DECISIONS/code: DIAG heap corruption in "planar: T3 builds from its mapped component layout" (20 | pre-rewrite STATUS 1-40 |
| wip/DIAG-ll2shape.md | 2026-11-18 | probe/diag/spike finished, result in DESIGN-tracks/DECISIONS/code: Diagnosis: why the shape program's shapes fail or grow on LL² (at 21cdedd3e6, PL | pre-rewrite STATUS 1-40 |
| wip/PROBE-ll2-fan.md | 2026-11-17 | probe/diag/spike finished, result in DESIGN-tracks/DECISIONS/code: Probe: why the W-port fan of LL2 o49t0f1s0 climbs 16 half tiles (area 1654.25) |  |
| wip/RECON-ll2-area.md | 2026-11-17 | probe/diag/spike finished, result in DESIGN-tracks/DECISIONS/code: RECON: why mapped LL2 is 2.5x the hand area, and what makes it slow |  |
| wip/S2b-wip.patch | 2026-11-18 | experiment patch/output of a finished or abandoned probe | pre-rewrite STATUS 1-40 |
| wip/SPIKE-slots-run1.md | 2026-11-18 | probe/diag/spike finished, result in DESIGN-tracks/DECISIONS/code: Spike: slot layer on T2 (stopped: model infeasible) |  |
| wip/ascii-T3-o1t3f1s0-A2.txt | 2026-11-18 | experiment patch/output of a finished or abandoned probe |  |
| wip/channel-jog-spike.patch | 2026-11-17 | experiment patch/output of a finished or abandoned probe |  |
| wip/ll2-fan-iis.patch | 2026-11-17 | experiment patch/output of a finished or abandoned probe |  |
| wip/localsearch/ls2.log | 2026-11-18 | log of a local-search run (ls2.log) |  |
| wip/pieces-spike.patch | 2026-11-17 | experiment patch/output of a finished or abandoned probe |  |
| wip/planar-probe-lp.py | 2026-11-15 | python prototype of the planar model; replaced by planar/ C++ |  |
| wip/planar-probe2.py | 2026-11-15 | python prototype of the planar model; replaced by planar/ C++ |  |
| wip/planar-probe3.py | 2026-11-16 | python prototype of the planar model; replaced by planar/ C++ |  |
| wip/planar-probe4.py | 2026-11-16 | python prototype of the planar model; replaced by planar/ C++ |  |
| wip/planar-probe5.py | 2026-11-16 | python prototype of the planar model; replaced by planar/ C++ |  |
| wip/planar-probe6.py | 2026-11-16 | python prototype of the planar model; replaced by planar/ C++ |  |
| wip/planar-probe7-ll2.py | 2026-11-16 | python prototype of the planar model; replaced by planar/ C++ |  |
| wip/planar-probe8b-fixz.py | 2026-11-16 | python prototype of the planar model; replaced by planar/ C++ |  |
| wip/planar-topo-rule.py | 2026-11-16 | python prototype of the planar model; replaced by planar/ C++ |  |
| wip/planar-topo5.py | 2026-11-16 | python prototype of the planar model; replaced by planar/ C++ |  |
| wip/probe-ladderbends.patch | 2026-11-18 | experiment patch/output of a finished or abandoned probe |  |
| wip/probe-roughuse.patch | 2026-11-18 | experiment patch/output of a finished or abandoned probe |  |
| wip/probe-zerostep-stopped.patch | 2026-11-17 | experiment patch/output of a finished or abandoned probe |  |
| wip/probe6-checker-hook.patch | 2026-11-16 | experiment patch/output of a finished or abandoned probe |  |
| wip/probe7-snapshot/README.md | 2026-11-16 | python planar prototype snapshot/output; ported to planar/ C++ | RECON-1c.md, ST-step4-rig.md |
| wip/probe7-snapshot/asc.py | 2026-11-16 | python planar prototype snapshot/output; ported to planar/ C++ |  |
| wip/probe7-snapshot/checked-ll2g.json | 2026-11-16 | python planar prototype snapshot/output; ported to planar/ C++ |  |
| wip/probe7-snapshot/checked-t2.json | 2026-11-16 | python planar prototype snapshot/output; ported to planar/ C++ |  |
| wip/probe7-snapshot/clear_rig.py | 2026-11-16 | python planar prototype snapshot/output; ported to planar/ C++ |  |
| wip/probe7-snapshot/dump_rig.py | 2026-11-16 | python planar prototype snapshot/output; ported to planar/ C++ |  |
| wip/probe7-snapshot/probe6.py | 2026-11-16 | python planar prototype snapshot/output; ported to planar/ C++ |  |
| wip/probe7-snapshot/probe7.py | 2026-11-16 | python planar prototype snapshot/output; ported to planar/ C++ |  |
| wip/probe7-snapshot/routes-ll2.txt | 2026-11-16 | python planar prototype snapshot/output; ported to planar/ C++ |  |
| wip/probe8-out/checked-ll2-p8.json | 2026-11-16 | python planar prototype snapshot/output; ported to planar/ C++ |  |
| wip/probe8-out/checked-t2-p8.json | 2026-11-16 | python planar prototype snapshot/output; ported to planar/ C++ |  |
| wip/probe8-out/cpp-ll2.json | 2026-11-16 | python planar prototype snapshot/output; ported to planar/ C++ |  |
| wip/probe8-out/cpp-t2.json | 2026-11-16 | python planar prototype snapshot/output; ported to planar/ C++ |  |
| wip/probe8-out/hub-cpp.sav | 2026-11-16 | python planar prototype snapshot/output; ported to planar/ C++ |  |
| wip/probe8-out/routes-ll2-p8.txt | 2026-11-16 | python planar prototype snapshot/output; ported to planar/ C++ |  |
| wip/probe8-out/routes-t2-p8.txt | 2026-11-16 | python planar prototype snapshot/output; ported to planar/ C++ |  |
| wip/probe9-out/checked-t3.json | 2026-11-16 | python planar prototype snapshot/output; ported to planar/ C++ |  |
| wip/probe9-out/hubs256.sav | 2026-11-16 | python planar prototype snapshot/output; ported to planar/ C++ |  |
| wip/probe9-out/render-t3.txt | 2026-11-16 | python planar prototype snapshot/output; ported to planar/ C++ |  |
| wip/probe9-out/routes-t3.txt | 2026-11-16 | python planar prototype snapshot/output; ported to planar/ C++ |  |
| wip/review-channels/T2-o0t2f1-best.txt | 2026-11-17 | raw data behind REVIEW-channels.md (kept) |  |
| wip/review-channels/T3-o0t1f1-best.txt | 2026-11-17 | raw data behind REVIEW-channels.md (kept) |  |
| wip/review-channels/T3-o0t1f1-jog-combos.txt | 2026-11-17 | raw data behind REVIEW-channels.md (kept) |  |
| wip/spike-ll3.patch | 2026-11-18 | experiment patch/output of a finished or abandoned probe |  |
| wip/spike-regions-test.cpp | 2026-11-17 | experiment patch/output of a finished or abandoned probe |  |
| wip/spike-regions.patch | 2026-11-17 | experiment patch/output of a finished or abandoned probe |  |
| yr1-wip.patch | 2026-11-11 | old-pipeline WIP patch, never merged or finished |  |

## UNSURE (84)

| file | date | reason | referenced by |
|---|---|---|---|
| AUDIT-rule-coverage.md | 2026-11-11 | rule ids cited by ORACLE-psg227, but it audits the old node template (RC0) | ORACLE-psg227.md |
| DESIGN-blocks.md | 2026-11-15 | superseded per ROADMAP, but DESIGN-components cites its sections 1, 2, 2.2 and R9 as definitions; fold them in or keep | DESIGN-components.md, ROADMAP.md; DECISIONS D299/D303/D311 |
| PATHS.md | 2026-11-13 | maps pre-MV1 flat paths to new ones; useful only while old docs/code comments cite old paths |  |
| PLAN-1c.md | 2026-11-07 | unbuilt future plan, pre-pivot; not superseded by an explicit ruling |  |
| PLAN-endpoint-draft.md | 2026-11-07 | unbuilt future plan, pre-pivot; not superseded by an explicit ruling |  |
| PLAN-joiner.md | 2026-11-08 | unbuilt future plan, pre-pivot; not superseded by an explicit ruling | DECISIONS D112 |
| PLAN-sim-driven.md | 2026-11-08 | unbuilt future plan, pre-pivot; not superseded by an explicit ruling | DECISIONS D119/D121 |
| RECON-blitter.md | 2026-11-07 | tool/UI research (MCP server for checks, ghost blitter); unclear if still wanted | DECISIONS D57 |
| RECON-joiner.md | 2026-11-08 | prior art on endpoint joiner incl. coop merge practice; goes with PLAN-joiner |  |
| RECON-mcp.md | 2026-11-07 | tool/UI research (MCP server for checks, ghost blitter); unclear if still wanted | DECISIONS D124 |
| RECON-mergers.md | 2026-11-13 | section 1 is old D224 band merge; ABR06 merger styles and Merger Logics are coop practice | DECISIONS D231 |
| RECON-organic.md | 2026-11-09 | organic hub piece catalogue from 27 coop screenshots; STATUS "organic feel" item may still want it | DECISIONS D153 |
| RECON-prio-element.md | 2026-11-12 | describes CheckPrio/PlacePrio prio-coverage clauses (checker still live); cited by kept RESEARCH-prio-freedom | RESEARCH-prio-freedom.md |
| RECON-prio-footprint.md | 2026-11-11 | describes CheckPrio/PlacePrio prio-coverage clauses (checker still live); cited by kept RESEARCH-prio-freedom | RESEARCH-prio-freedom.md |
| RECON-prio-rule.md | 2026-11-11 | describes CheckPrio/PlacePrio prio-coverage clauses (checker still live); cited by kept RESEARCH-prio-freedom | RESEARCH-prio-freedom.md; DECISIONS D342 |
| RECON-terrain.md | 2026-11-08 | game rules on rail slopes / tunnel portals mixed with old-pipeline terrain code reading |  |
| RECON-tunnel-crossings.md | 2026-11-13 | game rules on rail slopes / tunnel portals mixed with old-pipeline terrain code reading |  |
| REVIEW-G4-overprescribed.md | 2026-11-14 | old-pipeline review, but its Q1 table (real LL² layouts: gaps, bridge counts, doubling) is corpus fact; move then remove | DECISIONS D276/D277/D278 |
| briefs/LP-step2-templates.md | 2026-11-08 | line painter workstream brief (parked, live by instruction) but the step is finished; Pete to decide |  |
| briefs/LP1-notation-groups.md | 2026-11-09 | line painter workstream brief (parked, live by instruction) but the step is finished; Pete to decide |  |
| briefs/LP10-line-build-signals-cost.md | 2026-11-10 | line painter workstream brief (parked, live by instruction) but the step is finished; Pete to decide |  |
| briefs/LP4-line-router.md | 2026-11-09 | line painter workstream brief (parked, live by instruction) but the step is finished; Pete to decide |  |
| briefs/LP5-terrain-obstacles.md | 2026-11-09 | line painter workstream brief (parked, live by instruction) but the step is finished; Pete to decide | DECISIONS D147 |
| briefs/LP6-line-bridges.md | 2026-11-09 | line painter workstream brief (parked, live by instruction) but the step is finished; Pete to decide | DECISIONS D149 |
| briefs/LP7-waypoints.md | 2026-11-09 | line painter workstream brief (parked, live by instruction) but the step is finished; Pete to decide | DECISIONS D150 |
| briefs/LP9-terrain-rect.md | 2026-11-10 | line painter workstream brief (parked, live by instruction) but the step is finished; Pete to decide |  |
| briefs/RECON-corpus-census.md | 2026-11-14 | method brief cited by RECON-corpus-census2 | RECON-corpus-census2.md |
| wip/2e-merge-steps-wip.patch | 2026-11-17 | 2e parked work; cited at old STATUS parked line, not by current docs |  |
| wip/P2-change-wip.patch | 2026-11-18 | prio-signals WIP patches; cannot tell from docs whether they landed |  |
| wip/P2s-wip-clean.patch | 2026-11-18 | prio-signals WIP patches; cannot tell from docs whether they landed |  |
| wip/PROBE-2e-merge.md | 2026-11-17 | 2e parked work; cited at old STATUS parked line, not by current docs |  |
| wip/PROBE-ladderbends.md | 2026-11-18 | results for open ROADMAP items (free bends on ladders, rough-solve uses); only the pre-rewrite STATUS cited them | pre-rewrite STATUS 1-40 |
| wip/PROBE-roughuse.md | 2026-11-18 | results for open ROADMAP items (free bends on ladders, rough-solve uses); only the pre-rewrite STATUS cited them | pre-rewrite STATUS 1-40 |
| wip/SPIKE-regions.md | 2026-11-17 | predecessor of kept SPIKE-regions2 (which cites it) | SPIKE-regions2.md |
| wip/localsearch/cache.json | 2026-11-18 | regenerable cache of live driver ls.py |  |
| wip/oracle-psg227-probe.patch | 2026-11-14U | reproduces ORACLE-psg227 on the old checker; ORACLE cites it | ORACLE-psg227.md |
| wip/planar-topo/topo-ll2.json | 2026-11-16 | topology text for T1/T2/LL2; ROADMAP says topologies exist as text only |  |
| wip/planar-topo/topo-t1.json | 2026-11-16 | topology text for T1/T2/LL2; ROADMAP says topologies exist as text only |  |
| wip/planar-topo/topo-t2.json | 2026-11-16 | topology text for T1/T2/LL2; ROADMAP says topologies exist as text only |  |
| wip/probe-anchor.patch | 2026-11-18 | reproduction patch/test for kept PROBE-anchor.md; remove together with the doc or keep both | PROBE-anchor.md |
| wip/probe-corridors.patch | 2026-11-17 | reproduction patch/test for kept CHECK-rungorder.md, PROBE-corridors.md; remove together with the doc or keep both | CHECK-rungorder.md, PROBE-corridors.md, PROBE-ladders.md |
| wip/probe-flowturns.patch | 2026-11-18 | reproduction patch/test for kept PROBE-flowturns.md; remove together with the doc or keep both | PROBE-flowturns.md |
| wip/probe-joiner-over.patch | 2026-11-18 | reproduction patch/test for kept PROBE-joiner-over.md; remove together with the doc or keep both | PROBE-joiner-over.md |
| wip/probe-ladders.patch | 2026-11-17 | reproduction patch/test for kept PROBE-ladders.md, PROBE-ladders2.md; remove together with the doc or keep both | PROBE-ladders.md, PROBE-ladders2.md |
| wip/probe-ladders2.patch | 2026-11-17 | reproduction patch/test for kept PROBE-ladders2.md, PROBE-ladders3.md; remove together with the doc or keep both | PROBE-ladders2.md, PROBE-ladders3.md |
| wip/probe-ladders3.patch | 2026-11-17 | reproduction patch/test for kept PROBE-ladders3.md, PROBE-ladders4.md; remove together with the doc or keep both | PROBE-ladders3.md, PROBE-ladders4.md |
| wip/probe-ladders4.patch | 2026-11-17 | reproduction patch/test for kept PROBE-ladders4.md, PROBE-ladders5.md; remove together with the doc or keep both | pre-rewrite STATUS 1-40, PROBE-ladders4.md, PROBE-ladders5.md |
| wip/probe-ladders5.patch | 2026-11-18 | reproduction patch/test for kept PROBE-ladders5.md; remove together with the doc or keep both | pre-rewrite STATUS 1-40, PROBE-ladders5.md |
| wip/probe-noclash.patch | 2026-11-18 | reproduction patch/test for kept PROBE-noclash.md; remove together with the doc or keep both | PROBE-noclash.md |
| wip/probe7-snapshot/hub807.sav | 2026-11-16 | engineer's hand edits of the rig hub (source of D318 variants); not regenerable |  |
| wip/probe7-snapshot/pete-ll2.json | 2026-11-16 | engineer's hand edits of the rig hub (source of D318 variants); not regenerable |  |
| wip/probe7-snapshot/pete-ll2b.json | 2026-11-16 | engineer's hand edits of the rig hub (source of D318 variants); not regenerable |  |
| wip/probe7-snapshot/pete-ll2c.json | 2026-11-16 | engineer's hand edits of the rig hub (source of D318 variants); not regenerable |  |
| wip/probe7-snapshot/pete-ll2d.json | 2026-11-16 | engineer's hand edits of the rig hub (source of D318 variants); not regenerable |  |
| wip/review-g4/feat-psg173_14.txt | 2026-11-14U | dumps/traces of real PSG hubs (173, 327, 325, 41); regenerable from saves; RECON-corpus-anatomy cites the scripts of this folder (gone); mostly untracked |  |
| wip/review-g4/feat-psg327_12.txt | 2026-11-14U | dumps/traces of real PSG hubs (173, 327, 325, 41); regenerable from saves; RECON-corpus-anatomy cites the scripts of this folder (gone); mostly untracked |  |
| wip/review-g4/psg173_14.txt | 2026-11-14U | dumps/traces of real PSG hubs (173, 327, 325, 41); regenerable from saves; RECON-corpus-anatomy cites the scripts of this folder (gone); mostly untracked |  |
| wip/review-g4/psg325_8.txt | 2026-11-14U | dumps/traces of real PSG hubs (173, 327, 325, 41); regenerable from saves; RECON-corpus-anatomy cites the scripts of this folder (gone); mostly untracked |  |
| wip/review-g4/psg327_12.txt | 2026-11-14U | dumps/traces of real PSG hubs (173, 327, 325, 41); regenerable from saves; RECON-corpus-anatomy cites the scripts of this folder (gone); mostly untracked |  |
| wip/review-g4/psg41_90.txt | 2026-11-14U | dumps/traces of real PSG hubs (173, 327, 325, 41); regenerable from saves; RECON-corpus-anatomy cites the scripts of this folder (gone); mostly untracked |  |
| wip/review-g4/stats.txt | 2026-11-14U | dumps/traces of real PSG hubs (173, 327, 325, 41); regenerable from saves; RECON-corpus-anatomy cites the scripts of this folder (gone); mostly untracked |  |
| wip/review-g4/trace-psg173_14.txt | 2026-11-14U | dumps/traces of real PSG hubs (173, 327, 325, 41); regenerable from saves; RECON-corpus-anatomy cites the scripts of this folder (gone); mostly untracked |  |
| wip/review-g4/trace-psg327_12.txt | 2026-11-14U | dumps/traces of real PSG hubs (173, 327, 325, 41); regenerable from saves; RECON-corpus-anatomy cites the scripts of this folder (gone); mostly untracked |  |
| wip/review-g4/z.txt | 2026-11-14U | dumps/traces of real PSG hubs (173, 327, 325, 41); regenerable from saves; RECON-corpus-anatomy cites the scripts of this folder (gone); mostly untracked |  |
| wip/rig-2026-11-17/box-LL2-o1t3f0s0.json | 2026-11-17 | rig run logs/layouts of T2/T3/LL2; RECON-prio-straight cites the folder |  |
| wip/rig-2026-11-17/box-LL2-o1t3f0s0.log | 2026-11-17U | rig run logs/layouts of T2/T3/LL2; RECON-prio-straight cites the folder |  |
| wip/rig-2026-11-17/box-T2-o1t2f0s0.json | 2026-11-17 | rig run logs/layouts of T2/T3/LL2; RECON-prio-straight cites the folder |  |
| wip/rig-2026-11-17/box-T2-o1t2f0s0.log | 2026-11-17U | rig run logs/layouts of T2/T3/LL2; RECON-prio-straight cites the folder |  |
| wip/rig-2026-11-17/box-T3-o1t2f0s0.json | 2026-11-17 | rig run logs/layouts of T2/T3/LL2; RECON-prio-straight cites the folder |  |
| wip/rig-2026-11-17/box-T3-o1t2f0s0.log | 2026-11-17U | rig run logs/layouts of T2/T3/LL2; RECON-prio-straight cites the folder |  |
| wip/rig-2026-11-17/mapped-LL2-o1t3f0s1.json | 2026-11-17 | rig run logs/layouts of T2/T3/LL2; RECON-prio-straight cites the folder |  |
| wip/rig-2026-11-17/mapped-LL2-o1t3f0s1.log | 2026-11-17U | rig run logs/layouts of T2/T3/LL2; RECON-prio-straight cites the folder |  |
| wip/rig-2026-11-17/mapped-LL2.json | 2026-11-17 | rig run logs/layouts of T2/T3/LL2; RECON-prio-straight cites the folder |  |
| wip/rig-2026-11-17/mapped-LL2.log | 2026-11-17U | rig run logs/layouts of T2/T3/LL2; RECON-prio-straight cites the folder |  |
| wip/rig-2026-11-17/mapped-T2.json | 2026-11-17 | rig run logs/layouts of T2/T3/LL2; RECON-prio-straight cites the folder |  |
| wip/rig-2026-11-17/mapped-T2.log | 2026-11-17U | rig run logs/layouts of T2/T3/LL2; RECON-prio-straight cites the folder |  |
| wip/rig-2026-11-17/mapped-T3.json | 2026-11-17 | rig run logs/layouts of T2/T3/LL2; RECON-prio-straight cites the folder |  |
| wip/rig-2026-11-17/mapped-T3.log | 2026-11-17U | rig run logs/layouts of T2/T3/LL2; RECON-prio-straight cites the folder |  |
| wip/spike-freebend.patch | 2026-11-17 | reproduction patch/test for kept SPIKE-freebend.md; remove together with the doc or keep both | SPIKE-freebend.md |
| wip/spike-regions2-test.cpp | 2026-11-17 | reproduction patch/test for kept SPIKE-regions2.md; remove together with the doc or keep both | SPIKE-regions2.md |
| wip/spike-regions2.patch | 2026-11-17 | reproduction patch/test for kept SPIKE-regions2.md; remove together with the doc or keep both | SPIKE-regions2.md |
| wip/spike-shapeflow.patch | 2026-11-18 | reproduction patch/test for kept SPIKE-shapeflow.md, SPIKE-shapeflow2.md; remove together with the doc or keep both | SPIKE-shapeflow.md, SPIKE-shapeflow2.md |
| wip/spike-shapeflow2.patch | 2026-11-18 | reproduction patch/test for kept SPIKE-shapeflow2.md, SPIKE-shapeflow3.md; remove together with the doc or keep both | SPIKE-shapeflow2.md, SPIKE-shapeflow3.md |
| wip/spike-shapeflow3.patch | 2026-11-18 | reproduction patch/test for kept SPIKE-shapeflow3.md; remove together with the doc or keep both | SPIKE-shapeflow3.md |

## Live facts at risk

Facts about coop practice or the game that I found only in files proposed for removal (or UNSURE). Move the fact before the file goes. Only files whose title or first lines suggested such content were opened; the rest are mechanism descriptions of the old code.

- **REVIEW-G4-overprescribed.md (UNSURE):** Real LL² hubs (psg173_14, psg327_12) are lane-keep: 12 lane paths, plain merges (no main-merge prios), core 46x30; direction-group gaps 7/10/10 and 3/7; port gaps 3-12 (gap 1 at only 7 of 79 ends); 18 bridges + 4 tunnels, doubled main-traffic bridges with stagger 1; coop runs turning pairs horizontally, diagonally, through 90 degree corners, on three levels. Check RECON-corpus-ll2/anatomy first: anatomy has the 12-path/woven-box points but I found no "46x30" or the gap table.
- **DESIGN-modules.md §2-§3:** Block catalogue (exit = direction split per lane, join = merger with prio by balancing pattern) and the balancing-pattern table D279/D282 with real examples (PSG 227 gives the prio to the fixed lane and both choice lanes yield). RECON-corpus-anatomy, RULES-hubs and RULES-coop mention balancing; verify the pattern table is there.
- **DESIGN-blocks.md (UNSURE):** R9 "prio room": no node and no bend on the fixed track from mL-P-1 to mL, prio row holds nothing else; 227 join block shape (2 splits, 1 crossing, choice legs merge first, fixed lane prio). DESIGN-components cites these as definitions.
- **RECON-prio-rule.md / RECON-prio-element.md / RECON-prio-footprint.md (UNSURE):** Prio coverage conditions the checker enforces: no ML split, curve or lane merge inside the coverage of 2p units; one side only; a join in each coverage block; signal pass (D107). Cited by RESEARCH-prio-freedom. Probably duplicated in src/junction/signals/CLAUDE.md; not verified.
- **RECON-tunnel-crossings.md §1 (UNSURE):** Game tunnel rules: entry portal must be an inclined slope; exit portal is the FIRST tile whose lowest corner equals the entry height, whatever lies between (CmdBuildTunnel). Not in RECON-foundations.
- **RECON-organic.md (UNSURE):** Piece catalogue of real organic hubs (tunnel flyunder, short flyovers, hairpin-free diagonal runs, land modified only locally) from 27 screenshots. The engineer wants "organic feeling" hubs later (old STATUS LATER item).
- **RECON-mergers.md (UNSURE):** ABR06 merger styles and "Merger Logics" (lines 54-77, 94-98 of the ABR06 text) as coop practice; the rest is the old D224 band merge.
- **RECON-joiner.md (UNSURE):** Coop practice on merging/joining tracks (Mainline, Merging_Tracks, Basic_Junctions), from search excerpts only, marked "verify when the wiki is reachable".
- **RECON-llrr.md / RECON-llrr-facts.md:** Quotes of coop rules for LL_RR (R41 "taken literally only one tile, usually more", R42 "more spaces the better") and D38/D142 (gap is a guideline; same-direction tracks adjacent). These are in RULES-hubs/RULES-coop and DECISIONS; no action beyond a spot check.
- **wip/probe7-snapshot/pete-ll2*.json, hub807.sav (UNSURE):** The engineer's hand-edited LL² rig hubs ("a great hub", source of D318 variants). Not regenerable.
- **wip/review-g4/* (UNSURE):** Piece dumps/traces of real PSG hubs 173_14, 327_12, 325_8, 41_90; regenerable from the saves on OneDrive but the scripts that read them (`review-g4/py/c2.py`, `scripts/legs.py`) are already gone, and RECON-corpus-anatomy cites them.
- **briefs/* in general:** Briefs hold specs, not facts; their results are in code. I did not open each. The corpus briefs (HF1, SV1, GR1, MX1, MX2, JN1, CK1) describe how the PSG corpus was fetched and exported; RECON-psg-hubs/saves record the recipe in outline. MX1 documents the `map_export` console command usage; check it is in a CLAUDE.md.

## Dangling citations once REMOVE files go

Kept (KEEP/UNSURE) doc -> file proposed for removal that it names. Fix the citation or move the fact first.

- DESIGN-G1.md <- ROADMAP.md
- DESIGN-N3.md <- RECON-foundations.md, RESEARCH-prio-freedom.md
- DESIGN-modules.md <- ROADMAP.md, REPRESENT-psg227.md
- DESIGN-nodes.md <- RECON-foundations.md
- DESIGN-width.md <- ROADMAP.md
- RECON-bundle-copies.md <- RECON-mfb-core.md
- RECON-stretch.md <- TESTS.md

Removed files that DECISIONS.md rows also cite (the DECISIONS editor should drop or repoint): C1-compactness-score.md (D154), CARDS-LLRR.md (D186), CARDS-crossings.md (D227), DESIGN-G1.md (D216/D218), DESIGN-N3.md (D342), DESIGN-O4.md (D156/D157), DESIGN-modules.md (D277/D282), DESIGN-nodes.md (D182/D183), DESIGN-width.md (D296/D303), FABLE-G1c.md (D205), FABLE-G1c2.md (D208), FABLE-nodes-perf.md (D183), FABLE-terrain-placement.md (D216), FABLE-topology-graph.md (D202), O2-router-tunnels.md (D155), PLAN-D78.md (D93), PLAN-organic.md (D152), PRIO-diag.md (D107), RECON-corpus-anatomy.md (D277), RECON-lane-geometry.md (D145), RECON-llrr-placement-contradiction.md (D230), RECON-llrr-round0.md (D248), REPRESENT-psg227.md (D287), REVIEW-G4-overprescribed.md (D276/D277), REVIEW-graph-model.md (D235), REVIEW-joint-loop.md (D253), REVIEW-joint-loop.md (D253), REVIEW-opus-since-move.md (D227), REVIEW-port-fans.md (D230), T1-tunnels-in-layouts.md (D154)
