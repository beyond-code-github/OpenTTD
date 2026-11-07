# Brief: 1a step 9, building the junction (sequencer, callback, Retry and Cancel)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- Repo `/home/pete/OpenTTD` (main checkout), branch `feature/junction-tool`, head `{HEAD}`. You are the only agent here. A copy of the game, `build/openttd-check`, may be running. Don't touch it.
- Plan: `docs/junction-tool/PLAN-1a.md`, "JunctionBuilder" and "Steps" item 9. **This brief supersedes the plan's builder design** because of a code fact the plan missed: `Command<>::Post` "must not be called recursively" (`command_func.h:221`), so the next step can't be posted from inside a command callback. Instead, a **sequencer** posts one step per tick from the window's `OnRealtimeTick`, and the callback only reports the result. This also works in multiplayer, where the callback runs when the server's result arrives.
- **Decisions:** D7 (sequential commands; stop on failure; Retry) and D9 (re-check the cost just before building).
- **Already on the branch:**
  - `junction_layout.*`, `junction_game.*` (`GameJunctionPricing`, `ChooseJunctionBridgeType`), `junction_ghost_draw.*`
  - `junction_gui.*`: `JunctionGhost`, `DrawJunctionGhost`, `BuildJunctionWindow`, and an empty `JunctionToolClick`
  - `widgets/junction_widget.h`
- **Baseline:** `{BASELINE}`, all passing.
- **Rebase-friendliness (D42):** the only upstream edit is registering the callback (section 4.4).

## 2. Read first, once each

- `CODINGSTYLE.md` in full. Quote one rule verbatim that previous reports haven't quoted. Those were the `this->` rule and "Use /* */ for single line comments."
- `src/command_func.h` lines 195–360: `Post`, `InternalPost`, and when the callback is and isn't called.
- `src/network/network_command.cpp` lines 1–125: the `_callback_tuple` registry, and `IsNetworkRegisteredCallback`, which `Post` asserts.
- `src/junction_layout.h`, `src/junction_game.h` and `src/junction_gui.h`. Also `src/junction_gui.cpp` (`BuildJunctionWindow`, `JunctionToolClick`) and `src/widgets/junction_widget.h`.
- `src/rail_gui.cpp` around lines 95–110: `CcPlaySound_CONSTRUCTION_RAIL` (the sound pattern) and a `Post` call with an error string.
- `src/tests/junction_layout.cpp`, the first 40 lines, for style.

## 3. Tests (write first, see them fail)

Create `src/tests/junction_build.cpp` and register it in `src/tests/CMakeLists.txt` alphabetically, before `junction_game.cpp`. Each test uses a `FakeStepPoster` defined in the test file. It derives from `JunctionStepPoster`, holds a `JunctionSequencer &`, records every posted step index (or step) in a `std::vector`, and has a configurable behaviour per call:
- `Sync(ok)`: calls `seq.OnStepResult(ok)` and then returns `ok`. This simulates single player.
- `Async`: returns `true` without calling back. This simulates a network client waiting for the server.
- `Refuse`: returns `false` without calling back. This simulates `Post` refusing, e.g. estimate-only mode.

Use 5 dummy `JunctionRailStep`s with distinct tiles `TileXY(1..5, 1)`. Call `Map::Allocate(64, 64)`.

1. **`"JunctionSequencer outer: builds every step in order"`:** all calls are `Sync(true)`. `Start` with the 5 steps, then call `Tick` until the state is no longer `Building` (cap at 20 ticks). Expect: the posted tiles are 1..5 in order, the state is `Done`, `GetDone() == 5` and `GetTotal() == 5`.
2. **`"JunctionSequencer outer: a failure stops, retry resumes from the failed step"`:**
   - Step 3 (index 2) is `Sync(false)` on its first attempt; everything else is `Sync(true)`.
   - After ticking: state `Failed`, `GetDone() == 2`, and posted tiles {1, 2, 3}.
   - Three more ticks post nothing.
   - `Retry()`, then tick to the end: state `Done`, and posted tiles {1, 2, 3, 3, 4, 5}.
3. **`"JunctionSequencer: waits for an asynchronous result"`:**
   - The first call is `Async`. After one `Tick`, 1 post. After another `Tick`, still 1 post.
   - Then `OnStepResult(true)`; the next `Tick` posts step 2.
