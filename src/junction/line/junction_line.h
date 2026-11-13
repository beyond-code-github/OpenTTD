/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_line.h Multi-track line geometry for the line painter (D112). */

#ifndef JUNCTION_LINE_H
#define JUNCTION_LINE_H

#include "../junction_spec.h"
#include "../junction_route.h"

#include <string>
#include <string_view>
#include <vector>

/**
 * A multi-track line: N parallel tracks in a left and a right direction group (D142).
 * Tracks of one group are adjacent; `gap` tiles lie only between the two groups (0 means adjacent).
 */
struct JunctionLineSpec {
	uint8_t tracks = 2;
	uint8_t gap = 1;
	uint8_t left_tracks = 1; ///< Tracks in the left group (D142); the gap is only between track left_tracks - 1 and left_tracks, other neighbours are adjacent.
};

/** The result of parsing a line notation. */
struct JunctionLineNotation {
	JunctionLineSpec spec;
	JunctionNotationError error = JunctionNotationError::None;
};

/**
 * Parse a line notation with direction groups (D142), such as "LL_RR": one or more L, zero to three underscores, one or more R.
 * @param text The notation.
 * @return The line (tracks = L + R, left_tracks = L, gap = underscores), or Invalid.
 */
JunctionLineNotation ParseLineNotation(std::string_view text);

/**
 * A line program: a start (the first piece of the leftmost track, track 0) and moves, as JunctionRoute, applied to the whole line.
 * The line extends to the right of the start heading, so the other tracks are to the right of track 0.
 */
struct JunctionLineRoute {
	TileIndexDiffC start;
	Trackdir start_trackdir;
	std::string moves; ///< F, L, R, and B<n> (a bridge with n middle tiles, D149), T<n> (a tunnel, D155).
};

/** Rasterize a line program into one raster per track, outermost-left first; error text in the first raster when the line can't make a move. */
std::vector<JunctionRaster> RasterizeJunctionLine(const JunctionLineSpec &line, const JunctionLineRoute &route);

/**
 * Extend a line by one move: from each track's last piece, the pieces each track adds (D112, LP2 geometry).
 * @return The added pieces per track (without the last pieces), or an error text (empty vector) when the move can't be made.
 */
std::vector<std::vector<JunctionPiece>> ExtendJunctionLine(const JunctionLineSpec &line, const std::vector<JunctionPiece> &last, char move, std::string &error);

/**
 * Extend a line by a bridge move B<middle> (D149): every track bridges `middle` tiles straight on from its last piece, which must be an axis piece.
 * D110: a single-track line may also bridge from a half piece, closing its S-bend with the near head; the rasterizer decides whether that is legal. Tracks of a multi-track line bridge together from axis pieces (D149).
 * @param line The line.
 * @param last Each track's last piece.
 * @param middle The bridge's middle-tile count.
 * @param[out] bridges One bridge per track, in track order.
 * @param[out] error The reason, when the move can't be made.
 * @param tunnel Whether the move is a tunnel T<middle> (D155): the same pieces, with tunnel bridges (z left 0).
 * @return The added pieces per track (on_bridge, near head to far head), or an empty vector when the move can't be made.
 */
std::vector<std::vector<JunctionPiece>> ExtendJunctionLineBridge(const JunctionLineSpec &line, const std::vector<JunctionPiece> &last, uint middle, std::vector<JunctionBridge> &bridges, std::string &error, bool tunnel = false);

/** A painted line ready to build (LP10): its layout with signals, and its cost. */
struct JunctionLineBuild {
	JunctionLayout layout;
	JunctionEstimate estimate;
	std::string error;
};

/**
 * Lay a line program as a layout, signal each track as drag signalling would (D97) in its direction of travel, and price it.
 * @param line The line.
 * @param route The line program.
 * @param along_route One entry per track: whether the track travels in the route's heading (the caller's choice).
 * @param sd Signal distance (drag density, tiles).
 * @param signal_type The signal type.
 * @param pricing The game's prices.
 * @param origin The map tile of the layout's north corner.
 * @return The layout, its estimate, or an error (empty layout) when the line can't be laid or along_route has the wrong size.
 */
JunctionLineBuild BuildJunctionLine(const JunctionLineSpec &line, const JunctionLineRoute &route, const std::vector<bool> &along_route, uint8_t sd, SignalType signal_type, const JunctionPricing &pricing, TileIndex origin);

#endif /* JUNCTION_LINE_H */
