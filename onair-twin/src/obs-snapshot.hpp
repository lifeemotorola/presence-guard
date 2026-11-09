/*
OnAirTwin for OBS Studio
Copyright (C) 2026 Emmanuel Suah

Licensed under the GNU General Public License, version 2 or (at your option)
any later version. See LICENSE. This program comes WITHOUT ANY WARRANTY.
*/
#pragma once
#include "twin-core.hpp"

/* Runs on the OBS/Qt frontend thread. Uses read-only OBS APIs only. */
twin::Snapshot captureObsSnapshot(const std::string &capturedAtUtc);
