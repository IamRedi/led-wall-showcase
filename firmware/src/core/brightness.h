#pragma once

#include <cstdint>

namespace ledwall {

// Frozen spec §5/§13, ADR-011: semantic brightness 0–100 is clamped by a
// locally enforced physical ceiling regardless of what any client requests.
// The ceiling default is the conservative heritage value until §22 power
// verification measures a real one.
class Brightness {
 public:
  static constexpr uint32_t kMaxLevel = 100;

  void configure(uint32_t default_level, uint32_t ceiling);
  bool set_level(uint32_t requested);  // clamped to [0, ceiling]; true if changed
  bool set_ceiling(uint32_t ceiling);  // config path; re-clamps the active level

  uint32_t requested() const { return requested_; }
  uint32_t level() const { return level_; }
  uint32_t ceiling() const { return ceiling_; }

  uint8_t hardware_scale() const {  // 0..255 for FastLED, deterministic rounding
    return static_cast<uint8_t>((level_ * 255u + 50u) / 100u);
  }

  bool dirty() const { return dirty_; }
  void clear_dirty() { dirty_ = false; }

 private:
  void reclamp_();

  uint32_t requested_{0};
  uint32_t ceiling_{0};
  uint32_t level_{0};
  bool dirty_{false};
};

}  // namespace ledwall
