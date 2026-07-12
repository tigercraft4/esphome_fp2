#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"

#include "../aqara_fp2_accel/aqara_fp2_accel.h"

#include <ArduinoJson.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace esphome {
namespace aqara_fp2 {

static const char *const TAG = "aqara_fp2";

// 40-byte Grid Map
using GridMap = std::array<uint8_t, 40>;

struct FP2Zone : public Component {
  FP2Zone(uint8_t zone_id, const GridMap grid, uint8_t sensitivity) : id(zone_id), grid(grid), sensitivity(sensitivity) {}

  void set_presence_sensor(binary_sensor::BinarySensor *sensor) {
    this->presence_sensor = sensor;
  }

  void set_motion_sensor(binary_sensor::BinarySensor *sensor) {
    this->motion_sensor = sensor;
  }

  void set_map_sensor(text_sensor::TextSensor *sensor) {
    this->map_sensor = sensor;
  }

  void publish_presence(bool state) {
    if (this->presence_sensor != nullptr) {
      this->presence_sensor->publish_state(state);
    }
  }

  void publish_motion(bool state) {
    if (this->motion_sensor != nullptr) {
      this->motion_sensor->publish_state(state);
    }
  }

  void publish_map(const std::string &map_hex) {
    if (this->map_sensor != nullptr) {
      this->map_sensor->publish_state(map_hex);
    }
  }

  // Zone motion events (0x0115) are momentary. Hold the motion sensor ON and
  // only release it after motion_timeout_ms with no new movement event (debounce).
  void set_motion_timeout(uint32_t ms) { this->motion_timeout_ms = ms; }

  void note_motion_event(uint32_t now) {
    this->last_motion_millis = now;
    if (!this->motion_active) {
      this->motion_active = true;
      this->publish_motion(true);
    }
  }

  void tick_motion(uint32_t now) {
    if (this->motion_active && (now - this->last_motion_millis) >= this->motion_timeout_ms) {
      this->motion_active = false;
      this->publish_motion(false);
    }
  }

  void reset_motion() {
    this->motion_active = false;
    this->last_motion_millis = 0;
    this->publish_motion(false);
  }

  uint8_t id;
  esphome::binary_sensor::BinarySensor *presence_sensor{nullptr};
  esphome::binary_sensor::BinarySensor *motion_sensor{nullptr};
  esphome::text_sensor::TextSensor *map_sensor{nullptr};
  GridMap grid;
  uint8_t sensitivity; // 1=Low, 2=Med, 3=High
  uint32_t motion_timeout_ms{5000};
  uint32_t last_motion_millis{0};
  bool motion_active{false};
};

class FP2Component;

enum class DataType : uint8_t {
    UINT8 = 0x00,
    UINT16 = 0x01,
    UINT32 = 0x02,
    VOID = 0x03,
    BOOL = 0x04,
    STRING = 0x05,
    BINARY = 0x06,
};

enum class OpCode : uint8_t {
  // Device -> Host: Standard Response to Read (Values).
  // Device -> Host: Reverse Read Request (SubID only, len=2).
  RESPONSE = 0x01,

  // Host -> Device: Write Attribute (Values).
  WRITE = 0x02,

  // Both: Acknowledge.
  ACK = 0x03,

  // Host -> Device: Standard Read Request (SubID only).
  // Host -> Device: Reverse Read Response (Values, in response to 0x01 Query).
  READ = 0x04,

