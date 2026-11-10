/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_line.cpp Multi-track line geometry for the line painter (D112). */

/*
 * Construction (RECON-linepainter option B, corner templates).
 *
 * Model. Work in the (x, y) tile plane with every coordinate doubled, so tile (x, y) spans (2x..2x+2, 2y..2y+2) and
 * edge midpoints are integer points. A track piece joins two edge midpoints: an axis piece moves 2 along an axis, a
 * diagonal piece moves 1 along both axes. A track is therefore a polyline of 8 possible headings with vertices at edge
 * midpoints. A line is described by the exit point of the leftmost track (track 0) and a heading; the other tracks are
 * track 0 shifted across the heading, so all of them have an equal progress along the heading. Tracks are in a left and a
 * right direction group (D142): neighbours in a group are adjacent (gap 0) and only the two groups are `gap` apart.
 *
 * Spacing. The right vector is perpendicular to the heading (rotating (hx, hy) to (hy, -hx)):
 *  - Axis heading: R = 2 (gap + 1) (hy, -hx). Neighbouring tracks are gap + 1 tiles apart (gap 0 is adjacent tiles).
 *  - Diagonal heading: R = S (gap) (hy, -hx), where S is the number of diagonals (lines x + y = k or x - y = k)
 *    between neighbours: S = round((gap + 1) * sqrt 2) = 1, 3, 4, 6 for gap 0, 1, 2, 3.
 *    S = 1 puts the two tracks in the same tiles as Upper/Lower or Left/Right pairs, the doubled track of OpenTTD.
 * An axis spacing of d tiles and a diagonal spacing of S diagonals cannot be exactly equal in perpendicular distance
 * (it would need S = d sqrt 2, which is never an integer), so the diagonal spacing is the nearest whole number of
 * diagonals; it is the same for every turn, so L then R restores the axis spacing exactly.
 *
 * Turn. A whole-line turn gives track i the move string F^n_i, then L (or R), then F^(c_i - 1): n_i pieces in the old
 * heading, then c_i pieces in the new one. The tracks end with equal progress along the new heading at distance
 * the right spacing apart exactly when, for every i, with u(h) the step of one piece (2h axis, h diagonal):
 *
 *   O_i(new) - O_i(old) = dn_i u(old) + dc_i u(new)
 *
 * where O_i(h) is the offset of track i from track 0 for heading h (see Spacing). This is a 2x2 integer system, solved per track. The shortest solution shifts these so min n = base and min c = 1.
 * The base is 0, or more (at most 2) when the piece the tracks stand on cannot take the turn: a diagonal piece can turn
 * towards an axis on one side only (Left_N can turn left to NW, Right_N only right to NE), which the router finds by
 * trying. For an axis heading turning 45 degrees, tracks with i on the outer side turn later, by (S - d) pieces, and
 * run (2d - S) pieces more diagonally; table for the leftmost track as the inner track of a left turn (tracks i = 0, 1),
 * moves of the whole line at one turn from an axis heading:
 *
 *   gap 0 (d 1, S 1):  track 0 "L"   track 1 "LF"
 *   gap 1 (d 2, S 3):  track 0 "L"   track 1 "FLF"
 *   gap 2 (d 3, S 4):  track 0 "L"   track 1 "FLFF"
 *
 * Each track's offset from track 0 is the sum of the spacings of its neighbours (R of the gap between the groups, R of gap 0 within a group), so the difference of a track is the sum of the neighbours' differences. Turning back to an
 * axis heading (diagonal to axis) uses the same equation with the roles swapped. The equation always has an integer
 * solution for gaps 0 to 3 in all 8 headings and both hands (checked by the tests); the code reports an error if it
 * ever does not.
 */

#include "stdafx.h"

#include "junction_line.h"
#include "junction_signal_pass.h"
#include "direction_func.h"
#include "track_func.h"

#include <algorithm>

#include "safeguards.h"

/** A 2D integer vector or point; points are in doubled tile coordinates. */
struct LineVec {
	int x;
	int y;
};

/** The unit vector of a direction in the (x, y) tile plane. */
static LineVec HeadingVector(Direction dir)
{
	switch (dir) {
		case Direction::N: return {-1, -1};
		case Direction::NE: return {-1, 0};
		case Direction::E: return {-1, 1};
		case Direction::SE: return {0, 1};
		case Direction::S: return {1, 1};
		case Direction::SW: return {1, 0};
		case Direction::W: return {1, -1};
		case Direction::NW: return {0, -1};
		default: NOT_REACHED();
	}
}

