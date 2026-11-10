/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_line_build.cpp Test functionality from junction_line: a painted line laid with signals and a cost (LP10). */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../junction_build.h"
#include "../junction_line.h"
#include "../map_func.h"
#include "../track_func.h"

#include <algorithm>
#include <optional>
#include <set>
#include <variant>
#include <vector>

#include "../safeguards.h"

/** Pricing with fixed prices on flat ground. */
class FlatLinePricing : public JunctionPricing {
public:
	std::optional<JunctionTileSlope> GetTileSlope(TileIndex tile) const override
	{
		if (tile == INVALID_TILE) return std::nullopt;
		return JunctionTileSlope{1, 1, false, true};
	}
	CommandCost TestRail([[maybe_unused]] TileIndex tile, [[maybe_unused]] Track track) const override { return CommandCost(ExpensesType::Construction, 100); }
	CommandCost PriceBridge([[maybe_unused]] TileIndex start, [[maybe_unused]] TileIndex end) const override { return CommandCost(ExpensesType::Construction, 1000); }
	CommandCost PriceTunnel([[maybe_unused]] TileIndex start, [[maybe_unused]] TileIndex end) const override { return CommandCost(ExpensesType::Construction, 700); }
	CommandCost TestStation([[maybe_unused]] TileIndex tile, [[maybe_unused]] Axis axis, [[maybe_unused]] uint8_t platforms, [[maybe_unused]] uint8_t length) const override { return CommandCost(ExpensesType::Construction, 500); }
	CommandCost TestClear([[maybe_unused]] TileIndex tile) const override { return CommandCost(ExpensesType::Construction, 0); }
	Money SignalCost() const override { return 50; }
	Money AvailableMoney() const override { return 1000000; }
};

/** Poster that records every posted step. */
class RecordingPoster : public JunctionStepPoster {
public:
	JunctionSequencer &seq;
	std::vector<JunctionBuildStep> posted;

	explicit RecordingPoster(JunctionSequencer &seq) : seq(seq) {}

	bool Post(const JunctionBuildStep &step) override
	{
		this->posted.push_back(step);
		this->seq.OnStepResult(true);
		return true;
	}
};

static const uint8_t SD = 4;

/** A straight L_R line heading south-west (+x) with its start on a map row far enough from the edges. */
static JunctionLineRoute MakeRoute(const std::string &moves)
{
	return {{2, 30}, Trackdir::X_SW, moves};
}

static JunctionLineBuild Build(const JunctionLineRoute &route, std::vector<bool> along)
{
	Map::Allocate(64, 64);
	const JunctionLineNotation notation = ParseLineNotation("L_R");
	REQUIRE(notation.error == JunctionNotationError::None);
	return BuildJunctionLine(notation.spec, route, along, SD, SignalType::Block, FlatLinePricing{}, TileXY(0, 0));
}

/** The pieces of a track in its direction of travel. */
static std::vector<JunctionPiece> WalkPieces(const JunctionRaster &raster, bool along)
{
	std::vector<JunctionPiece> pieces = raster.pieces;
	if (!along) {
		std::reverse(pieces.begin(), pieces.end());
		for (JunctionPiece &piece : pieces) piece.trackdir = ReverseTrackdir(piece.trackdir);
	}
	return pieces;
}

/** Indices into the walked pieces whose piece carries a signal facing along it. */
static std::vector<size_t> SignalIndices(const JunctionLayout &layout, const std::vector<JunctionPiece> &pieces)
{
	std::vector<size_t> result;
	for (size_t i = 0; i < pieces.size(); i++) {
		if (pieces[i].on_bridge) continue;
		const bool signalled = std::ranges::any_of(layout.signals, [&](const JunctionSignal &s) {
			return s.offset.x == pieces[i].tile.x && s.offset.y == pieces[i].tile.y && s.facing == pieces[i].trackdir;
		});
		if (signalled) result.push_back(i);
	}
	return result;
}

/** The logical weight of the pieces first..last inclusive (192 per axis piece, 128 per diagonal half piece). */
static uint Weight(const std::vector<JunctionPiece> &pieces, size_t first, size_t last)
{
	uint d = 0;
	for (size_t m = first; m <= last; m++) d += pieces[m].units == 2 ? 192 : 128;
	return d;
}

TEST_CASE("a straight L_R line is signalled at SD 4", "[linebuild]")
{
	const JunctionLineRoute route = MakeRoute(std::string(29, 'F'));
	for (const std::vector<bool> &along : {std::vector<bool>{false, true}, std::vector<bool>{true, false}}) {
		const JunctionLineBuild build = Build(route, along);
		REQUIRE(build.error.empty());
		const JunctionLineNotation notation = ParseLineNotation("L_R");
		const std::vector<JunctionRaster> rasters = RasterizeJunctionLine(notation.spec, route);
		REQUIRE(rasters.size() == 2);
		size_t total = 0;
		for (size_t t = 0; t < 2; t++) {
			const std::vector<JunctionPiece> pieces = WalkPieces(rasters[t], along[t]);
			REQUIRE(pieces.size() == 30);
			const std::vector<size_t> indices = SignalIndices(build.layout, pieces);
			total += indices.size();
			/* Walking from the downstream end: pieces 25, 21, ..., 1; none on the first or last piece. */
			REQUIRE(indices.size() == 7);
			CHECK(indices.front() == 1);
			for (size_t i = 1; i < indices.size(); i++) CHECK(indices[i] - indices[i - 1] == 4);
		}
		CHECK(build.layout.signals.size() == total);
	}
}

