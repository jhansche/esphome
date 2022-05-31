#include "bedjet_climate.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace bedjet {

using namespace esphome::climate;

/// Converts a BedJet temp step into degrees Celsius.
float bedjet_temp_to_c(const uint8_t temp) {
  // BedJet temp is "C*2"; to get C, divide by 2.
  return temp / 2.0f;
}

/// Converts a BedJet fan step to a speed percentage, in the range of 5% to 100%.
uint8_t bedjet_fan_step_to_speed(const uint8_t fan) {
  //  0 =  5%
  // 19 = 100%
  return 5 * fan + 5;
}

static const std::string *bedjet_fan_step_to_fan_mode(const uint8_t fan_step) {
  if (fan_step >= 0 && fan_step <= 19)
    return &BEDJET_FAN_STEP_NAME_STRINGS[fan_step];
  return nullptr;
}

static uint8_t bedjet_fan_speed_to_step(const std::string &fan_step_percent) {
  for (int i = 0; i < sizeof(BEDJET_FAN_STEP_NAME_STRINGS); i++) {
    if (fan_step_percent == BEDJET_FAN_STEP_NAME_STRINGS[i]) {
      return i;
    }
  }
  return -1;
}

static BedjetButton heat_button(BedjetHeatMode mode) {
  BedjetButton btn = BTN_HEAT;
  if (mode == HEAT_MODE_EXTENDED) {
    btn = BTN_EXTHT;
  }
  return btn;
}

void Bedjet::upgrade_firmware() {
  this->parent_->upgrade_firmware();
}

void Bedjet::dump_config() {
  LOG_CLIMATE("", "BedJet Climate", this);
  auto traits = this->get_traits();

  ESP_LOGCONFIG(TAG, "  Supported modes:");
  for (auto mode : traits.get_supported_modes()) {
    ESP_LOGCONFIG(TAG, "   - %s", LOG_STR_ARG(climate_mode_to_string(mode)));
  }

  ESP_LOGCONFIG(TAG, "  Supported fan modes:");
  for (const auto &mode : traits.get_supported_fan_modes()) {
    ESP_LOGCONFIG(TAG, "   - %s", LOG_STR_ARG(climate_fan_mode_to_string(mode)));
  }
  for (const auto &mode : traits.get_supported_custom_fan_modes()) {
    ESP_LOGCONFIG(TAG, "   - %s (c)", mode.c_str());
  }

  ESP_LOGCONFIG(TAG, "  Supported presets:");
  for (auto preset : traits.get_supported_presets()) {
    ESP_LOGCONFIG(TAG, "   - %s", LOG_STR_ARG(climate_preset_to_string(preset)));
  }
  for (const auto &preset : traits.get_supported_custom_presets()) {
    ESP_LOGCONFIG(TAG, "   - %s (c)", preset.c_str());
  }
}

void Bedjet::setup() {
  // restore set points
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    ESP_LOGI(TAG, "Restored previous saved state.");
    restore->apply(this);
  } else {
    // Initial status is unknown until we connect
    this->reset_state_();
  }

#ifdef USE_TIME
  this->setup_time_();
#endif
}

/** Resets states to defaults. */
void Bedjet::reset_state_() {
  this->mode = climate::CLIMATE_MODE_OFF;
  this->action = climate::CLIMATE_ACTION_IDLE;
  this->target_temperature = NAN;
  this->current_temperature = NAN;
  this->preset.reset();
  this->custom_preset.reset();
  this->publish_state();
}

void Bedjet::loop() {}

