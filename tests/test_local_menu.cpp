#include "test_framework.h"

#include "menu/local_menu.h"

// LED-L009 local pixel menu: navigation, enter/back, mode transitions,
// brightness editing, standby toggle — pure decisions with recorded actions.

using namespace ledwall;

namespace {

class FakeDevice final : public MenuActions {
 public:
  uint32_t brightness_level() const override { return applied; }
  bool standby_on() const override { return standby; }
  bool standby{false};
  uint32_t applied{40};  // the runtime applies each SetBrightness action
};

MenuAction handle(MenuState& m, MenuActions& dev, InputEvent ev) {
  MenuAction a;
  m.handle_input(ev, dev, &a);
  return a;
}

}  // namespace

void test_menu_open_close_and_root_navigation() {
  FakeDevice dev;
  MenuState m;
  CHECK(!m.active());
  m.open(1000);
  CHECK(m.active());
  CHECK_EQ_INT(static_cast<int>(m.screen()), static_cast<int>(MenuScreen::Root));
  CHECK_EQ_INT(m.root_index(), 0);
  handle(m, dev, InputEvent::Down);
  handle(m, dev, InputEvent::Down);
  CHECK_EQ_INT(m.root_index(), 2);
  handle(m, dev, InputEvent::Up);
  CHECK_EQ_INT(m.root_index(), 1);
  // Bounds: never below 0 / above kMenuRootItems-1.
  for (int i = 0; i < 20; ++i) handle(m, dev, InputEvent::Up);
  CHECK_EQ_INT(m.root_index(), 0);
  for (int i = 0; i < 20; ++i) handle(m, dev, InputEvent::Down);
  CHECK_EQ_INT(m.root_index(), kMenuRootItems - 1);
  for (int i = 0; i < 20; ++i) handle(m, dev, InputEvent::Down);
  CHECK_EQ_INT(m.root_index(), kMenuRootItems - 1);
}

void test_menu_timer_and_standby_and_exit_actions() {
  FakeDevice dev;
  MenuState m;
  m.open(0);
  // Root index 0 = Timer: Confirm emits EnterTimer and closes.
  MenuAction a = handle(m, dev, InputEvent::Confirm);
  CHECK_EQ_INT(static_cast<int>(a.kind), static_cast<int>(MenuAction::Kind::EnterTimer));
  CHECK(!m.active());
  // Standby (index 5) toggles and closes.
  m.open(0);
  for (int i = 0; i < 5; ++i) handle(m, dev, InputEvent::Down);
  a = handle(m, dev, InputEvent::Confirm);
  CHECK_EQ_INT(static_cast<int>(a.kind), static_cast<int>(MenuAction::Kind::ToggleStandby));
  CHECK(!m.active());
  // Exit (index 6) closes.
  m.open(0);
  for (int i = 0; i < 6; ++i) handle(m, dev, InputEvent::Down);
  a = handle(m, dev, InputEvent::Confirm);
  CHECK_EQ_INT(static_cast<int>(a.kind), static_cast<int>(MenuAction::Kind::Exit));
  // Home exits from anywhere.
  m.open(0);
  a = handle(m, dev, InputEvent::Home);
  CHECK_EQ_INT(static_cast<int>(a.kind), static_cast<int>(MenuAction::Kind::Exit));
  CHECK(!m.active());
  // Back at the root exits.
  m.open(0);
  a = handle(m, dev, InputEvent::Back);
  CHECK_EQ_INT(static_cast<int>(a.kind), static_cast<int>(MenuAction::Kind::Exit));
}

