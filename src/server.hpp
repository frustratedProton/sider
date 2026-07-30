#pragma once

#include "resp.hpp"

#include <array>
#include <cctype>
#include <netinet/in.h>
#include <print>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

class TcpServer {
public:
  explicit TcpServer(uint16_t port) : m_port{port} {}

  ~TcpServer() {
    if (m_server_fd >= 0)
      close(m_server_fd);
  }

  void start() {
    m_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_server_fd < 0)
      throw std::runtime_error("failed to create socket");

    int opt{1};
    setsockopt(m_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{.sin_family = AF_INET,
                     .sin_port = htons(m_port),
                     .sin_addr = {.s_addr = INADDR_ANY},
                     .sin_zero = {}};

    if (bind(m_server_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) <
        0)
      throw std::runtime_error{"failed to bind to port " +
                               std::to_string((m_port))};

    if (listen(m_server_fd, 128) < 0)
      throw std::runtime_error{"failed to listen"};

    std::println("listening on port {}", m_port);

    while (true) {
      sockaddr_in client_addr{};
      socklen_t client_len{sizeof(client_addr)};

      int client_fd = accept(
          m_server_fd, reinterpret_cast<sockaddr *>(&client_addr), &client_len);

      if (client_fd < 0) {
        std::println("failed to accept, skipping");
        continue;
      }

      std::println("client connected fd={}", client_fd);

      std::thread([this, client_fd] { handle_client(client_fd); }).detach();
    }
  }

private:
  uint16_t m_port{};
  int m_server_fd{-1};

  RespParser m_parser{};
  RespSerializer m_serializer{};

  void handle_client(int client_fd) {
    std::string buffer{};

    while (true) {
      std::array<char, 4096> chunk{};
      ssize_t bytes_read = read(client_fd, chunk.data(), chunk.size());

      if (bytes_read <= 0) {
        std::println("client disconencted fd={}", client_fd);
        break;
      }
      buffer.append(chunk.data(), bytes_read);

      while (!buffer.empty()) {
        try {
          auto [val, consumed] = m_parser.parse(buffer);
          buffer.erase(0, consumed);
          auto resp = handle_cmd(val);
          auto serialized = m_serializer.serialize(resp);
          write(client_fd, serialized.data(), serialized.size());
        } catch (const std::runtime_error &e) {
          std::string err{e.what()};
          if (err.contains("incomplete"))
            break;

          auto error_resp =
              m_serializer.serialize(RespValue::error("ERR" + err));

          write(client_fd, error_resp.data(), error_resp.size());
          buffer.clear();
          break;
        }
      }
    }

    close(client_fd);
  }

  RespValue handle_cmd(const RespValue &rv) {
    if (rv.type != RespValue::Type::Array)
      return RespValue::error("ERR expected array");

    const auto &args = std::get<RespArray>(rv.value);
    if (args.empty())
      return RespValue::error("ERR empty command");

    std::string cmd = std::get<std::string>(args[0].value);

    for (char &ch : cmd)
      ch = toupper(ch);

    if (cmd == "PING") {
      if (args.size() > 1)
        return RespValue::bulk(std::get<std::string>(args[1].value));
      return RespValue::ss("PONG");
    }

    if (cmd == "ECHO") {
      if (args.size() != 2)
        return RespValue::error("ERR wrong number of arguments for ECHO");
      return RespValue::bulk(std::get<std::string>(args[1].value));
    }
    return RespValue::error("ERR unknown command '" + cmd + "'");
  }
};