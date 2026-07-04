#include "cli.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "test.hpp"

namespace {

using namespace arrstyle;

std::string fixture(const std::string& name) { return std::string(ARRSTYLE_FIXTURES) + name; }

int run_args(const std::vector<std::string>& args, std::string& out, std::string& err) {
  std::ostringstream o;
  std::ostringstream e;
  const int code = run(args, o, e);
  out = o.str();
  err = e.str();
  return code;
}

std::string read_file(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  std::ostringstream ss;
  ss << file.rdbuf();
  return ss.str();
}

void test_no_args_is_usage_error() {
  std::string out;
  std::string err;
  CHECK(run_args({}, out, err) == kExitUsage);
}

void test_help_and_version_succeed() {
  std::string out;
  std::string err;
  CHECK(run_args({"help"}, out, err) == kExitOk);
  CHECK(out.find("usage") != std::string::npos);
  CHECK(run_args({"version"}, out, err) == kExitOk);
}

void test_unknown_command_is_usage_error() {
  std::string out;
  std::string err;
  CHECK(run_args({"frobnicate"}, out, err) == kExitUsage);
}

void test_inspect_midi_succeeds() {
  std::string out;
  std::string err;
  CHECK(run_args({"inspect", fixture("tiny.mid")}, out, err) == kExitOk);
  CHECK(out.find("format: midi") != std::string::npos);
}

void test_import_midi_writes_deterministic_file() {
  const std::string out_path = "cli_test_out.arrstyle.json";
  std::string out;
  std::string err;
  CHECK(run_args({"import-midi", fixture("tiny.mid"), "--out", out_path}, out, err) == kExitOk);
  const std::string first = read_file(out_path);
  CHECK(!first.empty());
  CHECK(first.find("\"format\": \"arrstyle\"") != std::string::npos);
  // Re-run: byte-identical output.
  CHECK(run_args({"import-midi", fixture("tiny.mid"), "--out", out_path}, out, err) == kExitOk);
  CHECK(read_file(out_path) == first);

  // The produced file validates.
  std::string vout;
  std::string verr;
  CHECK(run_args({"validate", out_path}, vout, verr) == kExitOk);
}

void test_import_midi_missing_out_is_usage_error() {
  std::string out;
  std::string err;
  CHECK(run_args({"import-midi", fixture("tiny.mid")}, out, err) == kExitUsage);
}

void test_import_chordpro_writes_file() {
  const std::string out_path = "cli_test_out.arrsong.json";
  std::string out;
  std::string err;
  CHECK(run_args({"import-chordpro", fixture("sample.chopro"), "--out", out_path}, out, err) ==
        kExitOk);
  std::string vout;
  std::string verr;
  CHECK(run_args({"validate", out_path}, vout, verr) == kExitOk);
}

void test_import_sff_fails_cleanly() {
  std::string out;
  std::string err;
  // Reuse the MIDI fixture as a stand-in payload; import must refuse non-zero.
  CHECK(run_args({"import-sff", fixture("tiny.mid")}, out, err) == kExitFailure);
  CHECK(out.find("inspect-only") != std::string::npos);
}

void test_validate_bad_document_fails() {
  const std::string bad_path = "cli_test_bad.json";
  {
    std::ofstream f(bad_path, std::ios::binary | std::ios::trunc);
    f << "{ \"format\": \"arrstyle\", \"version\": 1 }";  // missing required fields
  }
  std::string out;
  std::string err;
  CHECK(run_args({"validate", bad_path}, out, err) == kExitFailure);
}

void test_missing_input_file_fails() {
  std::string out;
  std::string err;
  CHECK(run_args({"inspect", "does-not-exist.mid"}, out, err) == kExitFailure);
}

}  // namespace

int main() {
  test_no_args_is_usage_error();
  test_help_and_version_succeed();
  test_unknown_command_is_usage_error();
  test_inspect_midi_succeeds();
  test_import_midi_writes_deterministic_file();
  test_import_midi_missing_out_is_usage_error();
  test_import_chordpro_writes_file();
  test_import_sff_fails_cleanly();
  test_validate_bad_document_fails();
  test_missing_input_file_fails();
  return arrstyle::test::failures();
}
