// PrimedGun Echoes: the Metroid Prime port's PortLog::Write, which the VR layer (platform/vr, from
// the Metroid Prime VR fork) logs through. This port logs to stderr, which platform/main.cpp
// copies to the log file and, on Android, to logcat.
#pragma once

#include <cstdarg>
#include <cstdio>

namespace PortLog {

inline void Write(const char* format, ...) {
  va_list args;
  va_start(args, format);
  std::vfprintf(stderr, format, args);
  va_end(args);
}

} // namespace PortLog
