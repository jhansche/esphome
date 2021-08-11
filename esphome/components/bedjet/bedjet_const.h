#pragma once

namespace esphome {
namespace bedjet {


enum BedjetMode : uint8_t {
  /// BedJet is Off
  MODE_STANDBY = 0,
  /// BedJet is in Heat mode (limited to 4 hours)
  MODE_HEAT = 1,
  /// BedJet is in Turbo mode (high heat, limited time)
  MODE_TURBO = 2,
  /// BedJet is in Extended Heat mode (limited to 10 hours)
  MODE_EXTHT = 3,
  /// BedJet is in Cool mode (actually "Fan only" mode)
  MODE_COOL = 4,
  /// BedJet is in Dry mode (high speed, no heat)
  MODE_DRY = 5,
  /// BedJet is in "wait" mode, a step during a biorhythm program
  MODE_WAIT = 6,
};

enum BedjetButton : uint8_t {
  /// Turn BedJet off
  BTN_OFF = 0x1,
  /// Enter Cool mode (fan only)
  BTN_COOL = 0x2,
  /// Enter Heat mode (limited to 4 hours)
  BTN_HEAT = 0x3,
  /// Enter Turbo mode (high heat, limited to 10 minutes)
  BTN_TURBO = 0x4,
  /// Enter Dry mode (high speed, no heat)
  BTN_DRY = 0x5,
  /// Enter Extended Heat mode (limited to 10 hours)
  BTN_EXTHT = 0x6,

  /// Start the M1 biorhythm/preset program
  BTN_M1 = 0x20,
  /// Start the M2 biorhythm/preset program
  BTN_M2 = 0x21,
  /// Start the M3 biorhythm/preset program
  BTN_M3 = 0x22,

  /* These are "MAGIC" buttons */

  /// Turn debug mode on/off
  MAGIC_DEBUG_ON = 0x40,
  MAGIC_DEBUG_OFF = 0x41,
  /// Perform a connection test.
  MAGIC_CONNTEST = 0x42,
  /// Request a firmware update. This will also restart the Bedjet.
  MAGIC_UPDATE = 0x43,
};

enum BedjetCommand : uint8_t {
  CMD_BUTTON = 0x1,
  CMD_SET_TEMP = 0x3,
  CMD_STATUS = 0x6,
  CMD_SET_FAN = 0x7,
  CMD_SET_TIME = 0x8,
};

}  // namespace bedjet
}  // namespace esphome
