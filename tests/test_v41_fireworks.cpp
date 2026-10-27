#include "api/registry.h"
#include <initializer_list>
#include "render/renderer.h"
#include "render/body_topology.h"
#include "test_framework.h"

namespace {
ledwall::renderer::RenderInput fireworks(uint32_t now) {
  ledwall::renderer::RenderInput input{};
  input.kind = ledwall::ModeKind::Animation;
  input.name = "fireworks";
  input.target = ledwall::ZoneTarget::Both;
  input.started_ms = 0;
  input.now_ms = now;
  return input;
}
bool lit(ledwall::Rgb c) { return c != ledwall::kRgbBlack; }
}  // namespace

void test_v41_fireworks_crosses_anchor_on_one_clock() {
  ledwall::FrameBuffer matrix;
  ledwall::TailFramebuffer tail;
  tail.configure(150);
  ledwall::renderer::render(matrix, &tail, fireworks(0));
  CHECK(lit(tail.at(149)));
  CHECK(!lit(matrix.at(14, 9)));

  auto tagged = fireworks(1200);
  tagged.frame_id = 17;
  ledwall::renderer::render(matrix, &tail, tagged);
  CHECK(matrix.frame_id() == 17);
  CHECK(tail.frame_id() == 17);
  tagged.frame_id = 18;
  ledwall::renderer::render(matrix, &tail, tagged);
  CHECK(matrix.frame_id() == 18);
  CHECK(tail.frame_id() == 18);

  ledwall::renderer::render(matrix, &tail, fireworks(2350));
  CHECK(lit(matrix.at(14, 9)));
  ledwall::renderer::render(matrix, &tail, fireworks(2400));
  CHECK(lit(matrix.at(14, 9)));
  CHECK(lit(tail.at(0)));

  ledwall::renderer::render(matrix, &tail, fireworks(2750));
  CHECK(lit(matrix.at(7, 4)));
  for (uint16_t i = 0; i < 150; ++i) CHECK(!lit(tail.at(i)));
  ledwall::renderer::render(matrix, &tail, fireworks(3400));
  for (uint16_t i = 0; i < 150; ++i) CHECK(!lit(tail.at(i)));
  ledwall::renderer::render(matrix, &tail, fireworks(4700));
  CHECK(!lit(matrix.at(7, 4)));
  CHECK(!lit(tail.at(0)));
}

void test_v41_fireworks_is_deterministic_and_bounded() {
  ledwall::FrameBuffer a, b;
  ledwall::TailFramebuffer ta, tb;
  ta.configure(150);
  tb.configure(150);
  for (uint32_t time : {0u, 1200u, 2400u, 2750u, 3400u, 4100u, 4800u, 9600u}) {
    ledwall::renderer::render(a, &ta, fireworks(time));
    ledwall::renderer::render(b, &tb, fireworks(time));
    for (uint16_t i = 0; i < ledwall::kLedCount; ++i) CHECK(a.data()[i] == b.data()[i]);
    for (uint16_t i = 0; i < 150; ++i) CHECK(ta.at(i) == tb.at(i));
  }
  CHECK(ledwall::renderer::program_is_time_varying(fireworks(0)));
  CHECK(ledwall::animation_default_target("fireworks") == ledwall::ZoneTarget::Both);
}

void test_v5_body_topology_crosses_configured_attachment() {
  using namespace ledwall;
  CHECK(BodyTopology::valid(BodyAnchor{14, 15, 9}));
  CHECK(!BodyTopology::valid(BodyAnchor{14, 16, 9}));
  CHECK(!BodyTopology::valid(BodyAnchor{29, 30, 9}));
  BodyTopology body(BodyAnchor{14, 15, 9});
  const BodyPoint free_end = body.ascent(0, 150, 7, 4);
  CHECK(free_end.on_tail);
  CHECK_EQ_INT(free_end.tail_index, 149);
  const BodyPoint joint = body.ascent(56000, 150, 7, 4);
  CHECK(!joint.on_tail);
  CHECK_EQ_INT(joint.x, 14);
  CHECK_EQ_INT(joint.y, 9);
  const BodyPoint destination = body.ascent(65535, 150, 7, 4);
  CHECK(!destination.on_tail);
  CHECK_EQ_INT(destination.x, 7);
  CHECK_EQ_INT(destination.y, 4);
  FrameBuffer matrix;
  TailFramebuffer tail;
  tail.configure(150);
  body.plot(matrix, tail, free_end, Rgb{255, 240, 210});
  body.light_anchor(matrix, Rgb{40, 70, 90});
  CHECK(lit(tail.at(149)));
  CHECK(lit(matrix.at(14, 9)));
  CHECK(lit(matrix.at(15, 9)));
}
