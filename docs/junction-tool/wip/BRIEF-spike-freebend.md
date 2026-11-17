# Spike: every free stretch may bend (optional S-bends, the solver decides)

Throwaway spike: nothing is committed. Time box: 45 minutes of work; at the box, report what you have. Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only the `openttd_test` target in `build-rel`, never `openttd`. Keep context small: outputs under `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/freebend/`, code read with offset/limit, command output to files.

## State and question
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, clean at `{HEAD}`, Release build dir `build-rel`.
- Engineer: "Surely any free stretch of track between components should be able to bend." Today sideways steps exist only where a rule puts a required diagonal (merge exit steps, gap steps, the candidate step variant bit). The question: if every bundle piece and every merge exit may carry an optional S-bend to either side (binary per side, shared by the bundle's lanes, the solver choosing), with the step rules off, what are areas and solve times on T2, T3 and LL²? And how much is the bound on the step length (big-M) to blame for any slowdown?

## Read first
- `src/junction/planar/planar_model.h`: `AddSeg` (~367-400: `opt`, `opt_group` sharing one binary per (group, heading), the `z * hi >= L` row with `OPTMAX` 16 when `hi` is unset).
- `src/junction/planar/planar_components.cpp`: the fork legs (~820-835, the pattern for optional segments in `MappedComposer`), `Bundle` (~1037-1075), `Join` (~941-1007, the `exit_step` piece ~994), the site join call (~1361-1365, gap step), the per-node merge call (~1398), where candidate-step bits enter the variant (`MappedVariant::steps`).
- Confirm the mapped composer's curve rows (`Composer::RouteCurves`, ~1732) lapse with optional segments (the fork legs rely on it). If they do not, stop and report.

## Change (env knob `PLANAR_FREEBEND=<hi>`, hi in half-tiles; uncommitted)
When set:
1. `Join`: `exit_step` is ignored (no required step); after each out lane's `M.AddSeg(o, h, 2, ..., FANW)` add two optional S-bend slots in series: `Turn(h, +1)` optional (lo 2, hi), then `h` (lo 0), then `Turn(h, -1)` optional (lo 2, hi), then the existing `h` stub. Use `opt_group` = a key unique to this join and slot so the out lanes of one join share one binary per slot. Drop the `join exit` Ge row (it reads the removed step); keep the Eq spacing row.
2. `Bundle`: on every piece k, after each lane's lead segment, the same two optional slots (`Turn(h,+1)`, `h` lo 0, `Turn(h,-1)`, `h` lo 0), `opt_group` unique per (channel, piece, slot) so all lanes share the binary; the existing `bundle offset` rows already keep lanes offset-parallel (so their diagonals are equal). Doubled pieces: add the slots only to the lead before the parallelogram, nothing inside it.
3. Candidate step bits: run only variants with `s0` (pass tags accordingly); no other change to fray fans or gap steps for fans.
If any of these does not fit the code as described (e.g. a lo 0 axis segment is refused, or a lane's lead is not where the slots can go), stop and report rather than improvise.

## Measure
- Baselines at {HEAD} without the knob (only the s0 tags): T2 and T3 via `/home/pete/OpenTTD/tools/junction/pv run build-rel T2 --tags <s0 tags> --out ...` (get the T tag lists with a dry run or the pv save files in `tools/junction/`; ask nothing, find them), LL² from the 201-tag file `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/ll2-tags.txt` filtered to s0, `-j 8`.
- With `PLANAR_FREEBEND=16` and `PLANAR_FREEBEND=128`: the same three runs.
- Report per hub and setting: solved count, best area and tag, median and max ms per variant, count better / worse by more than 0.5% vs baseline, the number of optional binaries in one variant (print once). Also `pv diff` LL² FREEBEND=16 against `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/2e2/before.txt` (old optional merge steps).
- For the best LL² variant under FREEBEND=16: which slots the solver used (channel/join, piece, side, length).
- If a hub has more than 3 baseline-solving variants failing under a setting: `PLANAR_IIS=1` core (first 30 lines) for one.

## Output
- Findings under 40 lines: `/home/pete/OpenTTD/docs/junction-tool/wip/SPIKE-freebend.md` (verified vs inferred marked).
- `git diff > /home/pete/OpenTTD/docs/junction-tool/wip/spike-freebend.patch`; then `git checkout -- .`, confirm `git status --porcelain` empty, rebuild `openttd_test`.
- Report in under 15 lines. When you have reported, stop.
