#include "bedjet_hub.h"
#include "bedjet_child.h"

namespace esphome {
namespace bedjet {

void BedJetHub::register_child(BedJetClient *obj) {
  this->children_.push_back(obj);
  obj->register_parent(this);
}

} //namespace bedjet
} //namespace esphome
