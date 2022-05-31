#include "bedjet_hub.h"
#include "bedjet_child.h"

void BedJetHub::register_child(BedJetClient *obj) {
  this->children_.push_back(obj);
  obj->register_parent(this);
}
