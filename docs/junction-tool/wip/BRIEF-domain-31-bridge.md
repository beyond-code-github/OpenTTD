# BRIEF domain-31: a bridge statement and a bridge fixture (batch 2c)

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 75 minutes wall. A new statement in the structure loader (no change to existing statements or defaults); `[planarstructure]`-style fast tests plus the domain suite; no wide gate. Never run git stash. Do not touch branch wip-defaults.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- Every domain-suite fixture's structure is a tunnel. The domain knows bridges (head end run 1, middles exclusive to their crossers), and the drawing program takes each structure's least end run over its allowed kinds (planar_draw.cpp ~1191), but `LoadPlanarStructureVariant` only writes tunnels: it sets `st.kinds = 2` for every structure (planar_structure.cpp ~585).
- Read first: `CODINGSTYLE.md`; planar_structure.h ~138-152 (the loader's statement list doc); planar_structure.cpp ~380-470 (statement parsing: `tunnel`, `copy`) and ~578-590 (structures and kinds); `PlanarDomainInputOf`'s kind choice (planar_domain_map.cpp ~138); in the test file, the Y1 fixture (`DrawnFixture`, `least_extra = 2`) and its two tests; find the existing loader tests (grep `LoadPlanarStructureVariant` under src/tests) for the test pattern.

## 2. The statement
- `bridge B<n> <separator line>: <crosser line>, ...`: one structure of kind bridge, parsed exactly as `tunnel` (same id namespace: an id given twice across both words is the same "given twice" error; `copy <line> over <id>, ...` accepts either). Implement it in the `tunnel` branch: `if (word == "tunnel" || word == "bridge")`, recording per run its kind in a local `std::vector<uint8_t> run_kinds` (1 bridge, 2 tunnel) pushed alongside `runs`. Error texts that say "tunnel" use the word given.
- At ~585: `st.kinds = run_kinds[i]` for structure i. This needs `v.structures[i]` to come from `runs[i]`: confirm from `PlanarBuildStructures` (one per run, in order); if not, stop. The existing "gap: N tunnels gave M structures" text becomes "structures" for both.
- Doc: add the `bridge` line to the statement list in planar_structure.h.

## 3. Tests (red first; outermost first)
1. Loader (with the existing loader tests): "a bridge statement gives a structure of kind bridge": X1's topology (the text at the top of planar_domain_encoding.cpp) with `bridge B1 A1: B1` / `straight AC1: A1` / `straight BD1: B1`: no error, one structure, `kinds == 1`; the same with `tunnel T1 A1: B1` gives `kinds == 2`; `tunnel T1 A1: B1` plus `bridge T1 ...` is a "given twice" error.
2. Domain suite: fixture B1 (name the struct `BridgeFixture`, report env `PLANAR_DOMAIN_REPORT_BR1`, report file `encoding-br1-span_lead_endnode_crosspiece_grid_portzero.txt`): X1's topology (copy the text, with the D410 port positions X1 now uses), structure `bridge B1 A1: B1\nstraight AC1: A1\nstraight BD1: B1\n`, `DrawnFixture` with `least_extra = 2`, neighbourhood configurations. fast test "planar domain: BR1 loads, its structure is a bridge and its base is legal" (the domain input of the base names the structure a bridge, `ends` 1); slow `[.][planardomainenc]` test via `RunEncoding`.

## 4. Runs
Release (`build-rel`), BR1, rule set `span+lead+endnode+crosspiece+grid+portzero`. Timeout 300. Report every too strict / too loose group with key and first example in full (fix nothing).

## 5. Gates and commit
- The loader tests' tag via `tools/junction/jt fast build '<their tag>' 5`, plus `tools/junction/jt fast build '[planardomain]' 5` (Debug).
- One commit: `Add: Junction: a bridge statement in the structure loader, and a bridge fixture in the domain suite (D428)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 6. Report (short)
References read; commit; red evidence; members/locals added; BR1: Shape's headings, base status and size, classes per configuration, every too strict / too loose / off interface group with first example; gates; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
