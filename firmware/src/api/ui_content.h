#pragma once

#include <cstddef>

namespace ledwall {
namespace ui {

// LED-L004 — embedded Phone/Web dashboard. One self-contained HTML document
// (inline CSS/JS, no external resources) served same-origin at GET /.
//
// Boundary (PLAN Phase 3; frozen spec §1/§3/§16/§19): the dashboard is a pure
// LED-L003 semantic API client — it uses only the documented endpoints and
// never references renderers, FastLED, GPIO, or LED indexes. Serving a static
// PROGMEM string is transport glue (§12-bounded response class), not a UI
// subsystem. Favorites live in browser localStorage, never on the device (§18).
//
// is_index_route is the pure specification of the route registered in
// http_server (GET / exactly); the host suite pins both sides.

// PROGMEM pointer on ESP32; plain pointer on the host test build.
const char* index_html();
const char* index_content_type();

// Length of the embedded document (strlen-equivalent; PROGMEM-safe).
size_t index_html_len();

// True iff (method, uri) is the dashboard route: exactly "GET" and "/".
bool is_index_route(const char* method, const char* uri);

}  // namespace ui
}  // namespace ledwall
