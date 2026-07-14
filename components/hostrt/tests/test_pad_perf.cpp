// Functional tests for the pad-bank / performance-recall L1 grammar
// (Phase-5 Item #9, docs/phase5-design-reviews.md "Pad/Scene live ->
// Performance"): `pad assign|trigger|release` and `perf store|recall|save|
// load` through Shell::exec_line, mirroring test_host.cpp's own ShellFixture
// pattern. The subject is the STRING-GRAMMAR <-> Command translation plus the
// host-only file I/O (perf save/load) -- functional, same precedent as
// test_host.cpp's own shell command tests.

#include <string>
#include <vector>

#include "arrangrr/perf/performance.hpp"
#include "midisrc/file_io.hpp"
#include "shell.hpp"
#include "test.hpp"

namespace {

using namespace arrangrr;
using namespace arrangrr::host;

struct ShellFixture {
  std::vector<OutEvent> events;
  Shell shell{[this](const OutEvent& ev) { events.push_back(ev); }};
  std::string err;

  bool run(const std::string& line) { return shell.exec_line(line, err); }
  int warns() const {
    int n = 0;
    for (const OutEvent& e : events) {
      if (e.kind == OutEvent::Kind::kWarn) {
        ++n;
      }
    }
    return n;
  }
};

void test_pad_grammar_smoke() {
  ShellFixture f;
  CHECK(f.run("port open out synth"));
  CHECK(f.run("style load basic"));
  CHECK(f.run("clip add bass 0 style varB"));  // clip id 0
  CHECK(f.run("pad assign 0 phrase oneshot synth:1 0"));
  CHECK(f.run("pad trigger 0"));
  CHECK(f.shell.engine().arranger().current() == SectionType::kVarB);
  CHECK(f.run("pad release 0"));  // kOneShot: release is a documented no-op
  CHECK(f.warns() == 0);

  // Errors: unknown type/mode/dest, out-of-range id (rejected at PARSE time),
  // missing arguments.
  CHECK(!f.run("pad assign 0 nope oneshot synth:1 0"));
  CHECK(!f.run("pad assign 0 phrase nope synth:1 0"));
  CHECK(!f.run("pad assign 0 phrase oneshot nowhere 0"));
  CHECK(!f.run("pad assign 99999 phrase oneshot synth:1 0"));
  CHECK(!f.run("pad trigger 99999"));
  CHECK(!f.run("pad"));
}

void test_pad_quantized_assign_and_trigger() {
  ShellFixture f;
  CHECK(f.run("port open out synth"));
  CHECK(f.run("style load basic"));
  CHECK(f.run("clip add bass 0 style varB"));
  CHECK(f.run("pad assign 0 phrase hold synth:1 0 quantize 1"));  // sync = next bar
  CHECK(f.run("transport start"));
  CHECK(f.run("pad trigger 0"));
  CHECK(f.shell.engine().clips().get(0)->state == LaunchState::kArmed);  // not yet
  std::string advance_err;
  CHECK(f.shell.advance_by(kTicksPerBar, advance_err));
  CHECK(f.shell.engine().clips().get(0)->state == LaunchState::kPlaying);
}

void test_perf_store_recall_grammar() {
  ShellFixture f;
  CHECK(f.run("port open out synth"));
  CHECK(f.run("style load basic"));
  CHECK(f.run("style section varB"));
  CHECK(f.run("perf store 0"));
  CHECK(f.run("style section varC"));
  CHECK(f.shell.engine().arranger().current() == SectionType::kVarC);
  CHECK(f.run("perf recall 0"));
  CHECK(f.shell.engine().arranger().current() == SectionType::kVarB);

  // "perf recall 16" is rejected at PARSE time (16 == kMaxPerformances, out
  // of range); "perf recall 15" parses fine (in range) but was never stored,
  // so the ENGINE warns instead -- exec_line still returns true (the command
  // reached the engine), the rejection rides the event stream.
  CHECK(!f.run("perf recall 16"));
  CHECK(f.run("perf recall 15"));
  CHECK(f.warns() == 1);
}

void test_perf_save_load_round_trip() {
  ShellFixture f;
  CHECK(f.run("port open out synth"));
  CHECK(f.run("style load basic"));
  CHECK(f.run("style section varB"));
  CHECK(f.run("perf store 0"));
  CHECK(f.run("style load pop"));
  CHECK(f.run("style section varD"));
  CHECK(f.run("perf store 1"));
  CHECK(f.shell.engine().performances().size() == 2);

  const std::string path = "test_pad_perf_roundtrip.snpf";
  CHECK(f.run("perf save " + path));

  // A SEPARATE shell with a different, single-slot store: reload from disk
  // must replace it wholesale with the two-slot saved store.
  ShellFixture g;
  CHECK(g.run("port open out synth"));
  CHECK(g.run("style load basic"));
  CHECK(g.run("perf store 0"));
  CHECK(g.shell.engine().performances().size() == 1);

  CHECK(g.run("perf load " + path));
  CHECK(g.shell.engine().performances().size() == 2);
  CHECK(g.shell.engine().performances().get(0)->style_id == 0);  // basic
  CHECK(g.shell.engine().performances().get(0)->variation ==
        static_cast<std::uint8_t>(SectionType::kVarB));
  CHECK(g.shell.engine().performances().get(1)->style_id == 1);  // pop
  CHECK(g.shell.engine().performances().get(1)->variation ==
        static_cast<std::uint8_t>(SectionType::kVarD));
}

void test_perf_load_rejects_malformed_file() {
  ShellFixture f;
  CHECK(f.run("port open out synth"));
  CHECK(f.run("style load basic"));
  CHECK(f.run("perf store 0"));
  CHECK(f.shell.engine().performances().size() == 1);
  const std::uint16_t style_before = f.shell.engine().performances().get(0)->style_id;

  const std::string path = "test_pad_perf_malformed.snpf";
  std::string write_err;
  const std::vector<std::uint8_t> garbage = {0xDE, 0xAD, 0xBE, 0xEF, 1, 2, 3};
  CHECK(midisrc::write_binary_file(path, garbage, write_err));

  CHECK(!f.run("perf load " + path));
  CHECK(!f.err.empty());
  // The store is UNTOUCHED by a rejected load -- no half-applied state.
  CHECK(f.shell.engine().performances().size() == 1);
  CHECK(f.shell.engine().performances().get(0)->style_id == style_before);

  CHECK(!f.run("perf load /nonexistent/path/does/not/exist.snpf"));
  CHECK(!f.err.empty());
}

}  // namespace

int main() {
  test_pad_grammar_smoke();
  test_pad_quantized_assign_and_trigger();
  test_perf_store_recall_grammar();
  test_perf_save_load_round_trip();
  test_perf_load_rejects_malformed_file();
  return arrangrr::test::failures();
}
