#include "render/output_limiter.h"
#include "test_framework.h"

void test_v41_output_limiter_combines_both_zones() {
  ledwall::Rgb matrix[300];
  ledwall::Rgb tail[150];
  for (auto& p : matrix) p = ledwall::Rgb{255, 255, 255};
  for (auto& p : tail) p = ledwall::Rgb{255, 255, 255};
  const auto limited = ledwall::output_limit(matrix, 300, tail, 150, 102);
  CHECK(limited.estimated_before_ma > 10000);
  CHECK(limited.matrix < 255);
  CHECK(limited.tail < 255);
  // Independent upper bound: 60 mA/full-white cell at 255, then both the
  // actual device brightness and returned channel scale. Integer floors in
  // the driver can only reduce this estimate.
  const uint32_t m = (300u * 60u * 102u * limited.matrix) / (255u * 255u);
  const uint32_t t = (150u * 60u * 102u * limited.tail) / (255u * 255u);
  CHECK(m <= 800);
  CHECK(t <= 200);
  CHECK(m + t <= 1000);

  ledwall::OutputBudget smaller{};
  smaller.total_ma = 300;
  smaller.matrix_ma = 800;
  smaller.tail_ma = 200;
  const auto global = ledwall::output_limit(matrix, 300, tail, 150, 102, smaller);
  CHECK(global.matrix < limited.matrix);
  CHECK(global.tail < limited.tail);
  const uint32_t total = (300u * 60u * 102u * global.matrix +
                          150u * 60u * 102u * global.tail) / (255u * 255u);
  CHECK(total <= 300);
}

void test_v41_output_limiter_preserves_dark_and_zero_brightness() {
  const ledwall::Rgb dark[1] = {ledwall::kRgbBlack};
  const ledwall::Rgb white[1] = {{255, 255, 255}};
  const auto d = ledwall::output_limit(dark, 1, dark, 1, 255);
  CHECK_EQ_INT(d.estimated_before_ma, 0);
  CHECK_EQ_INT(d.matrix, 255);
  CHECK_EQ_INT(d.tail, 255);
  const auto off = ledwall::output_limit(white, 1, white, 1, 0);
  CHECK_EQ_INT(off.estimated_before_ma, 0);
  CHECK_EQ_INT(off.matrix, 255);
  CHECK_EQ_INT(off.tail, 255);
  const auto no_tail = ledwall::output_limit(white, 1, nullptr, 0, 255);
  CHECK_EQ_INT(no_tail.estimated_before_ma, 60);
}
