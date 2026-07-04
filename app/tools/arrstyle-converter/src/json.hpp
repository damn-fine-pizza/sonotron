#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// A tiny self-contained JSON value with a DETERMINISTIC writer and a bounded
// recursive-descent reader. No third-party dependency (project policy): the
// writer emits objects in insertion order with fixed 2-space indentation and
// integer-only numbers, so the same model always serializes to the same bytes.
//
// The reader exists so `validate` can inspect a previously written file; it is
// deliberately minimal (numbers are stored as int64; fractional/exponent forms
// are rounded) because the writer never emits anything richer.

namespace arrstyle {

class Json {
 public:
  enum class Type : std::uint8_t {
    kNull = 0,
    kBool = 1,
    kInt = 2,
    kString = 3,
    kArray = 4,
    kObject = 5,
  };

  using Member = std::pair<std::string, Json>;

  Json() = default;

  static Json null() { return Json{}; }
  static Json boolean(bool v);
  static Json integer(std::int64_t v);
  static Json string(std::string v);
  static Json array();
  static Json object();

  Type type() const noexcept { return m_type; }
  bool is_object() const noexcept { return m_type == Type::kObject; }
  bool is_array() const noexcept { return m_type == Type::kArray; }
  bool is_int() const noexcept { return m_type == Type::kInt; }
  bool is_string() const noexcept { return m_type == Type::kString; }
  bool is_bool() const noexcept { return m_type == Type::kBool; }
  bool is_null() const noexcept { return m_type == Type::kNull; }

  bool as_bool() const noexcept { return m_bool; }
  std::int64_t as_int() const noexcept { return m_int; }
  const std::string& as_string() const noexcept { return m_string; }
  const std::vector<Json>& elements() const noexcept { return m_array; }
  const std::vector<Member>& members() const noexcept { return m_object; }

  // Object helpers (insertion order preserved => deterministic output).
  void set(std::string key, Json value);
  // Returns nullptr when absent (or when this is not an object).
  const Json* find(const std::string& key) const noexcept;

  // Array helper.
  void push_back(Json value);

  // Serialize with a trailing newline omitted; caller adds one when writing.
  std::string dump(int indent = 2) const;

 private:
  void dump_to(std::string& out, int indent, int depth) const;

  Type m_type = Type::kNull;
  bool m_bool = false;
  std::int64_t m_int = 0;
  std::string m_string;
  std::vector<Json> m_array;
  std::vector<Member> m_object;
};

// Parses `text` into `out`. On failure returns false and fills `error` with a
// human message including a 1-based line/column.
bool parse_json(const std::string& text, Json& out, std::string& error);

}  // namespace arrstyle
