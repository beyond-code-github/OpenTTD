/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_node_rules.cpp Test the node rule's proof (D400): where the legs of a split, a trident and a cascade stop clashing, per heading class. */

#include "../../../stdafx.h"

#include "../../../3rdparty/catch2/catch.hpp"

#include "../../../junction/junction_route.h"
#include "../../../junction/planar/planar_layout.h"
#include "../../../3rdparty/fmt/format.h"
#include "../../../track_func.h"

#include <algorithm>
#include <string>
#include <vector>

#include "../../../safeguards.h"

/*
 * The proof's measured facts (D400): the piece index (counted from the node's trunk start, legs of 13 pieces) from which the legs of a node
 * no longer clash, per heading class. An axis trunk lays one piece per tile; a diagonal trunk a staircase of half pieces, one per tile.
 */
static constexpr int SPLIT_AXIS = 2; ///< A split's straight and diverging legs, on an axis trunk (either side).
static constexpr int SPLIT_DIAGONAL = 2; ///< The same on a diagonal trunk (the side that exists there).
static constexpr int TRIDENT_AXIS = 2; ///< A trident's three legs (axis trunk only).
static constexpr int CASCADE_AXIS_SAME = 4; ///< Two splits one tile apart, the second on the first's side (axis trunk).
static constexpr int CASCADE_AXIS_OTHER = 4; ///< The second split on the other side (axis trunk).
static constexpr int CASCADE_DIAGONAL = 4; ///< Two splits one tile apart on a diagonal trunk (the side that exists there).

static constexpr int LEG_MOVES = 12; ///< Moves per leg (13 pieces).

/** A laid leg: its pieces as the planar clash rule sees them, and their trackdirs. */
struct Leg {
	std::vector<PlanarPiece> pieces;
	std::vector<Trackdir> trackdirs;
};

static const Trackdir AXIS_STARTS[] = {Trackdir::X_NE, Trackdir::X_SW, Trackdir::Y_SE, Trackdir::Y_NW};
static const Trackdir DIAGONAL_STARTS[] = {Trackdir::Upper_E, Trackdir::Lower_E, Trackdir::Left_S, Trackdir::Right_S, Trackdir::Upper_W, Trackdir::Lower_W, Trackdir::Left_N, Trackdir::Right_N};

/** Lay a route from tile (0, 0); false when the rasteriser reports an error. */
static bool TryLay(Trackdir start, const std::string &moves, Leg &leg)
{
	const JunctionRaster raster = RasterizeJunctionRoute({{0, 0}, start, moves});
	if (!raster.error.empty()) return false;
	leg = {};
	for (const JunctionPiece &jp : raster.pieces) {
		PlanarPiece piece{};
		piece.tile = jp.tile;
		piece.bits = TrackBits{TrackdirToTrack(jp.trackdir)};
		piece.kind = PlanarPiece::Kind::Ground;
		leg.pieces.push_back(piece);
		leg.trackdirs.push_back(jp.trackdir);
	}
	return true;
}

static Leg Lay(Trackdir start, const std::string &moves)
{
	Leg leg;
	INFO("start " << static_cast<int>(start) << " moves " << moves);
	REQUIRE(TryLay(start, moves, leg));
	return leg;
}

static std::string Straight() { return std::string(LEG_MOVES, 'F'); }
static std::string Turn(char side) { return std::string(1, side) + std::string(LEG_MOVES - 1, 'F'); }

/** The leg walked the other way: pieces and trackdirs reversed. */
static Leg Reversed(const Leg &leg)
{
	Leg out;
	out.pieces.assign(leg.pieces.rbegin(), leg.pieces.rend());
	for (auto it = leg.trackdirs.rbegin(); it != leg.trackdirs.rend(); ++it) out.trackdirs.push_back(ReverseTrackdir(*it));
	return out;
}

/**
 * The piece index from which the legs of one node no longer clash.
 * @param legs The legs, with their shared trunk at the start (or at the end when `merge`; indices then count from the end).
 * @param merge Whether the trunk is at the end.
 * @return For each pair of legs, with d the first index their trackdirs differ at: the least s > d such that no pair of pieces of the two legs, both with index >= s, clashes; the largest over the pairs. The two pieces at index d must lie on one tile and form a legal switch (no X|Y diamond); they are exempt, as is the trunk.
 */
