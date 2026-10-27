#pragma once

#include <cstdint>

#include "render/pixel.h"

namespace ledwall {

// Conservative design envelope, NOT a power measurement or certification.
// Full-white RGB is estimated at 60 mA per LED, then scaled by the actual
// FastLED brightness byte. The physical LED type and supply remain unverified.
struct OutputBudget {
  uint32_t total_ma{1000};
  uint32_t matrix_ma{800};
  uint32_t tail_ma{200};
};

struct OutputScale {
  uint8_t matrix{255};
  uint8_t tail{255};
  uint32_t estimated_before_ma{0};
};

OutputScale output_limit(const Rgb* matrix, uint16_t matrix_count,
                         const Rgb* tail, uint16_t tail_count,
                         uint8_t hardware_brightness,
                         const OutputBudget& budget = OutputBudget{});

}  // namespace ledwall
