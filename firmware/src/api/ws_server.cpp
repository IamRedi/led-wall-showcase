#include "api/ws_server.h"

namespace ledwall {

void WsApi::begin(uint16_t port) {
  server_ = new WebSocketsServer(port);
  // No client->server command traffic in v1: commands are HTTP-only (§9
  // canonical transport). Client messages are ignored (and drained) here.
  server_->begin();
  running_ = true;
}

void WsApi::tick() { server_->loop(); }

uint8_t WsApi::clients_count() { return running_ ? server_->connectedClients() : 0; }

uint8_t WsApi::broadcast(const char* json) {
  if (!running_ || json == nullptr) {
    return 0;
  }
  const uint8_t delivered = server_->broadcastTXT(json);
  // broadcastTXT reports per-client failure by disconnecting broken sockets;
  // anything not delivered was a slow/dead client and has been dropped (§9).
  return delivered;
}

}  // namespace ledwall
