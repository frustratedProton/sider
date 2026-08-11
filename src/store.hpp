#pragma once

#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

struct Entry {
  std::string value;
  std::optional<TimePoint> expires_at{};
};

class Store {
public:
  void set(const std::string &key, std::string value,
           std::optional<std::chrono::milliseconds> ttl = std::nullopt) {
    std::unique_lock lock{m_mutex};
    std::optional<TimePoint> expires_at{};
    if (ttl)
      expires_at = Clock::now() + *ttl;
    m_data[key] = Entry{.value = std::move(value), .expires_at = expires_at};
  }

  std::optional<std::string> get(const std::string &key) {
    std::shared_lock lock{m_mutex};
    auto it = m_data.find(key);

    if (it == m_data.end())
      return std::nullopt;

    if (is_expired(it->second)) {
      m_data.erase(it);
      return std::nullopt;
    }

    return it->second.value;
  }

  bool del(const std::string &key) {
    std::unique_lock lock{m_mutex};
    return m_data.erase(key) > 0;
  }

  bool exists(const std::string &key) {
    std::shared_lock lock{m_mutex};
    auto it = m_data.find(key);
    if (it == m_data.end())
      return false;

    if (is_expired(it->second)) {
      m_data.erase(it);
      return false;
    }

    return true;
  }

  int64_t ttl_ms(const std::string &key) {
    std::shared_lock lock{m_mutex};
    auto it = m_data.find(key);

    if (it == m_data.end())
      return -2;
    if (is_expired(it->second))
      return -2;
    if (!it->second.expires_at)
      return -1;

    auto rem = std::chrono::duration_cast<std::chrono::milliseconds>(
        *it->second.expires_at - Clock::now());

    return rem.count();
  }

  bool persist(const std::string &key) {
    std::unique_lock lock{m_mutex};
    auto it = m_data.find(key);

    if (it == m_data.end())
      return false;
    if (!it->second.expires_at)
      return false;
    it->second.expires_at = std::nullopt;
    return true;
  }

  void purge_expired() {
    std::unique_lock lock{m_mutex};
    for (auto it = m_data.begin(); it != m_data.end();) {
      if (is_expired(it->second))
        it = m_data.erase(it);
      else
        ++it;
    }
  }

private:
  std::unordered_map<std::string, Entry> m_data{};
  std::shared_mutex m_mutex{};

  bool is_expired(const Entry &e) {
    if (!e.expires_at)
      return false;
    return Clock::now() >= *e.expires_at;
  }
};