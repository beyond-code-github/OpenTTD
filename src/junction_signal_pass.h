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

/** Add signals and prios to a layout of tracks and bridges, in the canonical frame. */
void PlaceJunctionSignals(JunctionLayout &layout, const JunctionSpec &spec);

/** Of two signals on adjacent pieces of a path, drop one when every rule still holds without it (D117 b, D117b: run once per chosen layout, not per candidate). */
void RemoveRedundantJunctionSignals(JunctionLayout &layout, const JunctionSpec &spec);

#endif /* JUNCTION_SIGNAL_PASS_H */
