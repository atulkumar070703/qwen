#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <algorithm>
#include <stdexcept>
#include <type_traits>
#include <string>
#include <string_view>

namespace hft_capture::angel {

constexpr std::size_t kTokenBytes = 25;
constexpr std::size_t kLtpPacketBytes = 51;
constexpr std::size_t kQuotePacketBytes = 123;
constexpr std::size_t kSnapQuotePacketBytes = 379;
constexpr std::size_t kDepthLevelBytes = 20;
constexpr int kSnapQuoteMode = 3;

struct DepthLevel {
  std::uint16_t side_flag{};      // 1 buy, 0 sell per Angel docs.
  std::int64_t quantity{};
  std::int64_t price{};           // Angel sends scaled integer price.
  std::uint16_t orders{};
};

struct SnapQuote {
  std::uint8_t subscription_mode{};
  std::uint8_t exchange_type{};
  std::array<char, kTokenBytes> token{};
  std::int64_t sequence_number{};
  std::int64_t exchange_timestamp_ms{};
  std::int64_t last_traded_price{};
  std::int64_t last_traded_quantity{};
  std::int64_t average_traded_price{};
  std::int64_t volume_trade_for_the_day{};
  std::int64_t total_buy_quantity{};
  std::int64_t total_sell_quantity{};
  std::int64_t open_price{};
  std::int64_t high_price{};
  std::int64_t low_price{};
  std::int64_t close_price{};
  std::int64_t last_traded_timestamp{};
  std::int64_t open_interest{};
  std::int64_t open_interest_change_percentage{};
  std::array<DepthLevel, 10> best_five{};  // First 5 buy, next 5 sell.
  std::int64_t upper_circuit_limit{};
  std::int64_t lower_circuit_limit{};
  std::int64_t fifty_two_week_high{};
  std::int64_t fifty_two_week_low{};

  [[nodiscard]] std::string token_string() const {
    const auto end = static_cast<std::size_t>(
        std::find(token.begin(), token.end(), '\0') - token.begin());
    return std::string(token.data(), end);
  }
};

namespace detail {

template <class T>
[[nodiscard]] T read_le(std::span<const std::byte> bytes, std::size_t offset) {
  static_assert(std::is_integral_v<T> || std::is_enum_v<T>);
  if (offset + sizeof(T) > bytes.size()) {
    throw std::out_of_range("SmartAPI packet is shorter than the requested field");
  }
  T value{};
  std::memcpy(&value, bytes.data() + offset, sizeof(T));
  if constexpr (std::endian::native == std::endian::big) {
    if constexpr (sizeof(T) == 2) value = static_cast<T>(__builtin_bswap16(static_cast<std::uint16_t>(value)));
    if constexpr (sizeof(T) == 4) value = static_cast<T>(__builtin_bswap32(static_cast<std::uint32_t>(value)));
    if constexpr (sizeof(T) == 8) value = static_cast<T>(__builtin_bswap64(static_cast<std::uint64_t>(value)));
  }
  return value;
}

inline DepthLevel read_depth(std::span<const std::byte> bytes, std::size_t offset) {
  return DepthLevel{
      .side_flag = read_le<std::uint16_t>(bytes, offset + 0),
      .quantity = read_le<std::int64_t>(bytes, offset + 2),
      .price = read_le<std::int64_t>(bytes, offset + 10),
      .orders = read_le<std::uint16_t>(bytes, offset + 18),
  };
}

}  // namespace detail

[[nodiscard]] inline SnapQuote parse_snapquote(std::span<const std::byte> bytes) {
  if (bytes.size() < kSnapQuotePacketBytes) {
    throw std::invalid_argument("SmartAPI Snap Quote packet must be at least 379 bytes");
  }
  SnapQuote quote{};
  quote.subscription_mode = detail::read_le<std::uint8_t>(bytes, 0);
  quote.exchange_type = detail::read_le<std::uint8_t>(bytes, 1);
  if (quote.subscription_mode != kSnapQuoteMode) {
    throw std::invalid_argument("packet is not SmartAPI Snap Quote mode 3");
  }
  std::memcpy(quote.token.data(), bytes.data() + 2, kTokenBytes);
  quote.sequence_number = detail::read_le<std::int64_t>(bytes, 27);
  quote.exchange_timestamp_ms = detail::read_le<std::int64_t>(bytes, 35);
  quote.last_traded_price = detail::read_le<std::int64_t>(bytes, 43);
  quote.last_traded_quantity = detail::read_le<std::int64_t>(bytes, 51);
  quote.average_traded_price = detail::read_le<std::int64_t>(bytes, 59);
  quote.volume_trade_for_the_day = detail::read_le<std::int64_t>(bytes, 67);
  quote.total_buy_quantity = detail::read_le<std::int64_t>(bytes, 75);
  quote.total_sell_quantity = detail::read_le<std::int64_t>(bytes, 83);
  quote.open_price = detail::read_le<std::int64_t>(bytes, 91);
  quote.high_price = detail::read_le<std::int64_t>(bytes, 99);
  quote.low_price = detail::read_le<std::int64_t>(bytes, 107);
  quote.close_price = detail::read_le<std::int64_t>(bytes, 115);
  quote.last_traded_timestamp = detail::read_le<std::int64_t>(bytes, 123);
  quote.open_interest = detail::read_le<std::int64_t>(bytes, 131);
  quote.open_interest_change_percentage = detail::read_le<std::int64_t>(bytes, 139);
  for (std::size_t i = 0; i < quote.best_five.size(); ++i) {
    quote.best_five[i] = detail::read_depth(bytes, 147 + i * kDepthLevelBytes);
  }
  quote.upper_circuit_limit = detail::read_le<std::int64_t>(bytes, 347);
  quote.lower_circuit_limit = detail::read_le<std::int64_t>(bytes, 355);
  quote.fifty_two_week_high = detail::read_le<std::int64_t>(bytes, 363);
  quote.fifty_two_week_low = detail::read_le<std::int64_t>(bytes, 371);
  return quote;
}

}  // namespace hft_capture::angel
