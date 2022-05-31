#pragma once

#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "bedjet_base.h"
#include "bedjet_child.h"

#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#endif

#ifdef USE_ESP32

#include <esp_gattc_api.h>

namespace esphome {
namespace bedjet {

namespace espbt = esphome::esp32_ble_tracker;

// Enable temporary cross-reference: see bedjet_climate.h
class Bedjet;
class BedJetClient;

static const espbt::ESPBTUUID BEDJET_SERVICE_UUID = espbt::ESPBTUUID::from_raw("00001000-bed0-0080-aa55-4265644a6574");
static const espbt::ESPBTUUID BEDJET_STATUS_UUID = espbt::ESPBTUUID::from_raw("00002000-bed0-0080-aa55-4265644a6574");
static const espbt::ESPBTUUID BEDJET_COMMAND_UUID = espbt::ESPBTUUID::from_raw("00002004-bed0-0080-aa55-4265644a6574");
static const espbt::ESPBTUUID BEDJET_NAME_UUID = espbt::ESPBTUUID::from_raw("00002001-bed0-0080-aa55-4265644a6574");

class BedJetHub : public esphome::ble_client::BLEClientNode, public PollingComponent {
 public:
  void setup() override {
    this->codec_ = make_unique<BedjetCodec>();
  }

  void loop() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void register_child(BedJetClient *obj);

  void set_status_timeout(uint32_t timeout) { this->timeout_ = timeout; }
  /** Attempts to check for and apply firmware updates. */
  void upgrade_firmware();

#ifdef USE_TIME
  void set_time_id(time::RealTimeClock *time_id) { this->time_id_ = time_id; }
#endif

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  bool is_connected() { return this->node_state == espbt::ClientState::ESTABLISHED; }

 protected:
  // FIXME: temporarily expose this.
  friend Bedjet;

  std::vector<BedJetClient *> children_;
  // FIXME
  std::string get_name() { return "TODO::get_name"; }

#ifdef USE_TIME
  void setup_time_();
  void send_local_time_();
  optional<time::RealTimeClock *> time_id_{};
#endif

  uint32_t timeout_{DEFAULT_STATUS_TIMEOUT};
  static const uint32_t MIN_NOTIFY_THROTTLE = 5000;
  static const uint32_t NOTIFY_WARN_THRESHOLD = 300000;
  static const uint32_t DEFAULT_STATUS_TIMEOUT = 900000;

  uint8_t set_notify_(bool enable);
  uint8_t write_bedjet_packet_(BedjetPacket *pkt);

  uint32_t last_notify_ = 0;
  bool force_refresh_ = false;

  std::unique_ptr<BedjetCodec> codec_;
  uint16_t char_handle_cmd_;
  uint16_t char_handle_name_;
  uint16_t char_handle_status_;
  uint16_t config_descr_status_;

  uint8_t write_notify_config_descriptor_(bool enable);
};

} //namespace bedjet
} //namespace esphome

#endif
