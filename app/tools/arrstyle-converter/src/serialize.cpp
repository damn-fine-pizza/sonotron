#include "serialize.hpp"

namespace arrstyle {

namespace {

Json event_to_json(const PhraseEvent& e) {
  Json j = Json::object();
  j.set("tick", Json::integer(e.tick));
  j.set("note", Json::integer(e.note));
  j.set("velocity", Json::integer(e.velocity));
  j.set("gate_ticks", Json::integer(e.gate_ticks));
  return j;
}

Json lane_to_json(const PhraseLane& lane) {
  Json j = Json::object();
  j.set("role", Json::string(to_string(lane.role)));
  j.set("source_channel", Json::integer(lane.source_channel));
  j.set("transposition", Json::string(to_string(lane.transposition)));
  j.set("retrigger", Json::string(to_string(lane.retrigger)));
  // SFF/CASM provenance is emitted only when a source chord was decoded, so
  // non-SFF imports keep their exact previous byte layout.
  if (lane.source_root_pc >= 0) {
    j.set("source_root_pc", Json::integer(lane.source_root_pc));
    j.set("source_quality", Json::string(to_string(lane.source_quality)));
    j.set("note_low", Json::integer(lane.note_low));
    j.set("note_high", Json::integer(lane.note_high));
  }
  Json events = Json::array();
  for (const PhraseEvent& e : lane.events) {
    events.push_back(event_to_json(e));
  }
  j.set("events", std::move(events));
  return j;
}

Json section_to_json(const StyleSection& s) {
  Json j = Json::object();
  j.set("kind", Json::string(to_string(s.kind)));
  j.set("variation", Json::string(to_string(s.variation)));
  j.set("bars", Json::integer(s.bars));
  Json lanes = Json::array();
  for (const PhraseLane& lane : s.lanes) {
    lanes.push_back(lane_to_json(lane));
  }
  j.set("lanes", std::move(lanes));
  return j;
}

Json chord_to_json(const ChordEvent& c) {
  Json j = Json::object();
  j.set("position", Json::integer(c.position));
  j.set("bar", Json::integer(c.bar));
  j.set("root_pc", Json::integer(c.root_pc));
  j.set("quality", Json::string(to_string(c.quality)));
  j.set("bass_pc", Json::integer(c.bass_pc));
  j.set("source_text", Json::string(c.source_text));
  return j;
}

Json song_section_to_json(const SongSection& s) {
  Json j = Json::object();
  j.set("label", Json::string(s.label));
  j.set("position", Json::integer(s.position));
  return j;
}

}  // namespace

Json to_json(const StyleModel& style) {
  Json root = Json::object();
  root.set("format", Json::string("arrstyle"));
  root.set("version", Json::integer(kSchemaVersion));
  root.set("name", Json::string(style.name));
  root.set("source_format", Json::string(to_string(style.source_format)));
  root.set("source_ppqn", Json::integer(style.source_ppqn));
  root.set("tempo_milli_bpm", Json::integer(style.tempo_milli_bpm));
  root.set("time_sig_num", Json::integer(style.time_sig_num));
  root.set("time_sig_den", Json::integer(style.time_sig_den));
  Json sections = Json::array();
  for (const StyleSection& s : style.sections) {
    sections.push_back(section_to_json(s));
  }
  root.set("sections", std::move(sections));
  return root;
}

Json to_json(const SongModel& song) {
  Json root = Json::object();
  root.set("format", Json::string("arrsong"));
  root.set("version", Json::integer(kSchemaVersion));
  root.set("name", Json::string(song.name));
  root.set("key_root_pc", Json::integer(song.key_root_pc));
  root.set("key_mode", Json::integer(song.key_mode));
  root.set("tempo_milli_bpm", Json::integer(song.tempo_milli_bpm));
  root.set("time_sig_num", Json::integer(song.time_sig_num));
  root.set("time_sig_den", Json::integer(song.time_sig_den));
  root.set("harmony_source", Json::string(to_string(song.harmony_source)));
  Json sections = Json::array();
  for (const SongSection& s : song.sections) {
    sections.push_back(song_section_to_json(s));
  }
  root.set("sections", std::move(sections));
  Json chords = Json::array();
  for (const ChordEvent& c : song.chords) {
    chords.push_back(chord_to_json(c));
  }
  root.set("chords", std::move(chords));
  return root;
}

}  // namespace arrstyle
