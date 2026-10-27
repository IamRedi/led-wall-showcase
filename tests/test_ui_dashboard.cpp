#include "test_framework.h"
#include <cstring>
#include <cstdio>
#include <initializer_list>
#include "api/ui_content.h"

namespace {
bool has(const char* needle) { return std::strstr(ledwall::ui::index_html(), needle) != nullptr; }
int occurrences(const char* needle) {
  int count=0; const char* p=ledwall::ui::index_html(); const size_t n=std::strlen(needle);
  while ((p=std::strstr(p,needle))!=nullptr) { ++count; p+=n; }
  return count;
}
}

void test_ui_route_matching() {
  CHECK(ledwall::ui::is_index_route("GET","/"));
  CHECK(!ledwall::ui::is_index_route("POST","/"));
  CHECK(!ledwall::ui::is_index_route("GET","/state"));
  CHECK(!ledwall::ui::is_index_route("GET","/index.html"));
  CHECK(!ledwall::ui::is_index_route(nullptr,"/"));
  CHECK(!ledwall::ui::is_index_route("GET",nullptr));
}
void test_ui_content_bounds_and_type() {
  size_t n=ledwall::ui::index_html_len();
  CHECK(n>30000); CHECK(n<96u*1024u); CHECK(n==std::strlen(ledwall::ui::index_html()));
  CHECK_STR_EQ(ledwall::ui::index_content_type(),"text/html; charset=utf-8");
  CHECK(has("<!doctype html>")); CHECK(has("</html>"));
}
void test_ui_content_ascii_only() {
  // V5 intentionally uses UTF-8 glyphs in its visual cards. Confirm complete
  // served bytes and a declared charset; no longer impose the V3 ASCII rule.
  CHECK(has("charset=\"utf-8\""));
  CHECK(std::strchr(ledwall::ui::index_html(), '\0') ==
        ledwall::ui::index_html()+ledwall::ui::index_html_len());
}
void test_ui_content_endpoints() {
  for (const char* path : {"/display/animation/set","/display/party/play",
      "/display/party/stop","/display/game/set","/display/game/difficulty",
      "/display/game/command","/display/input/session","/display/input/event",
      "/display/input/heartbeat","/display/input/release","/display/pixel/session",
      "/display/pixel/frame","/display/pixel/projects","/display/text/show",
      "/display/timer/set","/display/timer/start",
      "/display/brightness/set","/display/standby/set","/display/clear","/state"})
    CHECK(has(path));
  CHECK(has("'/display/timer/'+a"));
  CHECK(has("['timerPause','pause']"));
  CHECK(has("['timerResume','resume']"));
  CHECK(has("['timerReset','reset']"));
  CHECK(has("['timerStop','stop']"));
  CHECK(!has("/display/visualizer/data"));
  CHECK(!has("/display/tail/point"));
}
void test_ui_content_event_names() {
  CHECK(has("state.changed")); CHECK(has("mode.changed"));
  CHECK(has("timer.done")); CHECK(has("text.done"));
}
void test_ui_content_vocabularies() {
  for (const char* name : {"fireworks","meteor_impact","energy_bloom","rainbow_river",
      "storm","lava_fountain","rain_drain","aurora_current","starstream","heartbeat",
      "pong","tail_tennis","light_race","reaction_stop","quick_draw",
      "tug_of_war","memory","color_chain","light_quest","breakout","snake","dodger","color_catch"})
    CHECK(has(name));
  CHECK(!has("id=\"zoneChips\""));
  CHECK(!has("data-z=\"matrix\""));
  CHECK(!has("data-z=\"tail\""));
  CHECK(!has("data-z=\"both\""));
}
void test_ui_content_error_vocabulary() {
  CHECK(has("textTooWide")); CHECK(has("unknownGame"));
  CHECK(has("invalidParameter")); CHECK(has("ledUnavailable"));
}
void test_ui_content_contract_markers() {
  CHECK(occurrences("/*LWCORE-START*/")==1);
  CHECK(occurrences("/*LWCORE-END*/")==1);
  CHECK(has("ledwall-v5-draft")); CHECK(has("ledwall-v5-pixels"));
  CHECK(has("maxlength=\"128\"")); CHECK(has("revision:next"));
}
void test_ui_content_forbidden_constructs() {
  CHECK(!has("innerHTML")); CHECK(!has("document.write")); CHECK(!has("eval("));
  CHECK(!has("<script src")); CHECK(!has("http://")); CHECK(!has("https://"));
  CHECK(!has("fetch('http")); CHECK(!has("fetch(\"http"));
}
void test_ui_v3_progressive_disclosure_structure() {
  // The V5 replacement has one compact six-card Home, dedicated destinations,
  // and a contextual bottom sheet. None of the old six-tab dashboard survives.
  CHECK(has("id=\"view-home\"")); CHECK(has("id=\"sheetWrap\""));
  for (const char* name : {"scenes","games","create","text","timer","device"}) {
    char target[48]; std::snprintf(target,sizeof(target),"data-go=\"%s\"",name); CHECK(has(target));
    std::snprintf(target,sizeof(target),"id=\"view-%s\"",name); CHECK(has(target));
  }
  CHECK(has("id=\"matrixCanvas\"")); CHECK(has("id=\"tailOverview\""));
  CHECK(has("id=\"tailDetail\"")); CHECK(has("id=\"gamePad\""));
  CHECK(has("id=\"deviceStorage\"")); CHECK(!has("nav#tabs"));
}
void test_ui_v3_wire_vocabulary() {
  CHECK(has("target:'both'")); CHECK(has("buildGameSet"));
  CHECK(has("sequence:++s.seq")); CHECK(has("ack.applied!==true"));
  CHECK(has("gameExit")); CHECK(has("pixelLive"));
}