static bool IsDiagonalHeading(Direction dir)
{
	return dir == Direction::N || dir == Direction::E || dir == Direction::S || dir == Direction::W;
}

/** The number of diagonals between neighbouring tracks of a diagonal line: round((gap + 1) * sqrt 2). */
static int DiagonalSpacing(uint8_t gap)
{
	return ((gap + 1) * 1414 + 500) / 1000;
}

/** The vector from one track to its right-hand neighbour, in doubled coordinates. */
static LineVec RightVector(uint8_t gap, Direction heading)
{
	const LineVec h = HeadingVector(heading);
	const int scale = IsDiagonalHeading(heading) ? DiagonalSpacing(gap) : 2 * (gap + 1);
	return {scale * h.y, -scale * h.x};
}

/** The offset of track i from track 0: the sum of the spacings of its neighbours, the gap only between the two direction groups (D142). */
static LineVec TrackOffset(const JunctionLineSpec &spec, int i, Direction heading)
{
	LineVec offset{0, 0};
	for (int j = 0; j < i; j++) {
		const LineVec step = RightVector(j == spec.left_tracks - 1 ? spec.gap : 0, heading);
		offset.x += step.x;
		offset.y += step.y;
	}
	return offset;
}

/** The movement of the exit point for one piece in a heading, in doubled coordinates. */
static LineVec PieceStep(Direction heading)
{
	const LineVec h = HeadingVector(heading);
	const int scale = IsDiagonalHeading(heading) ? 1 : 2;
	return {scale * h.x, scale * h.y};
}

/** The midpoint of an edge of a tile, in doubled coordinates. */
static LineVec EdgePoint(TileIndexDiffC tile, DiagDirection edge)
{
	switch (edge) {
		case DiagDirection::NE: return {2 * tile.x, 2 * tile.y + 1};
		case DiagDirection::SE: return {2 * tile.x + 1, 2 * tile.y + 2};
		case DiagDirection::SW: return {2 * tile.x + 2, 2 * tile.y + 1};
		case DiagDirection::NW: return {2 * tile.x + 1, 2 * tile.y};
		default: NOT_REACHED();
	}
}

/**
 * Find the per-track move strings of one whole-line turn.
 * @param spec The line.
 * @param heading The current heading of the line.
 * @param left Whether to turn left, otherwise right.
 * @param base The number of pieces the tracks with the least straight run go straight before turning.
 * @param[out] strings The move string of each track.
 * @return Whether the equation has an integer solution.
 */
static bool TurnStrings(const JunctionLineSpec &spec, Direction heading, bool left, int base, std::vector<std::string> &strings)
{
	const Direction next = ChangeDir(heading, left ? DirDiff::Left45 : DirDiff::Right45);
	const LineVec u = PieceStep(heading);
	const LineVec w = PieceStep(next);
	const int det = u.x * w.y - u.y * w.x;

	std::vector<int> dn;
	std::vector<int> dc;
	for (int i = 0; i < spec.tracks; i++) {
		const LineVec off_old = TrackOffset(spec, i, heading);
		const LineVec off_new = TrackOffset(spec, i, next);
		const int vx = off_new.x - off_old.x;
		const int vy = off_new.y - off_old.y;
		const int num_n = vx * w.y - vy * w.x;
		const int num_c = u.x * vy - u.y * vx;
		if (num_n % det != 0 || num_c % det != 0) return false;
		dn.push_back(num_n / det);
		dc.push_back(num_c / det);
	}
	const int min_n = *std::ranges::min_element(dn);
	const int min_c = *std::ranges::min_element(dc);

	strings.clear();
	for (int i = 0; i < spec.tracks; i++) {
		const int straight = dn[i] - min_n + base;
		const int after = dc[i] - min_c;
		strings.push_back(std::string(straight, 'F') + (left ? 'L' : 'R') + std::string(after, 'F'));
	}
	return true;
}

/**
 * Find the piece of a neighbouring track, which is the given piece moved by a vector.
 * @param piece The piece to move.
 * @param shift The vector, in doubled coordinates.
 * @param[out] result The moved piece.
 * @return Whether such a piece exists.
 */
