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

} //namespace bedjet
} //namespace esphome
