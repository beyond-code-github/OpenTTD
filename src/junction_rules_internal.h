/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_rules_internal.h Junction rule checker helpers shared with the signal pass. */

#ifndef JUNCTION_RULES_INTERNAL_H
#define JUNCTION_RULES_INTERNAL_H

#include "junction_rules.h"

#include <map>
#include <optional>
#include <utility>
#include <vector>

/** The logical length drag-signalling gives a piece: 192 for an axis piece or a bridge piece, 128 for a diagonal half piece (D97). */
inline uint JunctionSignalWeight(const JunctionPiece &piece)
{
	return piece.units == 2 ? 192 : 128;
}

/** A switch on a path: whether it is a split, and the index of the switch tile's piece. */
using SwitchEvent = std::pair<bool, size_t>;

/** The six movements of a T junction. */
const std::vector<std::pair<JunctionPortRole, JunctionPortRole>> &JunctionTMovements();

/** The track bits of all layout entries on a tile. */
TrackBits TracksAt(const JunctionLayout &layout, TileIndexDiffC tile);

/** The bridge that has the tile as one of its heads, or nullptr. */
const JunctionBridge *FindBridgeByHead(const JunctionLayout &layout, TileIndexDiffC tile);

/** Whether the path is a through movement: both groups of its movement are main line. */
bool IsThroughPath(const JunctionCheckContext &ctx, const JunctionPath &path);

/** Whether the path is an exit: from a main line group to a group that is not main line. */
bool IsExitPath(const JunctionCheckContext &ctx, const JunctionPath &path);

/** The tracks used by the paths, per tile. */
std::map<std::pair<int, int>, TrackBits> UsedTracks(const JunctionCheckContext &ctx);

/**
 * The switches along a path, ordered by the index of their switch tile's piece.
 * A split is at the piece where more than one continuation exists; a merge is at the piece
 * where another track on the same tile leaves in the same direction and reaches the next piece.
 * Only tracks used by some path make merges; unused tracks, such as prio joins, never do.
 * Bridge pieces are never switches.
 */
std::vector<SwitchEvent> SwitchEventsOf(const JunctionLayout &layout, const std::map<std::pair<int, int>, TrackBits> &used, const JunctionPath &path);

/** Whether the piece is on a tile that has a signal facing along the piece's trackdir. */
bool IsSignalledPiece(const JunctionLayout &layout, const JunctionPiece &piece);

/** Whether a signal could be placed on the piece. */
bool IsLegalSignalPiece(const JunctionLayout &layout, const JunctionPiece &piece);

/** The signals that are the joiner's nearest signal before an ML merge. */
std::vector<std::pair<TileIndexDiffC, Trackdir>> JoinerSignals(const JunctionCheckContext &ctx);

/**
 * Compute the signal blocks. Tiles with a signal belong to no block; all track bits of a tile are one unit;
 * a bridge joins the tile before its near head and the tile after its far head, and its heads belong to that block.
 * @return Map from tile to block id.
 */
std::map<std::pair<int, int>, size_t> ComputeBlocks(const JunctionLayout &layout);

/** The block of a path piece; bridge pieces are in the block of their bridge. */
std::optional<size_t> BlockOfPiece(const JunctionLayout &layout, const std::map<std::pair<int, int>, size_t> &blocks, const JunctionPiece &piece);

#endif /* JUNCTION_RULES_INTERNAL_H */
