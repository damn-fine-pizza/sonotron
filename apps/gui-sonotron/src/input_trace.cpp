#include "input_trace.hpp"

#include "imgui.h"
// ImGui::IsAliasKey() (mouse-button/wheel key aliases) is only declared in
// the internal header -- already used elsewhere in this app (tests/
// imgui_headless_harness.hpp), not a new dependency on our part.
#include "imgui_internal.h"

#include <cctype>
#include <cstdio>
#include <fstream>
#include <optional>
#include <stdexcept>

namespace sonotron {

namespace {

constexpr int kImGuiMouseButtonCountLocal = 5;  // mirrors ImGuiMouseButton_COUNT

// -------------------------------------------------------------------------
// Minimal flat-JSON-line field extraction, in the same discipline
// scenes_json.cpp/layout_json.cpp use (a hand-rolled parser bounded to the
// exact subset this schema needs -- one flat object per line, no nesting,
// no arrays -- not a general JSON value type, and no new dependency).
// -------------------------------------------------------------------------

std::optional<std::string> extract_string_field(const std::string& line, const std::string& key) {
  const std::string needle = "\"" + key + "\":\"";
  const std::size_t pos = line.find(needle);
  if (pos == std::string::npos) {
    return std::nullopt;
  }
  std::size_t i = pos + needle.size();
  std::string out;
  while (i < line.size() && line[i] != '"') {
    if (line[i] == '\\' && i + 1 < line.size()) {
      ++i;
      switch (line[i]) {
        case 'n':
          out.push_back('\n');
          break;
        case 'r':
          out.push_back('\r');
          break;
        case 't':
          out.push_back('\t');
          break;
        default:
          out.push_back(line[i]);
          break;
      }
    } else {
      out.push_back(line[i]);
    }
    ++i;
  }
  if (i >= line.size()) {
    return std::nullopt;  // unterminated string -- malformed line
  }
  return out;
}

std::optional<double> extract_number_field(const std::string& line, const std::string& key) {
  const std::string needle = "\"" + key + "\":";
  const std::size_t pos = line.find(needle);
  if (pos == std::string::npos) {
    return std::nullopt;
  }
  const std::size_t start = pos + needle.size();
  std::size_t i = start;
  while (i < line.size() && (std::isdigit(static_cast<unsigned char>(line[i])) != 0 ||
                             line[i] == '-' || line[i] == '+' || line[i] == '.')) {
    ++i;
  }
  if (i == start) {
    return std::nullopt;
  }
  try {
    return std::stod(line.substr(start, i - start));
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

}  // namespace

// -------------------------------------------------------------------------
// InputTraceRecorder
// -------------------------------------------------------------------------

InputTraceRecorder::~InputTraceRecorder() {
  if (m_file != nullptr) {
    std::fclose(m_file);
  }
}

bool InputTraceRecorder::start(const std::string& path, int width, int height) {
  m_file = std::fopen(path.c_str(), "w");
  if (m_file == nullptr) {
    std::fprintf(stderr, "sonotron: could not open input trace file '%s' for writing\n",
                 path.c_str());
    return false;
  }
  std::fprintf(m_file, "{\"t\":\"meta\",\"w\":%d,\"h\":%d}\n", width, height);
  std::fflush(m_file);
  return true;
}

void InputTraceRecorder::capture_frame(int frame, const ImGuiIO& io) {
  if (m_file == nullptr) {
    return;
  }

  if (!m_have_prev_mouse || io.MousePos.x != m_prev_x || io.MousePos.y != m_prev_y) {
    std::fprintf(m_file, "{\"f\":%d,\"t\":\"mp\",\"x\":%.3f,\"y\":%.3f}\n", frame,
                 static_cast<double>(io.MousePos.x), static_cast<double>(io.MousePos.y));
    m_prev_x = io.MousePos.x;
    m_prev_y = io.MousePos.y;
    m_have_prev_mouse = true;
  }

  for (int button = 0; button < kImGuiMouseButtonCountLocal; ++button) {
    if (io.MouseDown[button] != m_prev_button[button]) {
      std::fprintf(m_file, "{\"f\":%d,\"t\":\"mb\",\"b\":%d,\"d\":%d}\n", frame, button,
                   io.MouseDown[button] ? 1 : 0);
      m_prev_button[button] = io.MouseDown[button];
    }
  }

  if (m_prev_key_down.empty()) {
    m_prev_key_down.assign(static_cast<std::size_t>(ImGuiKey_NamedKey_COUNT), false);
  }
  for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; ++key) {
    const auto imgui_key = static_cast<ImGuiKey>(key);
    // Alias keys (ImGuiKey_MouseLeft/Right/Middle/X1/X2, ImGuiKey_MouseWheelX/
    // Y) mirror mouse button/wheel state into the SAME io.KeysData array this
    // loop scans, purely so callers can query mouse state through the key
    // API too -- they are NOT independent key events. Skipping them here is
    // not a loss of information (that same state is already captured above
    // as "mb" lines) and is required on the replay side anyway: ImGui::
    // AddKeyEvent() hard-asserts IsAliasKey(key) == false, so a recorded
    // alias line would abort a later replay (see feed_frame()'s matching
    // guard, kept as defense-in-depth for traces recorded before this fix).
    if (ImGui::IsAliasKey(imgui_key)) {
      continue;
    }
    const std::size_t index = static_cast<std::size_t>(key - ImGuiKey_NamedKey_BEGIN);
    const bool down = io.KeysData[index].Down;
    if (down != m_prev_key_down[index]) {
      std::fprintf(m_file, "{\"f\":%d,\"t\":\"key\",\"k\":%d,\"d\":%d}\n", frame, key,
                   down ? 1 : 0);
      m_prev_key_down[index] = down;
    }
  }

  std::fflush(m_file);
}

// -------------------------------------------------------------------------
// InputTraceReplayer
// -------------------------------------------------------------------------

bool InputTraceReplayer::load(const std::string& path) {
  m_events.clear();
  m_next = 0;
  m_consumed = 0;

  std::ifstream in(path);
  if (!in.is_open()) {
    std::fprintf(stderr, "sonotron: could not open input replay trace '%s'\n", path.c_str());
    return false;
  }

  std::string line;
  while (std::getline(in, line)) {
    const std::optional<std::string> type = extract_string_field(line, "t");
    if (!type.has_value() || *type == "meta" || *type == "hit") {
      continue;
    }
    const std::optional<double> frame_value = extract_number_field(line, "f");
    if (!frame_value.has_value()) {
      continue;
    }
    const int frame = static_cast<int>(*frame_value);

    if (*type == "mp") {
      const std::optional<double> x = extract_number_field(line, "x");
      const std::optional<double> y = extract_number_field(line, "y");
      if (!x.has_value() || !y.has_value()) {
        continue;
      }
      m_events.push_back(ReplayEvent{.frame = frame,
                                     .kind = ReplayEvent::Kind::kMousePos,
                                     .x = static_cast<float>(*x),
                                     .y = static_cast<float>(*y)});
    } else if (*type == "mb") {
      const std::optional<double> button = extract_number_field(line, "b");
      const std::optional<double> down = extract_number_field(line, "d");
      if (!button.has_value() || !down.has_value()) {
        continue;
      }
      m_events.push_back(ReplayEvent{.frame = frame,
                                     .kind = ReplayEvent::Kind::kMouseButton,
                                     .code = static_cast<int>(*button),
                                     .down = *down != 0.0});
    } else if (*type == "key") {
      const std::optional<double> key = extract_number_field(line, "k");
      const std::optional<double> down = extract_number_field(line, "d");
      if (!key.has_value() || !down.has_value()) {
        continue;
      }
      m_events.push_back(ReplayEvent{.frame = frame,
                                     .kind = ReplayEvent::Kind::kKey,
                                     .code = static_cast<int>(*key),
                                     .down = *down != 0.0});
    }
  }
  return true;
}

void InputTraceReplayer::feed_frame(int frame, ImGuiIO& io) {
  bool touched_position = false;
  while (m_next < m_events.size() && m_events[m_next].frame <= frame) {
    const ReplayEvent& event = m_events[m_next];
    switch (event.kind) {
      case ReplayEvent::Kind::kMousePos:
        io.AddMousePosEvent(event.x, event.y);
        m_last_x = event.x;
        m_last_y = event.y;
        m_have_last_pos = true;
        touched_position = true;
        break;
      case ReplayEvent::Kind::kMouseButton:
        io.AddMouseButtonEvent(event.code, event.down);
        break;
      case ReplayEvent::Kind::kKey: {
        // Defense-in-depth against a trace recorded before capture_frame()
        // learned to skip alias keys (ImGuiKey_MouseLeft/Right/Middle/X1/X2,
        // MouseWheelX/Y): io.AddKeyEvent() hard-asserts IsAliasKey(key) ==
        // false, so feeding one back verbatim would abort the whole replay.
        // The mouse state an alias line would have carried is redundant
        // with the trace's own "mb"/"mp" lines, so silently dropping it here
        // loses nothing -- it keeps an OLDER trace file usable rather than
        // forcing every existing recording to be redone.
        const auto imgui_key = static_cast<ImGuiKey>(event.code);
        if (!ImGui::IsAliasKey(imgui_key)) {
          io.AddKeyEvent(imgui_key, event.down);
        }
        break;
      }
    }
    ++m_next;
    ++m_consumed;
  }

  // Real-cursor leak mitigation (see this method's own doc comment in
  // input_trace.hpp): re-assert our last known position every frame, even
  // when this exact frame had no new "mp" line, so it keeps winning over
  // imgui_impl_glfw.cpp's unconditional real-cursor fallback.
  if (m_have_last_pos && !touched_position) {
    io.AddMousePosEvent(m_last_x, m_last_y);
  }
}

}  // namespace sonotron
