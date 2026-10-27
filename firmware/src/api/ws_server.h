#pragma once

#include <cstdint>

#include <WebSockets.h>
#include <WebSocketsServer.h>

namespace ledwall {

// WebSocket event channel (frozen spec §9): pushes JSON events
// (state.changed / mode.changed / transient.expired) to connected clients.
// Bounded broadcast with slow-client drop (§9 heritage hardening): a client
// whose send fails is disconnected. No discovery, no authentication (v1,
// documented LAN-trust residual risk).
class WsApi {
 public:
  void begin(uint16_t port);
  void tick();  // services the WebSocket loop (once per main-loop iteration)
  bool running() const { return running_; }
  uint8_t clients_count();

  // Broadcasts a fully serialized JSON document. Returns the number of
  // clients the payload was delivered to.
  uint8_t broadcast(const char* json);

 private:
  WebSocketsServer* server_{nullptr};
  bool running_{false};
};

}  // namespace ledwall
