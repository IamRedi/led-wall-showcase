#pragma once

#include <cstdint>

#include "api/registry.h"
#include "input/input_events.h"

namespace ledwall {

// LED-L009 local pixel menu (reconstruction brief §13): the panel stays usable
// without a phone. Pure state machine — it renders through the renderer layer
// (a MenuState view) and applies its decisions through MenuActions, an
// interface the runtime implements with real commands and host tests with a
// fake. The menu itself is a semantic mode (ModeKind::Menu), never random
// renderer code.
//
// Root sections: Timer, Animations, Expressions, Games, Brightness, Standby,
// Exit (brief §13 list + Back/Home).
enum class MenuScreen : uint8_t {
  Root = 0,
  Animations,
  Expressions,
  Games,
  Brightness,
};
constexpr uint8_t kMenuRootItems = 7;  // timer, animations, expressions, games, brightness, standby, exit

// What the menu wants the runtime to do (pure decision, side-effect free).
struct MenuAction {
  enum class Kind : uint8_t {
    None = 0,
    EnterTimer,       // open the timer display mode
    SelectAnimation,  // | animation index (registry order)
    SelectExpression, // | expression index
    SelectGame,       // | game index
    SetBrightness,    // | level 0..100
    ToggleStandby,    // standby on/off toggle
    Exit,             // close the menu (Back/Home/Exit)
  };
  Kind kind{Kind::None};
  uint8_t index{0};      // SelectAnimation/Expression/Game: registry index
  uint32_t level{0};     // SetBrightness: semantic level
};

class MenuActions {
 public:
  virtual ~MenuActions() = default;
  // Context the menu needs from the device (read-only).
  virtual uint8_t animation_count() const { return kAnimationCount; }
  virtual uint8_t expression_count() const { return kExpressionCount; }
  virtual uint8_t game_count() const { return kGameCount; }
  virtual uint32_t brightness_level() const = 0;
  virtual bool standby_on() const = 0;
};

class MenuState {
 public:
  void open(uint32_t now_ms);
  void close();
  bool active() const { return active_; }
  MenuScreen screen() const { return screen_; }
  uint8_t root_index() const { return root_index_; }
  uint8_t list_index() const { return list_index_; }

  // Display mirrors refreshed by the runtime each tick (renderer-side label
  // content only — never menu logic inputs).
  uint32_t brightness_mirror{0};
  bool standby_mirror{false};

  // Handles one input event; may emit one action through `out`.
  void handle_input(InputEvent event, const MenuActions& device, MenuAction* out);

 private:
  static void emit_(MenuAction* out, MenuAction::Kind kind, uint8_t index, uint32_t level);
  bool active_{false};
  MenuScreen screen_{MenuScreen::Root};
  uint8_t root_index_{0};
  uint8_t list_index_{0};
};

}  // namespace ledwall