4. **`"JunctionSequencer: a refused post counts as a failure"`:** the first call is `Refuse`. After one `Tick`: state `Failed` and `GetDone() == 0`.
5. **`"JunctionSequencer: cancel returns to idle"`:** after `Start`, call `Cancel()`. Then state `Idle`, `GetTotal() == 0`, and a `Tick` posts nothing.
6. **`"JunctionSequencer: a stray result while idle is ignored"`:** `OnStepResult(true)` on a fresh sequencer leaves it `Idle`, with no crash.

**Red:** add the header and a `.cpp` with stub bodies (`Tick` does nothing, `GetState` returns `Idle`). Build, run `./build/openttd_test "JunctionSequencer*"` and record the failures. Tests 5 and 6 may pass against the stub; that's acceptable.

## 4. Changes

### 4.1 `src/junction_build.h` and `src/junction_build.cpp` (new, pure, guard `JUNCTION_BUILD_H`)

```cpp
#include "junction_layout.h"

/** Progress of building a junction. */
enum class JunctionBuildState : uint8_t {
	Idle,     ///< Nothing to build.
	Building, ///< Posting steps one at a time.
	Failed,   ///< A step failed; waiting for Retry or Cancel.
	Done,     ///< Every step succeeded.
};

/** Posts one build step as a command; tests provide a fake. */
class JunctionStepPoster {
public:
	virtual ~JunctionStepPoster() = default;
	/**
	 * Post a build step.
	 * @return The result of posting; the outcome may also arrive later through JunctionSequencer::OnStepResult.
	 */
	virtual bool Post(const JunctionBuildStep &step) = 0;
};

/** Builds a junction one step at a time, never posting from inside a command callback. */
class JunctionSequencer {
public:
	void Start(std::vector<JunctionBuildStep> steps);
	void Tick(JunctionStepPoster &poster);
	void OnStepResult(bool success);
	void Retry();
	void Cancel();
	JunctionBuildState GetState() const { return this->state; }
	size_t GetDone() const { return this->next; }
	size_t GetTotal() const { return this->steps.size(); }
	std::span<const JunctionBuildStep> GetRemaining() const;

private:
	std::vector<JunctionBuildStep> steps; ///< Steps in build order.
	size_t next = 0; ///< Index of the next step to post; equals the number of succeeded steps.
	bool awaiting = false; ///< A step has been posted and its result hasn't arrived yet.
	JunctionBuildState state = JunctionBuildState::Idle; ///< Current progress.
};
```

**Semantics:**
- **`Start`:** store the steps, set `next = 0` and `awaiting = false`. The state becomes `Building`, or `Done` if there are no steps.
- **`Tick`:** do nothing unless the state is `Building` and `!awaiting`. Otherwise:
  1. Set `awaiting = true`, then `bool ok = poster.Post(this->steps[this->next]);`.
  2. Afterwards, if `this->awaiting && !ok`, the poster refused without a callback, so call `this->OnStepResult(false)`.
- **`OnStepResult(success)`:** ignore it unless the state is `Building` and `awaiting` is set. Then set `awaiting = false`.
  - On success: `++next`. If `next == steps.size()`, the state becomes `Done`.
  - On failure: the state becomes `Failed`, and `next` is unchanged.
- **`Retry`:** if the state is `Failed`, it becomes `Building`, with `awaiting = false`.
- **`Cancel`:** clear the steps, set `next = 0` and `awaiting = false`, and the state becomes `Idle`.
- **`GetRemaining`:** `steps` from `next` to the end.

Doc comments go on each member function in the `.cpp`. Register both files in `src/CMakeLists.txt` alphabetically, before `junction_game.cpp`.

### 4.2 `src/junction_game.h` and `src/junction_game.cpp`

**`GameJunctionStepPoster`:**
```cpp
/** Posts junction build steps as real commands for the local company. */
class GameJunctionStepPoster : public JunctionStepPoster {
public:
	explicit GameJunctionStepPoster(RailType railtype) : railtype(railtype) {}
	bool Post(const JunctionBuildStep &step) override;

private:
	RailType railtype; ///< Rail type the junction is built in.
};
```

`Post` dispatches with `std::visit`:
- **Rail:** `return Command<Commands::BuildRail>::Post(STR_ERROR_CAN_T_BUILD_RAILROAD_TRACK, CcJunctionStep, s.tile, this->railtype, s.track, false);`
- **Bridge:**
  1. Set `uint length = GetTunnelBridgeLength(s.start, s.end)` and `auto type = ChooseJunctionBridgeType(this->railtype, length)`.
  2. If there's no type, return `false`. The sequencer treats that as a failure.
  3. Otherwise `return Command<Commands::BuildBridge>::Post(STR_ERROR_CAN_T_BUILD_BRIDGE_HERE, CcJunctionStep, s.end, s.start, TransportType::Rail, *type, this->railtype, INVALID_ROADTYPE);`