static int Separation(const std::vector<Leg> &legs, bool merge)
{
	const auto at = [&](const Leg &leg, size_t i) -> size_t { return merge ? leg.pieces.size() - 1 - i : i; };
	int result = 0;
	for (size_t a = 0; a < legs.size(); a++) {
		for (size_t b = a + 1; b < legs.size(); b++) {
			const Leg &la = legs[a];
			const Leg &lb = legs[b];
			REQUIRE(la.pieces.size() == lb.pieces.size());
			const size_t n = la.pieces.size();
			size_t d = 0;
			while (d < n && la.trackdirs[at(la, d)] == lb.trackdirs[at(lb, d)]) d++;
			REQUIRE(d < n);
			const PlanarPiece &pa = la.pieces[at(la, d)];
			const PlanarPiece &pb = lb.pieces[at(lb, d)];
			INFO("legs " << a << ", " << b << " node index " << d);
			CHECK((pa.tile.x == pb.tile.x && pa.tile.y == pb.tile.y));
			CHECK(((pa.bits | pb.bits) != TRACK_BIT_CROSS));
			int s = static_cast<int>(d) + 1;
			for (size_t i = d + 1; i < n; i++) {
				for (size_t j = d + 1; j < n; j++) {
					if (PlanarPiecesClash(la.pieces[at(la, i)], lb.pieces[at(lb, j)])) s = std::max<int>(s, static_cast<int>(std::min(i, j)) + 1);
				}
			}
			result = std::max(result, s);
		}
	}
	return result;
}

/** One measured row: start trackdir, a label, the separation. */
struct Row {
	Trackdir start;
	std::string what;
	int s;
};

static void Report(const std::string &title, const std::vector<Row> &rows)
{
	std::string text = title;
	for (const Row &row : rows) text += fmt::format("\n  {} {} s={}", static_cast<int>(row.start), row.what, row.s);
	WARN(text);
}

/** Check every row equals the pinned value. */
static void CheckRows(const std::vector<Row> &rows, int expected)
{
	for (const Row &row : rows) {
		INFO("start " << static_cast<int>(row.start) << " " << row.what << " s=" << row.s);
		CHECK(row.s == expected);
	}
}

/** The sides a first move can take from a start trackdir. */
static std::vector<char> Sides(Trackdir start)
{
	std::vector<char> sides;
	Leg leg;
	for (char side : {'L', 'R'}) {
		if (TryLay(start, Turn(side), leg)) sides.push_back(side);
	}
	return sides;
}

/** The legs of a cascade: straight, the first split on `first`, the second on `second` two tiles past the first's divergence tile; false when the second side does not exist there. */
static bool CascadeLegs(Trackdir start, char first, char second, std::vector<Leg> &legs, std::string &second_moves)
{
	const Leg straight = Lay(start, Straight());
	const Leg split = Lay(start, Turn(first));
	size_t d1 = 0;
	while (straight.trackdirs[d1] == split.trackdirs[d1]) d1++;
	const size_t d2 = d1 + 2; /* One plain tile between the two divergence tiles (D110). */
	for (size_t i = d1; i < d2; i++) {
		INFO("start " << static_cast<int>(start));
		REQUIRE((straight.pieces[i].tile.x != straight.pieces[i + 1].tile.x || straight.pieces[i].tile.y != straight.pieces[i + 1].tile.y));
	}
	second_moves = std::string(d2 - 1, 'F') + std::string(1, second) + std::string(LEG_MOVES - d2, 'F');
	Leg other;
	if (!TryLay(start, second_moves, other)) return false;
	legs = {straight, split, other};
	return true;
}

TEST_CASE("planar node rule: a split's legs separate at a fixed point per heading class", "[junction][planar][planarnode]")
{
	std::vector<Row> axis, diagonal;
	for (Trackdir start : AXIS_STARTS) {
		for (char side : Sides(start)) axis.push_back({start, std::string(1, side), Separation({Lay(start, Straight()), Lay(start, Turn(side))}, false)});
	}
	for (Trackdir start : DIAGONAL_STARTS) {
		for (char side : Sides(start)) diagonal.push_back({start, std::string(1, side), Separation({Lay(start, Straight()), Lay(start, Turn(side))}, false)});
	}
	Report("split, axis trunks", axis);
	Report("split, diagonal trunks", diagonal);
	CHECK(axis.size() == 8);
	CHECK(diagonal.size() == 8);
	CheckRows(axis, SPLIT_AXIS);
	CheckRows(diagonal, SPLIT_DIAGONAL);
}

