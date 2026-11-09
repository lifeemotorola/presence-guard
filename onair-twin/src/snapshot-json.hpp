/*
OnAirTwin for OBS Studio
Copyright (C) 2026 Emmanuel Suah

Licensed under the GNU General Public License, version 2 or (at your option)
any later version. See LICENSE. This program comes WITHOUT ANY WARRANTY.
*/
#pragma once
#include "twin-core.hpp"

#include <QByteArray>
#include <QString>

/* These serializers include only the explicit fields in twin::Snapshot, not
 * OBS settings or credentials. The caller owns atomic file I/O. */
QByteArray snapshotToJson(const twin::Snapshot &snapshot);
bool snapshotFromJson(const QByteArray &bytes, twin::Snapshot &out, QString &error);
