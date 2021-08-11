#pragma once

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include "bedjet_const.h"

namespace esphome {
namespace bedjet {

struct BedjetPacket {
  uint8_t data_length;
  BedjetCommand command;
  uint8_t data[2];
};


struct BedjetFlags {
  /* uint8_t */
  int a_: 1; // 0x80
  int b_: 1; // 0x40
  int conn_test_passed: 1; ///< (0x20) Bit is set `1` if the last connection test passed.
  int leds_enabled: 1; ///< (0x10) Bit is set `1` if the LEDs on the device are enabled.
  int c_: 1; // 0x08
  int units_setup: 1; ///< (0x04) Bit is set `1` if the device's units have been configured.
  int d_: 1; // 0x02
  int beeps_muted: 1; ///< (0x01) Bit is set `1` if the device's sound output is muted.
} __attribute__((packed));

enum BedjetPacketFormat : uint8_t {
  PACKET_FORMAT_DEBUG = 0x05,   //  5
  PACKET_FORMAT_V3_HOME = 0x56, // 86
};

enum BedjetPacketType : uint8_t {
  PACKET_TYPE_STATUS = 0x1,
  PACKET_TYPE_DEBUG = 0x2,
};

/** The format of a BedJet V3 status packet. */
struct BedjetStatusPacket {
  // [0]
  uint8_t is_partial: 8; ///< `1` indicates that this is a partial packet, and more data can be read directly from the characteristic.
  BedjetPacketFormat packet_format: 8; ///< BedjetPacketFormat::PACKET_FORMAT_V3_HOME for BedJet V3 status packet format.
                                       ///< BedjetPacketFormat::PACKET_FORMAT_DEBUG for debugging packets.
  uint8_t expecting_length: 8; ///< The expected total length of the status packet after merging the additional packet.
  BedjetPacketType packet_type: 8; ///< Typically BedjetPacketType::PACKET_TYPE_STATUS for BedJet V3 status packet.

  // [4]
  uint8_t time_remaining_hrs: 8; ///< Hours remaining in program runtime
  uint8_t time_remaining_mins: 8; ///< Minutes remaining in program runtime
  uint8_t time_remaining_secs: 8; ///< Seconds remaining in program runtime

  // [7]
  uint8_t actual_temp_step: 8; ///< Actual temp of the air blown by the BedJet fan; value represents `2 * degrees_celsius`.
                               ///< See #bedjet_temp_to_c_ and #bedjet_temp_to_f_
  uint8_t target_temp_step: 8; ///< Target temp that the BedJet will try to heat to. See #actual_temp_step.

  // [9]
  BedjetMode mode: 8; ///< BedJet operating mode.

  // [10]
  uint8_t fan_step: 8; ///< BedJet fan speed; value is in the 0-19 range, representing 5% increments (5%-100%): `5 + 5 * fan_step`
  uint8_t max_hrs: 8; ///< Max hours of mode runtime
  uint8_t max_mins: 8; ///< Max minutes of mode runtime
  uint8_t min_temp_step: 8; ///< Min temp allowed in mode. See #actual_temp_step.
  uint8_t max_temp_step: 8; ///< Max temp allowed in mode. See #actual_temp_step.

  // [15-16]
  uint16_t turbo_time: 16; ///< Time remaining in BedjetMode::MODE_TURBO.

  // [17]
  uint8_t ambient_temp_step: 8; ///< Current ambient air temp. This is the coldest air the BedJet can blow. See #actual_temp_step.
  uint8_t shutdown_reason: 8; ///< The reason for the last device shutdown.

  // [19-25]; the initial partial packet cuts off here after [19]
  // Skip 7 bytes?
  uint32_t __skip_1_: 32; // Unknown 19-22 = 0x01810112

  uint16_t __skip_2_: 16; // Unknown 23-24 = 0x1310
  uint8_t __skip_3_: 8;   // Unknown 25 = 0x00

  // [26]
  //   0x18(24) = "Connection test has completed OK"
  //   0x1a(26) = "Firmware update is not needed"
  uint8_t update_phase: 8; ///< The current status/phase of a firmware update.

