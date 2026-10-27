// LED-L010 DS4 device glue implementation (ADR-LED-L010 D5). Compiled ONLY
// when LED_WALL_ENABLE_DS4 is defined (esp32_eq_ds4 env with the Bluepad32
// prebuilt Arduino core, upstream release 4.1.0). Maps the Bluepad32 v4
// callback API into the pure ds4_core and forwards normalized player-tagged
// events to the unified queue.
//
// Bluepad32 v4 integration notes:
// - BP32.setup(onConnected, onDisconnected) arms the BT-Classic HID host
//   (scanning starts automatically; a DS4 associates when its PS button is
//   pressed while unpaired);
// - BP32.update() pumps the stack on the Arduino loop; controller data is
//   read from the stored ControllerPtr handles;
// - slots: the glue keeps up to two handles; the pure core assigns P1/P2 by
//   connect order (slot 0 → P1, slot 1 → P2).

#include "input/ds4_provider_bluepad32.h"

#if defined(LED_WALL_ENABLE_DS4)

#include <Bluepad32.h>

#include "input/ds4_core.h"

namespace ledwall {
namespace ds4 {

namespace {

constexpr int kMaxHandles = 4;  // BP32_MAX_GAMEPADS (we use the first two slots)

Ds4Core g_core;
InputEventQueue* g_queue = nullptr;
bool g_ready = false;
uint8_t g_phone_mask = 0;
uint32_t g_input_events = 0;

class QueueSink final : public EventSink {
 public:
  void push(uint8_t player, InputEvent event) override {
    if (g_queue != nullptr) {
      g_queue->push(player, event, InputSource::Ds4);
      ++g_input_events;
    }
  }
};
QueueSink g_sink;

// Controller handles (raw pointers owned by the Bluepad32 stack; slot index
// is the pure-core controller_id).
ControllerPtr g_handles[kMaxHandles] = {nullptr, nullptr, nullptr, nullptr};

int find_slot_(ControllerPtr ctl) {
  for (int i = 0; i < kMaxHandles; ++i) {
    if (g_handles[i] == ctl) {
      return i;
    }
  }
  return -1;
}

void on_connected_cb(ControllerPtr ctl) {
  if (ctl == nullptr) {
    return;
  }
  for (int i = 0; i < kMaxHandles; ++i) {
    if (g_handles[i] == nullptr) {
      g_handles[i] = ctl;
      g_core.on_connect(static_cast<uint8_t>(i), millis());
      return;
    }
  }
  // More than four controllers: the core cannot slot them (bounded product).
}

void on_disconnected_cb(ControllerPtr ctl) {
  const int slot = find_slot_(ctl);
  if (slot < 0) {
    return;
  }
  g_handles[slot] = nullptr;
  g_core.on_disconnect(static_cast<uint8_t>(slot), millis());
}

Snapshot read_snapshot(ControllerPtr c) {
  Snapshot s;
  s.dpad = static_cast<uint8_t>(c->dpad());
  s.cross = c->a();         // Cross
  s.circle = c->b();        // Circle
  s.square = c->x();        // Square
  s.triangle = c->y();      // Triangle
  s.options = c->miscStart();   // OPTIONS
  s.ps = c->miscSystem();       // PS (system/capture) button
  s.axis_x = static_cast<int16_t>(c->axisX());
  s.axis_y = static_cast<int16_t>(c->axisY());
  s.share = c->miscSelect();
  s.l1 = c->l1();
  s.r1 = c->r1();
  // Bluepad32 battery: 0 unknown; otherwise 1..255.
  const int raw = c->battery();
  s.battery = static_cast<uint8_t>((static_cast<uint32_t>(raw < 0 ? 0 : raw) * 100u) / 255u);
  return s;
}

}  // namespace

const char* state_name(ControllerState state) { return controller_state_name(state); }

void begin(bool enabled, InputEventQueue* queue) {
  g_queue = queue;
  g_core.begin(enabled, 0);
  g_ready = false;
  g_phone_mask = 0;
  g_input_events = 0;
  for (ControllerPtr& handle : g_handles) handle = nullptr;
}

void tick(uint32_t now_ms, bool wifi_ready, uint8_t phone_mask) {
  if (g_queue == nullptr || !g_core.enabled()) {
    return;
  }
  g_phone_mask = phone_mask;
  g_core.set_phone_mask(phone_mask);
  if (!g_ready) {
    // The previous DS4 image started Bluetooth before the first deferred Wi-Fi
    // association. Bring up the Wi-Fi station first; the prebuilt core has
    // CONFIG_ESP32_WIFI_SW_COEXIST_ENABLE. Device proof remains mandatory.
    if (!wifi_ready) return;
    BP32.setup(&on_connected_cb, &on_disconnected_cb);
    g_ready = true;
  }
  BP32.update();  // pump the stack (data-ready flag not needed: snapshots poll)
  for (int i = 0; i < kMaxHandles; ++i) {
    ControllerPtr c = g_handles[i];
    if (c == nullptr || !c->isConnected()) {
      continue;
    }
    if (g_core.player_for(static_cast<uint8_t>(i)) == 0)
      g_core.on_connect(static_cast<uint8_t>(i), now_ms);  // retry when a phone releases a slot
    g_core.apply_snapshot(static_cast<uint8_t>(i), read_snapshot(c), now_ms, &g_sink);
  }
}

bool pair() {
  if (!g_ready) return false;
  BP32.enableNewBluetoothConnections(true);
  g_core.on_pairing_started(millis());
  return true;
}

bool assign_primary(uint8_t player) {
  if (!g_ready || player < 1 || player > 2 ||
      (g_phone_mask & (1u << (player - 1u))) != 0) return false;
  for (int i = 0; i < kMaxHandles; ++i) {
    if (g_handles[i] != nullptr && g_handles[i]->isConnected() &&
        g_core.player_for(static_cast<uint8_t>(i)) != 0)
      return g_core.assign(static_cast<uint8_t>(i), player);
  }
  return false;
}

bool player_claimed(uint8_t player) {
  return player >= 1 && player <= 2 &&
         (g_core.occupied_mask() & (1u << (player - 1u))) != 0;
}

uint8_t take_lost_mask() { return g_core.take_lost_mask(); }

StateView view() {
  StateView v;
  v.enabled = g_core.enabled();
  v.ready = g_ready;
  v.state = g_core.state();
  v.players = g_core.players_connected();
  v.assigned_mask = g_core.occupied_mask();
  v.input_events = g_input_events;
  v.battery_p1 = g_core.battery_p1();
  v.battery_p2 = g_core.battery_p2();
  return v;
}

}  // namespace ds4
}  // namespace ledwall

#endif  // LED_WALL_ENABLE_DS4
