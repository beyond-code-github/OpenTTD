/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_signal_pass.h Junction signal and prio pass for the generator. */

#ifndef JUNCTION_SIGNAL_PASS_H
#define JUNCTION_SIGNAL_PASS_H

#include "junction_generator.h"
#include "junction_layout.h"
#include "junction_route.h"

#include <functional>
#include <vector>

/** Add signals and prios to a layout of tracks and bridges, in the canonical frame. */
void PlaceJunctionSignals(JunctionLayout &layout, const JunctionSpec &spec);

/**
 * Place signals along a path's pieces as drag-signalling would (D97): density sd x 192 logical units (192 per axis piece, 128 per diagonal half piece),
 * walking upstream from the last piece; an unsuitable piece moves a signal to the last suitable piece before it. The first and last piece never take one.
 */
void PlaceDragSignals(JunctionLayout &layout, const std::vector<JunctionPiece> &pieces, uint8_t sd, SignalType type);

/** PlaceDragSignals over callbacks: `signalled` and `legal` answer for a piece, `place` puts a signal on it (the token pass's own state, no layout). */
void PlaceDragSignals(const std::vector<JunctionPiece> &pieces, uint8_t sd, const std::function<bool(const JunctionPiece &)> &signalled, const std::function<bool(const JunctionPiece &)> &legal, const std::function<void(const JunctionPiece &)> &place);

/** Of two signals on adjacent pieces of a path, drop one when every rule still holds without it (D117 b, D117b: run once per chosen layout, not per candidate). */
void RemoveRedundantJunctionSignals(JunctionLayout &layout, const JunctionSpec &spec);

#endif /* JUNCTION_SIGNAL_PASS_H */
