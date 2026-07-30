#pragma once

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

struct RespValue;

using RespArray = std::vector<RespValue>;
using RespNull = std::monostate;

struct RespValue {
  using Value = std::variant<RespNull, std::string, int64_t, RespArray>;

  enum class Type { Null, SimpleString, Error, Integer, BulkString, Array };

  Type type{};
  Value value{};

  static RespValue null() { return {Type::Null, RespNull{}}; }
  static RespValue ss(std::string s) {
    return {Type::SimpleString, std::move(s)};
  }
  static RespValue error(std::string s) { return {Type::Error, std::move(s)}; }
  static RespValue integer(int64_t n) { return {Type::Integer, n}; }
  static RespValue bulk(std::string s) {
    return {Type::BulkString, std::move(s)};
  }
  static RespValue array(RespArray arr) {
    return {Type::Array, std::move(arr)};
  }
};

class RespParser {
public:
  struct ParseResult {
    RespValue value;
    std::size_t consumed{};
  };

  ParseResult parse(std::string_view input) {
    if (input.empty())
      throw std::runtime_error{"empty input"};

    switch (input[0]) {
    case '+':
      return parse_simple_str(input);
    case '-':
      return parse_err(input);
    case ':':
      return parse_int(input);
    case '$':
      return parse_bulk_str(input);
    case '*':
      return parse_arr(input);
    default:
      throw std::runtime_error{std::string("unknown type byte: ") + input[0]};
    }
  }

private:
  // find carriage return (CRLF)
  // and return the line content
  // + total bytes
  struct Line {
    std::string_view content;
    std::size_t total_len{};
  };

  Line read_line(std::string_view input) {
    auto pos = input.find("\r\n");

    if (pos == std::string_view::npos)
      throw std::runtime_error{"incomplete data: no CRLF found"};

    return {.content = input.substr(0, pos), .total_len = pos + 2};
  }

  int64_t parse_num(std::string_view s) {
    int64_t res{};
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), res);

    if (ec != std::errc{}) {
      throw std::runtime_error{std::string{"invalid number: "} +
                               std::string{s}};
    }

    return res;
  }

  // read +OK\r\n
  ParseResult parse_simple_str(std::string_view input) {
    auto [content, len] = read_line(input);

    return {.value = RespValue::ss(std::string{content}), .consumed = 1 + len};
  }

  // read -ERR message\r\n
  ParseResult parse_err(std::string_view input) {
    auto [content, len] = read_line(input.substr(1));

    return {.value = RespValue::error(std::string{content}),
            .consumed = 1 + len};
  }

  // read :1000\r\n
  ParseResult parse_int(std::string_view input) {
    auto [content, len] = read_line(input.substr(1));
    return {.value = RespValue::integer(parse_num(content)),
            .consumed = 1 + len};
  }

  // read $6\r\nfoobar\r\n  or  $-1\r\n (null)
  ParseResult parse_bulk_str(std::string_view input) {
    auto [len_str, header_len] = read_line(input.substr(1)); // skip '$'
    auto str_len = parse_num(len_str);

    // null bulk string
    if (str_len == -1) {
      return {.value = RespValue::null(), .consumed = 1 + header_len};
    }

    std::size_t total = 1 + header_len + str_len + 2;

    if (input.size() < total)
      throw std::runtime_error{"incomplete bulk string data"};

    auto data = input.substr(1 + header_len, str_len);

    return {.value = RespValue::bulk(std::string{data}), .consumed = total};
  }

  // read *2\r\n$3\r\nfoo\r\n$3\r\nbar\r\n
  ParseResult parse_arr(std::string_view input) {
    auto [count_str, header_len] = read_line(input.substr(1)); // skip '*'
    auto count = parse_num(count_str);

    if (count == -1) {
      return {.value = RespValue::null(), .consumed = 1 + header_len};
    }

    RespArray elements{};
    elements.reserve(count);

    std::size_t offset{1 + header_len};

    for (auto i = 0LL; i < count; ++i) {
      if (offset >= input.size()) {
        throw std::runtime_error{"incomplete array data"};
      }

      auto [val, consumed] = parse(input.substr(offset));
      elements.push_back(std::move(val));
      offset += consumed;
    }

    return {.value = RespValue::array(std::move(elements)), .consumed = offset};
  }
};

class RespSerializer {
public:
  std::string serialize(const RespValue &val) {
    switch (val.type) {
    case RespValue::Type::Null:
      return "$-1\r\n";

    case RespValue::Type::SimpleString:
      return '+' + std::get<std::string>(val.value) + "\r\n";

    case RespValue::Type::Error:
      return '-' + std::get<std::string>(val.value) + "\r\n";

    case RespValue::Type::Integer:
      return ':' + std::to_string(std::get<int64_t>(val.value)) + "\r\n";

    case RespValue::Type::BulkString: {
      const auto &s = std::get<std::string>(val.value);
      return '$' + std::to_string(s.size()) + "\r\n" + s + "\r\n";
    }

    case RespValue::Type::Array: {
      const auto &arr = std::get<RespArray>(val.value);
      std::string out{'*'};
      out += std::to_string(arr.size()) + "\r\n";
      for (const auto &elem : arr) {
        out += serialize(elem);
      }
      return out;
    }
    }
    std::unreachable();
  }
};