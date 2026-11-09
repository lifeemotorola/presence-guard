/*
OnAirTwin for OBS Studio
Copyright (C) 2026 Emmanuel Suah

Licensed under the GNU General Public License, version 2 or (at your option)
any later version. See LICENSE. This program comes WITHOUT ANY WARRANTY.
*/
#pragma once
#include "twin-core.hpp"
#include <QString>

/* Everything from OBS is escaped. The result contains no external resources. */
QString makeTwinReport(const twin::Snapshot &snapshot, const twin::Comparison *comparison,
		       const twin::Simulation *simulation, bool dark);
