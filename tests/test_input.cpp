#include "test_framework.h"

#include "input/button_decoder.h"
#include "input/input_events.h"

// LED-L009 unified input layer: normalized events, bounded queue, debounce
// and auto-repeat semantics, provider independence (any provider produces the
// same event stream; consumers see only InputEvent).

using namespace ledwall;

void test_input_event_wire_roundtrip() {
  for (uint8_t i = 0; i < kInputEventEnumCount; ++i) {
    const InputEvent ev = static_cast<InputEvent>(i);
    const char* name = input_event_name(ev);
    CHECK(name != nullptr);
    InputEvent back = InputEvent::Up;
    CHECK(input_event_from_name(name, &back));
    CHECK_EQ_INT(static_cast<int>(back), i);
  }
  CHECK(input_event_name(static_cast<InputEvent>(99)) == nullptr);
  InputEvent out = InputEvent::Up;
  CHECK(!input_event_from_name("diagonal", &out));
  CHECK(!input_event_from_name(nullptr, &out));
}

void test_input_queue_bounded_fifo_and_drop_count() {
  InputEventQueue q;
  PlayerEvent never{};
  CHECK(!q.pop(&never));
  for (int i = 0; i < 20; ++i) {
    q.push(static_cast<InputEvent>(i % kInputEventEnumCount));
  }
  CHECK_EQ_INT(q.pending(), InputEventQueue::kCapacity);
  CHECK(q.dropped() >= 4);      // overflow dropped, never grew memory
  PlayerEvent ev{};
  CHECK(q.pop(&ev));
  CHECK_EQ_INT(static_cast<int>(ev.event), 0);  // FIFO order preserved
  CHECK_EQ_INT(static_cast<int>(ev.player), kPlayer1);  // legacy push = P1
  q.clear();
  CHECK_EQ_INT(q.pending(), 0);
}

// LED-L010: explicit player slots ride the same bounded queue.
void test_input_queue_player_attribution() {
  InputEventQueue q;
  q.push(InputEvent::Confirm);        // legacy overload → P1
  q.push(kPlayer2, InputEvent::Up);   // P2-attributed
  PlayerEvent a{}, b{};
  CHECK(q.pop(&a));
  CHECK(q.pop(&b));
  CHECK_EQ_INT(static_cast<int>(a.event), static_cast<int>(InputEvent::Confirm));
  CHECK_EQ_INT(static_cast<int>(a.player), kPlayer1);
  CHECK_EQ_INT(static_cast<int>(b.event), static_cast<int>(InputEvent::Up));
  CHECK_EQ_INT(static_cast<int>(b.player), kPlayer2);
  CHECK(!q.pop(&a));
}

void test_button_decoder_debounce_and_edge() {
  ButtonDecoder d;
  d.begin(false, 0);
  CHECK(!d.feed_press(true, 5));    // raw bounce window (changed at 5)
  CHECK(!d.feed_press(true, 20));
  CHECK(!d.feed_press(true, 34));   // 34-5 = 29 ms < 30 ms debounce
  CHECK(d.feed_press(true, 35));    // debounced press edge (stable >= 30 ms)
  CHECK(!d.feed_press(true, 40));   // no repeat on feed_press
  CHECK(!d.feed_press(false, 45));  // release raw change at 45
  CHECK(d.is_down());               // debounced release not settled yet
  CHECK(!d.feed_press(false, 50));
  CHECK(d.is_down());               // 50-45 = 5 ms < debounce
  CHECK(!d.feed_press(false, 76));  // settles here (31 ms stable); no edge
  CHECK(d.is_down() == false);
}

void test_button_decoder_repeat_schedule() {
  ButtonDecoder d;
  d.begin(false, 0);
  CHECK(!d.feed_press_or_repeat(true, 100));   // raw change at 100, bounce window
  CHECK(d.feed_press_or_repeat(true, 130));    // debounced press edge
  CHECK(!d.feed_press_or_repeat(true, 700));   // held, before repeat delay (130+600)
  CHECK(!d.feed_press_or_repeat(true, 729));
  CHECK(d.feed_press_or_repeat(true, 730));    // first repeat at +600
  CHECK(!d.feed_press_or_repeat(true, 800));
  CHECK(d.feed_press_or_repeat(true, 881));    // second repeat at +150
  CHECK(!d.feed_press_or_repeat(false, 890));  // release raw change at 890
  CHECK(d.is_down());                          // not settled yet
  CHECK(!d.feed_press_or_repeat(false, 921));  // settled; releases never emit
  CHECK(d.is_down() == false);
  CHECK(!d.feed_press_or_repeat(true, 1000));  // new press bounces
  CHECK(d.feed_press_or_repeat(true, 1031));
}

void test_button_decoder_start_state() {
  ButtonDecoder d;
  d.begin(true, 0);  // initialized held: no spurious edge
  CHECK(d.is_down());
  CHECK(!d.feed_press(true, 10));
  CHECK(!d.feed_press(false, 100));  // release raw change at 100
  CHECK(d.is_down());                // not settled yet
  CHECK(!d.feed_press(false, 131));  // settled at 100 + 30
  CHECK(!d.is_down());
}

InputEvent input_from_name_or_die(const char* name) {
  InputEvent ev = InputEvent::Up;
  (void)input_event_from_name(name, &ev);
  return ev;
}
// Provider independence: an API-originated event and a GPIO-originated event
// are indistinguishable once in the queue (consumers never see the source).
void test_input_provider_independence() {
  InputEventQueue q;
  q.push(InputEvent::Left);                       // "GPIO provider"
  q.push(input_from_name_or_die("left"));         // "API provider" (same name)
  InputEvent a = InputEvent::Up, b = InputEvent::Up;
  CHECK(q.pop(&a));
  CHECK(q.pop(&b));
  CHECK_EQ_INT(static_cast<int>(a), static_cast<int>(b));
}