static bool ShiftPiece(const JunctionPiece &piece, LineVec shift, JunctionPiece &result)
{
	const Direction heading = TrackdirToDirection(piece.trackdir);
	const LineVec exit = EdgePoint(piece.tile, TrackdirToExitdir(piece.trackdir));
	const LineVec entry = EdgePoint(piece.tile, TrackdirToExitdir(ReverseTrackdir(piece.trackdir)));
	const int span = std::max(std::abs(shift.x), std::abs(shift.y)) / 2 + 2;
	for (int dy = -span; dy <= span; dy++) {
		for (int dx = -span; dx <= span; dx++) {
			const TileIndexDiffC tile{static_cast<int16_t>(piece.tile.x + dx), static_cast<int16_t>(piece.tile.y + dy)};
			for (Trackdir td : {Trackdir::X_NE, Trackdir::X_SW, Trackdir::Y_NW, Trackdir::Y_SE, Trackdir::Upper_E, Trackdir::Lower_E, Trackdir::Upper_W, Trackdir::Lower_W,
					Trackdir::Left_N, Trackdir::Right_N, Trackdir::Left_S, Trackdir::Right_S}) {
				if (TrackdirToDirection(td) != heading) continue;
				const LineVec e = EdgePoint(tile, TrackdirToExitdir(td));
				const LineVec n = EdgePoint(tile, TrackdirToExitdir(ReverseTrackdir(td)));
				if (e.x != exit.x + shift.x || e.y != exit.y + shift.y || n.x != entry.x + shift.x || n.y != entry.y + shift.y) continue;
				result = {tile, td, static_cast<uint8_t>(IsDiagonalTrackdir(td) ? 2 : 1), false};
				return true;
			}
		}
	}
	return false;
}

std::vector<std::vector<JunctionPiece>> ExtendJunctionLine(const JunctionLineSpec &line, const std::vector<JunctionPiece> &last, char move, std::string &error)
{
	if (move != 'F' && move != 'L' && move != 'R') {
		error = " is not supported in a line";
		return {};
	}

	/* Try the shortest straight run first; a track may need one more piece to be on a piece that can turn. */
	const Direction heading = TrackdirToDirection(last[0].trackdir);
	std::vector<std::vector<JunctionPiece>> extension;
	std::string last_error;
	for (int base = 0; base < 3; base++) {
		std::vector<std::string> strings;
		if (move == 'F') {
			strings.assign(line.tracks, "F");
		} else if (!TurnStrings(line, heading, move == 'L', base, strings)) {
			error = " has no equal-spacing turn";
			return {};
		}

		extension.clear();
		last_error.clear();
		for (int i = 0; i < line.tracks && last_error.empty(); i++) {
			JunctionRaster sub = RasterizeJunctionRoute({last[i].tile, last[i].trackdir, strings[i]});
			if (!sub.error.empty()) {
				last_error = fmt::format("track {}: {}", i, sub.error);
			} else {
				extension.emplace_back(sub.pieces.begin() + 1, sub.pieces.end());
			}
		}
		if (last_error.empty()) break;
		if (move == 'F') break;
	}
	if (!last_error.empty()) {
		error = ": " + last_error;
		return {};
	}
	return extension;
}

std::vector<std::vector<JunctionPiece>> ExtendJunctionLineBridge(const JunctionLineSpec &line, const std::vector<JunctionPiece> &last, uint middle, std::vector<JunctionBridge> &bridges, std::string &error, bool tunnel)
{
	for (const JunctionPiece &piece : last) {
		if (!IsDiagonalTrackdir(piece.trackdir)) {
			error = " must start from axis pieces";
			return {};
		}
	}

	std::vector<std::vector<JunctionPiece>> extension;
	bridges.clear();
	for (int i = 0; i < line.tracks; i++) {
		JunctionRaster sub = RasterizeJunctionRoute({last[i].tile, last[i].trackdir, fmt::format("{}{}", tunnel ? 'T' : 'B', middle)});
		if (!sub.error.empty()) {
			error = fmt::format(": track {}: {}", i, sub.error);
			return {};
		}
		extension.emplace_back(sub.pieces.begin() + 1, sub.pieces.end());
		bridges.push_back(sub.bridges[0]);
	}
	return extension;
}

