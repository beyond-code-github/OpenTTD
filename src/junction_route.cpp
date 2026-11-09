/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_route.cpp Junction route programs, rasterizing and ASCII rendering. */

#include "stdafx.h"

#include "junction_route.h"
#include "direction_func.h"
#include "map_func.h"
#include "track_func.h"

#include <algorithm>

#include "safeguards.h"

/**
 * Get the direction a trackdir heads in, as used by the route programs.
 * @param td The trackdir; must be one of the 12 valid ones.
 * @return The direction.
 */
Direction TrackdirToDirection(Trackdir td)
{
	switch (td) {
		case Trackdir::X_NE: return Direction::NE;
		case Trackdir::X_SW: return Direction::SW;
		case Trackdir::Y_NW: return Direction::NW;
		case Trackdir::Y_SE: return Direction::SE;
		case Trackdir::Upper_E:
		case Trackdir::Lower_E: return Direction::E;
		case Trackdir::Upper_W:
		case Trackdir::Lower_W: return Direction::W;
		case Trackdir::Left_N:
		case Trackdir::Right_N: return Direction::N;
		case Trackdir::Left_S:
		case Trackdir::Right_S: return Direction::S;
		default: NOT_REACHED();
	}
}

bool JunctionNextTrackdir(Trackdir current, DirDiff diff, Trackdir &next)
{
	const Direction wanted = ChangeDir(TrackdirToDirection(current), diff);
	uint matches = 0;
	for (Trackdir td : TrackdirReachesTrackdirs(current)) {
		if (TrackdirToDirection(td) != wanted) continue;
		next = td;
		matches++;
	}
	return matches == 1;
}

/**
 * Append the piece reached by leaving the last piece of a raster.
 * @param raster The raster to extend; must not be empty.
 * @param trackdir The trackdir of the new piece.
 * @param on_bridge Whether the piece is a bridge tile.
 */
static void AppendPiece(JunctionRaster &raster, Trackdir trackdir, bool on_bridge)
{
	const JunctionPiece &cur = raster.pieces.back();
	const TileIndexDiffC step = TileIndexDiffCByDiagDir(TrackdirToExitdir(cur.trackdir));
	JunctionPiece piece;
	piece.tile = {static_cast<int16_t>(cur.tile.x + step.x), static_cast<int16_t>(cur.tile.y + step.y)};
	piece.trackdir = trackdir;
	piece.units = on_bridge || IsDiagonalTrackdir(trackdir) ? 2 : 1;
	piece.on_bridge = on_bridge;
	raster.pieces.push_back(piece);
}

/**
 * Turn a route program into track pieces and bridges.
 * @param route The route program.
 * @return The pieces and bridges, or an error message.
 */
