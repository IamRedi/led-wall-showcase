#include "games/game_host.h"

#include <cstring>

namespace ledwall {
namespace games {
namespace {

constexpr Rgb kWhite{230, 230, 220};
constexpr Rgb kP1{45, 170, 245};
constexpr Rgb kP2{245, 100, 45};
constexpr Rgb kEnemy{230, 35, 45};
constexpr Rgb kGate{35, 220, 90};

class V41Game : public Game {
 public:
  void set_difficulty(GameDifficulty d) override { difficulty_ = d; }
  void set_phase(GamePhase p) override { phase_ = p; }
  GamePhase phase() const override { return phase_; }
  uint32_t score() const override { return score_; }
  void set_lane(uint16_t n) override { lane_ = n >= kTailGameMinLane ? n : kTailGameMinLane; }
 protected:
  GamePhase phase_{GamePhase::Ready};
  GameDifficulty difficulty_{GameDifficulty::Easy};
  uint16_t lane_{150};
  uint32_t score_{0};
};

// One physical ball, with one press event per hit. The host's fixed 90 ms
// cadence advances it; a held button never creates repeat hits.
class TailTennis final : public V41Game {
 public:
  const char* name() const override { return "tail_tennis"; }
  const char* label() const override { return "TENIS"; }
  uint32_t player_score(uint8_t player) const override { return player == 1 ? p1_ : player == 2 ? p2_ : 0; }
  uint8_t winner() const override { return phase_ != GamePhase::Over ? 0 : p1_ > p2_ ? 1 : p2_ > p1_ ? 2 : 3; }
  void set_players(uint8_t n) override { players_ = n == 2 ? 2 : 1; }
  uint8_t players() const override { return players_; }
  void reset(uint32_t) override {
    phase_ = GamePhase::Ready;
    score_ = p1_ = p2_ = 0;
    ball_ = static_cast<int16_t>(lane_ / 2);
    dir_ = -1;
    tick_ = 0;
    ai_tick_ = 0;
    countdown_steps_ = 0;
    window_ = difficulty_ == GameDifficulty::Easy ? 18 :
              difficulty_ == GameDifficulty::Normal ? 12 : 8;
    if (window_ >= lane_ / 2) window_ = static_cast<uint8_t>(lane_ / 4);
    step_div_ = difficulty_ == GameDifficulty::Easy ? 2 : 1;
    // On a short lane a one-cell/tick ball remains playable; the API
    // rejects lanes shorter than the shared minimum.
  }
  void set_phase(GamePhase p) override {
    if (phase_ == GamePhase::Ready && p == GamePhase::Running) countdown_steps_ = 30;
    phase_ = p;
  }
  void on_input(uint8_t player, InputEvent ev, uint32_t) override {
    if (phase_ != GamePhase::Running || countdown_steps_ || ev != InputEvent::Confirm) return;
    const bool left = ball_ <= window_ && dir_ < 0;
    const bool right = ball_ >= static_cast<int16_t>(lane_ - 1 - window_) && dir_ > 0;
    if ((player == 1 && left) || (player == 2 && players_ == 2 && right)) {
      dir_ = -dir_;
      ++score_;  // successful rallies; match scores remain separate
    }
  }
  void on_step(uint32_t) override {
    if (phase_ != GamePhase::Running) return;
    if (countdown_steps_) { --countdown_steps_; return; }
    if (++tick_ % step_div_ != 0) return;
    ball_ += dir_;
    // Bounded AI: it reacts only when the ball reaches the right-side
    // window, with a deterministic miss cadence that scales by difficulty.
    if (players_ == 1 && dir_ > 0 && ball_ >= static_cast<int16_t>(lane_ - 1 - window_)) {
      ++ai_tick_;
      const uint8_t miss_every = difficulty_ == GameDifficulty::Easy ? 2 :
                                 difficulty_ == GameDifficulty::Normal ? 4 : 7;
      if (ai_tick_ % miss_every != 0) dir_ = -1;
    }
    if (ball_ < 0) point_(false);
    if (ball_ >= static_cast<int16_t>(lane_)) point_(true);
  }
  void render(FrameBuffer& frame, uint32_t now) const override {
    if (phase_ == GamePhase::Ready) { render_blink_label(frame, "TENIS", now, kWhite); return; }
    if (phase_ == GamePhase::Paused) { render_blink_label(frame, "PAUSE", now, kWhite); return; }
    if (phase_ == GamePhase::Running && countdown_steps_) {
      render_centered_label(frame, countdown_steps_ > 20 ? "3" :
                                   countdown_steps_ > 10 ? "2" : "1", kWhite);
      return;
    }
    frame.clear();
    // Two scores, separated by a dark column; the ball itself lives on Tail.
    for (uint8_t i = 0; i < p1_ && i < 7; ++i) frame.set(2 + i * 2, 2, kP1);
    for (uint8_t i = 0; i < p2_ && i < 7; ++i) frame.set(16 + i * 2, 2, kP2);
    if (phase_ == GamePhase::Over) render_centered_label(frame, p1_ == p2_ ? "DRAW" : p1_ > p2_ ? "P1" : "P2", kWhite);
  }
  void render_tail(TailFramebuffer& tail, uint32_t) const override {
    const uint16_t n = tail.length() < lane_ ? tail.length() : lane_;
    if (!n) return;
    for (uint16_t i = 0; i < n; ++i) {
      if (i <= window_) tail.set(i, Rgb{0, 20, 38});
      else if (i >= n - 1 - window_) tail.set(i, Rgb{38, 12, 0});
    }
    if (ball_ >= 0 && ball_ < static_cast<int16_t>(n)) tail.set(ball_, kWhite);
    tail.set(0, kP1); tail.set(n - 1, kP2);
  }
 private:
  void point_(bool p1_point) {
    if (p1_point) ++p1_; else ++p2_;
    score_ = p1_;
    if (p1_ >= 5 || p2_ >= 5) { phase_ = GamePhase::Over; return; }
    ball_ = static_cast<int16_t>(lane_ / 2);
    dir_ = p1_point ? -1 : 1;
  }
  uint8_t players_{1}, p1_{0}, p2_{0}, window_{18}, step_div_{2};
  uint32_t tick_{0}, ai_tick_{0};
  uint8_t countdown_steps_{0};
  int16_t ball_{75};
  int8_t dir_{-1};
};

// Five short, bounded stages. Hazards occupy a single announced position;
// attacks clear enemies in a forward range and have a step-based cooldown.
class LightQuest final : public V41Game {
 public:
  const char* name() const override { return "light_quest"; }
  const char* label() const override { return "QUEST"; }
  uint8_t winner() const override { return phase_ != GamePhase::Over ? 0 : lives_ ? 1 : 2; }
  void reset(uint32_t) override {
    phase_ = GamePhase::Ready; score_ = 0; level_ = 1; lives_ = 3;
    start_level_();
  }
  void on_input(uint8_t, InputEvent ev, uint32_t) override {
    if (phase_ != GamePhase::Running) return;
    if (ev == InputEvent::Left && player_ + 1 < lane_) ++player_;  // toward anchor
    if (ev == InputEvent::Right && player_ > 0) --player_;
    if (ev == InputEvent::Confirm && cooldown_ == 0) {
      if (enemy1_ >= player_ && enemy1_ <= player_ + 8) enemy1_ = -1;
      if (enemy2_ >= player_ && enemy2_ <= player_ + 8) enemy2_ = -1;
      cooldown_ = difficulty_ == GameDifficulty::Hard ? 8 : 5;
    }
    collide_();
    if (player_ >= lane_ - 1 && enemy1_ < 0 && enemy2_ < 0) {
      score_ += 100;
      if (level_ == 5) phase_ = GamePhase::Over;
      else { ++level_; start_level_(); }
    }
  }
  void on_step(uint32_t) override {
    if (phase_ != GamePhase::Running) return;
    ++tick_;
    if (cooldown_) --cooldown_;
    // A red warning precedes each hazard pulse; the adjacent cell is always
    // safe and movement stays possible. Enemies oscillate within their bounds.
    if (level_ >= 4 && tick_ % 20 == 18) warning_ = true;
    else if (tick_ % 20 == 0) { hazard_ = true; warning_ = false; }
    else if (tick_ % 20 == 4) hazard_ = false;
    if (level_ >= 3 && tick_ % 4 == 0 && enemy2_ >= 0) {
      enemy2_ += enemy_dir_;
      if (enemy2_ <= static_cast<int16_t>(lane_ / 2) || enemy2_ >= static_cast<int16_t>(lane_ - 8)) enemy_dir_ = -enemy_dir_;
    }
    collide_();
  }
  void render(FrameBuffer& frame, uint32_t now) const override {
    if (phase_ == GamePhase::Ready) { render_blink_label(frame, "QUEST", now, kWhite); return; }
    if (phase_ == GamePhase::Paused) { render_blink_label(frame, "PAUSE", now, kWhite); return; }
    frame.clear();
    for (uint8_t i = 0; i < level_; ++i) frame.set(i + 2, 2, kGate);
    for (uint8_t i = 0; i < lives_; ++i) frame.set(i + 2, 7, kP1);
    if (phase_ == GamePhase::Over) render_centered_label(frame, lives_ ? "WIN" : "LOSE", lives_ ? kGate : kEnemy);
  }
  void render_tail(TailFramebuffer& tail, uint32_t) const override {
    const uint16_t n = tail.length() < lane_ ? tail.length() : lane_;
    if (!n) return;
    tail.set(n - 1, enemy1_ < 0 && enemy2_ < 0 ? kGate : Rgb{20, 65, 30});
    if (enemy1_ >= 0 && enemy1_ < n) tail.set(enemy1_, kEnemy);
    if (enemy2_ >= 0 && enemy2_ < n) tail.set(enemy2_, kEnemy);
    if ((warning_ || hazard_) && hazard_pos_ < n) tail.set(hazard_pos_, hazard_ ? kEnemy : Rgb{110, 90, 10});
    if (player_ < n) tail.set(player_, kP1);
  }
 private:
  void start_level_() {
    player_ = 0; tick_ = 0; cooldown_ = 0; warning_ = hazard_ = false;
    hazard_pos_ = static_cast<uint16_t>((lane_ * 2) / 3);
    enemy1_ = level_ >= 2 ? static_cast<int16_t>(lane_ / 3) : -1;
    enemy2_ = level_ >= 3 ? static_cast<int16_t>((lane_ * 3) / 4) : -1;
    enemy_dir_ = -1;
  }
  void collide_() {
    if (player_ == enemy1_ || player_ == enemy2_ || (level_ >= 4 && hazard_ && player_ == hazard_pos_)) {
      if (lives_) --lives_;
      if (!lives_) phase_ = GamePhase::Over;
      else start_level_();
    }
  }
  uint16_t player_{0}, hazard_pos_{0};
  int16_t enemy1_{-1}, enemy2_{-1};
  int8_t enemy_dir_{-1};
  uint8_t level_{1}, lives_{3}, cooldown_{0};
  uint32_t tick_{0};
  bool warning_{false}, hazard_{false};
};

// Two local input slots share the ESP32 monotonic clock. The 30 ms tie window
// is a game rule, not a claim about human reaction precision over Wi-Fi.
class QuickDraw final : public V41Game {
 public:
  const char* name() const override { return "quick_draw"; }
  const char* label() const override { return "DRAW"; }
  uint32_t player_score(uint8_t player) const override { return player >= 1 && player <= 2 ? wins_[player - 1] : 0; }
  uint8_t winner() const override { return phase_ != GamePhase::Over ? 0 : wins_[0] > wins_[1] ? 1 : wins_[1] > wins_[0] ? 2 : 3; }
  void set_players(uint8_t) override {}
  uint8_t players() const override { return 2; }
  void reset(uint32_t) override {
    phase_ = GamePhase::Ready;
    score_ = 0;
    wins_[0] = wins_[1] = 0;
    round_ = 1;
    rng_ = 0x51D2A19Bu;
    started_ = false;
    go_ = false;
  }
  void on_input(uint8_t player, InputEvent ev, uint32_t now) override {
    if (phase_ != GamePhase::Running || !started_ || ev != InputEvent::Confirm ||
        player < 1 || player > 2) return;
    const uint8_t slot = player - 1;
    if (!go_) { early_[slot] = true; return; }
    if (pressed_[slot]) return;
    pressed_[slot] = true;
    press_ms_[slot] = now;
    if (pressed_[0] && pressed_[1]) resolve_press_();
  }
  void on_step(uint32_t now) override {
    if (phase_ != GamePhase::Running) return;
    if (!started_) { begin_round_(now); return; }
    if (!go_ && now - round_ms_ >= wait_ms_) {
      go_ = true;
      go_ms_ = now;
      if (early_[0] || early_[1]) {
        finish_round_(early_[0] && early_[1] ? 0 : early_[0] ? 2 : 1, now);
        return;
      }
    }
    if (go_) {
      if (pressed_[0] != pressed_[1]) {
        const uint8_t first = pressed_[0] ? 0 : 1;
        if (now - press_ms_[first] > 30u) finish_round_(first + 1, now);
      } else if (!pressed_[0] && now - go_ms_ >= 3000u) finish_round_(0, now);
    }
  }
  void render(FrameBuffer& frame, uint32_t now) const override {
    if (phase_ == GamePhase::Ready) { render_blink_label(frame, "DRAW", now, kWhite); return; }
    if (phase_ == GamePhase::Paused) { render_blink_label(frame, "PAUSE", now, kWhite); return; }
    frame.clear();
    if (phase_ == GamePhase::Over) {
      render_centered_label(frame, wins_[0] > wins_[1] ? "P1" : "P2", kWhite);
      return;
    }
    render_centered_label(frame, go_ ? "GO" : "WAIT", go_ ? kGate : kEnemy);
    for (uint8_t i = 0; i < wins_[0]; ++i) frame.set(2 + i * 2, 0, kP1);
    for (uint8_t i = 0; i < wins_[1]; ++i) frame.set(20 + i * 2, 0, kP2);
  }
  void render_tail(TailFramebuffer& tail, uint32_t) const override {
    tail.clear();
    if (!go_ || phase_ != GamePhase::Running) return;
    const uint16_t mid = tail.length() / 2;
    for (uint16_t i = 0; i < tail.length(); ++i)
      tail.set(i, i < mid ? Rgb{0, 24, 34} : Rgb{34, 14, 0});
  }
 private:
  void begin_round_(uint32_t now) {
    started_ = true;
    round_ms_ = now;
    wait_ms_ = 1500u + (prng_next(&rng_) % 3001u);
    go_ = false;
    early_[0] = early_[1] = false;
    pressed_[0] = pressed_[1] = false;
  }
  void resolve_press_() {
    const uint32_t a = press_ms_[0], b = press_ms_[1];
    const uint32_t delta = a > b ? a - b : b - a;
    finish_round_(delta <= 30u ? 0 : a < b ? 1 : 2, a > b ? a : b);
  }
  void finish_round_(uint8_t winner, uint32_t now) {
    if (winner != 0) ++wins_[winner - 1];
    score_ = wins_[0];
    if (wins_[0] >= 5 || wins_[1] >= 5) { phase_ = GamePhase::Over; return; }
    if (winner != 0) ++round_;
    begin_round_(now);  // a tie repeats the same numbered round
  }
  uint32_t rng_{0x51D2A19Bu}, round_ms_{0}, wait_ms_{1500}, go_ms_{0};
  uint32_t press_ms_[2]{};
  uint8_t round_{1}, wins_[2]{};
  bool started_{false}, go_{false}, early_[2]{}, pressed_[2]{};
};

TailTennis s_tennis;
LightQuest s_quest;
QuickDraw s_draw;
}  // namespace

Game* v41_game_by_name(const char* name) {
  if (!name) return nullptr;
  if (std::strcmp(name, "tail_tennis") == 0) return &s_tennis;
  if (std::strcmp(name, "light_quest") == 0) return &s_quest;
  if (std::strcmp(name, "quick_draw") == 0) return &s_draw;
  return nullptr;
}

}  // namespace games
}  // namespace ledwall