std::vector<JunctionRaster> RasterizeJunctionLine(const JunctionLineSpec &line, const JunctionLineRoute &route)
{
	if (line.tracks == 0) return std::vector<JunctionRaster>(1, JunctionRaster{{}, {}, "a line needs at least one track"});

	std::vector<JunctionRaster> rasters(line.tracks);
	auto fail = [&](const std::string &message) {
		rasters[0].error = message;
		return rasters;
	};

	/* The other tracks start where the left one is, moved to the right. */
	const JunctionPiece first{route.start, route.start_trackdir, static_cast<uint8_t>(IsDiagonalTrackdir(route.start_trackdir) ? 2 : 1), false};
	for (int i = 0; i < line.tracks; i++) {
		JunctionPiece piece;
		if (!ShiftPiece(first, TrackOffset(line, i, TrackdirToDirection(route.start_trackdir)), piece)) return fail(fmt::format("track {} has no start piece", i));
		rasters[i].pieces.push_back(piece);
	}

	for (size_t pos = 0; pos < route.moves.size(); pos++) {
		std::vector<JunctionPiece> last;
		for (const JunctionRaster &raster : rasters) last.push_back(raster.pieces.back());
		std::string error;
		if (route.moves[pos] == 'B' || route.moves[pos] == 'T') {
			const bool tunnel = route.moves[pos] == 'T';
			const char *what = tunnel ? "tunnel" : "bridge";
			const size_t move_pos = pos;
			size_t digits = 0;
			uint middle = 0;
			while (pos + 1 < route.moves.size() && route.moves[pos + 1] >= '0' && route.moves[pos + 1] <= '9') {
				middle = middle * 10 + (route.moves[pos + 1] - '0');
				if (middle > 1000) return fail(fmt::format("{} length at position {} is too long", what, move_pos));
				digits++;
				pos++;
			}
			if (digits == 0) return fail(fmt::format("{} at position {} has no length", what, move_pos));
			std::vector<JunctionBridge> bridges;
			const std::vector<std::vector<JunctionPiece>> extension = ExtendJunctionLineBridge(line, last, middle, bridges, error, tunnel);
			if (extension.empty()) return fail(fmt::format("move '{}' at position {}{}", route.moves[move_pos], move_pos, error));
			for (int i = 0; i < line.tracks; i++) {
				rasters[i].pieces.insert(rasters[i].pieces.end(), extension[i].begin(), extension[i].end());
				rasters[i].bridges.push_back(bridges[i]);
			}
			continue;
		}
		const std::vector<std::vector<JunctionPiece>> extension = ExtendJunctionLine(line, last, route.moves[pos], error);
		if (extension.empty()) return fail(fmt::format("move '{}' at position {}{}", route.moves[pos], pos, error));
		for (int i = 0; i < line.tracks; i++) rasters[i].pieces.insert(rasters[i].pieces.end(), extension[i].begin(), extension[i].end());
	}
	return rasters;
}

JunctionLineNotation ParseLineNotation(std::string_view text)
{
	size_t lefts = 0;
	while (lefts < text.size() && text[lefts] == 'L') lefts++;
	size_t gap = 0;
	while (lefts + gap < text.size() && text[lefts + gap] == '_') gap++;
	size_t rights = 0;
	while (lefts + gap + rights < text.size() && text[lefts + gap + rights] == 'R') rights++;

	if (lefts == 0 || rights == 0 || gap > 3 || lefts + gap + rights != text.size() || lefts + rights > UINT8_MAX) {
		return {{}, JunctionNotationError::Invalid};
	}
	JunctionLineNotation result;
	result.spec.tracks = static_cast<uint8_t>(lefts + rights);
	result.spec.gap = static_cast<uint8_t>(gap);
	result.spec.left_tracks = static_cast<uint8_t>(lefts);
	return result;
}

JunctionLineBuild BuildJunctionLine(const JunctionLineSpec &line, const JunctionLineRoute &route, const std::vector<bool> &along_route, uint8_t sd, SignalType signal_type, const JunctionPricing &pricing, TileIndex origin)
{
	JunctionLineBuild result;
	if (along_route.size() != line.tracks) {
		result.error = "the travel directions do not match the number of tracks";
		return result;
	}
	const std::vector<JunctionRaster> rasters = RasterizeJunctionLine(line, route);
	if (!rasters.empty() && !rasters[0].error.empty()) {
		result.error = rasters[0].error;
		return result;
	}
	for (const JunctionRaster &raster : rasters) AddRasterToLayout(result.layout, raster);
	for (size_t i = 0; i < rasters.size(); i++) {
		std::vector<JunctionPiece> pieces = rasters[i].pieces;
		if (!along_route[i]) {
			std::reverse(pieces.begin(), pieces.end());
			for (JunctionPiece &piece : pieces) piece.trackdir = ReverseTrackdir(piece.trackdir);
		}
		PlaceDragSignals(result.layout, pieces, sd, signal_type);
	}
	result.estimate = EstimateJunction(result.layout, origin, pricing);
	return result;
}
