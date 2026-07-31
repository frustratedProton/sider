#pragma once

#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>

class Store {
public:
  void set(const std::string &key, std::string value) {
    std::unique_lock lock{m_mutex};
    m_data[key] = std::move(value);
  }

  std::optional<std::string> get(const std::string &key) {
    std::shared_lock lock{m_mutex};
    if (auto it = m_data.find(key); it != m_data.end())
      return it->second;

    return std::nullopt;
  }

  bool del(const std::string &key) {
    std::unique_lock lock{m_mutex};
    return m_data.erase(key) > 0;
  }

  bool exists(const std::string &key) {
    std::shared_lock lock{m_mutex};
    return m_data.contains(key);
  }

private:
  std::unordered_map<std::string, std::string> m_data{};
  std::shared_mutex m_mutex{};
};