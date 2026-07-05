#include "canon.hpp"

#include <cstdint>
#include <string>

#include "diagnostics.hpp"
#include "test.hpp"

namespace {

using namespace arrstyle;

bool contains(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

void test_degree_to_semitone() {
  CHECK(degree_to_semitone("1") == 0);
  CHECK(degree_to_semitone("2") == 2);
  CHECK(degree_to_semitone("3") == 4);
  CHECK(degree_to_semitone("4") == 5);
  CHECK(degree_to_semitone("5") == 7);
  CHECK(degree_to_semitone("6") == 9);
  CHECK(degree_to_semitone("7") == 11);
  CHECK(degree_to_semitone("b3") == 3);
  CHECK(degree_to_semitone("b5") == 6);
  CHECK(degree_to_semitone("b7") == 10);
  CHECK(degree_to_semitone("#4") == 6);
  CHECK(degree_to_semitone("") == kBassRestNibble);
  CHECK(degree_to_semitone("x") == kBassRestNibble);
}

void test_cell_to_mask() {
  CHECK(cell_to_mask({0, 4, 8, 12}) == 0x1111);
  CHECK(cell_to_mask({0, 2, 4, 6, 8, 10, 12, 14}) == 0x5555);
  CHECK(cell_to_mask({0, 8}) == 0x0101);
  CHECK(cell_to_mask({}) == 0x0000);
  CHECK(cell_to_mask({99, -1}) == 0x0000);  // out-of-grid steps ignored
}

void test_pack_bass_template() {
  // step0 in low nibble; rest = 0xF fills unused steps.
  CHECK(pack_bass_template("1-5-1-5") == 0xFFFF7070U);
  CHECK(pack_bass_template("1-1-1-1") == 0xFFFF0000U);
  CHECK(pack_bass_template("1-b3-5-b7") == 0xFFFFA730U);
  CHECK(pack_bass_template("") == 0xFFFFFFFFU);
}

void test_load_and_emit() {
  const std::string fixture = std::string(ARRSTYLE_FIXTURES) + "canon_input.json";
  Diagnostics diag;
  CanonInput input;
  CHECK(load_canon_input(fixture, input, diag));
  CHECK(!diag.has_errors());
  CHECK(input.genres.size() == 2);

  const CanonOptions opts;
  const std::string src = emit_canon_header(input, opts);

  // Freestanding contract: only <cstdint>, no std containers.
  CHECK(contains(src, "#pragma once"));
  CHECK(contains(src, "#include <cstdint>"));
  CHECK(contains(src, "namespace arrangrr {"));
  CHECK(contains(src, "namespace canon {"));
  CHECK(contains(src, "using RhythmCell = std::uint16_t;"));
  CHECK(contains(src, "using BassTemplate = std::uint32_t;"));
  CHECK(!contains(src, "std::vector"));
  CHECK(!contains(src, "std::string"));

  // Distilled tables, sorted by count desc.
  CHECK(contains(src, "kPopRhythmCells[] = {0x1111, 0x5555, 0x0101};"));
  CHECK(contains(src, "kPopBassTemplates[] = {0xFFFF7070, 0xFFFF0000, 0xFFFFA730};"));
  CHECK(contains(src, "kWaltzRhythmCells[] = {0x0111};"));

  // The per-genre index table and its count.
  CHECK(contains(src, "kGenreCanon[] = {"));
  CHECK(contains(src, "{\"pop\", 98, 4, 4, 0, kPopRhythmCells, 3, kPopBassTemplates, 3},"));
  CHECK(contains(src, "{\"waltz\", 90, 3, 4, 0, kWaltzRhythmCells, 1, kWaltzBassTemplates, 1},"));
  CHECK(contains(src, "kGenreCanonCount = 2;"));
}

void test_emit_is_deterministic() {
  const std::string fixture = std::string(ARRSTYLE_FIXTURES) + "canon_input.json";
  Diagnostics d1;
  Diagnostics d2;
  CanonInput a;
  CanonInput b;
  CHECK(load_canon_input(fixture, a, d1));
  CHECK(load_canon_input(fixture, b, d2));
  const CanonOptions opts;
  CHECK(emit_canon_header(a, opts) == emit_canon_header(b, opts));
}

void test_max_cells_truncates() {
  const std::string fixture = std::string(ARRSTYLE_FIXTURES) + "canon_input.json";
  Diagnostics diag;
  CanonInput input;
  CHECK(load_canon_input(fixture, input, diag));
  CanonOptions opts;
  opts.max_cells = 1;
  opts.max_bass = 1;
  const std::string src = emit_canon_header(input, opts);
  // Only the top (highest-count) pop cell/bass survive.
  CHECK(contains(src, "kPopRhythmCells[] = {0x1111};"));
  CHECK(contains(src, "kPopBassTemplates[] = {0xFFFF7070};"));
  CHECK(contains(src, "kPopRhythmCells, 1, kPopBassTemplates, 1"));
}

void test_missing_kb_errors() {
  Diagnostics diag;
  CanonInput input;
  CHECK(!load_canon_input("/no/such/kb/path.json", input, diag));
  CHECK(diag.has_errors());
}

// A hostile aggregate (name with quote/newline/brace, out-of-range numbers)
// must NOT inject source into the emitted header nor make it fail to compile:
// the name is escaped inside the literal, control bytes are dropped, and the
// numeric fields are clamped to their destination widths.
void test_emit_escapes_hostile_input() {
  CanonInput input;
  GenreAggregate g;
  g.name = "ev\"il\n},{";  // quote + newline + brace-injection attempt
  g.tempo_bpm = 70000;     // > uint16
  g.time_sig_num = 300;    // > uint8
  g.time_sig_den = 4;
  g.swing_percent = 999;   // > uint8
  input.genres.push_back(g);

  const std::string src = emit_canon_header(input, CanonOptions{});
  CHECK(contains(src, "\\\""));         // the quote is emitted ESCAPED
  CHECK(!contains(src, "\"ev\"il"));    // the raw literal breakout never appears
  CHECK(contains(src, "65535"));        // tempo clamped to uint16 max
  CHECK(contains(src, "255"));          // time_sig_num / swing clamped to uint8 max
  CHECK(!contains(src, "70000"));       // the out-of-range value is gone
  CHECK(!contains(src, "300"));
}

}  // namespace

int main() {
  test_degree_to_semitone();
  test_cell_to_mask();
  test_pack_bass_template();
  test_load_and_emit();
  test_emit_is_deterministic();
  test_max_cells_truncates();
  test_missing_kb_errors();
  test_emit_escapes_hostile_input();
  return arrstyle::test::failures();
}