JunctionRaster RasterizeJunctionRoute(const JunctionRoute &route)
{
	JunctionRaster raster;
	raster.pieces.push_back({route.start, route.start_trackdir, static_cast<uint8_t>(IsDiagonalTrackdir(route.start_trackdir) ? 2 : 1), false});

	for (size_t i = 0; i < route.moves.size(); i++) {
		const char move = route.moves[i];
		const Trackdir cur = raster.pieces.back().trackdir;
		Trackdir next;
		switch (move) {
			case 'F':
			case 'L':
			case 'R': {
				const DirDiff diff = move == 'F' ? DirDiff::Same : move == 'L' ? DirDiff::Left45 : DirDiff::Right45;
				if (!JunctionNextTrackdir(cur, diff, next)) {
					raster.error = fmt::format("move '{}' at position {} has no unique continuation", move, i);
					return raster;
				}
				AppendPiece(raster, next, false);
				break;
			}

			case 'B':
			case 'T': {
				/* D155: a tunnel starts from an axis piece only. D110: from a half piece the bridge completes the S-bend; its near head is where the closing axis piece would be. */
				const bool tunnel = move == 'T';
				const char *what = tunnel ? "tunnel" : "bridge";
				Trackdir head = cur;
				if (tunnel && !IsDiagonalTrackdir(cur)) {
					raster.error = fmt::format("tunnel at position {} must start from an axis piece", i);
					return raster;
				}
				if (!IsDiagonalTrackdir(cur)) {
					char last_turn = 0;
					for (size_t j = i; j-- > 0 && last_turn == 0;) {
						if (route.moves[j] == 'L' || route.moves[j] == 'R') last_turn = route.moves[j];
					}
					if (last_turn == 0 || !JunctionNextTrackdir(cur, last_turn == 'R' ? DirDiff::Left45 : DirDiff::Right45, head) || !IsDiagonalTrackdir(head)) {
						raster.error = fmt::format("{} at position {} must start from an axis piece", what, i);
						return raster;
					}
				}
				size_t digits = 0;
				uint middle = 0;
				while (i + 1 < route.moves.size() && route.moves[i + 1] >= '0' && route.moves[i + 1] <= '9') {
					middle = middle * 10 + (route.moves[i + 1] - '0');
					if (middle > 1000) {
						raster.error = fmt::format("{} length at position {} is too long", what, i);
						return raster;
					}
					digits++;
					i++;
				}
				if (digits == 0) {
					raster.error = fmt::format("{} at position {} has no length", what, i);
					return raster;
				}
				AppendPiece(raster, head, true);
				const TileIndexDiffC near_head = raster.pieces.back().tile;
				for (uint k = 0; k < middle + 1; k++) AppendPiece(raster, head, true);
				const TileIndexDiffC far_head = raster.pieces.back().tile;
				JunctionBridge bridge{near_head, far_head, tunnel};
				if (bridge.start.x + bridge.start.y > bridge.end.x + bridge.end.y) std::swap(bridge.start, bridge.end);
				raster.bridges.push_back(bridge);
				break;
			}

			default:
				raster.error = fmt::format("unknown move '{}' at position {}", move, i);
				return raster;
		}
	}
	return raster;
}

/**
 * Merge a raster into a layout: track bits are combined per tile and bridges added once.
 * @param layout The layout to extend.
 * @param raster The raster to add.
 */
void AddRasterToLayout(JunctionLayout &layout, const JunctionRaster &raster)
{
	for (const JunctionPiece &piece : raster.pieces) {
		if (piece.on_bridge) continue;
		auto it = std::ranges::find_if(layout.tracks, [&](const JunctionTrack &jt) { return jt.offset.x == piece.tile.x && jt.offset.y == piece.tile.y; });
		if (it == layout.tracks.end()) {
			layout.tracks.push_back({piece.tile, TrackBits{}});
			it = layout.tracks.end() - 1;
		}
		it->tracks.Set(TrackdirToTrack(piece.trackdir));
	}
	for (const JunctionBridge &bridge : raster.bridges) {
		const bool present = std::ranges::any_of(layout.bridges, [&](const JunctionBridge &b) {
			return b.start.x == bridge.start.x && b.start.y == bridge.start.y && b.end.x == bridge.end.x && b.end.y == bridge.end.y;
		});
		if (!present) layout.bridges.push_back(bridge);
	}
}

/**
 * Shift a layout so its minimum x and y are 0 and set its size to the bounding box.
 * @param layout The layout to normalise.
 */