  // Device -> Host: Async Report.
  REPORT = 0x05,
};

enum class AttrId : uint16_t {
    RADAR_HW_VERSION                = 0x0101,
    RADAR_SW_VERSION                = 0x0102,
    MOTION_DETECT                   = 0x0103,
    PRESENCE_DETECT                 = 0x0104,
    WORK_MODE                       = 0x0116,
    MONITOR_MODE                    = 0x0105, // Detection direction (0=default, 1=L/R)
    LEFT_RIGHT_REVERSE              = 0x0122, // L/R swap (0/1/2)
    PRESENCE_DETECT_SENSITIVITY     = 0x0111, // Sensitivity (1-3)
    CLOSING_SETTING                 = 0x0106, // Proximity (0=far, 1=med, 2=close)
    ZONE_CLOSE_AWAY_ENABLE          = 0x0153, // Zone N close/away enable
    FALL_DETECTION                  = 0x0121, // Fall detection enable
    FALL_SENSITIVITY                = 0x0123, // Fall sensitivity
    RADAR_INTERFERENCE_AUTO_SETTING = 0x0125,
    OTA_SET_FLAG                    = 0x0127,
    FALL_OVERTIME_REPORT_PERIOD     = 0x0134,
    FALL_OVERTIME_DETECTION         = 0x0135,
    INTERFERENCE_AUTO_ENABLE        = 0x0139,
    EDGE_AUTO_SETTING               = 0x0149,
    EDGE_AUTO_ENABLE                = 0x0150,
    TARGET_POSTURE                  = 0x0154,
    PEOPLE_COUNTING                 = 0x0155,
    SLEEP_REPORT_ENABLE             = 0x0156,
    POSTURE_REPORT_ENABLE           = 0x0157,
    PEOPLE_COUNT_REPORT_ENABLE      = 0x0158, // People counting enable
    SLEEP_DATA                      = 0x0159,
    DELETE_FALSE_TARGET             = 0x0160,
    SLEEP_STATE                     = 0x0161,
    PEOPLE_NUMBER_ENABLE            = 0x0162, // People number enable
    TARGET_TYPE_ENABLE              = 0x0163, // AI person detection
    REALTIME_PEOPLE_NUMBER          = 0x0164,
    REALTIME_PEOPLE_COUNTING        = 0x0166,
    SLEEP_PRESENCE                  = 0x0167,
    SLEEP_MOUNT_POSITION            = 0x0168, // Sleep mount position
    SLEEP_ZONE_SIZE                 = 0x0169, // Sleep zone dimensions
    WALL_CORNER_POS                 = 0x0170, // Wall/corner position
    SLEEP_INOUT_STATE               = 0x0171,
    DWELL_TIME_ENABLE               = 0x0172, // Dwell tracking
    WALK_DISTANCE_ENABLE            = 0x0173, // Walking distance
    WALK_DISTANCE_ALL               = 0x0174,
    SLEEP_EVENT                     = 0x0176,
    SLEEP_EVENT_DESCRIPTOR          = 0x0177,
    SLEEP_BED_HEIGHT                = 0x0178,
    OVERHEAD_HEIGHT                 = 0x0179,
    FALL_DELAY_TIME                 = 0x0180,
    INTERFERENCE_MAP                = 0x0110, // Interference map (40B)
    ENTRY_EXIT_MAP                  = 0x0109, // Enter/exit zones (40B)
    EDGE_MAP                        = 0x0107, // Detection boundary (40B)
    ZONE_MAP                        = 0x0114, // Zone N area map (1B ID + 40B)
    ZONE_SENSITIVITY                = 0x0151, // Zone N sensitivity
    ZONE_ACTIVATION_LIST            = 0x0202, // Auxiliary config (32B)
    DETECT_ZONE_TYPE                = 0x0152, // Zone N type
    DEVICE_DIRECTION                = 0x0143,
    ANGLE_SENSOR_DATA               = 0x0120,
    LOCATION_REPORT_ENABLE          = 0x0112,
    RESET_ABSENT_STATUS             = 0x0113,
    ZONE_PRESENCE                   = 0x0142,
    LOCATION_TRACKING_DATA          = 0x0117,
    THERMO_EN                       = 0x0138,
    THERMO_DATA                     = 0x0141,
    TEMPERATURE                     = 0x0128,
    DETECT_ZONE_MOTION              = 0x0115,
    ONTIME_PEOPLE_NUMBER            = 0x0165,
    DEBUG_LOG                       = 0x0201,
    RADAR_FLASH_ID                  = 0x0302,
    RADAR_ID                        = 0x0303,
    RADAR_CALIBRATION_RESULT        = 0x0305,
    INVALID                         = 0xFFFF,
};

struct FP2Command {
  OpCode type;
  AttrId attr_id;
  std::vector<uint8_t> data;
  uint32_t last_send_time;
  uint8_t retry_count;
};

class FP2LocationSwitch : public switch_::Switch {
public:
  void set_parent(FP2Component *parent) { parent_ = parent; }

protected:
  void write_state(bool state) override;
  FP2Component *parent_{nullptr};
};

class FP2Component : public Component, public uart::UARTDevice {
public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  // Configuration setters
  void set_radar_reset_pin(GPIOPin *pin) { reset_pin_ = pin; }
  void set_mounting_position(uint8_t pos) { mounting_position_ = pos; }
  void set_left_right_reverse(bool val) { left_right_reverse_ = val; }

