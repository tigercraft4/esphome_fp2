#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace esphome {
namespace aqara_fp2 {

static constexpr size_t LOCATION_TARGET_BYTES = 14;

struct LocationTargets {
  uint8_t count{0};
  std::vector<uint8_t> blob;
};

// Decode the complete 0x0117 payload:
// [SubID 2][BLOB2 type][declared length 2][count 1][target 14 * count].
// The outer UART CRC does not guarantee that the inner BLOB declaration and
// target count agree, so reject inconsistent payloads instead of publishing a
// count header that does not match the target records carried after it.
inline bool parse_location_targets(const std::vector<uint8_t> &payload, LocationTargets *out) {
  if (out == nullptr || payload.size() < 6 || payload[2] != 0x06) {
    return false;
  }

  const size_t declared_length = (static_cast<size_t>(payload[3]) << 8) | payload[4];
  if (declared_length != payload.size() - 5) {
    return false;
  }

  const uint8_t count = payload[5];
  const size_t expected_length = 1 + static_cast<size_t>(count) * LOCATION_TARGET_BYTES;
  if (declared_length != expected_length) {
    return false;
  }

  out->count = count;
  out->blob.assign(payload.begin() + 5, payload.end());
  return true;
}

inline bool target_count_changed(uint8_t count, uint8_t *last_count) {
  if (last_count == nullptr || *last_count == count) {
    return false;
  }
  *last_count = count;
  return true;
}

}  // namespace aqara_fp2
}  // namespace esphome
