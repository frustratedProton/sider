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
    if (args.size() != 3)
      return RespValue::error("ERR wrong number of arguments for SET");
    m_store.set(arg(args, 1), arg(args, 2));
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
      if (m_store.exists(arg(args, 1)))
        ++count;
    return RespValue::integer(count);
  }
};