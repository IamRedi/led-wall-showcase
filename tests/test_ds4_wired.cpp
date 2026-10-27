#include "test_framework.h"

#include <cstring>
#include <vector>

#include "input/ds4_core.h"
#include "input/wired_controller.h"

// LED-L010 DS4 pure core (ADR-LED-L010 D5): mapping, edges, auto-repeat,
// lifecycle, player slots — and the wired-controller protocol seam codec
// (ADR-LED-L010 D6 / brief §10).

using namespace ledwall;

namespace {

// Collects the mapped event stream.
class RecordSink final : public ds4::EventSink {
 public:
  void push(uint8_t player, InputEvent event) override {
    players.push_back(player);
    events.push_back(event);
  }
  std::vector<uint8_t> players;
  std::vector<InputEvent> events;
};

ds4::Snapshot make_snap(uint8_t dpad, bool cross, bool circle, bool square, bool triangle,
                        bool options, bool ps) {
  ds4::Snapshot s;
  s.dpad = dpad;
  s.cross = cross;
  s.circle = circle;
  s.square = square;
  s.triangle = triangle;
  s.options = options;
  s.ps = ps;
  s.battery = 80;
  return s;
}

}  // namespace

void test_ds4_face_button_mapping_and_edges() {
  ds4::Ds4Core core;
  RecordSink sink;
  core.begin(true, 0);
  core.on_connect(7, 100);  // controller handle 7 → P1
  CHECK_EQ_INT(core.players_connected(), 1);
  // Cross press → exactly one Confirm.
  core.apply_snapshot(7, make_snap(0, true, false, false, false, false, false), 200, &sink);
  CHECK_EQ_INT(static_cast<int>(sink.events.size()), 1);
  CHECK_EQ_INT(static_cast<int>(sink.events[0]), static_cast<int>(InputEvent::Confirm));
  CHECK_EQ_INT(sink.players[0], kPlayer1);
  // Held: no re-emit (edge-decoded).
  core.apply_snapshot(7, make_snap(0, true, false, false, false, false, false), 300, &sink);
  CHECK_EQ_INT(static_cast<int>(sink.events.size()), 1);
  // Release + re-press: one more event.
  core.apply_snapshot(7, make_snap(0, false, false, false, false, false, false), 400, &sink);
  core.apply_snapshot(7, make_snap(0, true, false, false, false, false, false), 500, &sink);
  CHECK_EQ_INT(static_cast<int>(sink.events.size()), 2);
  // The full face/system map: circle→Back, square→Left, triangle→Right,
  // options→StartPause, SHARE→Home; PS reserved.
  core.apply_snapshot(7, make_snap(0, false, true, false, false, false, false), 600, &sink);
  core.apply_snapshot(7, make_snap(0, false, false, true, false, false, false), 700, &sink);
  core.apply_snapshot(7, make_snap(0, false, false, false, true, false, false), 800, &sink);
  core.apply_snapshot(7, make_snap(0, false, false, false, false, true, false), 900, &sink);
  ds4::Snapshot share = make_snap(0, false, false, false, false, false, false);
  share.share = true;
  core.apply_snapshot(7, share, 1000, &sink);
  CHECK_EQ_INT(static_cast<int>(sink.events[2]), static_cast<int>(InputEvent::Back));
  CHECK_EQ_INT(static_cast<int>(sink.events[3]), static_cast<int>(InputEvent::Left));
  CHECK_EQ_INT(static_cast<int>(sink.events[4]), static_cast<int>(InputEvent::Right));
  CHECK_EQ_INT(static_cast<int>(sink.events[5]), static_cast<int>(InputEvent::StartPause));
  CHECK_EQ_INT(static_cast<int>(sink.events[6]), static_cast<int>(InputEvent::Home));
}