- **Signal:** `return Command<Commands::BuildSignal>::Post(STR_ERROR_CAN_T_BUILD_SIGNALS_HERE, CcJunctionStep, s.tile, TrackdirToTrack(s.facing), s.type, s.variant, false, false, false, SignalType::Block, SignalType::Block, 0, SignalAlongTrackdir(s.facing));`

**Also add:**
- `JunctionSequencer &GetJunctionSequencer();`, which returns a function-local static. Document it as "The single client-side junction build sequencer".
- `void CcJunctionStep(Commands cmd, const CommandCost &result, TileIndex tile);`, which calls `GetJunctionSequencer().OnStepResult(result.Succeeded());`. The parameters it doesn't use are `[[maybe_unused]]`.

Declare `GameJunctionStepPoster`, `GetJunctionSequencer` and `CcJunctionStep` in `junction_game.h`. `CcJunctionStep` must use the plain `CommandCallback` signature, as `CcPlaySound_CONSTRUCTION_RAIL` does. Include `junction_build.h` in the header, and in the `.cpp` add whatever the commands need (`rail_cmd.h`, `tunnelbridge_cmd.h`, `signal_func.h`, `tunnelbridge.h`).

### 4.3 `src/junction_gui.cpp`, `src/widgets/junction_widget.h` and `src/lang/english.txt`

**Widgets:** append to `BuildJunctionWidgets`, after `WID_BJ_MIRROR`:
- `WID_BJ_STATUS`, a status line
- `WID_BJ_RETRY`, Retry building after a failure
- `WID_BJ_CANCEL`, Cancel a failed or ongoing build

**Nested widgets:** below the Rotate/Mirror row, add a `WWT_LABEL WID_BJ_STATUS` with `SetFill(1, 0)`, then a horizontal row with `WWT_PUSHTXTBTN WID_BJ_RETRY` and `WWT_PUSHTXTBTN WID_BJ_CANCEL`.

**Strings,** in the junction block of `english.txt`, keeping the alignment:
- `STR_JUNCTION_STATUS_READY :{BLACK}Click to build`
- `STR_JUNCTION_STATUS_BUILDING :{BLACK}Building... {NUM} of {NUM}`
- `STR_JUNCTION_STATUS_FAILED :{RED}Stopped after {NUM} of {NUM} steps`
- `STR_JUNCTION_RETRY :{BLACK}Retry`
- `STR_JUNCTION_RETRY_TOOLTIP :{BLACK}Check the cost again and continue building from the failed step`
- `STR_JUNCTION_CANCEL :{BLACK}Cancel`
- `STR_JUNCTION_CANCEL_TOOLTIP :{BLACK}Stop building; pieces already built stay`

**`BuildJunctionWindow`:**
- **`GetWidgetString(WID_BJ_STATUS)`**, by sequencer state:
  - `Building`: `GetString(STR_JUNCTION_STATUS_BUILDING, done, total)`
  - `Failed`: `STR_JUNCTION_STATUS_FAILED` with the same parameters
  - otherwise: `STR_JUNCTION_STATUS_READY`

  Follow how other windows in this tree return label text for a widget.
- **`OnRealtimeTick`, at the start:**
  1. `GameJunctionStepPoster poster(this->railtype); JunctionSequencer &seq = GetJunctionSequencer(); seq.Tick(poster);`
  2. If the state is `Done`: play the confirm sound as `CcPlaySound_CONSTRUCTION_RAIL` does, at the last origin; call `seq.Cancel()` to return to `Idle`; set `this->last_tile = INVALID_TILE` to force a fresh ghost; and call `SetDirty()`.
  3. If the state is `Building` or `Failed`: update the disabled state of Retry and Cancel (Retry is enabled only when `Failed`; Cancel is enabled when `Building` or `Failed`), call `SetWidgetDirty(WID_BJ_STATUS)`, and **return before the hover code**, so the ghost stays where the build started.
  4. Otherwise (`Idle`): disable Retry and Cancel, and carry on with the existing hover code.
