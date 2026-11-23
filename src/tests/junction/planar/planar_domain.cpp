/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */


/** @file planar_domain.cpp Test the domain predicate (D428): hand cases laid in the game's vocabulary, and the domain rasteriser against the game's route rasteriser. */

#include "../../../stdafx.h"

#include "../../../3rdparty/catch2/catch.hpp"

#include "../../../core/format.hpp"
#include "../../../junction/junction_route.h"
#include "../../../junction/planar/planar_domain.h"
#include "../../../direction_func.h"
#include "../../../track_func.h"

#include <algorithm>
#include <array>
#include <set>
#include <string>
#include <vector>

#include "../../../safeguards.h"

using Point = std::array<int, 2>;
using Points = std::vector<Point>;

static const Trackdir AXIS_STARTS[] = {Trackdir::X_NE, Trackdir::X_SW, Trackdir::Y_SE, Trackdir::Y_NW};
static const Trackdir DIAGONAL_STARTS[] = {Trackdir::Upper_E, Trackdir::Lower_E, Trackdir::Left_S, Trackdir::Right_S, Trackdir::Upper_W, Trackdir::Lower_W, Trackdir::Left_N, Trackdir::Right_N};

static constexpr int LEG_MOVES = 12; ///< Moves per leg (13 pieces).

/** The midpoint of an edge of a tile, in half tiles. */
static Point EdgeMidpoint(TileIndexDiffC tile, DiagDirection edge)
{
	switch (edge) {
		case DiagDirection::NE: return {2 * tile.x, 2 * tile.y + 1};
		case DiagDirection::SW: return {2 * tile.x + 2, 2 * tile.y + 1};
		case DiagDirection::NW: return {2 * tile.x + 1, 2 * tile.y};
		default: return {2 * tile.x + 1, 2 * tile.y + 2};
	}
}

/** The route's points: the first piece's entry midpoint, then each piece's exit midpoint. */
static Points TrackFromRoute(const JunctionRoute &route, JunctionRaster &raster)
{
	raster = RasterizeJunctionRoute(route);
	REQUIRE(raster.error.empty());
	REQUIRE(!raster.pieces.empty());
	Points points;
	const JunctionPiece &first = raster.pieces.front();
	points.push_back(EdgeMidpoint(first.tile, TrackdirToExitdir(ReverseTrackdir(first.trackdir))));
	for (const JunctionPiece &jp : raster.pieces) points.push_back(EdgeMidpoint(jp.tile, TrackdirToExitdir(jp.trackdir)));
	return points;
}

/** A laid route: its points and its raster. */
struct Laid {
	Points points;
	JunctionRaster raster;
};

static Laid Lay(TileIndexDiffC start, Trackdir trackdir, const std::string &moves)
{
	Laid laid;
	INFO("start " << start.x << "," << start.y << " trackdir " << static_cast<int>(trackdir) << " moves " << moves);
	laid.points = TrackFromRoute({start, trackdir, moves}, laid.raster);
	return laid;
}

static std::string Straight() { return std::string(LEG_MOVES, 'F'); }
static std::string Turn(char side) { return std::string(1, side) + std::string(LEG_MOVES - 1, 'F'); }

/** The sides a first move can take from a start trackdir. */
static std::vector<char> Sides(Trackdir start)
{
	std::vector<char> sides;
	for (char side : {'L', 'R'}) {
		if (RasterizeJunctionRoute({{0, 0}, start, Turn(side)}).error.empty()) sides.push_back(side);
	}
	return sides;
}

/** Points first..last inclusive. */
static Points Slice(const Points &points, size_t first, size_t last)
{
	return Points(points.begin() + first, points.begin() + last + 1);
}

/** The first piece index at which the routes' trackdirs differ. */
static size_t Diverge(const Laid &a, const Laid &b)
{
	size_t d = 0;
	while (d < a.raster.pieces.size() && d < b.raster.pieces.size() && a.raster.pieces[d].trackdir == b.raster.pieces[d].trackdir) d++;
	REQUIRE(d < a.raster.pieces.size());
	REQUIRE(d < b.raster.pieces.size());
	return d;
}

/** A split: the trunk, then each leg from the first index its trackdirs differ from the others'. */
static PlanarDomainInput SplitInput(const std::vector<Laid> &legs)
{
	size_t d = SIZE_MAX;
	for (size_t i = 1; i < legs.size(); i++) d = std::min(d, Diverge(legs[0], legs[i]));
	REQUIRE(d >= 1);
	PlanarDomainInput input;
	input.tracks.push_back(Slice(legs[0].points, 0, d));
	PlanarDomainNode node{input.tracks[0].back(), {0}, {}};
	for (size_t i = 0; i < legs.size(); i++) {
		input.tracks.push_back(Slice(legs[i].points, d, legs[i].points.size() - 1));
		node.out.push_back(static_cast<uint32_t>(i + 1));
	}
	input.nodes.push_back(node);
	return input;
}

