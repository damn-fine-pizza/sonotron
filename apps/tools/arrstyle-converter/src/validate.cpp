#include "validate.hpp"

#include "model.hpp"
#include "serialize.hpp"

namespace arrstyle {

namespace {

constexpr int kMaxMidiValue = 127;
constexpr int kMaxPitchClass = 11;

// Field accessors that record a typed error when the shape is wrong.
bool require_string(const Json& parent, const std::string& key, const std::string& source,
                    Diagnostics& diag, std::string& out) {
  const Json* v = parent.find(key);
  if (v == nullptr || !v->is_string()) {
    diag.error("field '" + key + "' must be a string", source);
    return false;
  }
  out = v->as_string();
  return true;
}

bool require_int(const Json& parent, const std::string& key, const std::string& source,
                 Diagnostics& diag, std::int64_t& out) {
  const Json* v = parent.find(key);
  if (v == nullptr || !v->is_int()) {
    diag.error("field '" + key + "' must be an integer", source);
    return false;
  }
  out = v->as_int();
  return true;
}

const Json* require_array(const Json& parent, const std::string& key, const std::string& source,
                          Diagnostics& diag) {
  const Json* v = parent.find(key);
  if (v == nullptr || !v->is_array()) {
    diag.error("field '" + key + "' must be an array", source);
    return nullptr;
  }
  return v;
}

void check_range(std::int64_t value, std::int64_t lo, std::int64_t hi, const std::string& what,
                 const std::string& source, Diagnostics& diag) {
  if (value < lo || value > hi) {
    diag.error(what + " out of range [" + std::to_string(lo) + ".." + std::to_string(hi) +
                   "]: " + std::to_string(value),
               source);
  }
}

void validate_style(const Json& doc, const std::string& source, Diagnostics& diag) {
  std::string name;
  require_string(doc, "name", source, diag, name);
  std::int64_t ppqn = 0;
  if (require_int(doc, "source_ppqn", source, diag, ppqn) && ppqn <= 0) {
    diag.error("source_ppqn must be positive", source);
  }
  std::int64_t tempo = 0;
  if (require_int(doc, "tempo_milli_bpm", source, diag, tempo) && tempo <= 0) {
    diag.error("tempo_milli_bpm must be positive", source);
  }
  std::int64_t ts_num = 0;
  std::int64_t ts_den = 0;
  if (require_int(doc, "time_sig_num", source, diag, ts_num) && ts_num <= 0) {
    diag.error("time_sig_num must be positive", source);
  }
  if (require_int(doc, "time_sig_den", source, diag, ts_den) && ts_den <= 0) {
    diag.error("time_sig_den must be positive", source);
  }

  const Json* sections = require_array(doc, "sections", source, diag);
  if (sections == nullptr) {
    return;
  }
  if (sections->elements().empty()) {
    diag.warn("style has no sections", source);
  }
  for (const Json& section : sections->elements()) {
    std::string kind_name;
    if (require_string(section, "kind", source, diag, kind_name)) {
      SectionKind kind{};
      if (!parse_section_kind(kind_name, kind)) {
        diag.error("unknown section kind '" + kind_name + "'", source);
      }
    }
    std::string var_name;
    if (require_string(section, "variation", source, diag, var_name)) {
      SectionVariation var{};
      if (!parse_section_variation(var_name, var)) {
        diag.error("unknown section variation '" + var_name + "'", source);
      }
    }
    std::int64_t bars = 0;
    if (require_int(section, "bars", source, diag, bars) && bars <= 0) {
      diag.error("section bars must be positive", source);
    }
    const Json* lanes = require_array(section, "lanes", source, diag);
    if (lanes == nullptr) {
      continue;
    }
    for (const Json& lane : lanes->elements()) {
      std::string role_name;
      if (require_string(lane, "role", source, diag, role_name)) {
        Role role{};
        if (!parse_role(role_name, role)) {
          diag.error("unknown role '" + role_name + "'", source);
        }
      }
      std::string policy_name;
      if (require_string(lane, "transposition", source, diag, policy_name)) {
        TranspositionPolicy policy{};
        if (!parse_transposition_policy(policy_name, policy)) {
          diag.error("unknown transposition policy '" + policy_name + "'", source);
        }
      }
      std::string retrigger_name;
      if (require_string(lane, "retrigger", source, diag, retrigger_name)) {
        RetriggerPolicy retrigger{};
        if (!parse_retrigger_policy(retrigger_name, retrigger)) {
          diag.error("unknown retrigger policy '" + retrigger_name + "'", source);
        }
      }
      const Json* events = require_array(lane, "events", source, diag);
      if (events == nullptr) {
        continue;
      }
      std::int64_t last_tick = -1;
      for (const Json& event : events->elements()) {
        std::int64_t tick = 0;
        std::int64_t note = 0;
        std::int64_t vel = 0;
        std::int64_t gate = 0;
        require_int(event, "tick", source, diag, tick);
        require_int(event, "note", source, diag, note);
        require_int(event, "velocity", source, diag, vel);
        require_int(event, "gate_ticks", source, diag, gate);
        check_range(note, 0, kMaxMidiValue, "note", source, diag);
        check_range(vel, 1, kMaxMidiValue, "velocity", source, diag);
        if (tick < 0) {
          diag.error("event tick must be non-negative", source);
        }
        if (gate < 0) {
          diag.error("event gate_ticks must be non-negative", source);
        }
        if (tick < last_tick) {
          diag.warn("events are not sorted by tick", source);
        }
        last_tick = tick;
      }
    }
  }
}

void validate_song(const Json& doc, const std::string& source, Diagnostics& diag) {
  std::string name;
  require_string(doc, "name", source, diag, name);
  std::int64_t key_root = 0;
  if (require_int(doc, "key_root_pc", source, diag, key_root)) {
    check_range(key_root, -1, kMaxPitchClass, "key_root_pc", source, diag);
  }
  std::string harmony;
  require_string(doc, "harmony_source", source, diag, harmony);

  const Json* chords = require_array(doc, "chords", source, diag);
  if (chords == nullptr) {
    return;
  }
  if (chords->elements().empty()) {
    diag.warn("song has no chords", source);
  }
  for (const Json& chord : chords->elements()) {
    std::int64_t root = 0;
    if (require_int(chord, "root_pc", source, diag, root)) {
      check_range(root, -1, kMaxPitchClass, "chord root_pc", source, diag);
    }
    std::int64_t bass = 0;
    if (require_int(chord, "bass_pc", source, diag, bass)) {
      check_range(bass, -1, kMaxPitchClass, "chord bass_pc", source, diag);
    }
    std::string quality_name;
    if (require_string(chord, "quality", source, diag, quality_name)) {
      ChordQuality quality{};
      if (!parse_chord_quality(quality_name, quality)) {
        diag.error("unknown chord quality '" + quality_name + "'", source);
      }
    }
  }
}

}  // namespace

bool validate_document(const Json& doc, const std::string& source, Diagnostics& diag) {
  if (!doc.is_object()) {
    diag.error("top-level value is not a JSON object", source);
    return false;
  }
  std::int64_t version = 0;
  if (require_int(doc, "version", source, diag, version) && version != kSchemaVersion) {
    diag.warn("schema version " + std::to_string(version) + " != expected " +
                  std::to_string(kSchemaVersion),
              source);
  }
  std::string format;
  if (!require_string(doc, "format", source, diag, format)) {
    return false;
  }
  if (format == "arrstyle") {
    validate_style(doc, source, diag);
  } else if (format == "arrsong") {
    validate_song(doc, source, diag);
  } else {
    diag.error("unknown document format '" + format + "'", source);
  }
  return !diag.has_errors();
}

bool validate_text(const std::string& text, const std::string& source, Diagnostics& diag) {
  Json doc;
  std::string error;
  if (!parse_json(text, doc, error)) {
    diag.error("JSON parse error: " + error, source);
    return false;
  }
  return validate_document(doc, source, diag);
}

}  // namespace arrstyle
