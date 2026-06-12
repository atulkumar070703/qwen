# Angel One SmartAPI market-data capture design

This document describes the low-latency capture path added in `cpp/` for saving Angel One SmartAPI WebSocket 2.0 Snap Quote market data.

## What the SmartAPI docs imply

Angel One SmartAPI provides market data through WebSocket 2.0 at:

```text
wss://smartapisocket.angelone.in/smart-stream
```

The stream uses JSON control messages for subscribe/unsubscribe and binary market-data messages for the feed. Browser-style clients can also connect with `clientCode`, `feedToken`, and `apiKey` query parameters. The client must send a `ping` heartbeat every 30 seconds and should expect `pong`.

For market data, the important modes are:

| Mode | Meaning | Packet size |
| --- | --- | --- |
| 1 | LTP | 51 bytes |
| 2 | Quote | 123 bytes |
| 3 | Snap Quote | 379 bytes |

The C++ parser in this repo targets **mode 3 Snap Quote** because it includes LTP, OHLC/volume fields, total buy/sell quantity, best-five buy/sell depth, circuit limits, and 52-week high/low. Angel documents the wire format as little-endian binary.

Important operational limits from the docs:

- Up to 3 concurrent WebSocket connections per client code.
- Up to 1000 token subscriptions per WebSocket session.
- Subscribing the same token in LTP, Quote, and Snap Quote counts as 3 subscriptions.
- Prefer subscribing one mode per token to avoid duplicate data and wasted quota.

## Storage plan for every trading day

Create one immutable binary file per trading day and source:

```text
data/YYYY/MM/DD/angelone_snapquote_NSE_YYYYMMDD.hftbin
```

Recommended runtime behavior:

1. Start before market open after login/session generation.
2. Subscribe tokens in Snap Quote mode.
3. On every binary WebSocket message:
   - take a local receive timestamp using `clock_gettime(CLOCK_REALTIME)` in nanoseconds;
   - take a monotonic timestamp using `CLOCK_MONOTONIC_RAW` for latency measurements;
   - parse only the minimum header fields needed for indexing and validation;
   - append a compact binary record header plus the raw 379-byte Angel payload;
   - publish the same raw payload to a shared-memory SPSC ring for the strategy process.
4. Stop accepting new data at **15:31 IST**.
5. Flush userspace buffers and call `fdatasync()` so the file is durable on disk.
6. Move or upload the closed file to cold storage after checksum creation.

The file format intentionally stores the raw broker frame. This avoids lossy transformations and keeps future parser changes possible. Each record is:

```text
RecordHeader, 36 bytes
Raw SmartAPI Snap Quote payload, 379 bytes
```

A file starts with a 24-byte `StoreFileHeader`. That makes one Snap Quote record approximately 415 bytes before filesystem compression. If you later want smaller files, compress closed daily files with `zstd -T0 --long`, not in the hot path.

## IPC plan

The fastest simple IPC added here is an mmap-backed single-producer/single-consumer ring:

```text
capture process -> /dev/shm/angel.snapquote.ring -> strategy process
```

Properties:

- Fixed-size slots, no allocation on the hot path.
- Atomic head/tail counters on separate cache lines.
- Producer never blocks on the consumer.
- If the strategy is too slow, `try_push` returns false; the capture process still writes to disk first, so the disk archive remains complete.

For your stated priority of **no data loss**, disk append is the source of truth. IPC is for live strategy speed; if IPC drops, the strategy can recover from the binary file or a replay process.

## Timestamp notes

The broker exchange timestamp is epoch milliseconds. The local capture timestamp is nanoseconds, but true nanosecond accuracy depends on host clock quality. For production, run chrony/PTP, use a NIC with hardware timestamping if available, pin the receive thread, and monitor clock offset. Software timestamps are nanosecond-resolution values, not guaranteed nanosecond-accurate measurements.

## Build and run the C++ recorder scaffold

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The included `hft_capture_tool` reads concatenated 379-byte Snap Quote frames from a file. In production, wire the same append call into the WebSocket binary receive callback.

```bash
./build/hft_capture_tool \
  --input raw_snapquote_frames.bin \
  --output data/2026/06/12/angelone_snapquote_NSE_20260612.hftbin \
  --ring /dev/shm/angel.snapquote.ring
```

By default it stops at 15:31 IST and flushes data to disk. For replay/testing after market hours, pass `--no-stop-at-1531`.
