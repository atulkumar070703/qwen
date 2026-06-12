#pragma once

#include <chrono>
#include <cstdint>
#include <ctime>
#include <string>
#include <string_view>

namespace hft_capture {

inline std::uint64_t realtime_ns() noexcept {
  timespec ts{};
  clock_gettime(CLOCK_REALTIME, &ts);
  return static_cast<std::uint64_t>(ts.tv_sec) * 1'000'000'000ull +
         static_cast<std::uint64_t>(ts.tv_nsec);
}

inline std::uint64_t monotonic_ns() noexcept {
  timespec ts{};
  clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
  return static_cast<std::uint64_t>(ts.tv_sec) * 1'000'000'000ull +
         static_cast<std::uint64_t>(ts.tv_nsec);
}

struct IstWallClockStop {
  int hour{15};
  int minute{31};
  int second{0};

  [[nodiscard]] bool reached(std::time_t now_utc = std::time(nullptr)) const {
    constexpr long ist_offset_seconds = 5 * 3600 + 30 * 60;
    const std::time_t ist_epoch = now_utc + ist_offset_seconds;
    std::tm ist{};
    gmtime_r(&ist_epoch, &ist);
    if (ist.tm_hour > hour) return true;
    if (ist.tm_hour < hour) return false;
    if (ist.tm_min > minute) return true;
    if (ist.tm_min < minute) return false;
    return ist.tm_sec >= second;
  }
};

}  // namespace hft_capture