void test_ds4_dpad_directions_and_autorepeat() {
  ds4::Ds4Core core;
  RecordSink sink;
  core.begin(true, 0);
  core.on_connect(1, 0);
  // Bluepad32 bit 1 = up.
  core.apply_snapshot(1, make_snap(1, false, false, false, false, false, false), 1000, &sink);
  CHECK_EQ_INT(static_cast<int>(sink.events.size()), 1);
  CHECK_EQ_INT(static_cast<int>(sink.events[0]), static_cast<int>(InputEvent::Up));
  // Held without change: no event before the 400 ms delay.
  core.apply_snapshot(1, make_snap(1, false, false, false, false, false, false), 1300, &sink);
  CHECK_EQ_INT(static_cast<int>(sink.events.size()), 1);
  // At/after 400 ms: repeat starts.
  core.apply_snapshot(1, make_snap(1, false, false, false, false, false, false), 1401, &sink);
  CHECK_EQ_INT(static_cast<int>(sink.events.size()), 2);
  // Then every 150 ms.
  core.apply_snapshot(1, make_snap(1, false, false, false, false, false, false), 1500, &sink);
  CHECK_EQ_INT(static_cast<int>(sink.events.size()), 2);  // not yet
  core.apply_snapshot(1, make_snap(1, false, false, false, false, false, false), 1552, &sink);
  CHECK_EQ_INT(static_cast<int>(sink.events.size()), 3);
  // Released: nothing; then a different direction fires immediately.
  core.apply_snapshot(1, make_snap(0, false, false, false, false, false, false), 1600, &sink);
  core.apply_snapshot(1, make_snap(2, false, false, false, false, false, false), 1700, &sink);
  CHECK_EQ_INT(static_cast<int>(sink.events.size()), 4);
  CHECK_EQ_INT(static_cast<int>(sink.events[3]), static_cast<int>(InputEvent::Down));
  // Bluepad32 right/left bits.
  core.apply_snapshot(1, make_snap(4, false, false, false, false, false, false), 1800, &sink);
  CHECK_EQ_INT(static_cast<int>(sink.events[4]), static_cast<int>(InputEvent::Right));
  core.apply_snapshot(1, make_snap(8, false, false, false, false, false, false), 1900, &sink);
  CHECK_EQ_INT(static_cast<int>(sink.events[5]), static_cast<int>(InputEvent::Left));
}

void test_ds4_slots_lifecycle_and_state() {
  ds4::Ds4Core core;
  RecordSink sink;
  core.begin(true, 0);
  CHECK(core.state() == ds4::ControllerState::Unpaired);
  CHECK_STR_EQ(ds4::controller_state_name(core.state()), "unpaired");
  // Snapshots before any connect are ignored (no slot → no events).
  core.apply_snapshot(9, make_snap(0, true, false, false, false, false, false), 100, &sink);
  CHECK_EQ_INT(static_cast<int>(sink.events.size()), 0);
  // First connect → P1, second → P2.
  core.on_connect(3, 200);
  core.on_connect(4, 250);
  CHECK_EQ_INT(core.players_connected(), 2);
  CHECK(core.state() == ds4::ControllerState::Connected);
  core.apply_snapshot(3, make_snap(0, true, false, false, false, false, false), 300, &sink);
  core.apply_snapshot(4, make_snap(0, false, true, false, false, false, false), 310, &sink);
  CHECK_EQ_INT(sink.players[0], kPlayer1);
  CHECK_EQ_INT(sink.players[1], kPlayer2);
  // Disconnect P1: P2 keeps its slot and attribution.
  core.on_disconnect(3, 400);
  CHECK_EQ_INT(core.players_connected(), 1);
  core.apply_snapshot(4, make_snap(0, false, false, true, false, false, false), 410, &sink);
  CHECK_EQ_INT(sink.players[2], kPlayer2);
  // All disconnected → Disconnected state.
  core.on_disconnect(4, 500);
  CHECK_EQ_INT(core.players_connected(), 0);
  CHECK(core.state() == ds4::ControllerState::Disconnected);
  CHECK_STR_EQ(ds4::controller_state_name(core.state()), "disconnected");
  // Pairing transitions only from unpaired/disconnected.
  core.on_pairing_started(550);
  CHECK(core.state() == ds4::ControllerState::Pairing);
  CHECK_STR_EQ(ds4::controller_state_name(core.state()), "pairing");
  // Reconnect of a known handle re-slots as the free slot (P1 now).
  core.on_connect(4, 600);
  CHECK_EQ_INT(core.players_connected(), 1);
  CHECK(core.state() == ds4::ControllerState::Connected);
  core.apply_snapshot(4, make_snap(0, false, false, false, true, false, false), 610, &sink);
  CHECK_EQ_INT(sink.players[3], kPlayer1);
  // Pairing reports never override a live connection.
  core.on_pairing_started(700);
  CHECK(core.state() == ds4::ControllerState::Connected);
  // Battery normalization is stored per slot.
  CHECK_EQ_INT(core.battery_p1(), 80);
}

