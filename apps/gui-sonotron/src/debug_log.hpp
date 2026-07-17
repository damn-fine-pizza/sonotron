#pragma once

#include <chrono>
#include <cstdio>
#include <ctime>
#include <string_view>

// Owner ask: a `--debug` / SONOTRON_DEBUG root-cause trace for the still-
// broken live auto-song ("never leaves intro1 audibly"), see main.cpp's
// resolver, logging_brain_session.hpp's wire choke point, and grid_panel.
// cpp's update_auto_song. Header-only and dependency-free beyond STL/<ctime>
// so both main.cpp (sets the flag once at startup) and every panel .cpp that
// wants to trace (e.g. grid_panel.cpp) can include it directly without
// pulling in a new CMake library-link edge -- the same "exactly as core-free"
// discipline every other gui_sonotron_layout header already follows.
namespace sonotron {

namespace debug_detail {
// A plain (non-atomic) bool: set exactly once at startup from main(), on the
// GUI thread, before any other thread exists (InProcessBrainSession::start()
// spawns the engine thread strictly later) -- never written again
// afterward, and only ever read from the GUI thread. See main.cpp's
// --debug/SONOTRON_DEBUG resolver.
inline bool g_debug_enabled = false;
}  // namespace debug_detail

// Zero overhead when off: a single plain bool read, no atomics, no locks.
// Callers gate any (potentially allocating) message construction behind this
// themselves -- see debug_log()'s own comment below.
inline bool debug_enabled() { return debug_detail::g_debug_enabled; }

inline void set_debug_enabled(bool enabled) { debug_detail::g_debug_enabled = enabled; }

// Prints one stamped diagnostic line to stderr: "[HH:MM:SS.mmm] <line>\n".
// Callers are expected to gate the message construction behind
// debug_enabled() themselves (see e.g. logging_brain_session.hpp,
// grid_panel.cpp's update_auto_song) so a std::string is never even built
// when debug tracing is off.
inline void debug_log(std::string_view line) {
  const auto now = std::chrono::system_clock::now();
  const auto ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
  const std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm tm_buf{};
  localtime_r(&t, &tm_buf);
  std::fprintf(stderr, "[%02d:%02d:%02d.%03d] %.*s\n", tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
               static_cast<int>(ms), static_cast<int>(line.size()), line.data());
}

}  // namespace sonotron
