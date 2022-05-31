#include "bedjet_child.h"
#include "bedjet_hub.h"

namespace esphome {
namespace bedjet {

void BedJetClient::register_parent(BedJetHub *parent) {
  this->parent_ = parent;
}

} //namespace bedjet
} //namespace esphome
