#include <string>

#include "pixel/pixel_canvas.h"
#include "render/renderer.h"
#include "test_framework.h"

void test_v41_pixel_canvas_atomic_session_revision_and_tail_end() {
  ledwall::PixelCanvas canvas;
  const uint32_t session = canvas.begin_session();
  std::string pixels(ledwall::PixelCanvas::kHexChars, '0');
  pixels.replace(0, 6, "ff0000");
  pixels.replace((ledwall::PixelCanvas::kCells - 1u) * 6u, 6, "00ff00");
  CHECK(canvas.apply(session, 1, pixels.c_str(), pixels.size(), false, 1000) ==
        ledwall::PixelCanvas::ApplyResult::Accepted);
  ledwall::FrameBuffer matrix;
  ledwall::TailFramebuffer tail;
  tail.configure(150);
  canvas.render(matrix, &tail);
  CHECK(matrix.at(0, 0) == (ledwall::Rgb{255, 0, 0}));
  CHECK(tail.at(149) == (ledwall::Rgb{0, 255, 0}));
  CHECK(tail.at(0) == ledwall::kRgbBlack);

  std::string malformed = pixels;
  malformed[malformed.size() - 1u] = 'z';
  CHECK(canvas.apply(session, 2, malformed.c_str(), malformed.size(), false, 1100) ==
        ledwall::PixelCanvas::ApplyResult::InvalidFrame);
  CHECK_EQ_INT(canvas.revision(), 1);
  canvas.render(matrix, &tail);
  CHECK(tail.at(149) == (ledwall::Rgb{0, 255, 0}));
  CHECK(canvas.apply(session, 2, pixels.c_str(), pixels.size() - 1u, false, 1100) ==
        ledwall::PixelCanvas::ApplyResult::InvalidFrame);
  CHECK(canvas.apply(session, 1, pixels.c_str(), pixels.size(), false, 1100) ==
        ledwall::PixelCanvas::ApplyResult::StaleRevision);
  const uint32_t next_session = canvas.begin_session();
  CHECK(next_session != session);
  CHECK(canvas.apply(session, 2, pixels.c_str(), pixels.size(), false, 1100) ==
        ledwall::PixelCanvas::ApplyResult::StaleSession);
  CHECK(canvas.apply(next_session, 1, pixels.c_str(), pixels.size(), true, 1200) ==
        ledwall::PixelCanvas::ApplyResult::Accepted);
  CHECK(canvas.apply(next_session, 2, pixels.c_str(), pixels.size(), true, 1250) ==
        ledwall::PixelCanvas::ApplyResult::RateLimited);
  CHECK_EQ_INT(canvas.revision(), 1);
  CHECK(canvas.apply(next_session, 2, pixels.c_str(), pixels.size(), true, 1300) ==
        ledwall::PixelCanvas::ApplyResult::Accepted);
}

void test_v41_pixel_canvas_renderer_uses_device_snapshot() {
  ledwall::PixelCanvas canvas;
  const uint32_t session = canvas.begin_session();
  std::string pixels(ledwall::PixelCanvas::kHexChars, '0');
  pixels.replace(6u * 299u, 6, "0000ff");
  pixels.replace(6u * 300u, 6, "ffffff");
  CHECK(canvas.apply(session, 1, pixels.c_str(), pixels.size(), false, 1000) ==
        ledwall::PixelCanvas::ApplyResult::Accepted);
  ledwall::renderer::RenderInput in{};
  in.kind = ledwall::ModeKind::Paint;
  in.pixel = &canvas;
  ledwall::FrameBuffer matrix;
  ledwall::TailFramebuffer tail;
  tail.configure(150);
  ledwall::renderer::render(matrix, &tail, in);
  CHECK(matrix.at(29, 9) == (ledwall::Rgb{0, 0, 255}));
  CHECK(tail.at(0) == (ledwall::Rgb{255, 255, 255}));
  CHECK(tail.at(149) == ledwall::kRgbBlack);
  in.pixel = nullptr;
  ledwall::renderer::render(matrix, &tail, in);
  CHECK(matrix.at(29, 9) == ledwall::kRgbBlack);
  CHECK(tail.at(0) == ledwall::kRgbBlack);
}
