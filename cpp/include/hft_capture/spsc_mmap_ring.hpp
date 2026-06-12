#pragma once

#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace hft_capture {

constexpr std::uint32_t kRingMagic = 0x31475053;  // SPG1.
constexpr std::size_t kCacheLine = 64;
constexpr std::size_t kRingPayloadBytes = 512;

struct alignas(kCacheLine) RingHeader {
  std::uint32_t magic{kRingMagic};
  std::uint32_t slot_count{};
  std::uint32_t slot_bytes{kRingPayloadBytes};
  std::uint32_t reserved{};
  alignas(kCacheLine) std::atomic<std::uint64_t> head{0};
  alignas(kCacheLine) std::atomic<std::uint64_t> tail{0};
};

struct alignas(kCacheLine) RingSlot {
  std::uint32_t bytes{};
  std::uint32_t dropped_before{};
  std::byte payload[kRingPayloadBytes]{};
};

class MmapSpscRing {
 public:
  MmapSpscRing(const std::filesystem::path& path, std::uint32_t slots, bool create)
      : slots_(slots), bytes_(sizeof(RingHeader) + sizeof(RingSlot) * slots) {
    if (slots == 0 || (slots & (slots - 1)) != 0) {
      throw std::invalid_argument("ring slots must be a power of two");
    }
    const int flags = create ? (O_CREAT | O_RDWR | O_CLOEXEC) : (O_RDWR | O_CLOEXEC);
    fd_ = ::open(path.c_str(), flags, 0644);
    if (fd_ < 0) throw std::system_error(errno, std::generic_category(), "open ring " + path.string());
    if (create && ::ftruncate(fd_, static_cast<off_t>(bytes_)) != 0) {
      throw std::system_error(errno, std::generic_category(), "ftruncate ring " + path.string());
    }
    memory_ = ::mmap(nullptr, bytes_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (memory_ == MAP_FAILED) throw std::system_error(errno, std::generic_category(), "mmap ring " + path.string());
    header_ = static_cast<RingHeader*>(memory_);
    slots_mem_ = reinterpret_cast<RingSlot*>(static_cast<std::byte*>(memory_) + sizeof(RingHeader));
    if (create) {
      new (header_) RingHeader{};
      header_->slot_count = slots;
      for (std::uint32_t i = 0; i < slots; ++i) new (&slots_mem_[i]) RingSlot{};
    } else if (header_->magic != kRingMagic || header_->slot_count != slots) {
      throw std::runtime_error("ring file header does not match expected layout");
    }
  }

  MmapSpscRing(const MmapSpscRing&) = delete;
  MmapSpscRing& operator=(const MmapSpscRing&) = delete;

  ~MmapSpscRing() {
    if (memory_ && memory_ != MAP_FAILED) ::munmap(memory_, bytes_);
    if (fd_ >= 0) ::close(fd_);
  }

  [[nodiscard]] bool try_push(const void* payload, std::uint32_t bytes) noexcept {
    if (bytes > kRingPayloadBytes) return false;
    const std::uint64_t head = header_->head.load(std::memory_order_relaxed);
    const std::uint64_t tail = header_->tail.load(std::memory_order_acquire);
    if (head - tail >= slots_) return false;
    RingSlot& slot = slots_mem_[head & (slots_ - 1)];
    std::memcpy(slot.payload, payload, bytes);
    slot.bytes = bytes;
    header_->head.store(head + 1, std::memory_order_release);
    return true;
  }

  [[nodiscard]] bool try_pop(void* out, std::uint32_t& bytes) noexcept {
    const std::uint64_t tail = header_->tail.load(std::memory_order_relaxed);
    const std::uint64_t head = header_->head.load(std::memory_order_acquire);
    if (tail == head) return false;
    RingSlot& slot = slots_mem_[tail & (slots_ - 1)];
    bytes = slot.bytes;
    std::memcpy(out, slot.payload, bytes);
    header_->tail.store(tail + 1, std::memory_order_release);
    return true;
  }

 private:
  int fd_{-1};
  void* memory_{nullptr};
  RingHeader* header_{nullptr};
  RingSlot* slots_mem_{nullptr};
  std::uint32_t slots_{};
  std::size_t bytes_{};
};

}  // namespace hft_capture