  void set_fall_detection(bool val) {
    fall_detection_enabled_ = val;
    has_fall_detection_enabled_ = true;
  }

  void set_fall_detection_sensitivity(uint8_t val) {
    fall_detection_sensitivity_ = val;
    has_fall_detection_sensitivity_ = true;
  }

  void set_sleep_report_enable(bool val) {
    sleep_report_enable_ = val;
    has_sleep_report_enable_ = true;
  }
  void set_posture_report_enable(bool val) {
    posture_report_enable_ = val;
    has_posture_report_enable_ = true;
  }
  void set_people_counting_report_enable(bool val) { people_counting_report_enable_ = val; }
  void set_people_number_enable(bool val) { people_number_enable_ = val; }
  void set_target_type_enable(bool val) { target_type_enable_ = val; }
  void set_dwell_time_enable(bool val) { dwell_time_enable_ = val; }
  void set_walking_distance_enable(bool val) { walking_distance_enable_ = val; }
  void set_thermodynamic_chart_enable(bool val) { thermodynamic_chart_enable_ = val; }

  void set_interference_grid(const std::vector<uint8_t> &grid);
  void set_exit_grid(const std::vector<uint8_t> &grid);
  void set_edge_grid(const std::vector<uint8_t> &grid);

  void set_presence_sensitivity(uint8_t val) { global_presence_sensitivity_ = val; }
  void set_motion_sensor(binary_sensor::BinarySensor *sensor) { global_motion_sensor_ = sensor; }
  void set_presence_sensor(binary_sensor::BinarySensor *sensor) { global_presence_sensor_ = sensor; }

  //void add_zone(uint8_t id, binary_sensor::BinarySensor *sens,
  //              const std::vector<uint8_t> &grid, uint8_t sensitivity);
  void set_zones(const std::vector<FP2Zone*> &zones);

  void set_target_tracking_sensor(text_sensor::TextSensor *sensor) {
    target_tracking_sensor_ = sensor;
  }
  void set_location_report_switch(FP2LocationSwitch *sw) {
    location_report_switch_ = sw;
    sw->set_parent(this);
  }