- **When a step fails** (the state is `Failed` right after `Tick`, and it wasn't before): rebuild the ghost once, to show what's left. Call `JunctionGhost::Set(layout, origin, height, railtype, bridge_types, estimate)` with the layout, origin, height and bridge types stored at `Start`, and an estimate whose `bad_tiles` holds every tile in `seq.GetRemaining()`. For bridge steps, include both heads. So the window keeps `JunctionLayout build_layout; TileIndex build_origin; int build_height; std::vector<std::optional<BridgeType>> build_bridge_types;`, each documented, and a `JunctionBuildState last_state;` to detect the transition.
- **`OnClick`:**
  - `WID_BJ_RETRY`: re-estimate (`EstimateJunction(build_layout, build_origin, GameJunctionPricing(this->railtype))`), reporting nothing when it's affordable. Note that pieces already built make their tiles fail the estimate as "already built", so **only** check money here: `pricing.AvailableMoney() > 0`. A full re-estimate isn't valid mid-build. Then call `seq.Retry()`.
  - `WID_BJ_CANCEL`: `seq.Cancel()`, then `JunctionGhost::Clear()`, `this->last_tile = INVALID_TILE` and `SetDirty()`.
- **`Close`:** before the existing code, call `GetJunctionSequencer().Cancel()`.

**`JunctionToolClick(TileIndex tile)`,** replacing the empty body:
1. Find the window: `BuildJunctionWindow *w = dynamic_cast<BuildJunctionWindow *>(FindWindowById(WindowClass::BuildJunction, TransportType::Rail))`. Return if it's null, or if the sequencer state isn't `Idle`.
2. Call `w->StartBuild(tile)`, a new public member of the window:
   - work out the layout (`GetPlacementLayout()`), the estimate and the bridge types, exactly as the hover code does
   - if the estimate isn't affordable, return; the red tooltip already says why
   - otherwise store the `build_*` members and call `GetJunctionSequencer().Start(MakeJunctionBuildSteps(layout, tile))`
   - **To avoid duplicating the hover code,** factor the layout, estimate and bridge-type calculation into one private member, `void ComputePlacement(TileIndex tile, JunctionLayout &layout, JunctionEstimate &estimate, std::vector<std::optional<BridgeType>> &bridge_types, Money &available) const`, used by both. This is the one new helper allowed.

### 4.4 Register the callback (upstream edit, separate commit)

In `src/network/network_command.cpp`, add `#include "../junction_game.h"` after the other includes (before `safeguards.h`), and add `&CcJunctionStep,` as the **last** entry of `_callback_tuple`. Appending keeps the existing callback indices unchanged.

Commit this **first**, together with the `CcJunctionStep` declaration it needs. So the commit order is:

1. `Add: Junction build sequencer`: `junction_build.*`, its tests, CMake. Red, then green.
2. `Add: Command callback for junction build steps`: `GameJunctionStepPoster`, `GetJunctionSequencer` and `CcJunctionStep` in `junction_game.*`, plus the `network_command.cpp` registration.
3. `Add: Build junctions from the placement tool, with retry and cancel`: the GUI, widgets and strings.

Each commit ends with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 5. Do not

- Post a command from inside `CcJunctionStep`, or from anywhere except `JunctionSequencer::Tick` via the poster.
- Change the estimate, the ghost store's API, or the drawing code.
- Unit-test the GUI or the real poster.
- Edit upstream files other than `network_command.cpp` and the CMake lists.

## 6. Stop conditions

- The callback registry works differently from what's described, or `Post`'s assertion still fires after registration.
- A named API (`FindWindowById`, `GetString` for widget labels, `SetWidgetDisabledState`, the sound call) differs from its use in this tree.
- Any design choice the brief doesn't cover.

## 7. Gates

Logs go to `.../s9_*.log`.

1. Build `openttd` and `openttd_test` with no errors and no warnings, and no strgen warnings.
2. The red run is recorded. After commit 1: `{BASELINE}` plus 6, all passing. After commits 2 and 3: the same.
3. `./build/openttd -h` exits 0.
4. Report `git diff --stat {HEAD}..HEAD -- src/network/network_command.cpp`.

## 8. Report format

- **References read,** with the new CODINGSTYLE.md rule quoted
- **Commits**
- **Red evidence**
- **Types and members added or removed**
- **Upstream diff size**
- **Suite results**
- **Manual checks for the engineer** (the PLAN-1a checklist, items 2–9)
- **Stops, and anything left undone**
- **State:** `git status --porcelain` and `git log -3 --format='%h %s'`

When you have reported, stop.