void Bedjet::control(const ClimateCall &call) {
  ESP_LOGD(TAG, "Received Bedjet::control");
  if (!this->parent_->is_connected()) {
    ESP_LOGW(TAG, "Not connected, cannot handle control call yet.");
    return;
  }

  if (call.get_mode().has_value()) {
    ClimateMode mode = *call.get_mode();
    BedjetPacket *pkt;
    switch (mode) {
      case climate::CLIMATE_MODE_OFF:
        pkt = this->parent_->codec_->get_button_request(BTN_OFF);
        break;
      case climate::CLIMATE_MODE_HEAT:
        pkt = this->parent_->codec_->get_button_request(heat_button(this->heating_mode_));
        break;
      case climate::CLIMATE_MODE_FAN_ONLY:
        pkt = this->parent_->codec_->get_button_request(BTN_COOL);
        break;
      case climate::CLIMATE_MODE_DRY:
        pkt = this->parent_->codec_->get_button_request(BTN_DRY);
        break;
      default:
        ESP_LOGW(TAG, "Unsupported mode: %d", mode);
        return;
    }

    auto status = this->parent_->write_bedjet_packet_(pkt);

    if (status) {
      ESP_LOGW(TAG, "[%s] esp_ble_gattc_write_char failed, status=%d", this->parent_->parent_->address_str().c_str(), status);
    } else {
      this->force_refresh_ = true;
      this->mode = mode;
      // We're using (custom) preset for Turbo, EXT HT, & M1-3 presets, so changing climate mode will clear those
      this->custom_preset.reset();
      this->preset.reset();
    }
  }

  if (call.get_target_temperature().has_value()) {
    auto target_temp = *call.get_target_temperature();
    auto *pkt = this->parent_->codec_->get_set_target_temp_request(target_temp);
    auto status = this->parent_->write_bedjet_packet_(pkt);

    if (status) {
      ESP_LOGW(TAG, "[%s] esp_ble_gattc_write_char failed, status=%d", this->parent_->parent_->address_str().c_str(), status);
    } else {
      this->target_temperature = target_temp;
    }
  }

  if (call.get_preset().has_value()) {
    ClimatePreset preset = *call.get_preset();
    BedjetPacket *pkt;

    if (preset == climate::CLIMATE_PRESET_BOOST) {
      pkt = this->parent_->codec_->get_button_request(BTN_TURBO);
    } else {
      ESP_LOGW(TAG, "Unsupported preset: %d", preset);
      return;
    }

    auto status = this->parent_->write_bedjet_packet_(pkt);
    if (status) {
      ESP_LOGW(TAG, "[%s] esp_ble_gattc_write_char failed, status=%d", this->parent_->parent_->address_str().c_str(), status);
    } else {
      // We use BOOST preset for TURBO mode, which is a short-lived/high-heat mode.
      this->mode = climate::CLIMATE_MODE_HEAT;
      this->preset = preset;
      this->custom_preset.reset();
      this->force_refresh_ = true;
    }
  } else if (call.get_custom_preset().has_value()) {
    std::string preset = *call.get_custom_preset();
    BedjetPacket *pkt;

    if (preset == "M1") {
      pkt = this->parent_->codec_->get_button_request(BTN_M1);
    } else if (preset == "M2") {
      pkt = this->parent_->codec_->get_button_request(BTN_M2);
    } else if (preset == "M3") {
      pkt = this->parent_->codec_->get_button_request(BTN_M3);
    } else if (preset == "LTD HT") {
      pkt = this->parent_->codec_->get_button_request(BTN_HEAT);
    } else if (preset == "EXT HT") {
      pkt = this->parent_->codec_->get_button_request(BTN_EXTHT);
    } else {
      ESP_LOGW(TAG, "Unsupported preset: %s", preset.c_str());
      return;
    }

    auto status = this->parent_->write_bedjet_packet_(pkt);
    if (status) {
      ESP_LOGW(TAG, "[%s] esp_ble_gattc_write_char failed, status=%d", this->parent_->parent_->address_str().c_str(), status);
    } else {
      this->force_refresh_ = true;
      this->custom_preset = preset;
      this->preset.reset();
    }
  }

  if (call.get_fan_mode().has_value()) {
    // Climate fan mode only supports low/med/high, but the BedJet supports 5-100% increments.
    // We can still support a ClimateCall that requests low/med/high, and just translate it to a step increment here.
    auto fan_mode = *call.get_fan_mode();
    BedjetPacket *pkt;
    if (fan_mode == climate::CLIMATE_FAN_LOW) {
      pkt = this->parent_->codec_->get_set_fan_speed_request(3 /* = 20% */);
    } else if (fan_mode == climate::CLIMATE_FAN_MEDIUM) {
      pkt = this->parent_->codec_->get_set_fan_speed_request(9 /* = 50% */);
    } else if (fan_mode == climate::CLIMATE_FAN_HIGH) {
      pkt = this->parent_->codec_->get_set_fan_speed_request(14 /* = 75% */);
    } else {
      ESP_LOGW(TAG, "[%s] Unsupported fan mode: %s", this->get_name().c_str(),
               LOG_STR_ARG(climate_fan_mode_to_string(fan_mode)));
      return;
    }

    auto status = this->parent_->write_bedjet_packet_(pkt);
    if (status) {
      ESP_LOGW(TAG, "[%s] esp_ble_gattc_write_char failed, status=%d", this->parent_->parent_->address_str().c_str(), status);
    } else {
      this->force_refresh_ = true;
    }
  } else if (call.get_custom_fan_mode().has_value()) {
    auto fan_mode = *call.get_custom_fan_mode();
    auto fan_step = bedjet_fan_speed_to_step(fan_mode);
    if (fan_step >= 0 && fan_step <= 19) {
      ESP_LOGV(TAG, "[%s] Converted fan mode %s to bedjet fan step %d", this->get_name().c_str(), fan_mode.c_str(),
               fan_step);
      // The index should represent the fan_step index.
      BedjetPacket *pkt = this->parent_->codec_->get_set_fan_speed_request(fan_step);
      auto status = this->parent_->write_bedjet_packet_(pkt);
      if (status) {
        ESP_LOGW(TAG, "[%s] esp_ble_gattc_write_char failed, status=%d", this->parent_->parent_->address_str().c_str(), status);
      } else {
        this->force_refresh_ = true;
      }
    }
  }
}

