#include "json.hpp"

#include <string>

#include "test.hpp"

namespace {

using namespace arrstyle;

Json sample_document() {
  Json root = Json::object();
  root.set("format", Json::string("arrstyle"));
  root.set("version", Json::integer(1));
  root.set("count", Json::integer(-3));
  root.set("flag", Json::boolean(true));
  Json arr = Json::array();
  arr.push_back(Json::integer(10));
  arr.push_back(Json::string("a\"b\n"));
  root.set("items", std::move(arr));
  root.set("empty_obj", Json::object());
  root.set("empty_arr", Json::array());
  return root;
}

void test_writer_is_deterministic_and_ordered() {
  const std::string a = sample_document().dump();
  const std::string b = sample_document().dump();
  CHECK(a == b);
  // Insertion order is preserved; escaping is applied.
  const std::string expected =
      "{\n"
      "  \"format\": \"arrstyle\",\n"
      "  \"version\": 1,\n"
      "  \"count\": -3,\n"
      "  \"flag\": true,\n"
      "  \"items\": [\n"
      "    10,\n"
      "    \"a\\\"b\\n\"\n"
      "  ],\n"
      "  \"empty_obj\": {},\n"
      "  \"empty_arr\": []\n"
      "}";
  CHECK(a == expected);
}

void test_round_trip_preserves_bytes() {
  const std::string text = sample_document().dump();
  Json parsed;
  std::string error;
  const bool ok = parse_json(text, parsed, error);
  CHECK(ok);
  CHECK(error.empty());
  CHECK(parsed.dump() == text);
}

void test_parse_errors_are_reported() {
  Json parsed;
  std::string error;
  CHECK(!parse_json("{ \"a\": }", parsed, error));
  CHECK(!error.empty());
  CHECK(!parse_json("[1, 2", parsed, error));
  CHECK(!parse_json("nul", parsed, error));
}

void test_find_and_accessors() {
  Json parsed;
  std::string error;
  CHECK(parse_json(sample_document().dump(), parsed, error));
  const Json* fmt = parsed.find("format");
  CHECK(fmt != nullptr && fmt->is_string() && fmt->as_string() == "arrstyle");
  const Json* count = parsed.find("count");
  CHECK(count != nullptr && count->is_int() && count->as_int() == -3);
  CHECK(parsed.find("missing") == nullptr);
}

}  // namespace

int main() {
  test_writer_is_deterministic_and_ordered();
  test_round_trip_preserves_bytes();
  test_parse_errors_are_reported();
  test_find_and_accessors();
  return arrstyle::test::failures();
}