void test_ds4_disabled_core_is_inert() {
  ds4::Ds4Core core;
  RecordSink sink;
  core.begin(false, 0);
  core.on_connect(1, 0);
  core.apply_snapshot(1, make_snap(0, true, true, true, true, true, true), 100, &sink);
  CHECK_EQ_INT(static_cast<int>(sink.events.size()), 0);
  CHECK_EQ_INT(core.players_connected(), 0);
}

void test_ds4_stick_share_and_player_ownership() {
  ds4::Ds4Core core;
  RecordSink sink;
  core.begin(true, 0);
  core.set_phone_mask(1);  // phone owns P1
  core.on_connect(7, 100);
  CHECK_EQ_INT(core.player_for(7), kPlayer2);
  CHECK_EQ_INT(core.occupied_mask(), 2);
  CHECK(!core.assign(7, kPlayer1));

  ds4::Snapshot snap{};
  snap.axis_x = -300;
  core.apply_snapshot(7, snap, 200, &sink);
  CHECK_EQ_INT(static_cast<int>(sink.events[0]), static_cast<int>(InputEvent::Left));
  CHECK_EQ_INT(sink.players[0], kPlayer2);
  snap.axis_x = -120;  // hysteresis keeps the held direction
  core.apply_snapshot(7, snap, 300, &sink);
  CHECK_EQ_INT(static_cast<int>(sink.events.size()), 1);
  snap.axis_x = 0;
  snap.ps = true;  // PS is reserved and never generates gameplay input
  core.apply_snapshot(7, snap, 400, &sink);
  CHECK_EQ_INT(static_cast<int>(sink.events.size()), 1);
  snap.share = true;
  core.apply_snapshot(7, snap, 500, &sink);
  CHECK_EQ_INT(static_cast<int>(sink.events[1]), static_cast<int>(InputEvent::Home));
  core.set_phone_mask(0);
  CHECK(core.assign(7, kPlayer1));
  CHECK_EQ_INT(core.player_for(7), kPlayer1);
  CHECK_EQ_INT(core.take_lost_mask(), 2);
  core.on_disconnect(7, 600);
  CHECK_EQ_INT(core.take_lost_mask(), 1);
  CHECK_EQ_INT(core.occupied_mask(), 0);
  core.on_connect(7, 700);
  CHECK_EQ_INT(core.player_for(7), kPlayer1);
  core.on_connect(7, 710);  // duplicate callback is idempotent
  CHECK_EQ_INT(core.players_connected(), 1);
}

void test_wired_codec_roundtrip_and_rejection() {
  wired::Packet p;
  p.player = 2;
  p.dpad = wired::dpad_bits::kUp | wired::dpad_bits::kLeft;
  p.keys = wired::key_bits::kConfirm | wired::key_bits::kStartPause;
  p.axis_x = 200;
  p.axis_y = 55;
  p.joystick_push = true;
  p.seq = 0x7E;
  uint8_t frame[wired::kFrameLen];
  CHECK_EQ_INT(wired::encode(p, frame), wired::kFrameLen);
  CHECK_EQ_INT(frame[0], 0xAA);
  CHECK_EQ_INT(frame[1], 0x55);
  const wired::DecodeResult r = wired::decode(frame, wired::kFrameLen);
  CHECK(r.ok);
  CHECK_EQ_INT(r.packet.player, 2);
  CHECK_EQ_INT(r.packet.dpad, p.dpad);
  CHECK_EQ_INT(r.packet.keys, p.keys);
  CHECK_EQ_INT(r.packet.axis_x, 200);
  CHECK_EQ_INT(r.packet.axis_y, 55);
  CHECK(r.packet.joystick_push);
  CHECK_EQ_INT(r.packet.seq, 0x7E);
  // Corruption anywhere in the CRC-covered region rejects the frame.
  for (uint8_t i = 2; i < 9; ++i) {
    uint8_t bad[wired::kFrameLen];
    memcpy(bad, frame, sizeof(bad));
    bad[i] = static_cast<uint8_t>(bad[i] ^ 0x5Au);
    CHECK(!wired::decode(bad, wired::kFrameLen).ok);
  }
  // Short frames / bad magic reject.
  CHECK(!wired::decode(frame, 9).ok);
  CHECK(!wired::decode(nullptr, wired::kFrameLen).ok);
  frame[0] = 0x00;
  CHECK(!wired::decode(frame, wired::kFrameLen).ok);
  // CRC-8 known-answer (poly 0x07 over "123456789" = 0xF4 — standard check
  // value, transcribed independently).
  const uint8_t kat[9] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  CHECK_EQ_INT(wired::crc8(kat, 9), 0xF4);
}
