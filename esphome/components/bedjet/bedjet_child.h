#pragma once

#include "bedjet_base.h"
#include "bedjet_hub.h"

namespace esphome {
namespace bedjet {

class BedJetClient {
 public:
  void register_parent(BedJetHub *parent) {
    this->parent_ = parent;
  };
  void on_status(BedjetStatusPacket *data) {};

 protected:
  BedJetHub *parent_{};
}

} //namespace bedjet
} //namespace esphome