#ifdef USE_TIME
/** Attempts to sync the local time (via `time_id`) to the BedJet device. */
void Bedjet::send_local_time_() {
  this->parent_->send_local_time_();
}

/** Initializes time sync callbacks to support syncing current time to the BedJet. */
void Bedjet::setup_time_() {
  if (this->time_id_.has_value()) {
    this->send_local_time_();
    auto *time_id = *this->time_id_;
    time_id->add_on_time_sync_callback([this] { this->send_local_time_(); });
    time::ESPTime now = time_id->now();
    ESP_LOGD(TAG, "Using time component to set BedJet clock: %d:%02d", now.hour, now.minute);
  } else {
    ESP_LOGI(TAG, "`time_id` is not configured: will not sync BedJet clock.");
  }
}
#endif

/** Writes one BedjetPacket to the BLE client on the BEDJET_COMMAND_UUID. */
uint8_t Bedjet::write_bedjet_packet_(BedjetPacket *pkt) {
  // FIXME: remove
  return this->parent_->write_bedjet_packet_(pkt);
}

/** Configures the local ESP BLE client to register (`true`) or unregister (`false`) for status notifications. */
uint8_t Bedjet::set_notify_(const bool enable) {
  // FIXME: remove
  return this->parent_->set_notify_(enable);
}

/** Attempts to update the climate device from the last received BedjetStatusPacket.
 *
 * @return `true` if the status has been applied; `false` if there is nothing to apply.
 */
