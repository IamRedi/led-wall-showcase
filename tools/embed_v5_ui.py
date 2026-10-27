#!/usr/bin/env python3
"""Embed the checked-in V5 web source in the firmware's same-origin PROGMEM route."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
core = (root / 'firmware/web/ui_core.js').read_text()
html = (root / 'firmware/web/v5.html').read_text().replace('/*CORE_PLACEHOLDER*/', core)
assert ')lwui_v1"' not in html
out = root / 'firmware/src/api/ui_content.cpp'
out.write_text('''#include "api/ui_content.h"
#include <cstring>
#if defined(ARDUINO)
#include <pgmspace.h>
#define LW_UI_PROGMEM PROGMEM
#else
#define LW_UI_PROGMEM
#endif
namespace ledwall { namespace ui {
namespace {
const char kIndexHtmlStore[] LW_UI_PROGMEM = R"lwui_v1(''' + html + ''')lwui_v1";
}
const char* index_html() { return kIndexHtmlStore; }
const char* index_content_type() { return "text/html; charset=utf-8"; }
size_t index_html_len() { return sizeof(kIndexHtmlStore) - 1; }
bool is_index_route(const char* method, const char* uri) {
  return method != nullptr && uri != nullptr &&
         std::strcmp(method, "GET") == 0 && std::strcmp(uri, "/") == 0;
}
} }
''')