TEST_CASE("planar node rule: a diagonal trunk turns to one side per half piece", "[junction][planar][planarnode]")
{
	for (Trackdir start : DIAGONAL_STARTS) {
		INFO("start " << static_cast<int>(start));
		const std::vector<char> sides = Sides(start);
		REQUIRE(sides.size() == 1);
		/* The next half piece of the staircase takes the other side. */
		const Leg straight = Lay(start, Straight());
		const std::vector<char> next = Sides(straight.trackdirs[1]);
		REQUIRE(next.size() == 1);
		CHECK(next[0] != sides[0]);
	}
}

TEST_CASE("planar node rule: a trident's legs separate", "[junction][planar][planarnode]")
{
	std::vector<Row> axis;
	for (Trackdir start : AXIS_STARTS) {
		axis.push_back({start, "trident", Separation({Lay(start, Straight()), Lay(start, Turn('L')), Lay(start, Turn('R'))}, false)});
	}
	Report("trident, axis trunks", axis);
	CheckRows(axis, TRIDENT_AXIS);
}

TEST_CASE("planar node rule: two splits one tile apart", "[junction][planar][planarnode]")
{
	std::vector<Row> same, other, diagonal;
	for (Trackdir start : AXIS_STARTS) {
		for (char first : {'L', 'R'}) {
			for (bool same_side : {true, false}) {
				const char second = same_side ? first : (first == 'L' ? 'R' : 'L');
				std::vector<Leg> legs;
				std::string moves;
				INFO("start " << static_cast<int>(start));
				REQUIRE(CascadeLegs(start, first, second, legs, moves));
				(same_side ? same : other).push_back({start, std::string(1, first) + " then " + moves, Separation(legs, false)});
			}
		}
	}
	for (Trackdir start : DIAGONAL_STARTS) {
		for (char first : Sides(start)) {
			int found = 0;
			for (char second : {'L', 'R'}) {
				std::vector<Leg> legs;
				std::string moves;
				if (!CascadeLegs(start, first, second, legs, moves)) continue;
				found++;
				diagonal.push_back({start, std::string(1, first) + " then " + moves, Separation(legs, false)});
			}
			CHECK(found == 1);
		}
	}
	Report("cascade, axis, same side", same);
	Report("cascade, axis, other side", other);
	Report("cascade, diagonal", diagonal);
	CheckRows(same, CASCADE_AXIS_SAME);
	CheckRows(other, CASCADE_AXIS_OTHER);
	CheckRows(diagonal, CASCADE_DIAGONAL);
}

TEST_CASE("planar node rule: merges mirror splits", "[junction][planar][planarnode]")
{
	const auto mirrored = [](const std::vector<Leg> &legs) {
		std::vector<Leg> out;
		for (const Leg &leg : legs) out.push_back(Reversed(leg));
		return out;
	};
	for (Trackdir start : AXIS_STARTS) {
		for (char side : Sides(start)) {
			const std::vector<Leg> legs{Lay(start, Straight()), Lay(start, Turn(side))};
			CHECK(Separation(mirrored(legs), true) == Separation(legs, false));
		}
		const std::vector<Leg> trident{Lay(start, Straight()), Lay(start, Turn('L')), Lay(start, Turn('R'))};
		CHECK(Separation(mirrored(trident), true) == Separation(trident, false));
		for (char first : {'L', 'R'}) {
			for (char second : {'L', 'R'}) {
				std::vector<Leg> legs;
				std::string moves;
				REQUIRE(CascadeLegs(start, first, second, legs, moves));
				CHECK(Separation(mirrored(legs), true) == Separation(legs, false));
			}
		}
	}
	for (Trackdir start : DIAGONAL_STARTS) {
		for (char side : Sides(start)) {
			const std::vector<Leg> legs{Lay(start, Straight()), Lay(start, Turn(side))};
			CHECK(Separation(mirrored(legs), true) == Separation(legs, false));
		}
	}
}
