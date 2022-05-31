#include "bedjet_hub.h"
#include "bedjet_child.h"
#include "bedjet_const.h"

namespace esphome {
namespace bedjet {

void BedJetHub::register_child(BedJetClient *obj) {
  this->children_.push_back(obj);
  obj->register_parent(this);
}

void BedJetHub::upgrade_firmware() {
  auto *pkt = this->codec_->get_button_request(MAGIC_UPDATE);
  auto status = this->write_bedjet_packet_(pkt);

  if (status) {
    ESP_LOGW(TAG, "[%s] esp_ble_gattc_write_char failed, status=%d", this->parent_->address_str().c_str(), status);
  }
}

//region BTLE
 uint8_t Bedjet::write_bedjet_packet_(BedjetPacket *pkt) {
  if (!this->is_connected()) {
    if (!this->parent_->enabled) {
      ESP_LOGI(TAG, "[%s] Cannot write packet: Not connected, enabled=false", this->get_name().c_str());
    } else {
      ESP_LOGW(TAG, "[%s] Cannot write packet: Not connected", this->get_name().c_str());
    }
    return -1;
  }
  auto status = esp_ble_gattc_write_char(this->parent_->gattc_if, this->parent_->conn_id, this->char_handle_cmd_,
                                         pkt->data_length + 1, (uint8_t *) &pkt->command, ESP_GATT_WRITE_TYPE_NO_RSP,
                                         ESP_GATT_AUTH_REQ_NONE);
  return status;
}
//endregion

#ifdef USE_TIME
void BedJetHub::send_local_time_() {
  if (!this->is_connected()) {
    ESP_LOGV(TAG, "[%s] Not connected, cannot send time.", this->get_name().c_str());
    return;
  }
  auto *time_id = *this->time_id_;
  time::ESPTime now = time_id->now();
  if (now.is_valid()) {
    uint8_t hour = now.hour;
    uint8_t minute = now.minute;
    BedjetPacket *pkt = this->codec_->get_set_time_request(hour, minute);
    auto status = this->write_bedjet_packet_(pkt);
    if (status) {
      ESP_LOGW(TAG, "Failed setting BedJet clock: %d", status);
    } else {
      ESP_LOGD(TAG, "[%s] BedJet clock set to: %d:%02d", this->get_name().c_str(), hour, minute);
    }
  }
}

/** Initializes time sync callbacks to support syncing current time to the BedJet. */
void BedJetHub::setup_time_() {
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

} //namespace bedjet
} //namespace esphome
