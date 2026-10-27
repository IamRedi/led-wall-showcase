#pragma once

#include <cstdint>

#include "input/input_events.h"
#include "render/display_topology.h"
#include "render/framebuffer.h"

namespace ledwall {
namespace games {

// LED-L009 repair cycle 2 (V-L009-3): player-selectable difficulty. The
// dashboard selects it; the ENGINE enforces it (the phone never simulates
// speed). The selected level persists in the runtime across game sessions;
// changing it during a running game applies at the next start/restart
// (games compute their timing parameters from the level at reset only).
enum class GameDifficulty : uint8_t { Easy = 0, Normal, Hard };
constexpr uint8_t kGameDifficultyCount = 3;
constexpr const char* const kGameDifficultyNames[kGameDifficultyCount] = {
    "easy", "normal", "hard",
};

const char* game_difficulty_name(GameDifficulty difficulty);
bool game_difficulty_from_name(const char* name, GameDifficulty* out);

// LED-L009 deterministic games framework (reconstruction brief §14). Games are
// pure logic over the unified input model with fixed-cadence updates
// (GameHost), bounded static state, deterministic PRNG, and framebuffer-only
// rendering (bounds-checked). Game state lives on the ESP32; no game requires
// a phone. Lifecycle: Ready → (Confirm) → Running ⇄ Paused → Over →
// (Confirm) → Ready; Back/Home exits to the menu at any point (host-owned).
enum class GamePhase : uint8_t { Ready = 0, Running, Paused, Over };
constexpr uint8_t kGamePhaseCount = 4;

const char* game_phase_name(GamePhase phase);

// Repair cycle 1 (M-L010-2/3): the minimum configured tail lane (logical
// cells) every tail lane game supports. The dispatcher rejects tail games on
// shorter configured lanes with a typed invalidParameter instead of silently
// substituting a fake lane (the old set_lane fallback stays as a defensive
// default only). Matrix games require no lane (0).
constexpr uint16_t kTailGameMinLane = 12;

class Game {
 public:
  virtual ~Game() = default;

  virtual const char* name() const = 0;       // registry key (kGameNames)
  virtual const char* label() const = 0;      // short panel label (<= 5 glyphs)
  virtual void set_difficulty(GameDifficulty difficulty) = 0;  // applies at next reset
  virtual void reset(uint32_t now_ms) = 0;
  // LED-L010: player-tagged gameplay input (P1/P2 architecture). Solo games
  // ignore the player; duels route by slot. Lifecycle keys never reach here
  // (the host consumes them first).
  virtual void on_input(uint8_t player, InputEvent ev, uint32_t now_ms) = 0;
  virtual void on_step(uint32_t now_ms) = 0;                  // fixed-cadence physics (Running only)
  virtual void render(FrameBuffer& frame, uint32_t now_ms) const = 0;
  // LED-L010: 1D surface rendering (tail games draw their lane here; matrix
  // games inherit the safe dark default).
  virtual void render_tail(TailFramebuffer& tail, uint32_t now_ms) const {
    (void)tail;
    (void)now_ms;
  }
  virtual GamePhase phase() const = 0;
  virtual uint32_t score() const = 0;
  // Public scoreboard. winner: 0 while undecided, 1/2 for a player, 3 draw.
  virtual uint32_t player_score(uint8_t player) const {
    return player == 1 ? score() : 0;
  }
  virtual uint8_t winner() const { return 0; }
  virtual void set_phase(GamePhase phase) = 0;  // host-driven lifecycle transitions
  // LED-L010: player-mode selection (1P/2P where the registry metadata allows
  // it; solo games ignore it). Applies at the next reset like difficulty.
  virtual void set_players(uint8_t players) { (void)players; }
  virtual uint8_t players() const { return 1; }
  // LED-L010: lane length injection for tail games (logical cells 0..len-1;
  // host passes the configured tail zone length — tests inject synthetic
  // lanes). Matrix games ignore it. Applied at reset.
  virtual void set_lane(uint16_t cells) { (void)cells; }
};

// Deterministic PRNG (xorshift32) — same seed, same game. Never global state:
// each game owns its generator, reset to a fixed seed in reset().
uint32_t prng_next(uint32_t* state);

// Shared chrome: blinking ready/pause labels and the over screen.
// `now_ms` drives a 600 ms blink phase (deterministic).
void render_blink_label(FrameBuffer& frame, const char* label, uint32_t now_ms, Rgb color);
void render_centered_label(FrameBuffer& frame, const char* label, Rgb color);
void render_score_centered(FrameBuffer& frame, uint32_t score, Rgb color);

}  // namespace games
}  // namespace ledwall