  void set_edge_label_grid_sensor(text_sensor::TextSensor *sensor) {
    ESP_LOGI(TAG, "set_edge_label_grid_sensor called (has_edge_grid_=%d)", has_edge_grid_);
    edge_label_grid_sensor_ = sensor;
    if (has_edge_grid_ && edge_label_grid_sensor_ != nullptr) {
      ESP_LOGI(TAG, "Publishing edge label grid from setter");
      edge_label_grid_sensor_->publish_state(grid_to_hex_card_format(edge_grid_));
    } else {
      ESP_LOGW(TAG, "NOT publishing edge label grid from setter (has_grid=%d, sensor=%p)", has_edge_grid_, edge_label_grid_sensor_);
    }
  }
  void set_entry_exit_grid_sensor(text_sensor::TextSensor *sensor) {
    ESP_LOGI(TAG, "set_entry_exit_grid_sensor called (has_exit_grid_=%d)", has_exit_grid_);
    entry_exit_grid_sensor_ = sensor;
    if (has_exit_grid_ && entry_exit_grid_sensor_ != nullptr) {
      ESP_LOGI(TAG, "Publishing entry/exit grid from setter");
      entry_exit_grid_sensor_->publish_state(grid_to_hex_card_format(exit_grid_));
    } else {
      ESP_LOGW(TAG, "NOT publishing entry/exit grid from setter (has_grid=%d, sensor=%p)", has_exit_grid_, entry_exit_grid_sensor_);
    }
  }
  void set_interference_grid_sensor(text_sensor::TextSensor *sensor) {
    ESP_LOGI(TAG, "set_interference_grid_sensor called (has_interference_grid_=%d)", has_interference_grid_);
    interference_grid_sensor_ = sensor;
    if (has_interference_grid_ && interference_grid_sensor_ != nullptr) {
      ESP_LOGI(TAG, "Publishing interference grid from setter");
      interference_grid_sensor_->publish_state(grid_to_hex_card_format(interference_grid_));
    } else {
      ESP_LOGW(TAG, "NOT publishing interference grid from setter (has_grid=%d, sensor=%p)", has_interference_grid_, interference_grid_sensor_);
    }
  }
  void set_mounting_position_sensor(text_sensor::TextSensor *sensor) {
    mounting_position_sensor_ = sensor;
    if (mounting_position_sensor_ != nullptr) {
      const char* pos_str;
      switch (mounting_position_) {
        case 0x02: pos_str = "left_upper_corner"; break;
        case 0x03: pos_str = "right_upper_corner"; break;
        default: pos_str = "wall"; break;
      }
      mounting_position_sensor_->publish_state(pos_str);
    }
  }
  void set_radar_temperature_sensor(sensor::Sensor *sensor) {
      radar_temperature_sensor_ = sensor;
  }
  void set_realtime_people_number_sensor(sensor::Sensor *sensor) {
      realtime_people_number_sensor_ = sensor;
  }
  void set_ontime_people_number_sensor(sensor::Sensor *sensor) {
      ontime_people_number_sensor_ = sensor;
  }
  void set_realtime_people_counting_sensor(sensor::Sensor *sensor) {
      realtime_people_counting_sensor_ = sensor;
  }
  void set_walking_distance_sensor(sensor::Sensor *sensor) {
      walking_distance_sensor_ = sensor;
  }
  void set_radar_software_sensor(text_sensor::TextSensor *sensor) {
      radar_software_sensor_ = sensor;
  }
  void set_radar_debug_sensor(text_sensor::TextSensor *sensor) {
      radar_debug_sensor_ = sensor;
  }
  void set_sleep_state_sensor(text_sensor::TextSensor *sensor) {
      sleep_state_sensor_ = sensor;
  }
  void set_sleep_data_sensor(text_sensor::TextSensor *sensor) {
      sleep_data_sensor_ = sensor;
  }
  void set_sleep_event_sensor(text_sensor::TextSensor *sensor) {
      sleep_event_sensor_ = sensor;
  }
  void set_target_posture_sensor(text_sensor::TextSensor *sensor) {
      target_posture_sensor_ = sensor;
  }
  void set_people_counting_sensor(text_sensor::TextSensor *sensor) {
      people_counting_sensor_ = sensor;
  }
  void set_sleep_presence_sensor(binary_sensor::BinarySensor *sensor) {
      sleep_presence_sensor_ = sensor;
  }
  void set_sleep_inout_sensor(binary_sensor::BinarySensor *sensor) {
      sleep_inout_sensor_ = sensor;
  }
  void set_debug_probe_reads(bool enabled) { debug_probe_reads_ = enabled; }

  void set_fp2_accel(aqara_fp2_accel::AqaraFP2Accel *accel) {
      fp2_accel_ = accel;
  }