/** The same geometry walked the other way: every track reversed, every node's sides swapped. */
static PlanarDomainInput Reversed(PlanarDomainInput input)
{
	for (Points &track : input.tracks) std::reverse(track.begin(), track.end());
	for (PlanarDomainNode &node : input.nodes) std::swap(node.in, node.out);
	return input;
}

/** A cascade: straight, the first split on `first`, the second on `second` two tiles past the first's divergence; false when the second side does not exist there. */
static bool CascadeInput(Trackdir start, char first, char second, PlanarDomainInput &input)
{
	const Laid straight = Lay({0, 0}, start, Straight());
	const Laid split = Lay({0, 0}, start, Turn(first));
	const size_t d1 = Diverge(straight, split);
	const size_t d2 = d1 + 2;
	const std::string moves = std::string(d2 - 1, 'F') + std::string(1, second) + std::string(LEG_MOVES - d2, 'F');
	if (!RasterizeJunctionRoute({{0, 0}, start, moves}).error.empty()) return false;
	const Laid other = Lay({0, 0}, start, moves);
	REQUIRE(Diverge(straight, other) == d2);
	input = {};
	input.tracks = {Slice(straight.points, 0, d1), Slice(straight.points, d1, d2), Slice(split.points, d1, split.points.size() - 1), Slice(straight.points, d2, straight.points.size() - 1), Slice(other.points, d2, other.points.size() - 1)};
	input.nodes.push_back({input.tracks[0].back(), {0}, {1, 2}});
	input.nodes.push_back({input.tracks[1].back(), {1}, {3, 4}});
	return true;
}

/** A structure on the separator's on_bridge pieces. */
static PlanarDomainStructure StructureOn(const Laid &laid, uint32_t separator, bool tunnel, const std::vector<uint32_t> &crossers, int limit = -1)
{
	size_t first = SIZE_MAX, last = 0;
	for (size_t i = 0; i < laid.raster.pieces.size(); i++) {
		if (!laid.raster.pieces[i].on_bridge) continue;
		first = std::min(first, i);
		last = i;
	}
	REQUIRE(first != SIZE_MAX);
	REQUIRE(last - first == 3);
	return {separator, static_cast<uint32_t>(first), static_cast<uint32_t>(last), tunnel, limit, crossers};
}

/** The case-7 bridge as track 0, the other routes after it. */
static PlanarDomainInput BridgeInput(const std::vector<Laid> &others, const std::vector<uint32_t> &crossers, int limit = -1)
{
	const Laid separator = Lay({0, 2}, Trackdir::X_SW, "FFB2FF");
	PlanarDomainInput input;
	input.tracks.push_back(separator.points);
	for (const Laid &laid : others) input.tracks.push_back(laid.points);
	input.structures.push_back(StructureOn(separator, 0, false, crossers, limit));
	return input;
}

static std::string Describe(const PlanarDomainVerdict &verdict)
{
	std::string text;
	for (const PlanarDomainBreak &b : verdict.breaks) {
		text += fmt::format("{} ({},{}) tracks", b.rule, b.tile.x, b.tile.y);
		for (uint32_t t : b.tracks) text += fmt::format(" {}", t);
		text += fmt::format(": {}\n", b.why);
	}
	return text;
}

static void ExpectLegal(const PlanarDomainInput &input)
{
	const PlanarDomainVerdict verdict = CheckPlanarDomain(input);
	INFO(Describe(verdict));
	CHECK(verdict.legal);
	CHECK(verdict.breaks.empty());
}

static void ExpectBreaks(const PlanarDomainInput &input, const std::set<std::string> &rules)
{
	const PlanarDomainVerdict verdict = CheckPlanarDomain(input);
	std::set<std::string> found;
	for (const PlanarDomainBreak &b : verdict.breaks) found.insert(b.rule);
	INFO(Describe(verdict));
	CHECK(found == rules);
	CHECK(verdict.legal == rules.empty());
}

