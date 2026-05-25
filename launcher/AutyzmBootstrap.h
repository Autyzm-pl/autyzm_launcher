// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QString>

namespace AutyzmBootstrap {
void applyFirstRunDefaults();
QString defaultPreLaunchCommand();
bool writeDefaultInstanceConfig(const QString& path);
void ensureDefaultInstance();
}  // namespace AutyzmBootstrap
