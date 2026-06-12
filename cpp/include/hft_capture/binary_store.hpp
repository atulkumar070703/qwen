#pragma once

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <fcntl.h>

#include "hft_capture/time_utils.hpp"

namespace hft_capture {

constexpr std::uint32_t kStoreMagic = 0x31544648;  // HFT1 little endian.
constexpr std::uint16_t kStoreVersion = 1;
constexpr std::size_t kWriteBufferBytes = 4 * 1024 * 1024;

#pragma pack(push, 1)
struct StoreFileHeader {
  std::uint32_t magic{kStoreMagic};
  std::uint16_t version{kStoreVersion};
  std::uint16_t header_bytes{sizeof(StoreFileHeader)};
  std::uint64_t created_realtime_ns{};
  std::uint32_t record_header_bytes{0};
  std::uint32_t reserved{0};
};

struct RecordHeader {
  std::uint64_t capture_realtime_ns{};
  std::uint64_t capture_monotonic_ns{};
  std::uint64_t exchange_timestamp_ms{};
  std::int64_t sequence_number{};
  std::uint16_t payload_bytes{};
  std::uint8_t mode{};
  std::uint8_t exchange_type{};
};
#pragma pack(pop)

static_assert(sizeof(StoreFileHeader) == 24);
static_assert(sizeof(RecordHeader) == 36);

class BinaryStore {
 public:
  explicit BinaryStore(const std::filesystem::path& path) : path_(path) {
    fd_ = ::open(path.c_str(), O_CREAT | O_WRONLY | O_TRUNC | O_CLOEXEC, 0644);
    if (fd_ < 0) throw std::system_error(errno, std::generic_category(), "open " + path.string());
    buffer_ = new std::byte[kWriteBufferBytes];
    StoreFileHeader header{};
    header.created_realtime_ns = realtime_ns();
    header.record_header_bytes = sizeof(RecordHeader);
    write_bytes(&header, sizeof(header));
  }

  BinaryStore(const BinaryStore&) = delete;
  BinaryStore& operator=(const BinaryStore&) = delete;

  ~BinaryStore() {
    try {
      flush();
    } catch (...) {
    }
    if (fd_ >= 0) ::close(fd_);
    delete[] buffer_;
  }

  void append(const RecordHeader& header, const void* payload, std::size_t payload_bytes) {
    if (payload_bytes > UINT16_MAX) throw std::length_error("payload too large for compact record header");
    RecordHeader copy = header;
    copy.payload_bytes = static_cast<std::uint16_t>(payload_bytes);
    write_bytes(&copy, sizeof(copy));
    write_bytes(payload, payload_bytes);
  }

  void flush_to_disk() {
    flush();
    if (::fdatasync(fd_) != 0) {
      throw std::system_error(errno, std::generic_category(), "fdatasync " + path_.string());
    }
  }

 private:
  void write_bytes(const void* data, std::size_t bytes) {
    const auto* src = static_cast<const std::byte*>(data);
    while (bytes > 0) {
      const std::size_t room = kWriteBufferBytes - used_;
      if (room == 0) flush();
      const std::size_t chunk = std::min(bytes, kWriteBufferBytes - used_);
      std::memcpy(buffer_ + used_, src, chunk);
      used_ += chunk;
      src += chunk;
      bytes -= chunk;
    }
  }

  void flush() {
    std::size_t written = 0;
    while (written < used_) {
      const ssize_t rc = ::write(fd_, buffer_ + written, used_ - written);
      if (rc < 0) {
        if (errno == EINTR) continue;
        throw std::system_error(errno, std::generic_category(), "write " + path_.string());
      }
      written += static_cast<std::size_t>(rc);
    }
    used_ = 0;
  }

  std::filesystem::path path_;
  int fd_{-1};
  std::byte* buffer_{nullptr};
  std::size_t used_{0};
};

}  // namespace hft_capture
