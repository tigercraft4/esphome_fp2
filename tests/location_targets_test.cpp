#include <cassert>
#include <cstdint>
#include <vector>

#include "components/aqara_fp2/location_targets.h"

using esphome::aqara_fp2::LocationTargets;
using esphome::aqara_fp2::parse_location_targets;
using esphome::aqara_fp2::target_count_changed;

static std::vector<uint8_t> wrap_blob(const std::vector<uint8_t> &blob) {
  std::vector<uint8_t> payload{0x01, 0x17, 0x06,
                               static_cast<uint8_t>(blob.size() >> 8),
                               static_cast<uint8_t>(blob.size())};
  payload.insert(payload.end(), blob.begin(), blob.end());
  return payload;
}

int main() {
  // Verified capture: count=2 followed by two complete 14-byte target records.
  const std::vector<uint8_t> two_target_blob{
      0x02,
      0x00, 0xfe, 0xf2, 0x00, 0x37, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00, 0x02, 0x00,
      0x02, 0xfe, 0xb6, 0x00, 0x7f, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x02, 0x01,
  };
  LocationTargets parsed;
  assert(parse_location_targets(wrap_blob(two_target_blob), &parsed));
  assert(parsed.count == 2);
  assert(parsed.blob == two_target_blob);

  // A CRC-valid outer frame can still contain a semantically truncated BLOB.
  // Never publish a header saying two targets while carrying only one record.
  std::vector<uint8_t> truncated(two_target_blob.begin(), two_target_blob.begin() + 15);
  assert(!parse_location_targets(wrap_blob(truncated), &parsed));

  // Reject an inconsistent BLOB length declaration as well.
  auto bad_declared_length = wrap_blob(two_target_blob);
  bad_declared_length[4]--;
  assert(!parse_location_targets(bad_declared_length, &parsed));

  // Count publication is edge-driven: first sample and every change publish;
  // repeated samples do not wait for or consume a one-second throttle.
  uint8_t last_count = 0xff;
  assert(target_count_changed(2, &last_count));
  assert(last_count == 2);
  assert(!target_count_changed(2, &last_count));
  assert(target_count_changed(1, &last_count));
  assert(last_count == 1);
}
