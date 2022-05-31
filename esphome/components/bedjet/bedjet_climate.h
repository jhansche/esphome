#pragma once

#include "esphome/components/climate/climate.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "bedjet_base.h"
#include "bedjet_child.h"
#include "bedjet_hub.h"

#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#endif

#ifdef USE_ESP32

namespace esphome {
namespace bedjet {

class Bedjet : public climate::Climate, public BedJetClient, public PollingComponent {
 public:
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  /** Sets the default strategy to use for climate::CLIMATE_MODE_HEAT. */
  void set_heating_mode(BedjetHeatMode mode) { this->heating_mode_ = mode; }

#ifdef USE_TIME
  void set_time_id(time::RealTimeClock *time_id) {
    this->parent_->set_time_id(time_id);
  }
#endif

  // FIXME: remove
  void set_status_timeout(uint32_t timeout) { this->parent_->set_status_timeout(timeout); }

  /** Attempts to check for and apply firmware updates. */
  // FIXME: remove
  void upgrade_firmware();

  climate::ClimateTraits traits() override {
    auto traits = climate::ClimateTraits();
    traits.set_supports_action(true);
    traits.set_supports_current_temperature(true);
    traits.set_supported_modes({
        climate::CLIMATE_MODE_OFF,
        climate::CLIMATE_MODE_HEAT,
        // climate::CLIMATE_MODE_TURBO // Not supported by Climate: see presets instead
        climate::CLIMATE_MODE_FAN_ONLY,
        climate::CLIMATE_MODE_DRY,
    });

    // It would be better if we had a slider for the fan modes.
    traits.set_supported_custom_fan_modes(BEDJET_FAN_STEP_NAMES_SET);
    traits.set_supported_presets({
        // If we support NONE, then have to decide what happens if the user switches to it (turn off?)
        // climate::CLIMATE_PRESET_NONE,
        // Climate doesn't have a "TURBO" mode, but we can use the BOOST preset instead.
        climate::CLIMATE_PRESET_BOOST,
    });
    traits.set_supported_custom_presets({
        // We could fetch biodata from bedjet and set these names that way.
        // But then we have to invert the lookup in order to send the right preset.
        // For now, we can leave them as M1-3 to match the remote buttons.
        "M1",
        "M2",
        "M3",
    });
    if (this->heating_mode_ == HEAT_MODE_EXTENDED) {
      traits.add_supported_custom_preset("LTD HT");
    } else {
      traits.add_supported_custom_preset("EXT HT");
    }
    traits.set_visual_min_temperature(19.0);
    traits.set_visual_max_temperature(43.0);
    traits.set_visual_temperature_step(1.0);
    return traits;
  }

 protected:
  void control(const climate::ClimateCall &call) override;

  // FIXME remove
  bool force_refresh_ = false;
  uint8_t write_bedjet_packet_(BedjetPacket *pkt);
#ifdef USE_TIME
  // TODO: deprecated
  void setup_time_();
  void send_local_time_();
  // FIXME: remove
  optional<time::RealTimeClock *> time_id_{};
#endif

  BedjetHeatMode heating_mode_ = HEAT_MODE_HEAT;

  void reset_state_();
  bool update_status_();

  bool is_valid_() {
    // FIXME: find a better way to check this?
    return !std::isnan(this->current_temperature) && !std::isnan(this->target_temperature) &&
           this->current_temperature > 1 && this->target_temperature > 1;
  }
};

}  // namespace bedjet
}  // namespace esphome

#endif