void test_menu_lists_select_and_back() {
  FakeDevice dev;
  MenuState m;
  m.open(0);
  handle(m, dev, InputEvent::Down);  // Animations
  handle(m, dev, InputEvent::Confirm);
  CHECK_EQ_INT(static_cast<int>(m.screen()), static_cast<int>(MenuScreen::Animations));
  CHECK_EQ_INT(m.list_index(), 0);
  handle(m, dev, InputEvent::Down);
  handle(m, dev, InputEvent::Down);
  CHECK_EQ_INT(m.list_index(), 2);
  MenuAction a = handle(m, dev, InputEvent::Confirm);
  CHECK_EQ_INT(static_cast<int>(a.kind), static_cast<int>(MenuAction::Kind::SelectAnimation));
  CHECK_EQ_INT(a.index, 2);
  CHECK(!m.active());
  // Back from a list returns to the root (menu stays open).
  m.open(0);
  handle(m, dev, InputEvent::Down);
  handle(m, dev, InputEvent::Confirm);
  a = handle(m, dev, InputEvent::Back);
  CHECK(a.kind == MenuAction::Kind::None);
  CHECK(m.active());
  CHECK_EQ_INT(static_cast<int>(m.screen()), static_cast<int>(MenuScreen::Root));
  // List bounds track the real registry sizes.
  m.open(0);
  handle(m, dev, InputEvent::Down);      // Animations
  handle(m, dev, InputEvent::Confirm);
  for (int i = 0; i < kAnimationCount + 4; ++i) handle(m, dev, InputEvent::Down);
  CHECK_EQ_INT(m.list_index(), kAnimationCount - 1);
  // Games list selects games.
  m.open(0);
  for (int i = 0; i < 3; ++i) handle(m, dev, InputEvent::Down);  // Games
  handle(m, dev, InputEvent::Confirm);
  a = handle(m, dev, InputEvent::Confirm);
  CHECK_EQ_INT(static_cast<int>(a.kind), static_cast<int>(MenuAction::Kind::SelectGame));
  CHECK_EQ_INT(a.index, 0);
  // Expressions list.
  m.open(0);
  handle(m, dev, InputEvent::Down);
  handle(m, dev, InputEvent::Down);
  handle(m, dev, InputEvent::Confirm);
  a = handle(m, dev, InputEvent::Confirm);
  CHECK_EQ_INT(static_cast<int>(a.kind), static_cast<int>(MenuAction::Kind::SelectExpression));
}

void test_menu_brightness_editing() {
  FakeDevice dev;
  MenuState m;
  m.open(0);
  for (int i = 0; i < 4; ++i) handle(m, dev, InputEvent::Down);  // Brightness
  handle(m, dev, InputEvent::Confirm);
  CHECK_EQ_INT(static_cast<int>(m.screen()), static_cast<int>(MenuScreen::Brightness));
  MenuAction a = handle(m, dev, InputEvent::Right);
  CHECK_EQ_INT(static_cast<int>(a.kind), static_cast<int>(MenuAction::Kind::SetBrightness));
  CHECK_EQ_INT(a.level, 45u);  // +5 from the reported device level (40)
  dev.applied = 45;
  a = handle(m, dev, InputEvent::Left);
  CHECK_EQ_INT(a.level, 40u);
  dev.applied = 40;
  // Walk to the ceiling: the step clamps at 100 (runtime clamps again).
  for (int i = 0; i < 30; ++i) {
    a = handle(m, dev, InputEvent::Up);
    dev.applied = a.level;
  }
  CHECK_EQ_INT(a.level, 100u);
  a = handle(m, dev, InputEvent::Down);
  CHECK_EQ_INT(a.level, 95u);
  a = handle(m, dev, InputEvent::Confirm);
  CHECK(a.kind == MenuAction::Kind::None);
  CHECK(m.active());
  CHECK_EQ_INT(static_cast<int>(m.screen()), static_cast<int>(MenuScreen::Root));
}

void test_menu_closed_ignores_input() {
  FakeDevice dev;
  MenuState m;
  MenuAction a = handle(m, dev, InputEvent::Confirm);
  CHECK(a.kind == MenuAction::Kind::None);
  CHECK(!m.active());
}
