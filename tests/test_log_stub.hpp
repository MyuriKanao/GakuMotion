// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "GkLog.hpp"

// Offline executables have no plugin host; retain the production logging API
// without linking the DLL entry points or touching any game process.
inline void GakuMotion::Log::Write(Level, const char*, ...) {}