  // [27]
  // FIXME: cannot nest packed struct of matching length here?
  /* BedjetFlags */ uint8_t flags: 8; /// See BedjetFlags for the packed byte flags.
  // [28-31]; 20+11 bytes
  uint32_t __skip_4_: 32; // Unknown

} __attribute__((packed));

/// Converts a BedJet temp step into degrees Celsius.
static float bedjet_temp_to_c_(const uint8_t temp) {
  // BedJet temp is "C*2"; to get C, divide by 2.
  return temp / 2.0f;
}

/// Converts a BedJet temp step into degrees Fahrenheit.
static float bedjet_temp_to_f_(const uint8_t temp) {
  // BedJet temp is "C*2"; to get F, multiply by 0.9 (half 1.8) and add 32.
  return 0.9f * temp + 32.0f;
}

/// Converts a BedJet fan step to a speed percentage, in the range of 5% to 100%.
static uint8_t bedjet_fan_step_to_speed_(const uint8_t fan) {
  //  0 =  5%
  // 19 = 100%
  return 5 * fan + 5;
}

static const char * BEDJET_FAN_STEP_NAMES[20] = {
   "5%",  "10%",
  "15%",  "20%",
  "25%",  "30%",
  "35%",  "40%",
  "45%",  "50%",
  "55%",  "60%",
  "65%",  "70%",
  "75%",  "80%",
  "85%",  "90%",
  "95%",  "100%"
};

static const char * bedjet_fan_step_to_fan_mode_(const uint8_t fan_step) {
  if (fan_step >= 0 && fan_step <= 19) return BEDJET_FAN_STEP_NAMES[fan_step];
  return nullptr;
}

static uint8_t bedjet_fan_speed_to_step_(const std::string fan_step_percent) {
  if (fan_step_percent == "5%") return 0;
  if (fan_step_percent == "10%") return 1;
  if (fan_step_percent == "15%") return 2;
  if (fan_step_percent == "20%") return 3;
  if (fan_step_percent == "25%") return 4;
  if (fan_step_percent == "30%") return 5;
  if (fan_step_percent == "35%") return 6;
  if (fan_step_percent == "40%") return 7;
  if (fan_step_percent == "45%") return 8;
  if (fan_step_percent == "50%") return 9;
  if (fan_step_percent == "55%") return 10;
  if (fan_step_percent == "60%") return 11;
  if (fan_step_percent == "65%") return 12;
  if (fan_step_percent == "70%") return 13;
  if (fan_step_percent == "75%") return 14;
  if (fan_step_percent == "80%") return 15;
  if (fan_step_percent == "85%") return 16;
  if (fan_step_percent == "90%") return 17;
  if (fan_step_percent == "95%") return 18;
  if (fan_step_percent == "100%") return 19;
  return -1;
}

/** This class is responsible for encoding command packets and decoding status packets.
 *
 * Status Packets
 * ==============
 * The BedJet protocol depends on registering for notifications on the esphome::BedJet::BEDJET_SERVICE_UUID
 * characteristic. If the BedJet is on, it will send rapid updates as notifications. If it is off,
 * it generally will not notify of any status.
 *
 * As the BedJet V3's BedjetStatusPacket exceeds the buffer size allowed for BLE notification packets,
 * the notification packet will contain `BedjetStatusPacket::is_partial == 1`. When that happens, an additional
 * read of the esphome::BedJet::BEDJET_SERVICE_UUID characteristic will contain the second portion of the
 * full status packet.
 *
 * Command Packets
 * ===============
 * This class supports encoding a number of BedjetPacket commands:
 * - Button press
 *   This simulates a press of one of the BedjetButton values.
 *   - BedjetPacket#command = BedjetCommand::CMD_BUTTON
 *   - BedjetPacket#data [0] contains the BedjetButton value
 * - Set target temp
 *   This sets the BedJet's target temp to a concrete temperature value.
 *   - BedjetPacket#command = BedjetCommand::CMD_SET_TEMP
 *   - BedjetPacket#data [0] contains the BedJet temp value; see BedjetStatusPacket#actual_temp_step
 * - Set fan speed
 *   This sets the BedJet fan speed.
 *   - BedjetPacket#command = BedjetCommand::CMD_SET_FAN
 *   - BedjetPacket#data [0] contains the BedJet fan step in the range 0-19.
 * - Set current time
 *   The BedJet needs to have its clock set properly in order to run the biorhythm programs, which might
 *   contain time-of-day based step rules.
 *   - BedjetPacket#command = BedjetCommand::CMD_SET_TIME
 *   - BedjetPacket#data [0] is hours, [1] is minutes
 */
class BedjetCodec {
 public:

  BedjetPacket *get_button_request(BedjetButton button);
  BedjetPacket *get_set_target_temp_request(float temperature);
  BedjetPacket *get_set_fan_speed_request(const uint8_t fan_step);
  BedjetPacket *get_set_time_request(const uint8_t hour, const uint8_t minute);

  bool decode_notify(const uint8_t *data, uint16_t length);
  void decode_extra(const uint8_t *data, uint16_t length);

  inline bool has_status() { return this->status_packet_.has_value(); }
  const optional<BedjetStatusPacket> &get_status_packet() const { return this->status_packet_; }
  void clear_status() { this->status_packet_.reset(); }

 protected:
  BedjetPacket *clean_packet_();

  uint8_t last_buffer_size_ = 0;

  BedjetPacket packet_;

  optional<BedjetStatusPacket> status_packet_;
  BedjetStatusPacket buf_;
};

}  // namespace bedjet
}  // namespace esphome
