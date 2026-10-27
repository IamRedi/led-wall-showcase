#include "render/output_limiter.h"

namespace ledwall {
namespace {

uint32_t estimate_ma(const Rgb* pixels, uint16_t count, uint8_t brightness) {
  if (pixels == nullptr || brightness == 0) return 0;
  uint64_t channels = 0;
  for (uint16_t i = 0; i < count; ++i) {
    channels += static_cast<uint32_t>(pixels[i].r) + pixels[i].g + pixels[i].b;
  }
  // 20 mA/channel at 255; round UP to keep the estimate conservative.
  constexpr uint64_t kDenominator = 255u * 255u;
  return static_cast<uint32_t>((channels * 20u * brightness + kDenominator - 1u) /
                               kDenominator);
}

uint8_t cap_scale(uint32_t estimate, uint32_t budget) {
  if (estimate == 0) return 255;
  if (budget == 0) return 0;
  if (estimate <= budget) return 255;
  return static_cast<uint8_t>((static_cast<uint64_t>(budget) * 255u) / estimate);
}

uint32_t scaled_estimate(uint32_t estimate, uint8_t scale) {
  return static_cast<uint32_t>((static_cast<uint64_t>(estimate) * scale + 254u) / 255u);
}

}  // namespace

OutputScale output_limit(const Rgb* matrix, uint16_t matrix_count,
                         const Rgb* tail, uint16_t tail_count,
                         uint8_t hardware_brightness, const OutputBudget& budget) {
  const uint32_t matrix_ma = estimate_ma(matrix, matrix_count, hardware_brightness);
  const uint32_t tail_ma = estimate_ma(tail, tail_count, hardware_brightness);
  const uint8_t matrix_local = cap_scale(matrix_ma, budget.matrix_ma);
  const uint8_t tail_local = cap_scale(tail_ma, budget.tail_ma);
  const uint32_t total_after_local = scaled_estimate(matrix_ma, matrix_local) +
                                     scaled_estimate(tail_ma, tail_local);
  const uint8_t global = cap_scale(total_after_local, budget.total_ma);
  OutputScale result;
  result.matrix = static_cast<uint8_t>((static_cast<uint16_t>(matrix_local) * global) / 255u);
  result.tail = static_cast<uint8_t>((static_cast<uint16_t>(tail_local) * global) / 255u);
  result.estimated_before_ma = matrix_ma + tail_ma;
  return result;
}

}  // namespace ledwall
