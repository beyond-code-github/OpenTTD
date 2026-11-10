/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_generator.h Junction generator: spec, notation parsing and curve spacing. */

#ifndef JUNCTION_GENERATOR_H
#define JUNCTION_GENERATOR_H

#include "economy_type.h"
#include "junction_layout.h"
#include "junction_terrain.h"
#include "junction_train_model.h"
#include "signal_type.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

enum class JunctionNotationError : uint8_t { None, Invalid, MultiLane };
struct JunctionNotation { uint8_t gap = 0; JunctionNotationError error = JunctionNotationError::None; };
JunctionNotation ParseJunctionNotation(std::string_view text);

struct JunctionUnitPrices {
	Money rail_piece;
	Money signal;
	std::function<Money(uint)> bridge; ///< By middle-tile count.
	std::function<Money(uint)> tunnel; ///< One tunnel by middle-tile count, as CmdBuildTunnel charges it without clearing the portals; unset: no tunnels (D155).
};

/** Everything the generator needs; the game fills it (PLAN-1b §5.1). */
struct JunctionSpec {
	uint8_t ml_gap;
	uint8_t branch_gap;
	uint8_t tl; ///< Train length in tiles.
	uint8_t prio_tiles = 0; ///< Prio coverage upstream of a merge, in straight tiles; set by GenerateJunction's sizing (D137), where a non-zero value is the least coverage tried.
	uint8_t sd; ///< Signal spacing in tiles.
	uint16_t reference_speed; ///< Same units as Train::GetCurveSpeedLimit.
	uint8_t curve_speed; ///< RailTypeInfo::curve_speed.
	bool curve_limit; ///< False under the original acceleration model.
	SignalType signal_type; ///< Block or PathOneWay.
	std::function<bool(uint)> bridge_span_ok; ///< By middle-tile count.
	JunctionUnitPrices prices;
	uint16_t max_tunnel_length = 64; ///< The game's max_tunnel_length, in middle tiles.
	JunctionTrainParams train{}; ///< The loaded design train (D101, D137); required.
	bool prio_strict = true; ///< Reject layouts whose prio timing delays the ML (D119); false only for tests that pin geometry at a fixed coverage, and for rig comparisons (D137).
};

/** Why generation failed; PrioTooLong: the prio sizing found no coverage within its search bound; PrioDelaysMainline: the generated layout's prio delays the ML at the spec's coverage (D137). */
enum class JunctionGenerateError : uint8_t { None, InvalidNotation, MultiLane, NoLayout, NeedsMlGap, PrioTooLong, PrioDelaysMainline, PortBlocked };
struct JunctionGenerateResult {
	std::optional<JunctionLayout> layout;
	JunctionGenerateError error = JunctionGenerateError::None;
	TileIndexDiffC hub{}; ///< The offset, in the returned layout, of the frame's hub point (hub_x, 0).
	std::pair<int, Money> score{}; ///< The chosen layout's score (tiles, cost), terrain cost included in the cost.
	uint8_t prio_tiles = 0; ///< The prio coverage the sizing chose, in tiles.
};

uint16_t CurveSpeedLimitForSpacing(uint spacing, uint8_t curve_speed);
uint DeriveCurveSpacing(uint16_t speed, uint8_t curve_speed, uint8_t tl);
JunctionSpec MakeTestJunctionSpec();
/** The result of SizeJunctionPrio. */
struct JunctionPrioSizing {
	uint8_t prio_tiles = 0; ///< The sized coverage in tiles; 0 when sizing failed.
	JunctionGenerateError error = JunctionGenerateError::None; ///< Why sizing failed: the generation's error, or PrioTooLong when the coverage would exceed the bound; None on success.
};
/** The least prio coverage in tiles that does not delay the ML on flat ground for the spec's train, starting at max(1, spec.prio_tiles); {0, error} when none is found (D137). */
JunctionPrioSizing SizeJunctionPrio(const JunctionSpec &spec);
JunctionGenerateResult GenerateJunction(const JunctionSpec &spec, const JunctionTerrain *terrain = nullptr);

