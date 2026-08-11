#pragma once

#include "resp.hpp"
#include "store.hpp"

#include <cctype>
#include <print>
#include <string>

class CommandDispatcher {
public:
  explicit CommandDispatcher(Store &store) : m_store(store) {}

  RespValue dispatch(const RespValue &req) {
    if (req.type != RespValue::Type::Array)
      return RespValue::error("ERR expected array");

    const auto args = std::get<RespArray>(req.value);
    if (args.empty())
      return RespValue::error("ERR empty command");

    std::string cmd = std::get<std::string>(args[0].value);
    for (char &c : cmd)
      c = toupper(c);

    std::println("cmd: {} ({} args)", cmd, args.size() - 1);

    if (cmd == "PING")
      return ping_cmd(args);
    if (cmd == "ECHO")
      return echo_cmd(args);
    if (cmd == "SET")
      return set_cmd(args);
    if (cmd == "GET")
      return get_cmd(args);
    if (cmd == "DEL")
      return del_cmd(args);
    if (cmd == "EXISTS")
      return exists_cmd(args);
    if (cmd == "TTL")
      return ttl_cmd(args);
    if (cmd == "PTTL")
      return pttl_cmd(args);
    if (cmd == "PERSIST")
      return persist_cmd(args);

    return RespValue::error("ERR unknown command '" + cmd + "'");
  }

private:
  Store &m_store;

  const std::string &arg(const RespArray &args, std::size_t idx) {
    return std::get<std::string>(args[idx].value);
  }

  // PING [msg]
  RespValue ping_cmd(const RespArray &args) {
    if (args.size() > 1)
      return RespValue::bulk(arg(args, 1));
    return RespValue::ss("PONG");
  }

  // ECHO msg
  RespValue echo_cmd(const RespArray &args) {
    if (args.size() != 2)
      return RespValue::error("ERR wrong number of arguments for ECHO");
    return RespValue::bulk(arg(args, 1));
  }

  // SET key value
  RespValue set_cmd(const RespArray &args) {
    if (args.size() < 3)
      return RespValue::error("ERR wrong number of arguments for SET");
    const auto &key = arg(args, 1);
    const auto &val = arg(args, 2);

    std::optional<std::chrono::milliseconds> ttl{};

    for (std::size_t i = 3; i < args.size(); i += 2) {
      if (i + 1 >= args.size())
        return RespValue::error("ERR syntax error");

      std::string opt = arg(args, i);
      for (char &ch : opt)
        ch = toupper(ch);

      int64_t n{};
      try {
        n = std::stoll(arg(args, i + 1));
      } catch (...) {
        return RespValue::error("ERR value is not an integer");
      }

      if (opt == "EX")
        ttl = std::chrono::seconds{n};
      else if (opt == "PX")
        ttl = std::chrono::milliseconds{n};
      else
        return RespValue::error("ERR syntax error");
    }

    m_store.set(key, val, ttl);
    return RespValue::ss("OK");
  }

  // GET key
  RespValue get_cmd(const RespArray &args) {
    if (args.size() != 2)
      return RespValue::error("ERR wrong number of arguments for GET");
    auto val = m_store.get(arg(args, 1));
    if (!val)
      return RespValue::null();
    return RespValue::bulk(*val);
  }

  // DEL key [key...]
  RespValue del_cmd(const RespArray &args) {
    if (args.size() < 2)
      return RespValue::error("ERR wrogn number of arguments for DEL");
    int64_t deleted{0};
    for (std::size_t i = 1; i < args.size(); ++i)
      if (m_store.del(arg(args, i)))
        ++deleted;
    return RespValue::integer(deleted);
  }

  // EXISTS key [key...]
  RespValue exists_cmd(const RespArray &args) {
    if (args.size() < 2)
      return RespValue::error("ERR wrong number of arguments for EXISTS");
    int64_t count{0};
    for (std::size_t i = 1; i < args.size(); ++i)
      if (m_store.exists(arg(args, i)))
        ++count;
    return RespValue::integer(count);
  }

  // TTL key -> seconds
  RespValue ttl_cmd(const RespArray &args) {
    if (args.size() != 2)
      return RespValue::error("ERR wrong number of arguments for TTL");
    auto ms = m_store.ttl_ms(arg(args, 1));
    if (ms < 0)
      return RespValue::integer(ms);
    return RespValue::integer(ms / 1000);
  }

  // PTTL key -> milliseconds
  RespValue pttl_cmd(const RespArray &args) {
    if (args.size() != 2)
      return RespValue::error("ERR wrong number of arguments for PTTL");
    return RespValue::integer(m_store.ttl_ms(arg(args, 1)));
  }

  // PERSIST key
  RespValue persist_cmd(const RespArray &args) {
    if (args.size() != 2)
      return RespValue::error("ERR wrong number of arguments for PERSIST");
    return RespValue::integer(m_store.persist(arg(args, 1)) ? 1 : 0);
  }
};