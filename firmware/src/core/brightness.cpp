#include "core/brightness.h"

namespace ledwall {

namespace {

uint32_t clamp_level(uint32_t v) { return (v > Brightness::kMaxLevel) ? Brightness::kMaxLevel : v; }

}  // namespace

void Brightness::configure(uint32_t default_level, uint32_t ceiling) {
  requested_ = clamp_level(default_level);
  ceiling_ = clamp_level(ceiling);
  reclamp_();
}

bool Brightness::set_level(uint32_t requested) {
  requested_ = clamp_level(requested);
  const uint32_t new_level = (requested_ > ceiling_) ? ceiling_ : requested_;
  if (new_level == level_) {
    return false;
  }
  level_ = new_level;
  dirty_ = true;
  return true;
}

bool Brightness::set_ceiling(uint32_t ceiling) {
  ceiling_ = clamp_level(ceiling);
  const uint32_t new_level = (requested_ > ceiling_) ? ceiling_ : requested_;
  if (new_level != level_) {
    level_ = new_level;
    dirty_ = true;
    return true;
  }
  return false;
}

void Brightness::reclamp_() { level_ = (requested_ > ceiling_) ? ceiling_ : requested_; }

}  // namespace ledwall