TEST_CASE("signals skip bridges", "[linebuild]")
{
	const JunctionLineRoute route = MakeRoute("FFFFFFFFFFB3FFFFFFFFFF");
	const JunctionLineNotation notation = ParseLineNotation("L_R");
	const std::vector<JunctionRaster> rasters = RasterizeJunctionLine(notation.spec, route);
	REQUIRE(rasters[0].error.empty());
	for (const std::vector<bool> &along : {std::vector<bool>{false, true}, std::vector<bool>{true, false}}) {
		const JunctionLineBuild build = Build(route, along);
		REQUIRE(build.error.empty());
		REQUIRE(build.layout.bridges.size() == 2);
		for (size_t t = 0; t < 2; t++) {
			const std::vector<JunctionPiece> pieces = WalkPieces(rasters[t], along[t]);
			const std::vector<size_t> indices = SignalIndices(build.layout, pieces);
			REQUIRE(!indices.empty());
			/* The first and last bridge piece in walking order are its heads. */
			size_t near_head = pieces.size();
			size_t far_head = 0;
			for (size_t i = 0; i < pieces.size(); i++) {
				if (!pieces[i].on_bridge) continue;
				near_head = std::min(near_head, i);
				far_head = std::max(far_head, i);
			}
			REQUIRE(near_head < far_head);
			for (size_t i : indices) CHECK((i < near_head || i > far_head));
			/* The signal due on the bridge moves to the last suitable piece before the near head (the head itself is not suitable). */
			CHECK(std::ranges::find(indices, near_head - 1) != indices.end());
		}
	}
}

TEST_CASE("diagonal pieces count 128", "[linebuild]")
{
	const JunctionLineRoute route = MakeRoute("FFFFFLFFFFFFRFFFF");
	const JunctionLineNotation notation = ParseLineNotation("L_R");
	const std::vector<JunctionRaster> rasters = RasterizeJunctionLine(notation.spec, route);
	REQUIRE(rasters[0].error.empty());
	const std::vector<bool> along{false, true};
	const JunctionLineBuild build = Build(route, along);
	REQUIRE(build.error.empty());
	const uint density = SD * 192u;
	bool diagonals = false;
	for (size_t t = 0; t < 2; t++) {
		const std::vector<JunctionPiece> pieces = WalkPieces(rasters[t], along[t]);
		diagonals = diagonals || std::ranges::any_of(pieces, [](const JunctionPiece &p) { return p.units == 1; });
		const std::vector<size_t> indices = SignalIndices(build.layout, pieces);
		REQUIRE(indices.size() >= 2);
		CHECK(indices.front() == 1);
		for (size_t i = 1; i < indices.size(); i++) {
			/* Between consecutive signals the distance is within sd x 192, and one piece more would exceed it. */
			CHECK(Weight(pieces, indices[i - 1] + 1, indices[i]) <= density);
			CHECK(Weight(pieces, indices[i - 1], indices[i]) > density);
		}
		CHECK(Weight(pieces, indices.back() + 1, pieces.size() - 1) <= density);
	}
	CHECK(diagonals);
}

TEST_CASE("the line builds through the fake poster", "[linebuild]")
{
	const JunctionLineBuild build = Build(MakeRoute("FFFFFFFFFFB3FFFFFFFFFF"), {false, true});
	REQUIRE(build.error.empty());
	std::vector<JunctionBuildStep> steps = MakeJunctionBuildSteps(build.layout, TileXY(0, 0));
	const size_t expected = steps.size();
	JunctionSequencer seq;
	RecordingPoster poster(seq);
	seq.Start(std::move(steps));
	for (size_t i = 0; i < expected + 5 && seq.GetState() == JunctionBuildState::Building; i++) seq.Tick(poster);
	CHECK(seq.GetState() == JunctionBuildState::Done);
	REQUIRE(poster.posted.size() == expected);

	size_t rails = 0;
	size_t bridges = 0;
	size_t signals = 0;
	std::set<TileIndex> railed;
	for (const JunctionBuildStep &step : poster.posted) {
		if (const auto *rail = std::get_if<JunctionRailStep>(&step)) {
			rails++;
			railed.insert(rail->tile);
		} else if (std::holds_alternative<JunctionBridgeStep>(step)) {
			bridges++;
		} else if (const auto *signal = std::get_if<JunctionSignalStep>(&step)) {
			signals++;
			/* Tracks before signals on the same tile. */
			CHECK(railed.contains(signal->tile));
		}
	}
	size_t track_pieces = 0;
	for (const JunctionTrack &jt : build.layout.tracks) {
		for ([[maybe_unused]] Track t : jt.tracks) track_pieces++;
	}
	CHECK(rails == track_pieces);
	CHECK(bridges == build.layout.bridges.size());
	CHECK(bridges == 2);
	CHECK(signals == build.layout.signals.size());
	CHECK(signals > 0);
}

TEST_CASE("the estimate counts every piece", "[linebuild]")
{
	const JunctionLineRoute route = MakeRoute("FFFFFFFFFFB3FFFFFFFFFF");
	const JunctionLineBuild build = Build(route, {false, true});
	REQUIRE(build.error.empty());
	const JunctionEstimate expected = EstimateJunction(build.layout, TileXY(0, 0), FlatLinePricing{});
	CHECK(build.estimate.total == expected.total);
	CHECK(build.estimate.total > 0);
	CHECK(build.estimate.bad_tiles == expected.bad_tiles);
	CHECK(build.estimate.affordable == expected.affordable);
}

TEST_CASE("a bad line or a wrong direction count is an error", "[linebuild]")
{
	CHECK(!Build(MakeRoute("FFFFFFFFFF"), {true}).error.empty());
	const JunctionLineBuild bad = Build(MakeRoute("FFX"), {false, true});
	CHECK(!bad.error.empty());
	CHECK(bad.layout.tracks.empty());
}