/** The canonical working frame the generator, terrain snapshot and library fit share (RECON-frame). */
struct JunctionFrame {
	int hub_x = 30; ///< Working x of the hub (N column x E row), xI.
	int width = 64; ///< Working x runs 0 .. width-1.
	int reach_east = 16; ///< The router's search box ends this far east of xO.
	int terrain_height = 43; ///< Terrain rows, from terrain_origin_y.
	int terrain_origin_y = -2; ///< Working y of the terrain's first row.
};
/** The frame for a spec: the defaults up to a prio coverage of one train length, growing with the coverage beyond it. */
JunctionFrame MakeJunctionFrame(const JunctionSpec &spec);

class JunctionRouteCache;

/** The swept inputs of one candidate. */
struct JunctionCandidateParams {
	uint8_t height; ///< H: the branch edge is row H-1; the E track is row 0.
	Money crossing_cost; ///< crossable_cost when a route crosses the W track.
	uint8_t e_slack; ///< Extra tiles kept between the E->S split's prio coverage and where N->E may join (symmetric style 0); for the parallelogram style the E-side phase, 0 or 1 (D148a).
	uint8_t w_slack; ///< Extra tiles kept between the lane merge and where N->W may join; symmetric style (0) only, the parallelogram style ignores it (D145, D146).
	int plane = 0; ///< The level the junction is built on; only used with a terrain.
	uint8_t bridge_margin = 3; ///< Bridge heads sit this many tiles beyond the crossing set (D103); symmetric style (0) only, the parallelogram style ignores it (D145, D146).
	uint8_t lane_style = 0; ///< 0 = symmetric Y (D100), 1 = parallelogram (D105).
	JunctionRouteCache *route_cache = nullptr; ///< When set, router results are shared through it (one cache per generation).
	int k_w_delta = 0; ///< Test seam: added to the parallelogram style's prio coverage after the lane merge (D145).
	int k_sb_delta = 0; ///< Test seam: added to the parallelogram style's shift-back distance (D145).
	int k_e_delta = 0; ///< Test seam: added to the parallelogram style's E-side window start (D148).
};

/**
 * Build one generator candidate.
 * @param spec The junction spec.
 * @param params The swept inputs.
 * @param terrain Optional terrain snapshot, in the generator's working coordinates before cropping and normalising (x 0..frame.width-1, rows from 0).
 *                The anchor is the hub point (frame.hub_x, 0): the terrain's cell (frame.hub_x - origin.x, 0 - origin.y) must be the hovered tile.
 * @param terrain_cost When set together with terrain, receives the sum of CostAt over occupied track tiles and bridge heads, once per tile.
 * @param hub When set, receives the hub offset.
 * @param clean When set, receives whether the returned layout is rule-clean (already proven while building, so callers need not check again).
 * @return The layout, or std::nullopt when this candidate cannot be built.
 */
std::optional<JunctionLayout> BuildJunctionCandidate(const JunctionSpec &spec, const JunctionCandidateParams &params, const JunctionTerrain *terrain = nullptr, Money *terrain_cost = nullptr, TileIndexDiffC *hub = nullptr, bool *clean = nullptr);

/** Generator score of a layout: the count of tiles it builds on or over, then the build cost (D114). Lower is better. */
std::pair<int, Money> ScoreJunctionLayout(const JunctionLayout &layout, const JunctionSpec &spec);

/** The candidate planes for a terrain: the most common z_max values of Buildable tiles around the hub, at most 3. */
std::vector<int> JunctionTerrainPlanes(const JunctionTerrain &terrain);

/** One rule-clean flat candidate (D96 instant tier). */
struct JunctionLibraryEntry {
	JunctionLayout layout;
	TileIndexDiffC hub{}; ///< As JunctionGenerateResult::hub.
	std::pair<int, Money> score; ///< ScoreJunctionLayout on flat ground.
	int area = 0; ///< layout.size.x * layout.size.y.
	JunctionFrame frame; ///< The frame the entry was built with; FitJunctionLibrary places the hub at its hub_x.
	uint8_t prio_tiles = 0; ///< The prio coverage the entry was built at, in tiles.
};

/**
 * Every rule-clean flat candidate of the sweep, without the early stop: heights ml_gap+5 .. ml_gap+20, the D146 sweep.
 * Identical layouts (same JunctionLayoutToJson) are kept once, the first one found. Order is sweep order.
 * The spec must be sized (prio_tiles > 0, see SizeJunctionPrio); candidates whose prio delays the ML are dropped (unless spec.prio_strict is false), and every entry records spec.prio_tiles.
 */