void NormaliseJunctionLayout(JunctionLayout &layout)
{
	int min_x = INT16_MAX;
	int min_y = INT16_MAX;
	int max_x = INT16_MIN;
	int max_y = INT16_MIN;
	auto include = [&](TileIndexDiffC offset) {
		min_x = std::min<int>(min_x, offset.x);
		min_y = std::min<int>(min_y, offset.y);
		max_x = std::max<int>(max_x, offset.x);
		max_y = std::max<int>(max_y, offset.y);
	};
	for (const JunctionTrack &jt : layout.tracks) include(jt.offset);
	for (const JunctionBridge &bridge : layout.bridges) {
		include(bridge.start);
		include(bridge.end);
	}
	if (max_x < min_x) return;

	auto shift = [&](TileIndexDiffC &offset) {
		offset.x = static_cast<int16_t>(offset.x - min_x);
		offset.y = static_cast<int16_t>(offset.y - min_y);
	};
	for (JunctionTrack &jt : layout.tracks) shift(jt.offset);
	for (JunctionSignal &signal : layout.signals) shift(signal.offset);
	for (JunctionBridge &bridge : layout.bridges) {
		shift(bridge.start);
		shift(bridge.end);
	}
	for (JunctionStationBlock &station : layout.stations) shift(station.origin);
	layout.size = {static_cast<int16_t>(max_x - min_x + 1), static_cast<int16_t>(max_y - min_y + 1)};
}

/**
 * Render a layout as text, two characters per tile. This is for debugging and tests.
 * The grid is x to the right and y downwards; it is not the isometric screen view.
 * @param layout The layout to render.
 * @return One line per row of tiles: a track or bridge glyph and a signal glyph per tile.
 */
std::string RenderJunctionAscii(const JunctionLayout &layout)
{
	std::string out;
	for (int y = 0; y < layout.size.y; y++) {
		for (int x = 0; x < layout.size.x; x++) {
			char glyph = '.';
			bool bridge_head = false;
			bool tunnel_portal = false;
			bool bridge_span = false;
			for (const JunctionBridge &bridge : layout.bridges) {
				const bool is_start = bridge.start.x == x && bridge.start.y == y;
				const bool is_end = bridge.end.x == x && bridge.end.y == y;
				if (bridge.tunnel) {
					if (is_start || is_end) tunnel_portal = true;
					continue;
				}
				if (is_start || is_end) bridge_head = true;
				if (x >= std::min(bridge.start.x, bridge.end.x) && x <= std::max(bridge.start.x, bridge.end.x) &&
						y >= std::min(bridge.start.y, bridge.end.y) && y <= std::max(bridge.start.y, bridge.end.y)) {
					bridge_span = true;
				}
			}

			TrackBits tracks{};
			for (const JunctionTrack &jt : layout.tracks) {
				if (jt.offset.x == x && jt.offset.y == y) tracks = tracks | jt.tracks;
			}

			if (tunnel_portal) {
				glyph = 'O';
			} else if (bridge_head) {
				glyph = 'H';
			} else if (bridge_span) {
				glyph = '=';
			} else if (tracks.None()) {
				glyph = '.';
			} else if (tracks == TRACK_BIT_CROSS) {
				glyph = '+';
			} else if (tracks == TrackBits{Track::X}) {
				glyph = '-';
			} else if (tracks == TrackBits{Track::Y}) {
				glyph = '|';
			} else if (TRACK_BIT_HORZ.All(tracks)) {
				glyph = '/';
			} else if (TRACK_BIT_VERT.All(tracks)) {
				glyph = '\\';
			} else {
				glyph = '#';
			}
			out += glyph;

			char signal_glyph = ' ';
			uint signals = 0;
			for (const JunctionSignal &signal : layout.signals) {
				if (signal.offset.x != x || signal.offset.y != y) continue;
				signals++;
				switch (TrackdirToDirection(signal.facing)) {
					case Direction::NE: signal_glyph = '<'; break;
					case Direction::SW: signal_glyph = '>'; break;
					case Direction::NW: signal_glyph = '^'; break;
					case Direction::SE: signal_glyph = 'v'; break;
					case Direction::N: signal_glyph = 'n'; break;
					case Direction::S: signal_glyph = 's'; break;
					case Direction::E: signal_glyph = 'e'; break;
					case Direction::W: signal_glyph = 'w'; break;
					default: NOT_REACHED();
				}
			}
			out += signals > 1 ? '*' : signal_glyph;
		}
		out += '\n';
	}
	return out;
}
