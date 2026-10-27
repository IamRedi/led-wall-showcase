#include "menu/local_menu.h"

namespace ledwall {

// Root item order (brief §13: Timer, Animations, Expressions, Games,
// Brightness, Standby, Back/Home).
namespace {
constexpr uint8_t kRootTimer = 0;
constexpr uint8_t kRootAnimations = 1;
constexpr uint8_t kRootExpressions = 2;
constexpr uint8_t kRootGames = 3;
constexpr uint8_t kRootBrightness = 4;
constexpr uint8_t kRootStandby = 5;
constexpr uint8_t kRootExit = 6;
constexpr int16_t kBrightnessStep = 5;
}  // namespace

void MenuState::emit_(MenuAction* out, MenuAction::Kind kind, uint8_t index, uint32_t level) {
  if (out == nullptr) {
    return;
  }
  *out = MenuAction{};
  out->kind = kind;
  out->index = index;
  out->level = level;
}

void MenuState::open(uint32_t now_ms) {
  (void)now_ms;
  active_ = true;
  screen_ = MenuScreen::Root;
  root_index_ = 0;  // deterministic entry: the menu always opens at Timer
  list_index_ = 0;
}

void MenuState::close() {
  active_ = false;
  screen_ = MenuScreen::Root;
}

void MenuState::handle_input(InputEvent event, const MenuActions& device, MenuAction* out) {
  if (out != nullptr) {
    *out = MenuAction{};  // default: no action
  }
  if (!active_) {
    return;
  }

  if (event == InputEvent::Home) {
    emit_(out, MenuAction::Kind::Exit, 0, 0);
    close();
    return;
  }

  switch (screen_) {
    case MenuScreen::Root: {
      switch (event) {
        case InputEvent::Up:
          if (root_index_ > 0) {
            --root_index_;
          }
          break;
        case InputEvent::Down:
          if (root_index_ + 1 < kMenuRootItems) {
            ++root_index_;
          }
          break;
        case InputEvent::Confirm:
          switch (root_index_) {
            case kRootTimer:
              emit_(out, MenuAction::Kind::EnterTimer, 0, 0);
              close();
              break;
            case kRootAnimations:
              screen_ = MenuScreen::Animations;
              list_index_ = 0;
              break;
            case kRootExpressions:
              screen_ = MenuScreen::Expressions;
              list_index_ = 0;
              break;
            case kRootGames:
              screen_ = MenuScreen::Games;
              list_index_ = 0;
              break;
            case kRootBrightness:
              screen_ = MenuScreen::Brightness;
              list_index_ = 0;
              break;
            case kRootStandby:
              emit_(out, MenuAction::Kind::ToggleStandby, 0, 0);
              close();  // standby blanks the panel; menu never lingers over it
              break;
            case kRootExit:
            default:
              emit_(out, MenuAction::Kind::Exit, 0, 0);
              close();
              break;
          }
          break;
        case InputEvent::Back:
          emit_(out, MenuAction::Kind::Exit, 0, 0);
          close();
          break;
        default:
          break;
      }
      return;
    }

    case MenuScreen::Animations:
    case MenuScreen::Expressions:
    case MenuScreen::Games: {
      const uint8_t count =
          (screen_ == MenuScreen::Animations)
              ? device.animation_count()
              : (screen_ == MenuScreen::Expressions) ? device.expression_count()
                                                     : device.game_count();
      if (count == 0) {
        return;  // empty registry: fail closed, nothing to navigate
      }
      switch (event) {
        case InputEvent::Up:
          if (list_index_ > 0) {
            --list_index_;
          }
          break;
        case InputEvent::Down:
          if (list_index_ + 1 < count) {
            ++list_index_;
          }
          break;
        case InputEvent::Confirm: {
          const MenuAction::Kind kind = (screen_ == MenuScreen::Animations)
                                            ? MenuAction::Kind::SelectAnimation
                                        : (screen_ == MenuScreen::Expressions)
                                            ? MenuAction::Kind::SelectExpression
                                            : MenuAction::Kind::SelectGame;
          emit_(out, kind, list_index_, 0);
          close();  // selections hand control to the chosen mode
          break;
        }
        case InputEvent::Back:
          screen_ = MenuScreen::Root;
          list_index_ = 0;
          break;
        default:
          break;
      }
      return;
    }

    case MenuScreen::Brightness: {
      const uint32_t level = device.brightness_level();
      switch (event) {
        case InputEvent::Up:
        case InputEvent::Right:
          emit_(out, MenuAction::Kind::SetBrightness, 0,
                level + kBrightnessStep > 100 ? 100u : level + kBrightnessStep);
          break;
        case InputEvent::Down:
        case InputEvent::Left:
          emit_(out, MenuAction::Kind::SetBrightness, 0,
                level > kBrightnessStep ? level - kBrightnessStep : 0u);
          break;
        case InputEvent::Confirm:
        case InputEvent::Back:
          screen_ = MenuScreen::Root;
          break;
        default:
          break;
      }
      return;
    }
  }
}

}  // namespace ledwall