  void set_location_reporting_enabled(bool enabled);
  void force_detection_config();
  void read_detection_config();
  void read_mode_calibration_config();
  void read_attr(uint16_t attr_id);
  void write_attr_uint8(uint16_t attr_id, uint8_t value);
  void write_attr_uint16(uint16_t attr_id, uint16_t value);
  void write_attr_uint32(uint16_t attr_id, uint32_t value);
  void write_attr_bool(uint16_t attr_id, bool value);
  void configure_sleep_mode(uint16_t width, uint16_t length, uint8_t mount_position);
  void set_work_mode(uint8_t mode);
  void set_ai_target_filter_enabled(bool enabled);
  void calibrate_empty_room();
  void reset_radar();

  // Grid format conversion
  std::string grid_to_hex_card_format(const GridMap &grid);

  // Map configuration
  void set_map_config_json(const std::string &json) { map_config_json_ = json; }
  JsonDocument get_map_config_json();
  void json_get_map_data(JsonObject root);

protected:
  // Internal logic
  void process_command_queue_();
  void send_next_command_();
  void write_command_frame_(const FP2Command &cmd, bool track_timeout);
  void handle_incoming_byte_(uint8_t byte);
  const char* get_mounting_position_string_();
  void handle_parsed_frame_(uint8_t type, AttrId attr_id,
                            const std::vector<uint8_t> &payload);
  void handle_ack_(AttrId attr_id);
  void handle_report_(AttrId attr_id, const std::vector<uint8_t> &payload);
  void handle_location_tracking_report_(const std::vector<uint8_t> &payload);
  void handle_temperature_report_(const std::vector<uint8_t> &payload);
  void handle_debug_log_report_(const std::vector<uint8_t> &payload);
  void handle_simple_uint32_report_(const std::vector<uint8_t> &payload,
                                    sensor::Sensor *sensor, const char *name);
  void handle_simple_uint8_binary_report_(const std::vector<uint8_t> &payload,
                                          binary_sensor::BinarySensor *sensor,
                                          const char *name);
  void handle_sleep_data_report_(const std::vector<uint8_t> &payload);
  void handle_sleep_state_report_(const std::vector<uint8_t> &payload);
  void handle_sleep_event_report_(const std::vector<uint8_t> &payload);
  void handle_target_posture_report_(const std::vector<uint8_t> &payload);
  void handle_people_counting_report_(const std::vector<uint8_t> &payload);
  void handle_response_(AttrId attr_id, const std::vector<uint8_t> &payload);
  void handle_reverse_read_request_(AttrId attr_id);
  void send_ack_(AttrId attr_id);
  const char* attr_id_to_string_(AttrId attr_id);
  const char* op_code_to_string_(uint8_t type);
  std::string format_payload_hex_(const std::vector<uint8_t> &payload, size_t max_bytes);
  void publish_radar_debug_(const char *event, AttrId attr_id,
                            const std::vector<uint8_t> &payload);

  // Initialization
  void perform_reset_();
  void check_initialization_();

  aqara_fp2_accel::AqaraFP2Accel *fp2_accel_{nullptr};

  GPIOPin *reset_pin_{nullptr};
  bool init_done_{false};
  uint32_t last_radar_frame_millis_{0};
  uint32_t last_heartbeat_millis_{0};

  // Configuration State
  uint8_t mounting_position_{0x01}; // Default Wall
  bool left_right_reverse_{false};
  bool fall_detection_enabled_{false};
  bool has_fall_detection_enabled_{false};
  uint8_t fall_detection_sensitivity_{1};
  bool has_fall_detection_sensitivity_{false};
  bool sleep_report_enable_{false};
  bool has_sleep_report_enable_{false};
  bool posture_report_enable_{false};
  bool has_posture_report_enable_{false};
  bool people_counting_report_enable_{true};
  bool people_number_enable_{true};
  bool target_type_enable_{false};
  bool dwell_time_enable_{false};
  bool walking_distance_enable_{false};
  bool thermodynamic_chart_enable_{true};

  // Grids (Optional)
  GridMap interference_grid_{};
  bool has_interference_grid_{false};
  GridMap exit_grid_{};
  bool has_exit_grid_{false};
  GridMap edge_grid_{};
  bool has_edge_grid_{false};

