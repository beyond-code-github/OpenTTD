/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_compact.h Junction layout compactness measure. */

#ifndef JUNCTION_COMPACT_H
#define JUNCTION_COMPACT_H

#include "junction_layout.h"

/** Compactness of a layout (D153): occupied surface tiles, sliver tiles and enclosed tiles, as RECON-organic §7 defines them. */
struct JunctionCompactness {
	int occupied = 0;
	int slivers = 0;
	int enclosed = 0;
};

/** Measure a layout's compactness (D153). */
JunctionCompactness MeasureJunctionCompactness(const JunctionLayout &layout);

#endif /* JUNCTION_COMPACT_H */