bool Bedjet::update_status_() {
  if (!this->parent_->codec_->has_status())
    return false;

  BedjetStatusPacket status = *this->parent_->codec_->get_status_packet();

  auto converted_temp = bedjet_temp_to_c(status.target_temp_step);
  if (converted_temp > 0)
    this->target_temperature = converted_temp;
  converted_temp = bedjet_temp_to_c(status.ambient_temp_step);
  if (converted_temp > 0)
    this->current_temperature = converted_temp;

  const auto *fan_mode_name = bedjet_fan_step_to_fan_mode(status.fan_step);
  if (fan_mode_name != nullptr) {
    this->custom_fan_mode = *fan_mode_name;
  }

  // TODO: Get biorhythm data to determine which preset (M1-3) is running, if any.
  switch (status.mode) {
    case MODE_WAIT:  // Biorhythm "wait" step: device is idle
    case MODE_STANDBY:
      this->mode = climate::CLIMATE_MODE_OFF;
      this->action = climate::CLIMATE_ACTION_IDLE;
      this->fan_mode = climate::CLIMATE_FAN_OFF;
      this->custom_preset.reset();
      this->preset.reset();
      break;

    case MODE_HEAT:
      this->mode = climate::CLIMATE_MODE_HEAT;
      this->action = climate::CLIMATE_ACTION_HEATING;
      this->preset.reset();
      if (this->heating_mode_ == HEAT_MODE_EXTENDED) {
        this->set_custom_preset_("LTD HT");
      } else {
        this->custom_preset.reset();
      }
      break;

    case MODE_EXTHT:
      this->mode = climate::CLIMATE_MODE_HEAT;
      this->action = climate::CLIMATE_ACTION_HEATING;
      this->preset.reset();
      if (this->heating_mode_ == HEAT_MODE_EXTENDED) {
        this->custom_preset.reset();
      } else {
        this->set_custom_preset_("EXT HT");
      }
      break;

    case MODE_COOL:
      this->mode = climate::CLIMATE_MODE_FAN_ONLY;
      this->action = climate::CLIMATE_ACTION_COOLING;
      this->custom_preset.reset();
      this->preset.reset();
      break;

    case MODE_DRY:
      this->mode = climate::CLIMATE_MODE_DRY;
      this->action = climate::CLIMATE_ACTION_DRYING;
      this->custom_preset.reset();
      this->preset.reset();
      break;

    case MODE_TURBO:
      this->preset = climate::CLIMATE_PRESET_BOOST;
      this->custom_preset.reset();
      this->mode = climate::CLIMATE_MODE_HEAT;
      this->action = climate::CLIMATE_ACTION_HEATING;
      break;

    default:
      ESP_LOGW(TAG, "[%s] Unexpected mode: 0x%02X", this->get_name().c_str(), status.mode);
      break;
  }

  if (this->is_valid_()) {
    this->publish_state();
    this->parent_->codec_->clear_status();
    this->status_clear_warning();
  }

  return true;
}

void Bedjet::update() {
  ESP_LOGV(TAG, "[%s] update()", this->get_name().c_str());

#if false
  if (!this->parent_->is_connected()) {
    if (!this->parent()->enabled) {
      ESP_LOGD(TAG, "[%s] Not connected, because enabled=false", this->get_name().c_str());
    } else {
      // Possibly still trying to connect.
      ESP_LOGD(TAG, "[%s] Not connected, enabled=true", this->get_name().c_str());
    }

    return;
  }

  auto result = this->update_status_();
  if (!result) {
    uint32_t now = millis();
    uint32_t diff = now - this->last_notify_;

    if (this->last_notify_ == 0) {
      // This means we're connected and haven't received a notification, so it likely means that the BedJet is off.
      // However, it could also mean that it's running, but failing to send notifications.
      // We can try to unregister for notifications now, and then re-register, hoping to clear it up...
      // But how do we know for sure which state we're in, and how do we actually clear out the buggy state?

      ESP_LOGI(TAG, "[%s] Still waiting for first GATT notify event.", this->get_name().c_str());
      this->parent_->set_notify_(false);
    } else if (diff > NOTIFY_WARN_THRESHOLD) {
      ESP_LOGW(TAG, "[%s] Last GATT notify was %d seconds ago.", this->get_name().c_str(), diff / 1000);
    }

    if (this->timeout_ > 0 && diff > this->timeout_ && this->parent()->enabled) {
      ESP_LOGW(TAG, "[%s] Timed out after %d sec. Retrying...", this->get_name().c_str(), this->timeout_);
      this->parent()->set_enabled(false);
      this->parent()->set_enabled(true);
    }
  }
#endif

}

}  // namespace bedjet
}  // namespace esphome

#endif
