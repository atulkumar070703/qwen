#include <array>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include "hft_capture/angel_snapquote.hpp"
#include "hft_capture/binary_store.hpp"
#include "hft_capture/spsc_mmap_ring.hpp"
#include "hft_capture/time_utils.hpp"

namespace {

volatile std::sig_atomic_t g_stop = 0;

void handle_signal(int) { g_stop = 1; }

struct Args {
  std::filesystem::path input;
  std::filesystem::path output{"marketdata.hftbin"};
  std::filesystem::path ring;
  std::uint32_t ring_slots{65536};
  bool stop_at_1531{true};
  bool no_fsync{false};
};

void usage(const char* argv0) {
  std::cerr << "usage: " << argv0
            << " --input raw_snapquote_frames.bin --output YYYYMMDD.smartapi.hftbin"
               " [--ring /dev/shm/angel.snapquote.ring] [--ring-slots 65536]"
               " [--no-stop-at-1531] [--no-fsync]\n\n"
            << "Input format for this tool is concatenated 379-byte SmartAPI Snap Quote frames.\n"
            << "In production, feed the same append path from your WebSocket receive callback.\n";
}

Args parse_args(int argc, char** argv) {
  Args args;
  for (int i = 1; i < argc; ++i) {
    const std::string key = argv[i];
    auto need_value = [&](const char* name) -> const char* {
      if (i + 1 >= argc) throw std::invalid_argument(std::string("missing value for ") + name);
      return argv[++i];
    };
    if (key == "--input") args.input = need_value("--input");
    else if (key == "--output") args.output = need_value("--output");
    else if (key == "--ring") args.ring = need_value("--ring");
    else if (key == "--ring-slots") args.ring_slots = static_cast<std::uint32_t>(std::stoul(need_value("--ring-slots")));
    else if (key == "--no-stop-at-1531") args.stop_at_1531 = false;
    else if (key == "--no-fsync") args.no_fsync = true;
    else if (key == "--help" || key == "-h") {
      usage(argv[0]);
      std::exit(0);
    } else {
      throw std::invalid_argument("unknown argument: " + key);
    }
  }
  if (args.input.empty()) throw std::invalid_argument("--input is required");
  return args;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    const Args args = parse_args(argc, argv);
    std::ifstream in(args.input, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open input " + args.input.string());

    hft_capture::BinaryStore store(args.output);
    std::unique_ptr<hft_capture::MmapSpscRing> ring;
    if (!args.ring.empty()) {
      ring = std::make_unique<hft_capture::MmapSpscRing>(args.ring, args.ring_slots, true);
    }

    hft_capture::IstWallClockStop stop_time{};
    std::array<std::byte, hft_capture::angel::kSnapQuotePacketBytes> frame{};
    std::uint64_t records = 0;
    std::uint64_t ring_drops = 0;

    while (!g_stop) {
      if (args.stop_at_1531 && stop_time.reached()) break;
      in.read(reinterpret_cast<char*>(frame.data()), static_cast<std::streamsize>(frame.size()));
      if (in.gcount() == 0) break;
      if (in.gcount() != static_cast<std::streamsize>(frame.size())) {
        throw std::runtime_error("partial trailing frame: input must contain whole 379-byte frames");
      }

      const auto capture_realtime = hft_capture::realtime_ns();
      const auto capture_monotonic = hft_capture::monotonic_ns();
      const auto snap = hft_capture::angel::parse_snapquote(std::span<const std::byte>(frame.data(), frame.size()));
      hft_capture::RecordHeader header{
          .capture_realtime_ns = capture_realtime,
          .capture_monotonic_ns = capture_monotonic,
          .exchange_timestamp_ms = static_cast<std::uint64_t>(snap.exchange_timestamp_ms),
          .sequence_number = snap.sequence_number,
          .payload_bytes = static_cast<std::uint16_t>(frame.size()),
          .mode = snap.subscription_mode,
          .exchange_type = snap.exchange_type,
      };
      store.append(header, frame.data(), frame.size());
      ++records;

      if (ring && !ring->try_push(frame.data(), static_cast<std::uint32_t>(frame.size()))) {
        ++ring_drops;
      }
    }

    if (!args.no_fsync) store.flush_to_disk();
    std::cerr << "saved_records=" << records << " ring_drops=" << ring_drops << " output=" << args.output << '\n';
    return 0;
  } catch (const std::exception& exc) {
    std::cerr << "error: " << exc.what() << '\n';
    return 1;
  }
}
