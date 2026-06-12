#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <unistd.h>

#include "hft_capture/angel_snapquote.hpp"
#include "hft_capture/binary_store.hpp"
#include "hft_capture/spsc_mmap_ring.hpp"
#include "hft_capture/time_utils.hpp"

namespace {

void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

template <class T>
void put_le(std::array<std::byte, hft_capture::angel::kSnapQuotePacketBytes>& bytes, std::size_t offset, T value) {
  std::memcpy(bytes.data() + offset, &value, sizeof(T));
}

std::array<std::byte, hft_capture::angel::kSnapQuotePacketBytes> sample_frame() {
  std::array<std::byte, hft_capture::angel::kSnapQuotePacketBytes> bytes{};
  put_le<std::uint8_t>(bytes, 0, 3);
  put_le<std::uint8_t>(bytes, 1, 1);
  const char token[] = "3045";
  std::memcpy(bytes.data() + 2, token, sizeof(token) - 1);
  put_le<std::int64_t>(bytes, 27, 42);
  put_le<std::int64_t>(bytes, 35, 1710000000123);
  put_le<std::int64_t>(bytes, 43, 12345);
  put_le<std::int64_t>(bytes, 51, 10);
  put_le<std::int64_t>(bytes, 59, 12340);
  put_le<std::int64_t>(bytes, 67, 9999);
  put_le<std::int64_t>(bytes, 75, 1000);
  put_le<std::int64_t>(bytes, 83, 2000);
  put_le<std::int64_t>(bytes, 91, 12000);
  put_le<std::int64_t>(bytes, 99, 13000);
  put_le<std::int64_t>(bytes, 107, 11900);
  put_le<std::int64_t>(bytes, 115, 12100);
  put_le<std::int64_t>(bytes, 123, 1710000000);
  put_le<std::int64_t>(bytes, 131, 123456);
  put_le<std::int64_t>(bytes, 139, 7);
  for (std::size_t i = 0; i < 10; ++i) {
    const std::size_t base = 147 + i * 20;
    put_le<std::uint16_t>(bytes, base + 0, i < 5 ? 1 : 0);
    put_le<std::int64_t>(bytes, base + 2, static_cast<std::int64_t>(100 + i));
    put_le<std::int64_t>(bytes, base + 10, static_cast<std::int64_t>(12300 + i));
    put_le<std::uint16_t>(bytes, base + 18, static_cast<std::uint16_t>(2 + i));
  }
  put_le<std::int64_t>(bytes, 347, 15000);
  put_le<std::int64_t>(bytes, 355, 10000);
  put_le<std::int64_t>(bytes, 363, 16000);
  put_le<std::int64_t>(bytes, 371, 9000);
  return bytes;
}

}  // namespace

int main() {
  const auto frame = sample_frame();
  const auto quote = hft_capture::angel::parse_snapquote(std::span<const std::byte>(frame.data(), frame.size()));
  require(quote.subscription_mode == 3, "mode parse failed");
  require(quote.exchange_type == 1, "exchange parse failed");
  require(quote.token_string() == "3045", "token parse failed");
  require(quote.sequence_number == 42, "sequence parse failed");
  require(quote.exchange_timestamp_ms == 1710000000123, "exchange timestamp parse failed");
  require(quote.last_traded_price == 12345, "ltp parse failed");
  require(quote.best_five[0].side_flag == 1, "best five buy side parse failed");
  require(quote.best_five[0].quantity == 100, "best five quantity parse failed");
  require(quote.best_five[5].side_flag == 0, "best five sell side parse failed");
  require(quote.fifty_two_week_low == 9000, "52-week low parse failed");

  const std::filesystem::path store_path = std::filesystem::temp_directory_path() / "hft_capture_test.hftbin";
  {
    hft_capture::BinaryStore store(store_path);
    hft_capture::RecordHeader header{};
    header.capture_realtime_ns = hft_capture::realtime_ns();
    header.capture_monotonic_ns = hft_capture::monotonic_ns();
    header.exchange_timestamp_ms = static_cast<std::uint64_t>(quote.exchange_timestamp_ms);
    header.sequence_number = quote.sequence_number;
    header.mode = quote.subscription_mode;
    header.exchange_type = quote.exchange_type;
    store.append(header, frame.data(), frame.size());
    store.flush_to_disk();
  }
  const auto size = std::filesystem::file_size(store_path);
  require(size == sizeof(hft_capture::StoreFileHeader) + sizeof(hft_capture::RecordHeader) + frame.size(),
          "binary store size mismatch");
  std::filesystem::remove(store_path);

  const std::filesystem::path ring_path = std::filesystem::temp_directory_path() /
                                         ("hft_capture_ring_" + std::to_string(::getpid()) + ".mmap");
  {
    hft_capture::MmapSpscRing ring(ring_path, 8, true);
    require(ring.try_push(frame.data(), static_cast<std::uint32_t>(frame.size())), "ring push failed");
    std::array<std::byte, hft_capture::angel::kSnapQuotePacketBytes> out{};
    std::uint32_t out_bytes = 0;
    require(ring.try_pop(out.data(), out_bytes), "ring pop failed");
    require(out_bytes == frame.size(), "ring byte count mismatch");
    require(std::memcmp(out.data(), frame.data(), frame.size()) == 0, "ring payload mismatch");
  }
  std::filesystem::remove(ring_path);

  hft_capture::IstWallClockStop stop{};
  require(!stop.reached(0), "15:31 IST stop calculation failed for Unix epoch");
  std::cout << "hft_capture_tests passed\n";
  return 0;
}
