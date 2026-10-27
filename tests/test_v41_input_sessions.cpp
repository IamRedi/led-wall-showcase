#include "test_framework.h"

#include "input/input_sessions.h"

using namespace ledwall;

void test_v41_input_sessions_two_players_edges_and_replay() {
  InputSessions sessions;
  InputEventQueue queue;
  CHECK(sessions.claim(1, 101, 1000) == InputSessions::Result::Accepted);
  CHECK(sessions.claim(1, 202, 1001) == InputSessions::Result::Busy);
  CHECK(sessions.claim(2, 202, 1001) == InputSessions::Result::Accepted);
  CHECK(sessions.heartbeat(2, 202, 1, 1050) == InputSessions::Result::Accepted);
  CHECK(sessions.heartbeat(2, 202, 1, 1051) == InputSessions::Result::StaleSequence);
  CHECK(sessions.event(1, 101, 1, InputEvent::Confirm, true, 1100, &queue) ==
        InputSessions::Result::Accepted);
  CHECK(sessions.event(1, 101, 2, InputEvent::Confirm, true, 1101, &queue) ==
        InputSessions::Result::Duplicate);
  CHECK(sessions.event(2, 202, 2, InputEvent::Left, true, 1102, &queue) ==
        InputSessions::Result::Accepted);
  CHECK(sessions.event(1, 101, 1, InputEvent::Confirm, false, 1103, &queue) ==
        InputSessions::Result::StaleSequence);
  CHECK(sessions.event(1, 101, 3, InputEvent::Confirm, false, 1104, &queue) ==
        InputSessions::Result::Accepted);
  CHECK(sessions.event(1, 101, 4, InputEvent::Confirm, true, 1105, &queue) ==
        InputSessions::Result::Accepted);
  CHECK_EQ_INT(queue.pending(), 3);
  PlayerEvent e;
  CHECK(queue.pop(&e)); CHECK_EQ_INT(e.player, 1); CHECK(e.event == InputEvent::Confirm);
  CHECK(queue.pop(&e)); CHECK_EQ_INT(e.player, 2); CHECK(e.event == InputEvent::Left);
  CHECK(queue.pop(&e)); CHECK_EQ_INT(e.player, 1); CHECK(e.event == InputEvent::Confirm);
  CHECK(sessions.release(1, 202) == InputSessions::Result::NotOwned);
  CHECK(sessions.release(1, 101) == InputSessions::Result::Accepted);
  CHECK_EQ_INT(sessions.take_lost_mask(), 1);
  CHECK_EQ_INT(sessions.take_lost_mask(), 0);
  queue.push(1, InputEvent::Confirm, InputSource::PhoneSession);
  queue.push(2, InputEvent::Left, InputSource::PhoneSession);
  queue.push(1, InputEvent::Back, InputSource::Provider);
  queue.discard_phone(1);
  CHECK_EQ_INT(queue.pending(), 2);
  CHECK(queue.pop(&e)); CHECK_EQ_INT(e.player, 2);
  CHECK(queue.pop(&e)); CHECK(e.source == InputSource::Provider);
  CHECK(sessions.heartbeat(2, 202, 3, 14999) == InputSessions::Result::Accepted);
  sessions.expire(20000);
  CHECK(sessions.claimed(2));
  CHECK_EQ_INT(sessions.take_lost_mask(), 0);
  sessions.expire(30000);
  CHECK(!sessions.claimed(2));
  CHECK_EQ_INT(sessions.take_lost_mask(), 2);
}