  // Global zone
  uint8_t global_presence_sensitivity_{2}; // Default Medium
  binary_sensor::BinarySensor *global_presence_sensor_{nullptr};
  binary_sensor::BinarySensor *global_motion_sensor_{nullptr};

  // Zones
  std::vector<FP2Zone*> zones_;
  text_sensor::TextSensor *target_tracking_sensor_{nullptr};
  FP2LocationSwitch *location_report_switch_{nullptr};
  bool location_reporting_active_{true};
  uint32_t last_location_debug_millis_{0};
  uint8_t last_location_target_count_{0xFF};

  // Grid text sensors
  text_sensor::TextSensor *edge_label_grid_sensor_{nullptr};
  text_sensor::TextSensor *entry_exit_grid_sensor_{nullptr};
  text_sensor::TextSensor *interference_grid_sensor_{nullptr};
  text_sensor::TextSensor *mounting_position_sensor_{nullptr};

  sensor::Sensor *radar_temperature_sensor_{nullptr};
  sensor::Sensor *realtime_people_number_sensor_{nullptr};
  sensor::Sensor *ontime_people_number_sensor_{nullptr};
  sensor::Sensor *realtime_people_counting_sensor_{nullptr};
  sensor::Sensor *walking_distance_sensor_{nullptr};
  text_sensor::TextSensor *radar_software_sensor_{nullptr};
  text_sensor::TextSensor *radar_debug_sensor_{nullptr};
  text_sensor::TextSensor *sleep_data_sensor_{nullptr};
  text_sensor::TextSensor *sleep_state_sensor_{nullptr};
  text_sensor::TextSensor *sleep_event_sensor_{nullptr};
  text_sensor::TextSensor *target_posture_sensor_{nullptr};
  text_sensor::TextSensor *people_counting_sensor_{nullptr};
  binary_sensor::BinarySensor *sleep_presence_sensor_{nullptr};
  binary_sensor::BinarySensor *sleep_inout_sensor_{nullptr};
  bool debug_probe_reads_{false};

  // Map Configuration (compile-time generated)
  std::string map_config_json_;

  // Communication State
  std::deque<FP2Command> command_queue_;

  // Frame Decoder State
  enum DecoderState {
    SYNC,
    VER_H,
    VER_L,
    SEQ,
    OPCODE,
    LEN_H,
    LEN_L,
    H_CHECK,
    PAYLOAD,
    CRC_L,
    CRC_H
  } state_{SYNC};

  uint8_t rx_seq_;
  uint8_t rx_opcode_;
  uint16_t rx_len_;
  std::vector<uint8_t> rx_payload_;
  uint16_t rx_crc_;

  // Rolling checksum for header
  uint16_t header_sum_{0};

  // Ack Manager
  // We track the SubID of tAttrId::INVALID command we are currently waiting for an ACK for.
  // 0xFFFF = Not waiting.
  AttrId waiting_for_ack_attr_id_{AttrId::INVALID};
  AttrId waiting_for_response_attr_id_{AttrId::INVALID};
  uint32_t last_command_sent_millis_{0};
  static const uint32_t ACK_TIMEOUT_MS = 500;
  static const uint32_t READ_TIMEOUT_MS = 500;
  static const uint8_t MAX_RETRIES = 3;

  void enqueue_command_(OpCode type, AttrId attr_id, uint8_t byte_val);
  void enqueue_command_(OpCode type, AttrId attr_id, uint16_t word_val);
  void enqueue_command_(OpCode type, AttrId attr_id, uint32_t dword_val);
  void enqueue_command_(OpCode type, AttrId attr_id, bool bool_val);
  void enqueue_command_blob2_(AttrId attr_id,
                              const std::vector<uint8_t> &blob_content);
  void enqueue_read_(AttrId attr_id);
  void send_reverse_response_(AttrId attr_id, uint8_t byte_val);
};

} // namespace aqara_fp2
} // namespace esphome
