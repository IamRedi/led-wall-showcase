#pragma once

#include <cstdint>

#include "games/game.h"
#include "input/input_events.h"
#include "render/display_topology.h"
#include "render/framebuffer.h"
#include "sound/sound_sink.h"

namespace ledwall {
namespace games {

// Hosts the active game: lifecycle keys, fixed-cadence stepping (catch-up
// bounded), rendering delegation, and score/phase exposure for the API.
// Exactly one game exists at a time (static singletons — no heap).
class GameHost {
 public:
  static constexpr uint32_t kStepMs = 90;         // ~11 Hz, modestly quicker games
  static constexpr uint8_t kMaxCatchUpSteps = 4;  // bounded catch-up (no spiral)

  void begin(SoundSink* sound);  // not started; sound sink for lifecycle cues
  bool start_by_name(const char* name, uint32_t now_ms);
  bool start_by_index(uint8_t index, uint32_t now_ms);
  void exit();
  bool pause_for_disconnect();  // preserve board/score; explicit resume only

  // LED-L009 repair cycle 2 (V-L009-3): engine-enforced difficulty. The level
  // persists across game sessions (default Easy); games receive it before
  // every reset, so a mid-session change applies at the next start/restart.
  void set_difficulty(GameDifficulty difficulty);
  GameDifficulty difficulty() const { return difficulty_; }
  GameDifficulty active_difficulty() const { return active_difficulty_; }

  // LED-L010: engine-enforced player mode (1 or 2; default 1). Applied at the
  // next start/restart exactly like difficulty; rejected values fail closed
  // to 1. Rejected at the API for games whose registry metadata is solo-only.
  void set_players(uint8_t players);
  uint8_t players() const { return players_; }
  uint8_t active_players() const { return active_players_; }

  // LED-L010: tail lane length (logical cells) injected from the configured
  // tail zone; forwarded to games at reset (tests may inject synthetic lanes).
  void set_lane_length(uint16_t cells) { lane_length_ = cells; }
  uint16_t lane_length() const { return lane_length_; }

  // Routes lifecycle keys; passes player-tagged gameplay input through while
  // Running. Returns true if the event was consumed as a lifecycle key.
  bool handle(uint8_t player, InputEvent event, uint32_t now_ms);
  bool handle(InputEvent event, uint32_t now_ms) { return handle(kPlayer1, event, now_ms); }

  void update(uint32_t now_ms);  // fixed-cadence stepping while Running
  void render(FrameBuffer& frame, uint32_t now_ms) const;
  // LED-L010: also render the game's 1D surface (tail games; no-op lane for
  // matrix games). `tail` may be null (disabled) — skipped entirely.
  void render(FrameBuffer& frame, TailFramebuffer* tail, uint32_t now_ms) const;

  bool active() const { return game_ != nullptr; }
  const char* name() const { return game_ != nullptr ? game_->name() : ""; }
  GamePhase phase() const { return game_ != nullptr ? game_->phase() : GamePhase::Ready; }
  uint32_t score() const { return game_ != nullptr ? game_->score() : 0; }
  uint32_t player_score(uint8_t player) const {
    return game_ != nullptr ? game_->player_score(player) : 0;
  }
  uint8_t winner() const { return game_ != nullptr ? game_->winner() : 0; }
  Game* current_game() const { return game_; }  // renderer view (read-only use)

 private:
  void play_result_cue_();
  Game* game_{nullptr};
  uint32_t last_step_ms_{0};
  SoundSink* sound_{nullptr};
  GameDifficulty difficulty_{GameDifficulty::Easy};  // V-L009-3: default Easy
  GameDifficulty active_difficulty_{GameDifficulty::Easy};
  uint8_t players_{1};  // LED-L010: engine-enforced player mode (1..2)
  uint8_t active_players_{1};
  uint16_t lane_length_{60};  // LED-L010: tail lane cells (host-injected)
};

// Registry binding (registry order; nullptr for unknown names — fail closed).
Game* game_by_name(const char* name);
Game* game_by_index(uint8_t index);
// LED-L010: tail-game binding fallback (tail_games.cpp; tail lane games live
// in their own translation unit). nullptr for unknown names.
Game* tail_game_by_name(const char* name);
Game* v41_game_by_name(const char* name);

}  // namespace games
}  // namespace ledwall
