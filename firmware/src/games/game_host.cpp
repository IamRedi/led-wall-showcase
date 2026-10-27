#include "games/game_host.h"

#include <cstdio>
#include <cstring>

#include "api/registry.h"
#include "text/text_renderer.h"

namespace ledwall {
namespace games {

namespace {
constexpr uint32_t kBlinkPeriodMs = 600;
constexpr Rgb kChromeWhite{255, 255, 255};
constexpr Rgb kChromeRed{200, 30, 30};
constexpr Rgb kChromeGreen{40, 200, 60};
}  // namespace

const char* game_phase_name(GamePhase phase) {
  switch (phase) {
    case GamePhase::Ready:
      return "ready";
    case GamePhase::Running:
      return "running";
    case GamePhase::Paused:
      return "paused";
    case GamePhase::Over:
      return "over";
  }
  return "ready";
}

const char* game_difficulty_name(GameDifficulty difficulty) {
  if (static_cast<uint8_t>(difficulty) < kGameDifficultyCount) {
    return kGameDifficultyNames[static_cast<uint8_t>(difficulty)];
  }
  return kGameDifficultyNames[0];  // fail closed to Easy
}

bool game_difficulty_from_name(const char* name, GameDifficulty* out) {
  if (name == nullptr) {
    return false;
  }
  for (uint8_t i = 0; i < kGameDifficultyCount; ++i) {
    if (std::strcmp(name, kGameDifficultyNames[i]) == 0) {
      if (out != nullptr) {
        *out = static_cast<GameDifficulty>(i);
      }
      return true;
    }
  }
  return false;
}

uint32_t prng_next(uint32_t* state) {
  // xorshift32: deterministic, full-period mod 2^32-1; zero seed is remapped.
  uint32_t x = *state;
  if (x == 0) {
    x = 0x9E3779B9u;
  }
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *state = x;
  return x;
}

void render_blink_label(FrameBuffer& frame, const char* label, uint32_t now_ms, Rgb color) {
  frame.clear();
  if (((now_ms / kBlinkPeriodMs) & 1u) == 0u) {
    text::draw_static(frame, label, color);  // centered, heritage anchor y=2
  }
}

void render_centered_label(FrameBuffer& frame, const char* label, Rgb color) {
  frame.clear();
  text::draw_static(frame, label, color);
}

void render_score_centered(FrameBuffer& frame, uint32_t score, Rgb color) {
  char buf[12];
  std::snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(score));
  render_centered_label(frame, buf, color);
}

void GameHost::begin(SoundSink* sound) {
  sound_ = sound;
  game_ = nullptr;
}

void GameHost::set_difficulty(GameDifficulty difficulty) {
  difficulty_ = difficulty;
  // Applies to the CURRENT game at its next reset (start/restart); a running
  // session keeps its already-computed timing (documented semantics).
  if (game_ != nullptr) {
    game_->set_difficulty(difficulty);
  }
}

// LED-L010: engine-enforced player mode. Values other than 1/2 fail closed to
// 1 (the accepted solo behavior).
void GameHost::set_players(uint8_t players) {
  players_ = (players == kPlayerMax) ? kPlayerMax : kPlayer1;
  if (game_ != nullptr) {
    game_->set_players(players_);
  }
}

bool GameHost::start_by_name(const char* name, uint32_t now_ms) {
  Game* g = game_by_name(name);
  if (g == nullptr) {
    return false;
  }
  g->set_difficulty(difficulty_);
  g->set_players(players_);  // LED-L010: applied at reset like difficulty
  active_difficulty_ = difficulty_;
  active_players_ = players_;
  g->set_lane(lane_length_);  // LED-L010: tail lane bound (matrix games ignore)
  g->reset(now_ms);
  game_ = g;
  last_step_ms_ = now_ms;
  return true;
}

bool GameHost::start_by_index(uint8_t index, uint32_t now_ms) {
  if (index >= kGameCount) {
    return false;
  }
  return start_by_name(kGameNames[index], now_ms);
}

void GameHost::exit() { game_ = nullptr; }

bool GameHost::pause_for_disconnect() {
  if (game_ == nullptr || game_->phase() != GamePhase::Running) return false;
  game_->set_phase(GamePhase::Paused);
  return true;
}

bool GameHost::handle(uint8_t player, InputEvent event, uint32_t now_ms) {
  if (game_ == nullptr) {
    return false;
  }
  switch (game_->phase()) {
    case GamePhase::Ready:
      if (event == InputEvent::Confirm || event == InputEvent::StartPause) {
        game_->set_phase(GamePhase::Running);
        last_step_ms_ = now_ms;
        if (sound_ != nullptr) {
          sound_->play(SoundCue::GameStart);
        }
        return true;
      }
      if (event == InputEvent::Back) {
        return true;  // runtime exits the game mode
      }
      return false;

    case GamePhase::Running:
      if (event == InputEvent::StartPause) {
        game_->set_phase(GamePhase::Paused);
        return true;
      }
      if (event == InputEvent::Back) {
        return true;  // lifecycle: the runtime exits the game mode
      }
      {
        const uint32_t before = game_->score();
        game_->on_input(player, event, now_ms);
        if (sound_ != nullptr && game_->score() != before)
          sound_->play(game_->score() > before ?
              (std::strcmp(game_->name(), "tail_tennis") == 0 ? SoundCue::Hit : SoundCue::Score)
              : SoundCue::Error);
        if (game_->phase() == GamePhase::Over) play_result_cue_();
      }
      return false;  // gameplay event (was consumed by the game)

    case GamePhase::Paused:
      if (event == InputEvent::StartPause || event == InputEvent::Confirm) {
        game_->set_phase(GamePhase::Running);
        last_step_ms_ = now_ms;  // do not fast-forward through the pause
        return true;
      }
      if (event == InputEvent::Back) {
        return true;
      }
      return true;  // swallow gameplay input while paused

    case GamePhase::Over:
      if (event == InputEvent::Confirm || event == InputEvent::StartPause) {
        game_->set_difficulty(difficulty_);
        game_->set_players(players_);
        active_difficulty_ = difficulty_;
        active_players_ = players_;
        game_->reset(now_ms);  // back to Ready with a fresh board
        if (sound_ != nullptr) {
          sound_->play(SoundCue::GameStart);
        }
        return true;
      }
      if (event == InputEvent::Back) {
        return true;
      }
      return true;
  }
  return false;
}

void GameHost::update(uint32_t now_ms) {
  if (game_ == nullptr || game_->phase() != GamePhase::Running) {
    return;
  }
  uint8_t steps = 0;
  while (now_ms - last_step_ms_ >= kStepMs && steps < kMaxCatchUpSteps) {
    last_step_ms_ += kStepMs;
    const uint32_t before = game_->score();
    game_->on_step(last_step_ms_);
    if (sound_ != nullptr && game_->score() != before)
      sound_->play(game_->score() > before ? SoundCue::Score : SoundCue::Error);
    ++steps;
    if (game_->phase() != GamePhase::Running) {
      break;  // game ended inside this step
    }
  }
  if (now_ms - last_step_ms_ >= kStepMs) {
    last_step_ms_ = now_ms;  // fell far behind (stall): resync, no spiral
  }
  if (game_->phase() == GamePhase::Over && sound_ != nullptr) {
    play_result_cue_();
  }
}

void GameHost::play_result_cue_() {
  if (sound_ == nullptr || game_ == nullptr) return;
  sound_->play(game_->winner() == 1 ? SoundCue::Win :
               game_->winner() == 2 ? SoundCue::Lose : SoundCue::GameOver);
}

void GameHost::render(FrameBuffer& frame, uint32_t now_ms) const {
  if (game_ == nullptr) {
    frame.clear();
    return;
  }
  game_->render(frame, now_ms);
}

// LED-L010: two-surface render — the matrix lane above plus the game's 1D
// lane (tail games draw the rope; matrix games' default is a no-op, but the
// runtime still wants the rope DARK for them, so the host clears it first).
void GameHost::render(FrameBuffer& frame, TailFramebuffer* tail, uint32_t now_ms) const {
  render(frame, now_ms);
  if (tail != nullptr) {
    tail->clear();
    if (game_ != nullptr) {
      game_->render_tail(*tail, now_ms);
    }
  }
}

}  // namespace games
}  // namespace ledwall