TEST_CASE("planar domain: hand cases from the game", "[junction][planar][planardomain]")
{
	SECTION("1 legal: two parallel X tracks on neighbouring rows") {
		PlanarDomainInput input;
		input.tracks = {Lay({0, 2}, Trackdir::X_SW, "FFFF").points, Lay({0, 3}, Trackdir::X_SW, "FFFF").points};
		ExpectLegal(input);
	}

	SECTION("2 legal: two diagonal tracks sharing tiles as Upper|Lower") {
		PlanarDomainInput input;
		input.tracks = {Lay({0, 0}, Trackdir::Upper_E, "FFFFFF").points, Lay({0, 1}, Trackdir::Upper_E, "FFFFFF").points};
		/* The tracks do share tiles, each time as the parallel pair of corners. */
		const PlanarDomainRaster a = RasterPlanarDomainTrack(input.tracks[0]);
		const PlanarDomainRaster b = RasterPlanarDomainTrack(input.tracks[1]);
		size_t shared = 0;
		for (const PlanarDomainPiece &pa : a.pieces) {
			for (const PlanarDomainPiece &pb : b.pieces) {
				if (pa.tile.x != pb.tile.x || pa.tile.y != pb.tile.y) continue;
				shared++;
				CHECK((pa.track != pb.track));
			}
		}
		CHECK(shared >= 2);
		ExpectLegal(input);
	}

	SECTION("3 legal: a split on every axis and diagonal trunk") {
		for (const Trackdir *starts : {AXIS_STARTS, DIAGONAL_STARTS}) {
			const size_t n = starts == AXIS_STARTS ? std::size(AXIS_STARTS) : std::size(DIAGONAL_STARTS);
			for (size_t i = 0; i < n; i++) {
				for (char side : Sides(starts[i])) {
					INFO("start " << static_cast<int>(starts[i]) << " side " << side);
					ExpectLegal(SplitInput({Lay({0, 0}, starts[i], Straight()), Lay({0, 0}, starts[i], Turn(side))}));
				}
			}
		}
	}

	SECTION("4 legal: a trident") {
		for (Trackdir start : AXIS_STARTS) {
			INFO("start " << static_cast<int>(start));
			ExpectLegal(SplitInput({Lay({0, 0}, start, Straight()), Lay({0, 0}, start, Turn('L')), Lay({0, 0}, start, Turn('R'))}));
		}
	}

	SECTION("5 legal: a merge, the split's legs reversed") {
		for (const Trackdir *starts : {AXIS_STARTS, DIAGONAL_STARTS}) {
			const size_t n = starts == AXIS_STARTS ? std::size(AXIS_STARTS) : std::size(DIAGONAL_STARTS);
			for (size_t i = 0; i < n; i++) {
				for (char side : Sides(starts[i])) {
					INFO("start " << static_cast<int>(starts[i]) << " side " << side);
					ExpectLegal(Reversed(SplitInput({Lay({0, 0}, starts[i], Straight()), Lay({0, 0}, starts[i], Turn(side))})));
				}
			}
		}
	}

	SECTION("6 legal: a cascade") {
		for (const Trackdir *starts : {AXIS_STARTS, DIAGONAL_STARTS}) {
			const size_t n = starts == AXIS_STARTS ? std::size(AXIS_STARTS) : std::size(DIAGONAL_STARTS);
			for (size_t i = 0; i < n; i++) {
				for (char first : Sides(starts[i])) {
					for (char second : {'L', 'R'}) {
						PlanarDomainInput input;
						if (!CascadeInput(starts[i], first, second, input)) continue;
						INFO("start " << static_cast<int>(starts[i]) << " first " << first << " second " << second);
						ExpectLegal(input);
					}
				}
			}
		}
	}

	SECTION("7 legal: a bridge over a listed crosser") {
		ExpectLegal(BridgeInput({Lay({4, 0}, Trackdir::Y_SE, "FFFF")}, {1}));
	}

	SECTION("8 legal: a bridge middle over a tunnel middle, and two tunnel middles on one tile") {
		const Laid bridge = Lay({0, 2}, Trackdir::X_SW, "FFB2FF");
		const Laid tunnel_x = Lay({0, 2}, Trackdir::X_SW, "FFT2FF");
		const Laid tunnel_y = Lay({4, -1}, Trackdir::Y_SE, "FT2F");
		PlanarDomainInput input;
		input.tracks = {bridge.points, tunnel_y.points};
		input.structures = {StructureOn(bridge, 0, false, {}), StructureOn(tunnel_y, 1, true, {})};
		ExpectLegal(input);
		input = {};
		input.tracks = {tunnel_x.points, tunnel_y.points};
		input.structures = {StructureOn(tunnel_x, 0, true, {}), StructureOn(tunnel_y, 1, true, {})};
		ExpectLegal(input);
	}

	SECTION("8b legal: a listed crossing between two tunnels' middles is realised by depth (D390 (2); V's PSG 197 L30/L31 under L32/L47/L48, game tiles (91..93,163..164))") {
		const Laid tunnel_x = Lay({0, 2}, Trackdir::X_SW, "FFT2FF");
		const Laid tunnel_y = Lay({4, -1}, Trackdir::Y_SE, "FT2F");
		PlanarDomainInput input;
		input.tracks = {tunnel_x.points, tunnel_y.points};
		input.structures = {StructureOn(tunnel_x, 0, true, {1}), StructureOn(tunnel_y, 1, true, {0})};
		ExpectLegal(input);
	}

	SECTION("8d legal: a tunnel head above another tunnel's middle, listed under the lower tunnel (D390 (2), D393; V's PSG 197 L11/L47, game tile (92,171))") {
		const Laid lower = Lay({0, 2}, Trackdir::X_SW, "FFT2FF");
		const Laid upper = Lay({4, 0}, Trackdir::Y_SE, "FT2F");
		/* The upper tunnel's first end piece is on the lower tunnel's middle tile (4, 2). */
		const size_t head = StructureOn(upper, 1, true, {}).first;
		REQUIRE(upper.raster.pieces[head].tile.x == 4);
		REQUIRE(upper.raster.pieces[head].tile.y == 2);
		PlanarDomainInput input;
		input.tracks = {lower.points, upper.points};
		input.structures = {StructureOn(lower, 0, true, {1}), StructureOn(upper, 1, true, {})};
		ExpectLegal(input);
	}

	SECTION("8e illegal: a bridge head on another bridge's deck tile stays refused") {
		const Laid lower = Lay({0, 2}, Trackdir::X_SW, "FFB2FF");
		const Laid upper = Lay({4, 0}, Trackdir::Y_SE, "FB2F");
		PlanarDomainInput input;
		input.tracks = {lower.points, upper.points};
		input.structures = {StructureOn(lower, 0, false, {1}), StructureOn(upper, 1, false, {})};
		ExpectBreaks(input, {"head", "missing crossing"});
	}

	SECTION("8c illegal: a listed crossing between two bridges' middles stays refused") {
		const Laid a = Lay({0, 2}, Trackdir::X_SW, "FFB2FF");
		const Laid b = Lay({4, -1}, Trackdir::Y_SE, "FB2F");
		PlanarDomainInput input;
		input.tracks = {a.points, b.points};
		input.structures = {StructureOn(a, 0, false, {1}), StructureOn(b, 1, false, {0})};
		ExpectBreaks(input, {"decks", "missing crossing"});
	}

	SECTION("9 illegal: an X and a Y track cross at grade") {
		PlanarDomainInput input;
		input.tracks = {Lay({0, 2}, Trackdir::X_SW, "FFFF").points, Lay({2, 0}, Trackdir::Y_SE, "FFFF").points};
		ExpectBreaks(input, {"diamond"});
	}

	SECTION("10 illegal: a track ends on another's edge midpoint with no node") {
		PlanarDomainInput input;
		input.tracks = {Lay({0, 2}, Trackdir::X_SW, "FFFF").points, {{5, 2}, {5, 4}, {4, 5}}};
		ExpectBreaks(input, {"connection"});
	}

	SECTION("11 illegal: a fourth track ends at a split's node, unlisted") {
		PlanarDomainInput input = SplitInput({Lay({0, 0}, Trackdir::X_SW, Straight()), Lay({0, 0}, Trackdir::X_SW, Turn('L'))});
		REQUIRE(input.nodes[0].at == Point{2, 1});
		input.tracks.push_back({{1, -2}, {1, 0}, {2, 1}});
		ExpectBreaks(input, {"connection"});
	}

	SECTION("12 illegal: a node lists a track that does not start at it") {
		PlanarDomainInput input = SplitInput({Lay({0, 0}, Trackdir::X_SW, Straight()), Lay({0, 0}, Trackdir::X_SW, Turn('L'))});
		input.tracks.push_back(Lay({0, 10}, Trackdir::X_SW, "FFFF").points);
		input.nodes[0].out.push_back(3);
		ExpectBreaks(input, {"node"});
	}

	SECTION("13 illegal: a bend at a tile centre") {
		PlanarDomainInput input;
		input.tracks = {{{0, 1}, {3, 1}, {4, 2}}};
		ExpectBreaks(input, {"grid"});
	}

	SECTION("14 illegal: two corners on one tile") {
		PlanarDomainInput input;
		input.tracks = {{{0, 1}, {1, 0}, {2, 1}}};
		ExpectBreaks(input, {"turn"});
	}

	SECTION("15 illegal: a bridge over an unlisted track") {
		ExpectBreaks(BridgeInput({Lay({4, 0}, Trackdir::Y_SE, "FFFF")}, {}), {"under"});
	}

	SECTION("16 illegal: a listed crosser that never passes under") {
		ExpectBreaks(BridgeInput({Lay({0, 4}, Trackdir::X_SW, "FFFFFFFF")}, {1}), {"missing crossing"});
	}

	SECTION("17 illegal: two bridges' middles on one tile") {
		const Laid a = Lay({0, 2}, Trackdir::X_SW, "FFB2FF");
		const Laid b = Lay({4, -1}, Trackdir::Y_SE, "FB2F");
		PlanarDomainInput input;
		input.tracks = {a.points, b.points};
		input.structures = {StructureOn(a, 0, false, {}), StructureOn(b, 1, false, {})};
		ExpectBreaks(input, {"decks"});
	}

	SECTION("18 illegal: an unlisted track through a head tile") {
		ExpectBreaks(BridgeInput({Lay({3, 0}, Trackdir::Y_SE, "FFFF")}, {}), {"head"});
	}

	SECTION("19 illegal: a bridge longer than its limit") {
		ExpectBreaks(BridgeInput({}, {}, 1), {"span"});
	}

	/* A tunnel with two end pieces at each end: pieces 1, 2 and 5, 6 of the separator, middles 3 and 4, a crosser through the tile of piece `under` from row `crosser_y`. */
	const auto tunnel_input = [](const std::string &moves, uint32_t under, bool crosser_listed, int crosser_y = 0) {
		const Laid separator = Lay({0, 2}, Trackdir::X_SW, moves);
		REQUIRE(separator.raster.pieces.size() >= 8);
		const TileIndexDiffC at = separator.raster.pieces[under].tile;
		const Laid crosser = Lay({at.x, static_cast<int16_t>(crosser_y)}, Trackdir::Y_SE, "FFFF");
		PlanarDomainInput input;
		input.tracks = {separator.points, crosser.points};
		PlanarDomainStructure st;
		st.separator = 0;
		st.first = 1;
		st.last = 6;
		st.tunnel = true;
		st.ends = 2;
		if (crosser_listed) st.crossers = {1};
		input.structures.push_back(st);
		return input;
	};

	SECTION("20 legal: a tunnel with two straight end pieces at each end") {
		ExpectLegal(tunnel_input("FFFFFFFFF", 3, true));
	}

	SECTION("21 illegal: an unlisted track through a tunnel's ramp piece") {
		ExpectBreaks(tunnel_input("FFFFFFFFF", 2, false), {"head"});
	}

	SECTION("22 illegal: a tunnel's outer end piece on a diagonal") {
		ExpectBreaks(tunnel_input("FLFRFFFFF", 3, true, 8), {"span"});
	}
}

