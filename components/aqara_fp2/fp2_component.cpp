#include "fp2_component.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <cmath>
#include <cstdint>
#include <vector>

namespace esphome {
namespace aqara_fp2 {

// CRC16-MODBUS
static uint16_t crc16(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int j = 0; j < 8; j++) {
      if ((crc & 0x0001) != 0) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

const char* FP2Component::attr_id_to_string_(AttrId attr_id) {
  switch (attr_id) {
    case AttrId::RADAR_HW_VERSION: return "radar_hw_version";
    case AttrId::RADAR_SW_VERSION: return "radar_sw_version";
    case AttrId::MOTION_DETECT: return "motion_detection";
    case AttrId::PRESENCE_DETECT: return "presence_detection";
    case AttrId::MONITOR_MODE: return "monitor_mode";
    case AttrId::CLOSING_SETTING: return "closing_setting";
    case AttrId::EDGE_MAP: return "edge_label";
    case AttrId::ENTRY_EXIT_MAP: return "import_export_label";
    case AttrId::INTERFERENCE_MAP: return "interference_source";
    case AttrId::PRESENCE_DETECT_SENSITIVITY: return "presence_detection_sensitivity";
    case AttrId::LOCATION_REPORT_ENABLE: return "location_report_enable";
    case AttrId::RESET_ABSENT_STATUS: return "reset_absent_status";
    case AttrId::ZONE_MAP: return "zone_detect_setting";
    case AttrId::DETECT_ZONE_MOTION: return "detect_zone_motion";
    case AttrId::WORK_MODE: return "work_mode";
    case AttrId::LOCATION_TRACKING_DATA: return "location_track_data";
    case AttrId::ANGLE_SENSOR_DATA: return "angle_sensor_data";
    case AttrId::FALL_DETECTION_RESULT: return "fall_detection_result";
    case AttrId::LEFT_RIGHT_REVERSE: return "left_right_reverse";
    case AttrId::FALL_SENSITIVITY: return "fall_detection_sensitivity";
    case AttrId::RADAR_INTERFERENCE_AUTO_SETTING: return "radar_interference_auto_setting";
    case AttrId::OTA_SET_FLAG: return "ota_set_flag";
    case AttrId::TEMPERATURE: return "temperature";
    case AttrId::FALL_OVERTIME_REPORT_PERIOD: return "fall_overtime_report_period";
    case AttrId::FALL_OVERTIME_DETECTION: return "fall_overtime_detection";
    case AttrId::THERMO_EN: return "thermodynamic_chart_enable";
    case AttrId::INTERFERENCE_AUTO_ENABLE: return "interference_auto_enable";
    case AttrId::THERMO_DATA: return "thermodynamic_chart_data";
    case AttrId::ZONE_PRESENCE: return "detect_zone_presence";
    case AttrId::DEVICE_DIRECTION: return "device_direction";
    case AttrId::EDGE_AUTO_SETTING: return "edge_auto_setting";
    case AttrId::EDGE_AUTO_ENABLE: return "edge_auto_enable";
    case AttrId::ZONE_SENSITIVITY: return "detect_zone_sensitivity";
    case AttrId::DETECT_ZONE_TYPE: return "detect_zone_type";
    case AttrId::ZONE_CLOSE_AWAY_ENABLE: return "radar_detect_zone_close_away_enable";
    case AttrId::TARGET_POSTURE: return "target_posture";
    case AttrId::PEOPLE_COUNTING: return "people_counting";
    case AttrId::SLEEP_REPORT_ENABLE: return "sleep_report_enable";
    case AttrId::POSTURE_REPORT_ENABLE: return "posture_report_enable";
    case AttrId::PEOPLE_COUNT_REPORT_ENABLE: return "people_counting_report_enable";
    case AttrId::SLEEP_DATA: return "sleep_data";
    case AttrId::DELETE_FALSE_TARGET: return "delete_false_target";
    case AttrId::SLEEP_STATE: return "sleep_state";
    case AttrId::PEOPLE_NUMBER_ENABLE: return "people_number_enable";
    case AttrId::TARGET_TYPE_ENABLE: return "target_type_enable";
    case AttrId::REALTIME_PEOPLE_NUMBER: return "realtime_people_number";
    case AttrId::ONTIME_PEOPLE_NUMBER: return "ontime_people_number";
    case AttrId::REALTIME_PEOPLE_COUNTING: return "realtime_people_counting";
    case AttrId::SLEEP_PRESENCE: return "sleep_presence";
    case AttrId::SLEEP_MOUNT_POSITION: return "sleep_zone_mount_position";
    case AttrId::SLEEP_ZONE_SIZE: return "sleep_zone_size";
    case AttrId::WALL_CORNER_POS: return "wall_corner_mount_position";
    case AttrId::SLEEP_INOUT_STATE: return "sleep_inout_state";
    case AttrId::DWELL_TIME_ENABLE: return "dwell_time_enable";
    case AttrId::WALK_DISTANCE_ENABLE: return "walking_distance_enable";
    case AttrId::WALK_DISTANCE_ALL: return "walking_distance_all";
    case AttrId::SLEEP_EVENT: return "sleep_event";
    case AttrId::SLEEP_EVENT_DESCRIPTOR: return "sleep_event_descriptor";
    case AttrId::SLEEP_BED_HEIGHT: return "sleep_bed_height";
    case AttrId::FALL_DELAY_TIME: return "fall_delay_time";
    case AttrId::FALLDOWN_BLIND_ZONE: return "falldown_blind_zone";
    case AttrId::DEBUG_LOG: return "debug_log";
    case AttrId::ZONE_ACTIVATION_LIST: return "aux_data";
    case AttrId::SLEEP_HEARTBEAT_SYNC: return "sleep_heartbeat_sync";
    case AttrId::RADAR_FLASH_ID: return "radar_flash_id";
    case AttrId::RADAR_ID: return "radar_id";
    case AttrId::RADAR_CALIBRATION_RESULT: return "radar_calibration_result";
    default: return "unknown";
  }
}

const char* FP2Component::op_code_to_string_(uint8_t type) {
  switch ((OpCode) type) {
    case OpCode::RESPONSE: return "response";
    case OpCode::WRITE: return "write";
    case OpCode::ACK: return "ack";
    case OpCode::READ: return "read";
    case OpCode::REPORT: return "report";
    default: return "unknown";
  }
}

std::string FP2Component::format_payload_hex_(const std::vector<uint8_t> &payload, size_t max_bytes) {
  static const char hex[] = "0123456789abcdef";
  std::string out;
  size_t limit = std::min(max_bytes, payload.size());

  for (size_t i = 0; i < limit; i++) {
    if (i > 0)
      out.push_back(' ');
    out.push_back(hex[(payload[i] >> 4) & 0x0F]);
    out.push_back(hex[payload[i] & 0x0F]);
  }

  if (payload.size() > limit)
    out += " ...";

  return out;
}

void FP2Component::publish_radar_debug_(const char *event, AttrId attr_id,
                                        const std::vector<uint8_t> &payload) {
  ESP_LOGI(TAG, "%s %s (0x%04X) len=%u data=%s",
           event, attr_id_to_string_(attr_id), (uint16_t) attr_id,
           static_cast<unsigned>(payload.size()), format_payload_hex_(payload, 96).c_str());

  if (radar_debug_sensor_ == nullptr)
    return;

  std::string state(event);
  state += " ";
  state += attr_id_to_string_(attr_id);
  state += " 0x";
  static const char hex[] = "0123456789abcdef";
  uint16_t attr_raw = (uint16_t) attr_id;
  state.push_back(hex[(attr_raw >> 12) & 0x0F]);
  state.push_back(hex[(attr_raw >> 8) & 0x0F]);
  state.push_back(hex[(attr_raw >> 4) & 0x0F]);
  state.push_back(hex[attr_raw & 0x0F]);
  state += " len=";
  state += std::to_string(payload.size());
  if (!payload.empty()) {
    state += " data=";
    state += format_payload_hex_(payload, 80);
  }
  radar_debug_sensor_->publish_state(state);
}

void FP2Component::setup() {
  ESP_LOGI(TAG, "Setting up Aqara FP2...");

  // Reset internal state
  waiting_for_ack_attr_id_ = AttrId::INVALID;

  // GPIO Reset
  perform_reset_();
}

void FP2Component::perform_reset_() {
  command_queue_.clear();
  waiting_for_ack_attr_id_ = AttrId::INVALID;
  init_done_ = false;
  last_radar_frame_millis_ = 0;
  last_heartbeat_millis_ = 0;

  if (reset_pin_ != nullptr) {
    ESP_LOGI(TAG, "Performing Hardware Reset via Pin...");
    reset_pin_->setup();
    reset_pin_->digital_write(false);
    delay(100);
    reset_pin_->digital_write(true);
    ESP_LOGI(TAG, "Hardware Reset Done. Waiting for radar traffic...");
  } else {
    ESP_LOGI(TAG, "No Reset Pin configured. Waiting for radar traffic...");
  }

  if (this->location_report_switch_ != nullptr) {
    this->location_report_switch_->publish_state(this->location_reporting_active_);
  }
}

void FP2Component::set_location_reporting_enabled(bool enabled) {
  this->location_reporting_active_ = enabled;
  this->enqueue_command_(OpCode::WRITE, AttrId::LOCATION_REPORT_ENABLE, enabled);
  if (!enabled && this->target_tracking_sensor_ != nullptr) {
    // Clear the sensor state when location reporting is disabled
    this->target_tracking_sensor_->set_has_state(false);
  }
}

void FP2Component::force_detection_config() {
  ESP_LOGI(TAG, "Forcing radar detector/report configuration");
  this->location_reporting_active_ = true;
  if (this->location_report_switch_ != nullptr) {
    this->location_report_switch_->publish_state(true);
  }

  enqueue_command_(OpCode::WRITE, AttrId::WORK_MODE, (uint8_t) 3);
  enqueue_command_(OpCode::WRITE, AttrId::MOTION_DETECT, (uint8_t) 1);
  enqueue_command_(OpCode::WRITE, AttrId::PRESENCE_DETECT, (uint8_t) 1);
  enqueue_command_(OpCode::WRITE, AttrId::MONITOR_MODE, (uint8_t) 0);
  enqueue_command_(OpCode::WRITE, AttrId::PRESENCE_DETECT_SENSITIVITY, global_presence_sensitivity_);
  enqueue_command_(OpCode::WRITE, AttrId::LOCATION_REPORT_ENABLE, true);
  enqueue_command_(OpCode::WRITE, AttrId::PEOPLE_COUNT_REPORT_ENABLE, people_counting_report_enable_);
  enqueue_command_(OpCode::WRITE, AttrId::PEOPLE_NUMBER_ENABLE, people_number_enable_);
  enqueue_command_(OpCode::WRITE, AttrId::TARGET_TYPE_ENABLE, target_type_enable_);

  if (has_sleep_report_enable_) {
    enqueue_command_(OpCode::WRITE, AttrId::SLEEP_REPORT_ENABLE, sleep_report_enable_);
  }
  if (has_posture_report_enable_) {
    enqueue_command_(OpCode::WRITE, AttrId::POSTURE_REPORT_ENABLE, posture_report_enable_);
  }
}

void FP2Component::read_detection_config() {
  ESP_LOGI(TAG, "Queueing radar detector/report config reads");
  enqueue_read_(AttrId::MOTION_DETECT);
  enqueue_read_(AttrId::PRESENCE_DETECT);
  enqueue_read_(AttrId::MONITOR_MODE);
  enqueue_read_(AttrId::PRESENCE_DETECT_SENSITIVITY);
  enqueue_read_(AttrId::LOCATION_REPORT_ENABLE);
  enqueue_read_(AttrId::PEOPLE_COUNT_REPORT_ENABLE);
  enqueue_read_(AttrId::PEOPLE_NUMBER_ENABLE);
  enqueue_read_(AttrId::TARGET_TYPE_ENABLE);
  enqueue_read_(AttrId::SLEEP_REPORT_ENABLE);
  enqueue_read_(AttrId::POSTURE_REPORT_ENABLE);
}

void FP2Component::read_mode_calibration_config() {
  ESP_LOGI(TAG, "Queueing radar mode/calibration/fall/sleep config reads");
  enqueue_read_(AttrId::WORK_MODE);
  enqueue_read_(AttrId::MONITOR_MODE);
  enqueue_read_(AttrId::TARGET_TYPE_ENABLE);
  enqueue_read_(AttrId::RADAR_CALIBRATION_RESULT);
  enqueue_read_(AttrId::RESET_ABSENT_STATUS);
  enqueue_read_(AttrId::MOTION_DETECT);
  enqueue_read_(AttrId::PRESENCE_DETECT);
  enqueue_read_(AttrId::PRESENCE_DETECT_SENSITIVITY);
  enqueue_read_(AttrId::LOCATION_REPORT_ENABLE);
  enqueue_read_(AttrId::FALL_DETECTION_RESULT);
  enqueue_read_(AttrId::FALL_SENSITIVITY);
  enqueue_read_(AttrId::FALL_OVERTIME_REPORT_PERIOD);
  enqueue_read_(AttrId::FALL_OVERTIME_DETECTION);
  enqueue_read_(AttrId::FALL_DELAY_TIME);
  enqueue_read_(AttrId::SLEEP_REPORT_ENABLE);
  enqueue_read_(AttrId::POSTURE_REPORT_ENABLE);
  enqueue_read_(AttrId::SLEEP_MOUNT_POSITION);
  enqueue_read_(AttrId::SLEEP_ZONE_SIZE);
  enqueue_read_(AttrId::SLEEP_BED_HEIGHT);
  enqueue_read_(AttrId::FALLDOWN_BLIND_ZONE);
  enqueue_read_(AttrId::WALL_CORNER_POS);
  enqueue_read_(AttrId::SLEEP_STATE);
  enqueue_read_(AttrId::SLEEP_PRESENCE);
  enqueue_read_(AttrId::SLEEP_INOUT_STATE);
  enqueue_read_(AttrId::SLEEP_EVENT);
  enqueue_read_(AttrId::SLEEP_EVENT_DESCRIPTOR);
}

void FP2Component::read_attr(uint16_t attr_id) {
  AttrId attr = (AttrId) attr_id;
  ESP_LOGI(TAG, "Queueing raw radar read for %s (0x%04X)", attr_id_to_string_(attr), attr_id);
  enqueue_read_(attr);
}

void FP2Component::write_attr_uint8(uint16_t attr_id, uint8_t value) {
  AttrId attr = (AttrId) attr_id;
  ESP_LOGI(TAG, "Queueing raw UINT8 radar write for %s (0x%04X) = %u",
           attr_id_to_string_(attr), attr_id, value);
  enqueue_command_(OpCode::WRITE, attr, value);
}

void FP2Component::write_attr_uint16(uint16_t attr_id, uint16_t value) {
  AttrId attr = (AttrId) attr_id;
  ESP_LOGI(TAG, "Queueing raw UINT16 radar write for %s (0x%04X) = %u",
           attr_id_to_string_(attr), attr_id, value);
  enqueue_command_(OpCode::WRITE, attr, value);
}

void FP2Component::write_attr_uint32(uint16_t attr_id, uint32_t value) {
  AttrId attr = (AttrId) attr_id;
  ESP_LOGI(TAG, "Queueing raw UINT32 radar write for %s (0x%04X) = %u",
           attr_id_to_string_(attr), attr_id, value);
  enqueue_command_(OpCode::WRITE, attr, value);
}

void FP2Component::write_attr_bool(uint16_t attr_id, bool value) {
  AttrId attr = (AttrId) attr_id;
  ESP_LOGI(TAG, "Queueing raw BOOL radar write for %s (0x%04X) = %u",
           attr_id_to_string_(attr), attr_id, value ? 1 : 0);
  enqueue_command_(OpCode::WRITE, attr, value);
}

void FP2Component::configure_sleep_mode(uint16_t width, uint16_t length, uint8_t mount_position) {
  uint32_t zone_size = ((uint32_t) width << 16) | (uint32_t) length;
  std::vector<uint8_t> empty_zone(41, 0x00);

  ESP_LOGI(TAG, "Queueing sleep mode setup width=%u length=%u mount=%u zone_size=0x%08X",
           width, length, mount_position, zone_size);

  location_reporting_active_ = true;
  if (location_report_switch_ != nullptr) {
    location_report_switch_->publish_state(true);
  }

  // PROTO-02b (real-hardware finding, 2026-07-14): SLEEP_ZONE_SIZE and
  // SLEEP_MOUNT_POSITION must be queued and ACKed BEFORE the WORK_MODE=9
  // write. Per the reference fork (JameZUK/esphome_fp2_ng
  // set_operating_mode()), the WORK_MODE write triggers a radar-side
  // flash-save + self-restart into FW3 (Sleep/Vitals firmware); any
  // parameter written after that point arrives too late to be captured by
  // the flash save. FW3 then boots with no valid zone geometry, GTrack
  // never allocates a track, and the radar goes spontaneous-report-silent
  // (no heartbeat, no vitals) while still answering direct reads — exactly
  // the failure observed live on fp2-sala. This block was previously
  // ordered AFTER WORK_MODE=9 (a pre-existing bug predating PROTO-02, not
  // introduced by it) and is corrected here to match the reference fork's
  // write order.
  enqueue_command_(OpCode::WRITE, AttrId::SLEEP_REPORT_ENABLE, true);
  enqueue_command_blob2_(AttrId::ZONE_MAP, empty_zone);
  enqueue_command_blob2_(AttrId::ZONE_MAP, empty_zone);
  enqueue_command_(OpCode::WRITE, AttrId::LOCATION_REPORT_ENABLE, true);
  enqueue_command_(OpCode::WRITE, AttrId::SLEEP_ZONE_SIZE, zone_size);
  enqueue_command_(OpCode::WRITE, AttrId::SLEEP_MOUNT_POSITION, mount_position);

  enqueue_command_(OpCode::WRITE, AttrId::WORK_MODE, (uint8_t) 9);

  // PROTO-02: seed the heartbeat keepalive on sleep-mode entry. Safe to
  // leave after WORK_MODE=9 even if the radar restarts immediately — this
  // write is NOT part of the flash-saved config; it is a live protocol
  // keepalive re-primed every heartbeat via handle_report_(), so a dropped
  // seed write is harmless (the first post-restart heartbeat continues the
  // sequence from sleep_heartbeat_counter_ = 0 regardless).
  sleep_mode_active_ = true;
  sleep_heartbeat_counter_ = 0;
  enqueue_command_(OpCode::WRITE, AttrId::SLEEP_HEARTBEAT_SYNC, (uint8_t) 0);
}

void FP2Component::set_work_mode(uint8_t mode) {
  ESP_LOGI(TAG, "Setting radar work mode candidate to %u", mode);
  enqueue_command_(OpCode::WRITE, AttrId::WORK_MODE, mode);
}

void FP2Component::set_ai_target_filter_enabled(bool enabled) {
  ESP_LOGI(TAG, "Setting radar AI target filter to %s", enabled ? "enabled" : "disabled");
  target_type_enable_ = enabled;
  enqueue_command_(OpCode::WRITE, AttrId::TARGET_TYPE_ENABLE, enabled);
}

void FP2Component::calibrate_empty_room() {
  ESP_LOGW(TAG, "Starting candidate empty-room calibration/reset via reset_absent_status");
  enqueue_command_(OpCode::WRITE, AttrId::RESET_ABSENT_STATUS, true);
}

void FP2Component::reset_radar() {
  ESP_LOGI(TAG, "Resetting radar module via diagnostic action");
  perform_reset_();
}

void FP2Component::set_edge_auto_enabled(bool enabled) {
  ESP_LOGI(TAG, "Setting edge auto detection to %s", enabled ? "enabled" : "disabled");
  enqueue_command_(OpCode::WRITE, AttrId::EDGE_AUTO_ENABLE, enabled);
}

void FP2Component::set_interference_auto_enabled(bool enabled) {
  ESP_LOGI(TAG, "Setting interference auto detection to %s", enabled ? "enabled" : "disabled");
  enqueue_command_(OpCode::WRITE, AttrId::INTERFERENCE_AUTO_ENABLE, enabled);
}

void FP2LocationSwitch::write_state(bool state) {
  if (this->parent_ != nullptr) {
    this->parent_->set_location_reporting_enabled(state);
  }
  this->publish_state(state);
}

void FP2Component::loop() {
  while (available()) {
    uint8_t byte;
    read_byte(&byte);
    handle_incoming_byte_(byte);
  }

  check_initialization_();
  process_command_queue_();

  // Release zone motion sensors once their debounce timeout expires.
  uint32_t now = millis();
  for (auto &z : zones_) {
    z->tick_motion(now);
  }
}

void FP2Component::check_initialization_() {
  if (init_done_)
    return;

  if (last_radar_frame_millis_ > 0) {
    ESP_LOGI(TAG, "Radar traffic received. Starting initialization sequence...");
    init_done_ = true;

    // 1. Basic Settings
    enqueue_command_(OpCode::WRITE, AttrId::WORK_MODE, (uint8_t) 3);
    enqueue_command_(OpCode::WRITE, AttrId::MOTION_DETECT, (uint8_t) 1);
    enqueue_command_(OpCode::WRITE, AttrId::PRESENCE_DETECT, (uint8_t) 1);
    if (location_reporting_active_) {
      enqueue_command_(OpCode::WRITE, AttrId::LOCATION_REPORT_ENABLE, true);
      if (location_report_switch_ != nullptr) {
        location_report_switch_->publish_state(true);
      }
    }
    enqueue_command_(OpCode::WRITE, AttrId::MONITOR_MODE, (uint8_t) 0);
    enqueue_command_(OpCode::WRITE, AttrId::LEFT_RIGHT_REVERSE,
                     (uint8_t)(left_right_reverse_ ? 2 : 0));
    enqueue_command_(OpCode::WRITE, AttrId::PRESENCE_DETECT_SENSITIVITY, global_presence_sensitivity_);
    enqueue_command_(OpCode::WRITE, AttrId::CLOSING_SETTING, (uint8_t) 1);
    enqueue_command_(OpCode::WRITE, AttrId::ZONE_CLOSE_AWAY_ENABLE, (uint16_t) 0x0001);
    // PROTO-03 (2026-07-14): 0x0121 is report-only (fall event, not a
    // boolean enable) as of this fix — there is no separate fall-detection
    // enable register anywhere in the protocol. The old boot-time boolean
    // WRITE of AttrId::FALL_DETECTION has been removed. Fall events only
    // fire while the radar is in WORK_MODE=8; switch into it via the
    // already-shipped fp2_set_work_mode(8) diagnostic action.
    if (has_fall_detection_enabled_ && fall_detection_enabled_) {
      ESP_LOGW(TAG, "fall_detection: true has no boot-time write effect as of this fix — "
                    "0x0121 is fall-event-report-only. Fall events require WORK_MODE=8; "
                    "switch via the fp2_set_work_mode(8) diagnostic action.");
    }
    if (has_fall_detection_sensitivity_) {
      enqueue_command_(OpCode::WRITE, AttrId::FALL_SENSITIVITY, fall_detection_sensitivity_);
    }
    if (has_sleep_report_enable_) {
      enqueue_command_(OpCode::WRITE, AttrId::SLEEP_REPORT_ENABLE, sleep_report_enable_);
    }
    if (has_posture_report_enable_) {
      enqueue_command_(OpCode::WRITE, AttrId::POSTURE_REPORT_ENABLE, posture_report_enable_);
    }
    enqueue_command_(OpCode::WRITE, AttrId::PEOPLE_COUNT_REPORT_ENABLE, people_counting_report_enable_);
    enqueue_command_(OpCode::WRITE, AttrId::PEOPLE_NUMBER_ENABLE, people_number_enable_);
    enqueue_command_(OpCode::WRITE, AttrId::TARGET_TYPE_ENABLE, target_type_enable_);
    // enqueue_command_(OpCode::WRITE, AttrId::SLEEP_MOUNT_POSITION, (uint8_t) 0); // sleep zone mount pos
    enqueue_command_(OpCode::WRITE, AttrId::WALL_CORNER_POS, mounting_position_);
    enqueue_command_(OpCode::WRITE, AttrId::DWELL_TIME_ENABLE, (uint8_t)(dwell_time_enable_ ? 1 : 0));
    enqueue_command_(OpCode::WRITE, AttrId::WALK_DISTANCE_ENABLE, (uint8_t)(walking_distance_enable_ ? 1 : 0));
    enqueue_command_(OpCode::WRITE, AttrId::THERMO_EN, thermodynamic_chart_enable_);
    if (thermodynamic_chart_enable_) {
      enqueue_command_(OpCode::WRITE, AttrId::THERMO_DATA, (uint8_t) 1);
    }

    // 2. Grids
    if (has_interference_grid_) {
      // 0x0110 Interference Source
      enqueue_command_blob2_(AttrId::INTERFERENCE_MAP,
                             std::vector<uint8_t>(interference_grid_.begin(),
                                                  interference_grid_.end()));
    }
    if (has_exit_grid_) {
      // 0x0109 Enter/Exit Label
      enqueue_command_blob2_(
          AttrId::ENTRY_EXIT_MAP, std::vector<uint8_t>(exit_grid_.begin(), exit_grid_.end()));
    }
    if (has_edge_grid_) {
      // 0x0107 Edge Label
      enqueue_command_blob2_(
          AttrId::EDGE_MAP, std::vector<uint8_t>(edge_grid_.begin(), edge_grid_.end()));
    }

    // 3. Zones
    std::vector<uint8_t> activations(32, 0);
    for (const auto &zone : zones_) {
      // a. Send Zone Detect Setting (0x0114)
      // Structure: [ZoneID] [40 byte Map]
      std::vector<uint8_t> payload;
      payload.push_back(zone->id);
      payload.insert(payload.end(), zone->grid.begin(), zone->grid.end());
      enqueue_command_blob2_(AttrId::ZONE_MAP, payload);

      // b. Send Sensitivity (0x0151)
      // Structure: UINT16 (High=ID, Low=Sens)
      uint16_t sens_val = (zone->id << 8) | (zone->sensitivity & 0xFF);
      enqueue_command_(OpCode::WRITE, AttrId::ZONE_SENSITIVITY, sens_val);

      // c. Send Zone Type (0x0152) if configured: UINT16 (High=ID, Low=Type)
      if (zone->has_zone_type) {
        uint16_t type_val = (zone->id << 8) | (zone->zone_type & 0xFF);
        enqueue_command_(OpCode::WRITE, AttrId::DETECT_ZONE_TYPE, type_val);
      }

      activations[zone->id] = zone->id;
    }

    enqueue_command_blob2_(AttrId::ZONE_ACTIVATION_LIST, activations);

    for (const auto &zone : zones_) {
        // Close/Away Enable default?
        // Trace: 0x0153 Zone Close Away Enable.
        // We can enable it by default for now or add config options later.
        enqueue_command_(OpCode::WRITE, AttrId::ZONE_CLOSE_AWAY_ENABLE, (uint16_t)((zone->id << 8) | 1));
    }

    if (debug_probe_reads_) {
      ESP_LOGI(TAG, "Queueing radar debug probe reads");
      enqueue_read_(AttrId::MOTION_DETECT);
      enqueue_read_(AttrId::PRESENCE_DETECT);
      enqueue_read_(AttrId::LOCATION_REPORT_ENABLE);
      enqueue_read_(AttrId::RADAR_HW_VERSION);
      enqueue_read_(AttrId::RADAR_FLASH_ID);
      enqueue_read_(AttrId::RADAR_ID);
      enqueue_read_(AttrId::RADAR_CALIBRATION_RESULT);
    }

    // 5. Publish grid sensors once initialization completes
    ESP_LOGI(TAG, "Publishing grid sensors: has_edge=%d edge_sensor=%p has_exit=%d exit_sensor=%p has_interference=%d interference_sensor=%p",
             has_edge_grid_, edge_label_grid_sensor_, has_exit_grid_, entry_exit_grid_sensor_, has_interference_grid_, interference_grid_sensor_);

    if (has_edge_grid_ && edge_label_grid_sensor_ != nullptr) {
      ESP_LOGI(TAG, "Publishing edge label grid");
      edge_label_grid_sensor_->publish_state(grid_to_hex_card_format(edge_grid_));
    } else {
      ESP_LOGW(TAG, "NOT publishing edge label grid (has_grid=%d, sensor=%p)", has_edge_grid_, edge_label_grid_sensor_);
    }

    if (has_exit_grid_ && entry_exit_grid_sensor_ != nullptr) {
      ESP_LOGI(TAG, "Publishing entry/exit grid");
      entry_exit_grid_sensor_->publish_state(grid_to_hex_card_format(exit_grid_));
    } else {
      ESP_LOGW(TAG, "NOT publishing entry/exit grid (has_grid=%d, sensor=%p)", has_exit_grid_, entry_exit_grid_sensor_);
    }

    if (has_interference_grid_ && interference_grid_sensor_ != nullptr) {
      ESP_LOGI(TAG, "Publishing interference grid");
      interference_grid_sensor_->publish_state(grid_to_hex_card_format(interference_grid_));
    } else {
      ESP_LOGW(TAG, "NOT publishing interference grid (has_grid=%d, sensor=%p)", has_interference_grid_, interference_grid_sensor_);
    }

    // 6. Publish zone map sensors
    for (const auto &zone : zones_) {
      if (zone->map_sensor != nullptr) {
        zone->map_sensor->publish_state(grid_to_hex_card_format(zone->grid));
      }
    }

    // 7. Publish known initial states after reset
    // After radar reset, we know there is no occupancy/motion detected yet
    ESP_LOGI(TAG, "Publishing initial zone states (no presence/motion after reset)");
    for (const auto &zone : zones_) {
      zone->publish_presence(false);
      zone->reset_motion();
    }

    if (global_presence_sensor_ != nullptr) global_presence_sensor_->publish_state(false);
    if (global_motion_sensor_ != nullptr) global_motion_sensor_->publish_state(false);

    // Clear target tracking state - no targets after reset
    if (target_tracking_sensor_ != nullptr) {
      target_tracking_sensor_->set_has_state(false);
    }
  }
}

void FP2Component::process_command_queue_() {
  uint32_t now = millis();

  // If waiting for ACK
  if (waiting_for_ack_attr_id_ != AttrId::INVALID) {
    if (now - last_command_sent_millis_ > ACK_TIMEOUT_MS) {
      // Timeout
      if (!command_queue_.empty()) {
        auto &cmd = command_queue_.front();
        cmd.retry_count++;
        if (cmd.retry_count >= MAX_RETRIES) {
          ESP_LOGW(TAG, "Command 0x%04X timed out after %d retries. Dropping.",
                   (uint16_t) cmd.attr_id, MAX_RETRIES);
          publish_radar_debug_("command_ack_failed", cmd.attr_id, cmd.data);
          command_queue_.pop_front();
          waiting_for_ack_attr_id_ = AttrId::INVALID;
        } else {
          ESP_LOGW(TAG, "Command 0x%04X timed out. Retrying (%d/%d)...",
                   (uint16_t) cmd.attr_id, cmd.retry_count, MAX_RETRIES);
          publish_radar_debug_("command_retry", cmd.attr_id, cmd.data);
          // Resend handled by send_next_command_ logic once waiting state calls
          // reset? Actually, we should just resend immediately
          send_next_command_();
        }
      } else {
        // Queue empty but waiting state mismatch?
        waiting_for_ack_attr_id_ = AttrId::INVALID;
      }
    }
    return; // Still waiting
  }

  // Not waiting, send next
  if (!command_queue_.empty()) {
    send_next_command_();
  }
}

void FP2Component::write_command_frame_(const FP2Command &cmd, bool track_timeout) {
  static uint8_t next_tx_seq = 0;

  // Build frame: [Sync][Ver][Ver][Seq][Op][Len][Len][Check][Payload][CRC][CRC]
  std::vector<uint8_t> frame;
  frame.push_back(0x55);  // Sync
  frame.push_back(0x00);  // Version High
  frame.push_back(0x01);  // Version Low
  frame.push_back(next_tx_seq++);
  frame.push_back((uint8_t)cmd.type);

  uint16_t len = cmd.data.size();
  frame.push_back((len >> 8) & 0xFF);
  frame.push_back(len & 0xFF);

  // Header checksum: NOT((Sum(bytes 0-6) - 1))
  uint8_t sum = 0;
  for (int i = 0; i < 7; i++)
    sum += frame[i];
  frame.push_back((uint8_t)(~((sum - 1))));

  // Append payload
  frame.insert(frame.end(), cmd.data.begin(), cmd.data.end());

  // Append CRC16 (Little Endian)
  uint16_t crc = crc16(frame.data(), frame.size());
  frame.push_back(crc & 0xFF);
  frame.push_back((crc >> 8) & 0xFF);

  write_array(frame);
  if (track_timeout) {
    last_command_sent_millis_ = millis();
    publish_radar_debug_(cmd.type == OpCode::RESPONSE ? "command_tx_read" : "command_tx_write",
                         cmd.attr_id, cmd.data);
  }
  ESP_LOGD(TAG, "Sending %s %s (0x%04X), len=%u data=%s",
           op_code_to_string_((uint8_t) cmd.type), attr_id_to_string_(cmd.attr_id),
           (uint16_t) cmd.attr_id, static_cast<unsigned>(cmd.data.size()),
           format_payload_hex_(cmd.data, 64).c_str());
}

void FP2Component::send_next_command_() {
  if (command_queue_.empty())
    return;

  auto &cmd = command_queue_.front();
  write_command_frame_(cmd, cmd.type == OpCode::WRITE || cmd.type == OpCode::RESPONSE);

  // WRITE commands expect ACK. Host-initiated reads (OpCode::RESPONSE, wire
  // 0x01) are fire-and-forget per PROTO-01 — the radar's answer, if any,
  // arrives asynchronously as its own RESPONSE frame and is purely
  // observational (see handle_response_); it does not gate this queue.
  if (cmd.type == OpCode::WRITE) {
    waiting_for_ack_attr_id_ = cmd.attr_id;
  } else {
    command_queue_.pop_front();
  }
}

void FP2Component::send_ack_(AttrId attr_id) {
  FP2Command cmd;
  cmd.type = OpCode::ACK;
  cmd.attr_id = attr_id;
  cmd.retry_count = 0;
  cmd.last_send_time = 0;

  // ACK payload: [SubID 2 bytes] [DataType VOID]
  cmd.data.push_back((((uint16_t) attr_id) >> 8) & 0xFF);
  cmd.data.push_back(((uint16_t) attr_id) & 0xFF);
  cmd.data.push_back(0x03);  // DataType: VOID

  // Report ACKs must not disturb the write/ACK retry queue.
  write_command_frame_(cmd, false);
}

void FP2Component::send_reverse_response_(AttrId attr_id, uint8_t byte_val) {
  FP2Command cmd;
  cmd.type = OpCode::READ;  // Reverse Read Response uses READ opcode
  cmd.attr_id = attr_id;
  cmd.retry_count = 0;

  // Payload: [SubID 2 bytes] [DataType UINT8] [Value 1 byte]
  cmd.data.push_back((((uint16_t) attr_id) >> 8) & 0xFF);
  cmd.data.push_back(((uint16_t) attr_id) & 0xFF);
  cmd.data.push_back(0x00);  // DataType: UINT8
  cmd.data.push_back(byte_val);

  // The radar is synchronously waiting for this reverse-read response.
  write_command_frame_(cmd, false);
}

void FP2Component::handle_incoming_byte_(uint8_t byte) {
  if (state_ == SYNC) {
    if (byte == 0x55) {
      state_ = VER_H;
      rx_payload_.clear();
      header_sum_ = byte; // Start sum
    }
    return;
  }

  // Update sum for header fields (0..6)
  if (state_ < H_CHECK) {
    header_sum_ += byte;
  }

  switch (state_) {
  case VER_H:
    state_ = (byte == 0x00) ? VER_L : SYNC;
    break;
  case VER_L:
    state_ = (byte == 0x01) ? SEQ : SYNC;
    break;
  case SEQ:
    rx_seq_ = byte;
    state_ = OPCODE;
    break;
  case OPCODE:
    rx_opcode_ = byte;
    state_ = LEN_H;
    break;
  case LEN_H:
    rx_len_ = byte << 8;
    state_ = LEN_L;
    break;
  case LEN_L:
    rx_len_ |= byte;
    state_ = H_CHECK;
    break;

  case H_CHECK: {
    uint8_t expected = (uint8_t)(~((header_sum_ - 1)));
    if (byte != expected) {
      ESP_LOGW(TAG, "Header Checksum Fail: Exp %02X, Got %02X", expected, byte);
      state_ = SYNC;
    } else {
      if (rx_len_ > 4096) { // Sanity check, increased for potential BLOBs
        state_ = SYNC;
      } else if (rx_len_ == 0) {
        state_ = CRC_L;
      } else {
        state_ = PAYLOAD;
      }
    }
    break;
  }

  case PAYLOAD:
    rx_payload_.push_back(byte);
    if (rx_payload_.size() == rx_len_) {
      state_ = CRC_L;
    }
    break;

  case CRC_L:
    rx_crc_ = byte;
    state_ = CRC_H;
    break;

  case CRC_H: {
    rx_crc_ |= (byte << 8);
    // Validate CRC
    std::vector<uint8_t> frame;
    frame.push_back(0x55);
    frame.push_back(0x00);
    frame.push_back(0x01);
    frame.push_back(rx_seq_);
    frame.push_back(rx_opcode_);
    frame.push_back((rx_len_ >> 8) & 0xFF);
    frame.push_back(rx_len_ & 0xFF);
    frame.push_back((uint8_t)(~((header_sum_ - 1))));
    frame.insert(frame.end(), rx_payload_.begin(), rx_payload_.end());

    uint16_t calc = crc16(frame.data(), frame.size());
    if (calc == rx_crc_) {
      // Parse Payload
      // Note: rx_len_ >= 2 to allow Reverse Read Requests (RESPONSE with just SubID, no data)
      if (rx_len_ >= 2) {
        uint16_t attr_id_int = (rx_payload_[0] << 8) | rx_payload_[1];
        AttrId attr_id = (AttrId) attr_id_int;
        // DataType = rx_payload_[2] (if present)
        handle_parsed_frame_(rx_opcode_, attr_id, rx_payload_);
      }
    } else {
      ESP_LOGW(TAG, "CRC Fail: Exp %04X, Got %04X", calc, rx_crc_);
    }
    state_ = SYNC;
    break;
  }

  default:
    state_ = SYNC;
  }
}

void FP2Component::handle_parsed_frame_(uint8_t type, AttrId attr_id,
                                        const std::vector<uint8_t> &payload) {
  OpCode op = (OpCode)type;
  last_radar_frame_millis_ = millis();
  ESP_LOGV(TAG, "Received %s %s (0x%04X), len=%u",
           op_code_to_string_(type), attr_id_to_string_(attr_id),
           (uint16_t) attr_id, static_cast<unsigned>(payload.size()));

  switch (op) {
    case OpCode::ACK:
      handle_ack_(attr_id);
      break;
    case OpCode::REPORT:
      handle_report_(attr_id, payload);
      break;
    case OpCode::RESPONSE:
      handle_response_(attr_id, payload);
      break;
    case OpCode::READ:
      if (payload.size() > 2) {
        handle_response_(attr_id, payload);
      } else {
        publish_radar_debug_("unexpected_read_opcode", attr_id, payload);
        ESP_LOGW(TAG, "Unexpected READ opcode from radar for 0x%04X (%s)",
                 (uint16_t) attr_id, attr_id_to_string_(attr_id));
      }
      break;
    default:
      publish_radar_debug_("unhandled_opcode", attr_id, payload);
      ESP_LOGW(TAG, "Unhandled OpCode: %d", type);
      break;
  }
}

void FP2Component::handle_ack_(AttrId attr_id) {
  if (waiting_for_ack_attr_id_ == attr_id) {
    ESP_LOGD(TAG, "ACK Received for 0x%04X", (uint16_t) attr_id);
    publish_radar_debug_("command_ack", attr_id, std::vector<uint8_t>{});
    waiting_for_ack_attr_id_ = AttrId::INVALID;
    if (!command_queue_.empty()) {
      command_queue_.pop_front();
    }
  } else {
    ESP_LOGW(TAG, "Unexpected ACK 0x%04X (Waiting for 0x%04X)", attr_id,
             (uint16_t) waiting_for_ack_attr_id_);
    publish_radar_debug_("unexpected_ack", attr_id, std::vector<uint8_t>{});
  }
}

void FP2Component::handle_report_(AttrId attr_id, const std::vector<uint8_t> &payload) {
  // Send ACK for all reports except heartbeat
  if (attr_id != AttrId::RADAR_SW_VERSION) {
    send_ack_(attr_id);
  }

  // Process specific report types
  switch (attr_id) {
    case AttrId::RADAR_SW_VERSION:  // Heartbeat
      last_heartbeat_millis_ = millis();
      // PROTO-02: keep the radar's sleep state machine alive during extended
      // sleep/vitals sessions by piggybacking an incrementing 0x0203 write on
      // every heartbeat report while sleep mode is active.
      if (sleep_mode_active_ && init_done_) {
        enqueue_command_(OpCode::WRITE, AttrId::SLEEP_HEARTBEAT_SYNC,
                         (uint8_t) sleep_heartbeat_counter_++);
      }
      if (payload.size() == 4 && payload[2] == 0x00) {
        auto ver_str = std::to_string(payload[3]);
        if (radar_software_sensor_ != nullptr) {
            if (radar_software_sensor_->state != ver_str) {
                radar_software_sensor_->publish_state(ver_str);
            }
        }
        break;
      }
      // Malformed heartbeat: stop here instead of falling through to WORK_MODE.
      break;

    case AttrId::WORK_MODE:
        if (payload.size() == 4 && payload[2] == 0x00) {
            ESP_LOGI(TAG, "Received work mode report: %u", payload[3]);
            publish_radar_debug_("radar_report", attr_id, payload);
            break;
        }
        break;

    case AttrId::DETECT_ZONE_MOTION:
        if (payload.size() == 5 && payload[2] == 0x01) {
            uint8_t zone_id = payload[3];
            uint8_t event_type = payload[4];
            ESP_LOGD(TAG, "Zone Motion Report: Zone %u = 0x%02X", zone_id, event_type);

            // Low byte is an event-type bitmask (PROTOCOL 0x0115):
            // 1=Enter, 2=Move, 4=Exit, 8=L/R, 16=Interference.
            // Enter/Move/L-R count as movement and refresh the debounce timer;
            // Exit/Interference do not.
            if (event_type & 0x0B) {
              uint32_t now = millis();
              for (auto &z : zones_) {
                if (z->id == zone_id) {
                  z->note_motion_event(now);
                  break;
                }
              }
            }
            break;
        }
        break;

    case AttrId::MOTION_DETECT:
        if (payload.size() == 4 && payload[2]  == 0x00) {
            uint8_t state = payload[3];
            if (global_motion_sensor_ != nullptr) {
              global_motion_sensor_->publish_state(state != 0);
            }
            ESP_LOGI(TAG, "Received global motion report: %u", state);
            break;
        }
        break;

    case AttrId::PRESENCE_DETECT:
        if (payload.size() == 4 && payload[2]  == 0x00) {
            uint8_t state = payload[3];
            if (global_presence_sensor_ != nullptr) {
              global_presence_sensor_->publish_state(state != 0);
            }
            ESP_LOGI(TAG, "Received global presence report: %u", state);
            break;
        }
        break;

    case AttrId::REALTIME_PEOPLE_NUMBER:
      handle_simple_uint32_report_(payload, realtime_people_number_sensor_, "realtime_people_number");
      break;

    case AttrId::ONTIME_PEOPLE_NUMBER:
      handle_simple_uint32_report_(payload, ontime_people_number_sensor_, "ontime_people_number");
      break;

    case AttrId::REALTIME_PEOPLE_COUNTING:
      handle_simple_uint32_report_(payload, realtime_people_counting_sensor_, "realtime_people_counting");
      break;

    case AttrId::WALK_DISTANCE_ALL:
      handle_simple_uint32_report_(payload, walking_distance_sensor_, "walking_distance_all");
      break;

    case AttrId::ZONE_PRESENCE:  // Zone Presence
        // Payload: [SubID 2B] [Type 0x01(UINT16)] [ValH] [ValL]
        // ValH = ZoneID, ValL = State (1=Occ, 0=Empty)
        if (payload.size() >= 5 && payload[2] == 0x01) {
            uint8_t zone_id = payload[3];
            uint8_t state = payload[4];
            ESP_LOGD(TAG, "Zone Presence Report: Zone %d = %s", zone_id, state ? "ON" : "OFF");

            for (auto &z : zones_) {
                if (z->id == zone_id) {
                    z->publish_presence(state == 1);
                    break;
                }
            }
            break;
        }

    case AttrId::LOCATION_TRACKING_DATA:  // Location Tracking Data
      handle_location_tracking_report_(payload);
      break;

    case AttrId::TEMPERATURE:
      handle_temperature_report_(payload);
      break;

    case AttrId::DEBUG_LOG:
      handle_debug_log_report_(payload);
      break;

    case AttrId::TARGET_POSTURE:
      handle_target_posture_report_(payload);
      break;

    case AttrId::PEOPLE_COUNTING:
      handle_people_counting_report_(payload);
      break;

    case AttrId::THERMO_DATA:
      publish_radar_debug_("radar_report", attr_id, payload);
      break;

    case AttrId::SLEEP_DATA:
      handle_sleep_data_report_(payload);
      break;

    case AttrId::SLEEP_STATE:
      handle_sleep_state_report_(payload);
      break;

    case AttrId::SLEEP_PRESENCE:
      handle_simple_uint8_binary_report_(payload, sleep_presence_sensor_, "sleep_presence");
      break;

    case AttrId::SLEEP_INOUT_STATE:
      handle_simple_uint8_binary_report_(payload, sleep_inout_sensor_, "sleep_inout_state");
      break;

    case AttrId::FALL_DETECTION_RESULT:
      // Treats the 3-state event (0=clear/1=type A/2=type B) as boolean
      // (!=0) for the HA entity, matching the reference fork's own choice.
      // The raw payload is still visible via the ESP_LOGV frame-receipt log
      // in handle_parsed_frame_() and the malformed-report radar_debug path
      // inside handle_simple_uint8_binary_report_(), so no information is
      // silently dropped.
      handle_simple_uint8_binary_report_(payload, fall_detected_sensor_, "fall_detection_result");
      break;

    case AttrId::SLEEP_EVENT:
      handle_sleep_event_report_(payload);
      break;

    case AttrId::RESET_ABSENT_STATUS:
      publish_radar_debug_("radar_report", attr_id, payload);
      ESP_LOGI(TAG, "Received reset_absent_status report");
      break;

    default:
      publish_radar_debug_("unhandled_report", attr_id, payload);
      ESP_LOGW(TAG, "Unhandled report 0x%04X (%s)", (uint16_t) attr_id, attr_id_to_string_(attr_id));
      break;
  }
}

void FP2Component::handle_location_tracking_report_(const std::vector<uint8_t> &payload) {
  // Ignore stale data if location reporting has been disabled
  if (!this->location_reporting_active_) {
    return;
  }

  // Payload: [SubID 2] [Type 0x06(BLOB2)] [Len 2] [Count 1] [Target 14]...
  if (payload.size() < 6 || payload[2] != 0x06) {
    return;
  }

  uint8_t count = payload[5];
  uint32_t now = millis();
  if (count != last_location_target_count_ || now - last_location_debug_millis_ > 5000) {
    ESP_LOGD(TAG, "Location tracking report: targets=%u payload_len=%u",
             count, static_cast<unsigned>(payload.size()));
    last_location_target_count_ = count;
    last_location_debug_millis_ = now;
  }

  // Build binary buffer: [count][target1 14 bytes][target2 14 bytes]...
  // Each target is 14 bytes: id(1), x(2), y(2), z(2), velocity(2), snr(2), classifier(1), posture(1), active(1)
  std::vector<uint8_t> binary_data;
  binary_data.push_back(count);

  // Corner-mount coordinate scale (PROTOCOL 5.1): X/Y raw span 800 units = 7 m.
  static constexpr float METERS_PER_UNIT = 7.0f / 800.0f;
  float nearest = NAN;
  uint8_t valid = 0;

  for (int i = 0; i < count; i++) {
    int offset = 6 + (i * 14);
    if (offset + 14 > payload.size())
      break;

    // Copy raw 14-byte target data directly (already in correct big-endian format)
    binary_data.insert(binary_data.end(),
                       payload.begin() + offset,
                       payload.begin() + offset + 14);

    // Target layout: id(1), X s16(2), Y s16(2), ... (big endian)
    int16_t x = (int16_t)((payload[offset + 1] << 8) | payload[offset + 2]);
    int16_t y = (int16_t)((payload[offset + 3] << 8) | payload[offset + 4]);
    float dist_m = sqrtf((float) x * x + (float) y * y) * METERS_PER_UNIT;
    if (std::isnan(nearest) || dist_m < nearest)
      nearest = dist_m;
    valid++;
  }

  // Base64 encode the binary data
  std::string base64_str = esphome::base64_encode(binary_data);

  if (this->target_tracking_sensor_ != nullptr) {
    this->target_tracking_sensor_->publish_state(base64_str);
  }

  // Derived numeric sensors (throttled to ~1 Hz; the raw stream is 10-20 Hz).
  if ((target_count_sensor_ != nullptr || nearest_distance_sensor_ != nullptr) &&
      now - last_target_publish_millis_ >= 1000) {
    last_target_publish_millis_ = now;
    if (target_count_sensor_ != nullptr) {
      target_count_sensor_->publish_state(valid);
    }
    if (nearest_distance_sensor_ != nullptr) {
      nearest_distance_sensor_->publish_state(nearest);  // NAN -> unknown when no targets
    }
  }
}

void FP2Component::handle_temperature_report_(const std::vector<uint8_t> &payload) {
    if (payload.size() == 5 && payload[2] == 0x01) {
        uint16_t temp = payload[3] << 8 | payload[4];
        if (radar_temperature_sensor_ != nullptr) {
            radar_temperature_sensor_->publish_state(temp);
        }
        ESP_LOGD(TAG, "Radar temperature report: %d", temp);
    } else {
        ESP_LOGD(TAG, "Unexpected radar temperature report format");
    }
}

void FP2Component::handle_debug_log_report_(const std::vector<uint8_t> &payload) {
  if (payload.size() < 5 || payload[2] != 0x05) {
    publish_radar_debug_("debug_log_malformed", AttrId::DEBUG_LOG, payload);
    return;
  }

  uint16_t declared_len = (payload[3] << 8) | payload[4];
  size_t available_len = payload.size() - 5;
  size_t text_len = std::min(static_cast<size_t>(declared_len), available_len);
  std::string text(payload.begin() + 5, payload.begin() + 5 + text_len);

  while (!text.empty() && text.back() == '\0') {
    text.pop_back();
  }

  ESP_LOGI(TAG, "Radar debug_log: %s", text.c_str());
  if (radar_debug_sensor_ != nullptr) {
    radar_debug_sensor_->publish_state("debug_log " + text);
  }
}

void FP2Component::handle_simple_uint32_report_(const std::vector<uint8_t> &payload,
                                                sensor::Sensor *sensor, const char *name) {
  if (payload.size() != 7 || payload[2] != 0x02) {
    publish_radar_debug_("uint32_report_malformed", (AttrId)((payload.size() >= 2) ? ((payload[0] << 8) | payload[1]) : 0), payload);
    return;
  }

  uint32_t value = ((uint32_t) payload[3] << 24) |
                   ((uint32_t) payload[4] << 16) |
                   ((uint32_t) payload[5] << 8) |
                   (uint32_t) payload[6];
  ESP_LOGI(TAG, "%s report: %u", name, value);
  if (sensor != nullptr) {
    sensor->publish_state(value);
  }
}

void FP2Component::handle_simple_uint8_binary_report_(const std::vector<uint8_t> &payload,
                                                      binary_sensor::BinarySensor *sensor,
                                                      const char *name) {
  if (payload.size() != 4 || (payload[2] != 0x00 && payload[2] != 0x04)) {
    publish_radar_debug_("uint8_binary_report_malformed", (AttrId)((payload.size() >= 2) ? ((payload[0] << 8) | payload[1]) : 0), payload);
    return;
  }

  bool state = payload[3] != 0;
  ESP_LOGI(TAG, "%s report: %s", name, state ? "ON" : "OFF");
  if (sensor != nullptr) {
    sensor->publish_state(state);
  }
}

void FP2Component::handle_sleep_data_report_(const std::vector<uint8_t> &payload) {
  if (payload.size() < 5 || (payload[2] != 0x05 && payload[2] != 0x06)) {
    publish_radar_debug_("sleep_data_malformed", AttrId::SLEEP_DATA, payload);
    return;
  }

  uint16_t declared_len = (payload[3] << 8) | payload[4];
  if (declared_len != 12 || payload.size() < 5 + declared_len) {
    publish_radar_debug_("sleep_data_bad_len", AttrId::SLEEP_DATA, payload);
    return;
  }

  publish_radar_debug_("radar_report", AttrId::SLEEP_DATA, payload);

  // PROTOCOL 4.2.5: 12-byte item -> TargetID(0), ZoneID(1), Presence(2),
  // followed by 9 bytes of vital-sign / sleep-stage data (format still unknown).
  std::vector<uint8_t> blob(payload.begin() + 5, payload.begin() + 17);
  uint8_t target_id = blob[0];
  uint8_t zone_id = blob[1];
  uint8_t presence = blob[2];
  std::vector<uint8_t> vitals(blob.begin() + 3, blob.end());

  std::string state = "target=" + std::to_string(target_id) +
                      " zone=" + std::to_string(zone_id) +
                      " presence=" + std::to_string(presence) +
                      " vitals=" + format_payload_hex_(vitals, 9);

  ESP_LOGI(TAG, "sleep_data report: %s", state.c_str());
  if (sleep_data_sensor_ != nullptr) {
    sleep_data_sensor_->publish_state(state);
  }
}

void FP2Component::handle_sleep_state_report_(const std::vector<uint8_t> &payload) {
  if (payload.size() != 4 || payload[2] != 0x00) {
    publish_radar_debug_("sleep_state_malformed", AttrId::SLEEP_STATE, payload);
    return;
  }

  uint8_t value = payload[3];
  const char *label = "unknown";
  switch (value) {
    case 0: label = "inactive_or_out"; break;
    case 1: label = "in_bed_or_active"; break;
  }

  std::string state = std::to_string(value) + ":" + label;
  ESP_LOGI(TAG, "sleep_state report: %s", state.c_str());
  if (sleep_state_sensor_ != nullptr) {
    sleep_state_sensor_->publish_state(state);
  }
}

void FP2Component::handle_sleep_event_report_(const std::vector<uint8_t> &payload) {
  if (payload.size() != 4 || payload[2] != 0x00) {
    publish_radar_debug_("sleep_event_malformed", AttrId::SLEEP_EVENT, payload);
    return;
  }

  std::string state = std::to_string(payload[3]);
  ESP_LOGI(TAG, "sleep_event report: %s", state.c_str());
  if (sleep_event_sensor_ != nullptr) {
    sleep_event_sensor_->publish_state(state);
  }
}

void FP2Component::handle_target_posture_report_(const std::vector<uint8_t> &payload) {
  if (payload.size() != 5 || payload[2] != 0x01) {
    publish_radar_debug_("target_posture_malformed", AttrId::TARGET_POSTURE, payload);
    return;
  }

  uint8_t target_or_zone = payload[3];
  uint8_t posture = payload[4];
  std::string state = "id=" + std::to_string(target_or_zone) + " posture=" + std::to_string(posture);
  ESP_LOGI(TAG, "target_posture report: %s", state.c_str());
  if (target_posture_sensor_ != nullptr) {
    target_posture_sensor_->publish_state(state);
  }
}

void FP2Component::handle_people_counting_report_(const std::vector<uint8_t> &payload) {
  if (payload.size() < 5 || payload[2] != 0x06) {
    publish_radar_debug_("people_counting_malformed", AttrId::PEOPLE_COUNTING, payload);
    return;
  }

  uint16_t declared_len = (payload[3] << 8) | payload[4];
  if (declared_len < 7 || payload.size() < 5 + declared_len) {
    publish_radar_debug_("people_counting_bad_len", AttrId::PEOPLE_COUNTING, payload);
    return;
  }

  uint8_t target_or_zone = payload[5];
  uint32_t ontime = ((uint32_t) payload[6] << 24) |
                    ((uint32_t) payload[7] << 16) |
                    ((uint32_t) payload[8] << 8) |
                    (uint32_t) payload[9];
  uint16_t realtime = ((uint16_t) payload[10] << 8) | (uint16_t) payload[11];

  std::string state = "id=" + std::to_string(target_or_zone) +
                      " ontime=" + std::to_string(ontime) +
                      " realtime=" + std::to_string(realtime);
  ESP_LOGI(TAG, "people_counting report: %s", state.c_str());
  if (people_counting_sensor_ != nullptr) {
    people_counting_sensor_->publish_state(state);
  }
}

void FP2Component::handle_response_(AttrId attr_id, const std::vector<uint8_t> &payload) {
  // RESPONSE packets with only 2 bytes (just SubID) are Reverse Read Requests from the radar
  if (payload.size() == 2) {
    handle_reverse_read_request_(attr_id);
  } else {
    // PROTO-01: host reads are fire-and-forget (see enqueue_read_), so this
    // is purely observational — there is no wait state to match/clear here.
    publish_radar_debug_("radar_response", attr_id, payload);
    ESP_LOGD(TAG, "Received Response for 0x%04X (%s) with %u bytes",
             (uint16_t) attr_id, attr_id_to_string_(attr_id),
             static_cast<unsigned>(payload.size()));
  }
}

void FP2Component::handle_reverse_read_request_(AttrId attr_id) {
  ESP_LOGI(TAG, "Received Reverse Query for SubID 0x%04X", (uint16_t) attr_id);

  switch (attr_id) {
    case AttrId::DEVICE_DIRECTION:  // device_direction
      send_reverse_response_(attr_id, (uint8_t)fp2_accel_->get_orientation());
      ESP_LOGD(TAG, "Sending Device Direction: %d", fp2_accel_->get_orientation());
      break;

    case AttrId::ANGLE_SENSOR_DATA:  // angle_sensor_data
      {
        uint8_t angle = fp2_accel_->get_output_angle_z();
        send_reverse_response_(attr_id, angle);
        ESP_LOGD(TAG, "Sending Angle Sensor Data: %d", angle);
      }
      break;

    default:
      publish_radar_debug_("unknown_reverse_query", attr_id, std::vector<uint8_t>{});
      ESP_LOGW(TAG, "Unknown Reverse Query SubID 0x%04X (%s)",
               (uint16_t) attr_id, attr_id_to_string_(attr_id));
      break;
  }
}

// Command Queue Helpers
void FP2Component::enqueue_command_(OpCode type, AttrId attr_id,
                                    uint8_t byte_val) {
  FP2Command cmd;
  cmd.type = type;
  cmd.attr_id = attr_id;
  cmd.retry_count = 0;

  // Payload: [SubID 2] [Type 1] [Data 1]
  cmd.data.push_back((((uint16_t) attr_id) >> 8) & 0xFF);
  cmd.data.push_back(((uint16_t) attr_id) & 0xFF);
  cmd.data.push_back(0x00); // UINT8
  cmd.data.push_back(byte_val);

  command_queue_.push_back(cmd);
}

void FP2Component::enqueue_command_(OpCode type, AttrId attr_id,
                                    uint16_t word_val) {
  FP2Command cmd;
  cmd.type = type;
  cmd.attr_id = attr_id;
  cmd.retry_count = 0;

  // Payload: [SubID 2] [Type 1] [Data 2]
  cmd.data.push_back((((uint16_t) attr_id) >> 8) & 0xFF);
  cmd.data.push_back(((uint16_t) attr_id) & 0xFF);
  cmd.data.push_back(0x01); // UINT16
  cmd.data.push_back((word_val >> 8) & 0xFF);
  cmd.data.push_back(word_val & 0xFF);

  command_queue_.push_back(cmd);
}

void FP2Component::enqueue_command_(OpCode type, AttrId attr_id,
                                    uint32_t dword_val) {
  FP2Command cmd;
  cmd.type = type;
  cmd.attr_id = attr_id;
  cmd.retry_count = 0;

  // Payload: [SubID 2] [Type 1] [Data 4]
  cmd.data.push_back((((uint16_t) attr_id) >> 8) & 0xFF);
  cmd.data.push_back(((uint16_t) attr_id) & 0xFF);
  cmd.data.push_back(0x02); // UINT32
  cmd.data.push_back((dword_val >> 24) & 0xFF);
  cmd.data.push_back((dword_val >> 16) & 0xFF);
  cmd.data.push_back((dword_val >> 8) & 0xFF);
  cmd.data.push_back(dword_val & 0xFF);

  command_queue_.push_back(cmd);
}

void FP2Component::enqueue_command_(OpCode type, AttrId attr_id,
                                    bool bool_val) {
  FP2Command cmd;
  cmd.type = type;
  cmd.attr_id = attr_id;
  cmd.retry_count = 0;

  // Payload: [SubID 2] [Type 1] [Data 1]
  cmd.data.push_back((((uint16_t) attr_id) >> 8) & 0xFF);
  cmd.data.push_back(((uint16_t) attr_id) & 0xFF);
  cmd.data.push_back(0x04); // BOOL
  cmd.data.push_back((uint8_t) bool_val);

  command_queue_.push_back(cmd);
}


void FP2Component::enqueue_command_blob2_(
    AttrId attr_id, const std::vector<uint8_t> &blob_content) {
  FP2Command cmd;
  cmd.type = OpCode::WRITE; // Always Write for these configs
  cmd.attr_id = attr_id;
  cmd.retry_count = 0;

  // Payload: [SubID 2] [Type 1 (0x06)] [Len 2] [Content N]
  cmd.data.push_back((((uint16_t) attr_id) >> 8) & 0xFF);
  cmd.data.push_back(((uint16_t) attr_id) & 0xFF);
  cmd.data.push_back(0x06); // BLOB2

  uint16_t len = blob_content.size();
  cmd.data.push_back((len >> 8) & 0xFF);
  cmd.data.push_back(len & 0xFF);

  cmd.data.insert(cmd.data.end(), blob_content.begin(), blob_content.end());

  command_queue_.push_back(cmd);
}

void FP2Component::enqueue_read_(AttrId attr_id) {
    FP2Command cmd;
    // PROTO-01: host-initiated reads are OpCode::RESPONSE (wire 0x01), not
    // OpCode::READ (wire 0x04, which is the radar's inbound response opcode).
    // Fire-and-forget: send_next_command_() pops it immediately, no wait state.
    cmd.type = OpCode::RESPONSE;
    cmd.attr_id = attr_id;
    cmd.retry_count = 0;

    cmd.data.push_back((((uint16_t) attr_id) >> 8) & 0xFF);
    cmd.data.push_back(((uint16_t) attr_id) & 0xFF);

    command_queue_.push_back(cmd);
}

void FP2Component::set_interference_grid(const std::vector<uint8_t> &grid) {
  ESP_LOGI(TAG, "set_interference_grid called with size: %d", grid.size());
  if (grid.size() == 40) {
    std::copy(grid.begin(), grid.end(), interference_grid_.begin());
    has_interference_grid_ = true;
    ESP_LOGI(TAG, "Interference grid configured successfully");
  } else {
    ESP_LOGW(TAG, "Interference grid size mismatch! Expected 40, got %d", grid.size());
  }
}

void FP2Component::set_exit_grid(const std::vector<uint8_t> &grid) {
  ESP_LOGI(TAG, "set_exit_grid called with size: %d", grid.size());
  if (grid.size() == 40) {
    std::copy(grid.begin(), grid.end(), exit_grid_.begin());
    has_exit_grid_ = true;
    ESP_LOGI(TAG, "Exit grid configured successfully");
  } else {
    ESP_LOGW(TAG, "Exit grid size mismatch! Expected 40, got %d", grid.size());
  }
}

void FP2Component::set_edge_grid(const std::vector<uint8_t> &grid) {
  ESP_LOGI(TAG, "set_edge_grid called with size: %d", grid.size());
  if (grid.size() == 40) {
    std::copy(grid.begin(), grid.end(), edge_grid_.begin());
    has_edge_grid_ = true;
    ESP_LOGI(TAG, "Edge grid configured successfully");
  } else {
    ESP_LOGW(TAG, "Edge grid size mismatch! Expected 40, got %d", grid.size());
  }
}

void FP2Component::set_zones(const std::vector<FP2Zone*> &zones) {
    zones_ = zones;
}

std::string FP2Component::grid_to_hex_card_format(const GridMap &grid) {
  std::string result;
  result.reserve(56);  // 14 rows * 2 bytes * 2 hex chars

  // For each row R (0-13) in the card format
  for (int R = 0; R < 14; R++) {
    // Map to internal row (rows 3-16 in the 20-row grid)
    int internal_row_idx = R;
    int byte_idx = internal_row_idx * 2;

    // Encode the 2 bytes for this row as 4 hex chars
    char hex[5];
    snprintf(hex, sizeof(hex), "%02x%02x", grid[byte_idx], grid[byte_idx + 1]);
    result += hex;
  }

  return result;
}

// void FP2Component::add_zone(uint8_t id, binary_sensor::BinarySensor *sens,
//                             const std::vector<uint8_t> &grid,
//                             uint8_t sensitivity) {
//   FP2Zone z;
//   z.id = id;
//   z.occupancy = sens;
//   z.sensitivity = sensitivity;
//
//   if (grid.size() == 40) {
//     std::copy(grid.begin(), grid.end(), z.grid.begin());
//     zones_.push_back(z);
//   } else {
//     ESP_LOGE(TAG, "Zone %d grid size mismatch: %d != 40", id, grid.size());
//   }
// }

void FP2Component::dump_config() {
  ESP_LOGCONFIG(TAG, "Aqara FP2:");
  ESP_LOGCONFIG(TAG, "  Mounting Position: %d", mounting_position_);
  ESP_LOGCONFIG(TAG, "  Zones: %d", zones_.size());
  ESP_LOGCONFIG(TAG, "  People Counting Report: %s", people_counting_report_enable_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  People Number Report: %s", people_number_enable_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Target Type Report: %s", target_type_enable_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Dwell Time Report: %s", dwell_time_enable_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Walking Distance Report: %s", walking_distance_enable_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Thermodynamic Chart Report: %s", thermodynamic_chart_enable_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Sleep Report: %s", has_sleep_report_enable_ ? (sleep_report_enable_ ? "YES" : "NO") : "not configured");
  ESP_LOGCONFIG(TAG, "  Posture Report: %s", has_posture_report_enable_ ? (posture_report_enable_ ? "YES" : "NO") : "not configured");
  ESP_LOGCONFIG(TAG, "  Fall Detection: %s", has_fall_detection_enabled_ ? (fall_detection_enabled_ ? "YES" : "NO") : "not configured");
  ESP_LOGCONFIG(TAG, "  Debug Probe Reads: %s", debug_probe_reads_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Radar Debug Sensor: %s", radar_debug_sensor_ != nullptr ? "configured" : "not configured");
  if (reset_pin_ != nullptr) {
    LOG_PIN("  Reset Pin: ", reset_pin_);
  }
  for (auto &z : zones_) {
    if (z->presence_sensor != nullptr) {
      LOG_BINARY_SENSOR("  ", "Zone Presence", z->presence_sensor);
    }
    if (z->motion_sensor != nullptr) {
      LOG_BINARY_SENSOR("  ", "Zone Motion", z->motion_sensor);
    }
  }
}

JsonDocument FP2Component::get_map_config_json() {
  JsonDocument doc;

  // Deserialize the compile-time JSON
  DeserializationError error = deserializeJson(doc, this->map_config_json_);

  if (error) {
    ESP_LOGE(TAG, "Failed to parse map config JSON: %s", error.c_str());
    return doc;
  }

  // The base structure is already in the JSON, but we can add runtime data if needed
  // For example, current zone states could be added here in the future

  return doc;
}

const char* FP2Component::get_mounting_position_string_() {
  switch (mounting_position_) {
    case 0x02: return "left_upper_corner";
    case 0x03: return "right_upper_corner";
    default: return "wall";
  }
}

void FP2Component::json_get_map_data(JsonObject root) {
  // Global settings
  root["mounting_position"] = get_mounting_position_string_();
  root["left_right_reverse"] = left_right_reverse_;

  // Global grids (if configured)
  if (has_interference_grid_) {
    root["interference_grid"] = grid_to_hex_card_format(interference_grid_);
  }
  if (has_exit_grid_) {
    root["exit_grid"] = grid_to_hex_card_format(exit_grid_);
  }
  if (has_edge_grid_) {
    root["edge_grid"] = grid_to_hex_card_format(edge_grid_);
  }

  // Zones
  if (!zones_.empty()) {
    JsonArray zones_array = root["zones"].to<JsonArray>();
    for (FP2Zone *zone : zones_) {
      JsonObject zone_obj = zones_array.add<JsonObject>();
      zone_obj["sensitivity"] = zone->sensitivity;
      zone_obj["grid"] = grid_to_hex_card_format(zone->grid);
      if (zone->presence_sensor != nullptr) {
        zone_obj["presence_sensor"] = zone->presence_sensor->get_name().c_str();
      }
    }
  }
}

} // namespace aqara_fp2
} // namespace esphome