std::vector<JunctionLibraryEntry> BuildJunctionLibrary(const JunctionSpec &spec);

/** The result of fitting the library to a terrain. */
struct JunctionLibraryFit {
	size_t entry = 0; ///< Index into the library.
	int plane = 0;
	Money terrain_cost = 0;
};

/**
 * Fit each library entry to the terrain at each of JunctionTerrainPlanes(terrain), with tile lookups only.
 * A fit needs every tile holding track and every bridge head UsableAt the plane, and every bridge middle tile SpanOk.
 * Terrain cost is the sum of CostAt over the tiles holding track or a bridge head (each tile once).
 * Best = lowest (score.first, score.second + terrain_cost, area), ties to the earlier entry, then the earlier plane.
 * @return The best fit, or std::nullopt when nothing fits (also when the spec gives NeedsMlGap, i.e. an empty library).
 */
std::optional<JunctionLibraryFit> FitJunctionLibrary(const std::vector<JunctionLibraryEntry> &library, const JunctionTerrain &terrain);

struct RuleViolation;

/** The constructed map of a T (DESIGN-O4 "G1"): the decisions taken and what they fixed. */
struct JunctionTMapResult {
	std::optional<JunctionLayout> layout; ///< Finished (cropped, signalled) when built; set even when not rule-clean.
	bool clean = false; ///< JunctionRulesClean on the layout, run once.
	bool port_blocked = false; ///< D179: a port's approach is unusable; nothing was routed.
	std::vector<RuleViolation> violations; ///< CheckJunctionRules when not clean.
	int height = 0; ///< The height that routed.
	int heights_tried = 0;
	int layouts_built = 0; ///< Must equal the number of heights at which every edge routed (at most 1 with the current rule).
	uint8_t stagger = 0; ///< D118, chosen by the smaller bridge span.
	uint8_t e_phase = 0; ///< D148a.
	TileIndexDiffC hub{}; ///< The offset, in the layout, of the frame's hub point (hub_x, 0), as JunctionGenerateResult::hub.
	int64_t micros = 0;
	int64_t micros_flat = 0; ///< D173: the flat map that gives the line router its first height.
	int64_t micros_route = 0; ///< All router calls, every height tried.
	int64_t micros_skeleton = 0; ///< Crossing set, both skeleton solves, lanes.
	int64_t micros_finish = 0; ///< FinishJunctionCandidate (crop, normalise, signal pass).
	int64_t micros_check = 0; ///< JunctionRulesClean (and CheckJunctionRules when unclean).
	std::vector<int64_t> micros_per_height; ///< Wall time of each height tried, in order.
	std::string log; ///< One line per height tried: which edge failed, or "built".
	std::vector<TileIndexDiffC> a_to_s_tiles; ///< Test aid: the tiles of A->S's route, frame coordinates, before cropping.
	std::vector<TileIndexDiffC> n_to_a_tiles; ///< Test aid: the tiles of N->A's route, frame coordinates, before cropping.
	std::string line_router_trace; ///< Test aid: one line per line-router request: moves, cost, expansions.
};

/**
 * The lowest height at which the T can build (G2b amendment 1), derived from the rules G1 applies.
 * A->S must cross column xI between rows 0 and ys (its split is at x <= xI-2, its goal at xO > xI on row ys entered from above).
 * The column fence forbids rows 0..g+3 and ys-1, ys-2, so it crosses at y_c >= g+4.
 * N->A starts at (xI, ys), its first piece on ys-1 carries the signal and its near head is on ys-2 (D113, D3d);
 * its bridge over y_c needs a middle tile at y_c, so ys-2 >= y_c+1, giving ys >= g+7.
 * ys = H-3 (D115a stub), so H >= g+10.
 */
int JunctionTMapFirstHeight(const JunctionSpec &spec);

/**
 * Construct the T's map from the rules and build it once (DESIGN-O4 "G1").
 * @param first_height Test seam: when set, the heights start here instead of JunctionTMapFirstHeight.
 */
JunctionTMapResult ConstructJunctionTMap(const JunctionSpec &spec, const JunctionTerrain *terrain = nullptr, std::optional<int> first_height = std::nullopt, bool line_router = false);

/** D148a: the E-side window phase. 1 when prio_tiles is odd and at least 5, else 0. */
uint8_t JunctionEWindowPhase(int prio_tiles);

#endif /* JUNCTION_GENERATOR_H */