TEST_CASE("planar domain: the rasteriser matches the game's", "[junction][planar][planardomain]")
{
	const std::vector<std::string> moves_list = {"FFFF", "LFFF", "RFFF", "FLFRFFF", "FRFLFFF", "FLLFFF", "FRRFFF"};
	size_t kept = 0;
	for (const Trackdir *starts : {AXIS_STARTS, DIAGONAL_STARTS}) {
		const size_t n = starts == AXIS_STARTS ? std::size(AXIS_STARTS) : std::size(DIAGONAL_STARTS);
		for (size_t s = 0; s < n; s++) {
			for (const std::string &moves : moves_list) {
				if (!RasterizeJunctionRoute({{0, 0}, starts[s], moves}).error.empty()) continue;
				kept++;
				INFO("start " << static_cast<int>(starts[s]) << " moves " << moves);
				JunctionRaster raster;
				const Points points = TrackFromRoute({{0, 0}, starts[s], moves}, raster);
				const PlanarDomainRaster domain = RasterPlanarDomainTrack(points);
				REQUIRE(domain.rule.empty());
				REQUIRE(domain.pieces.size() == raster.pieces.size());
				for (size_t i = 0; i < raster.pieces.size(); i++) {
					INFO("piece " << i);
					CHECK((domain.pieces[i].tile.x == raster.pieces[i].tile.x && domain.pieces[i].tile.y == raster.pieces[i].tile.y));
					CHECK(domain.pieces[i].track == TrackdirToTrack(raster.pieces[i].trackdir));
				}
			}
		}
	}
	REQUIRE(kept >= 60);
}
