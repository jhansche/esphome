#pragma once

#include "bedjet_base.h"
#include "bedjet_hub.h"

namespace esphome {
namespace bedjet {

// Forward declaration
class BedJetHub;

class BedJetClient {
 public:
  void register_parent(BedJetHub *parent);

  virtual void on_status(BedjetStatusPacket *data) = 0;

 protected:
  friend BedJetHub;
  virtual std::string describe() = "";
  BedJetHub *parent_{};
};

} //namespace bedjet
} //namespace esphome
