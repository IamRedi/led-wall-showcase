// LED-L009 curated games (reconstruction brief §14): pong, snake, breakout,
// reaction (stop-at-zero), dodger. All deterministic (xorshift PRNG with
// fixed seeds), bounded static state, framebuffer-only rendering. Each game
// implements the LED-L009 Game interface; lifecycle transitions are
// host-driven (GameHost) with gameplay inside the games.

#include "games/game_host.h"

#include <cstdio>
#include <cstring>

#include "render/color.h"
#include "text/text_renderer.h"
#include "timer/countdown.h"

namespace ledwall {
namespace games {

namespace {
constexpr uint8_t kW = kMatrixWidth;   // 30
constexpr uint8_t kH = kMatrixHeight;  // 10

// Common chrome colors.
constexpr Rgb kWhite{255, 255, 255};
constexpr Rgb kGreen{40, 200, 60};
constexpr Rgb kRed{220, 40, 40};
constexpr Rgb kYellow{220, 190, 40};
constexpr Rgb kDim{60, 60, 70};

class BaseGame : public Game {
 public:
  GamePhase phase() const override { return phase_; }
  void set_phase(GamePhase phase) override { phase_ = phase; }
  uint32_t score() const override { return score_; }
  // V-L009-3: the level is stored for the next reset; a running session keeps
  // its already-computed timing (documented apply-at-restart semantics).
  void set_difficulty(GameDifficulty difficulty) override { difficulty_ = difficulty; }

 protected:
  GamePhase phase_{GamePhase::Ready};
  uint32_t score_{0};
  GameDifficulty difficulty_{GameDifficulty::Easy};
};

// Shared running/ready/paused/over chrome: ready = blinking label, paused =
// blinking label in yellow, over = score + red frame. Running: subclass draw.
class ChromeGame : public BaseGame {
 public:
  void render(FrameBuffer& frame, uint32_t now_ms) const override {
    switch (phase_) {
      case GamePhase::Ready:
        render_blink_label(frame, label(), now_ms, kWhite);
        break;
      case GamePhase::Paused:
        render_blink_label(frame, "PAUSE", now_ms, kYellow);
        break;
      case GamePhase::Over:
        draw_over(frame);
        break;
      case GamePhase::Running:
        draw_running(frame, now_ms);
        break;
    }
  }

 protected:
  virtual void draw_running(FrameBuffer& frame, uint32_t now_ms) const = 0;
  virtual void draw_over(FrameBuffer& frame) const {
    render_score_centered(frame, score_, kWhite);
    draw_border(frame, kRed);
  }
  static void draw_border(FrameBuffer& frame, Rgb color) {
    for (uint8_t x = 0; x < kW; ++x) {
      frame.set(static_cast<int16_t>(x), 0, color);
      frame.set(static_cast<int16_t>(x), kH - 1, color);
    }
    for (uint8_t y = 1; y < kH - 1; ++y) {
      frame.set(0, static_cast<int16_t>(y), color);
      frame.set(static_cast<int16_t>(kW - 1), static_cast<int16_t>(y), color);
    }
  }
};

// ---------------------------------------------------------------- Pong ----
// LED-L010 P1/P2: players_=1 keeps the accepted solo game (P1 left paddle,
// AI right); players_=2 is a genuine duel — P1 drives the left paddle,
// P2 drives the right paddle, the AI is off, and the over screen reports
// the P1-P2 score.
class PongGame final : public ChromeGame {
 public:
  const char* name() const override { return "pong"; }
  const char* label() const override { return "PONG"; }

  void reset(uint32_t now_ms) override {
    (void)now_ms;
    phase_ = GamePhase::Ready;
    score_ = 0;
    player_y_ = 4;  // paddle center (3 tall → rows 3..5)
    p2_y_ = 4;
    ball_x_ = kW / 2;
    ball_y_ = kH / 2;
    ball_dx_ = 1;  // toward the player first
    ball_dy_ = 1;
    // V-L009-3 difficulty: Easy = slow ball, inattentive AI; Normal = the
    // released balance; Hard = fast ball and a fully attentive AI. (The AI
    // cadence is irrelevant in 2P — the right paddle is human-driven.)
    switch (difficulty_) {
      case GameDifficulty::Easy:
        speed_ = 1;       // one pixel per 90 ms logic step
        speed_cap_ = 2;   // rally speedup stays gentle
        ai_div_ = 3;      // AI moves every 3rd step
        break;
      case GameDifficulty::Normal:
        speed_ = 2;
        speed_cap_ = 4;
        ai_div_ = 2;
        break;
      case GameDifficulty::Hard:
        speed_ = 3;       // three pixels per 90 ms logic step
        speed_cap_ = 5;
        ai_div_ = 1;
        break;
    }
    rally_ = 0;
    rally_steps_ = 0;
    player_score_ = 0;
    ai_score_ = 0;
  }

  void set_players(uint8_t players) override { players_ = (players == 2u) ? 2u : 1u; }
  uint8_t players() const override { return players_; }
  uint32_t player_score(uint8_t player) const override {
    return player == 1 ? player_score_ : player == 2 ? ai_score_ : 0;
  }
  uint8_t winner() const override {
    return phase_ != GamePhase::Over ? 0 : player_score_ > ai_score_ ? 1 : 2;
  }

  void on_input(uint8_t player, InputEvent ev, uint32_t now_ms) override {
    (void)now_ms;
    if (player == kPlayer2 && players_ == 2u) {
      // P2 drives the RIGHT paddle in the duel.
      if (ev == InputEvent::Up && p2_y_ > 1) {
        --p2_y_;
      }
      if (ev == InputEvent::Down && p2_y_ < kH - 2) {
        ++p2_y_;
      }
      return;
    }
    // P1 (any unattributed input): the LEFT paddle.
    if (ev == InputEvent::Up && player_y_ > 1) {
      --player_y_;
    }
    if (ev == InputEvent::Down && player_y_ < kH - 2) {
      ++player_y_;
    }
  }

  void on_step(uint32_t now_ms) override {
    // A deterministic stalemate breaker prevents an idle player and the
    // bounded AI from rallying forever on a repeating 10-row trajectory.
    if (++rally_steps_ >= 300) {
      point_(ball_dx_ > 0);
      return;
    }
    // AI: follows the ball on its difficulty cadence (Easy skips two steps per
    // move — beatable; Hard tracks every step), bounded. Human right paddle
    // in 2P: no AI movement at all.
    if (players_ == 1u && ((now_ms / static_cast<uint32_t>(GameHost::kStepMs)) % ai_div_) == 0u) {
      if (ball_y_ > ai_y_ && ai_y_ < kH - 2) {
        ++ai_y_;
      } else if (ball_y_ < ai_y_ && ai_y_ > 1) {
        --ai_y_;
      }
    }
    for (uint8_t m = 0; m < speed_; ++m) {
      move_ball_();
      if (phase_ != GamePhase::Running) {
        return;  // a point ended the game inside this move burst
      }
    }
  }

 protected:
  void draw_running(FrameBuffer& frame, uint32_t now_ms) const override {
    (void)now_ms;
    frame.clear();
    // Paddles (3 tall around their centers). 2P colors the right side green.
    const Rgb right_color = (players_ == 2u) ? kGreen : kWhite;
    for (int8_t d = -1; d <= 1; ++d) {
      frame.set(1, static_cast<int16_t>(player_y_ + d), kGreen);
      frame.set(static_cast<int16_t>(kW - 2), static_cast<int16_t>(right_paddle_y_() + d),
                right_color);
    }
    frame.set(static_cast<int16_t>(ball_x_), static_cast<int16_t>(ball_y_), kWhite);
    // Center line (dim dots).
    for (uint8_t y = 0; y < kH; y += 2) {
      frame.set(static_cast<int16_t>(kW / 2), static_cast<int16_t>(y), kDim);
    }
  }

  void draw_over(FrameBuffer& frame) const override {
    // Show who won: player score vs AI score (1P) or P1-P2 (2P).
    char buf[12];
    std::snprintf(buf, sizeof(buf), "%u-%u", static_cast<unsigned>(player_score_),
                  static_cast<unsigned>(ai_score_));
    render_centered_label(frame, buf, kWhite);
    draw_border(frame, player_score_ > ai_score_ ? kGreen : kRed);
  }

 public:
  void render_tail(TailFramebuffer& tail, uint32_t now_ms) const override {
    tail.clear();
    const uint16_t n = tail.length();
    if (n == 0) return;
    if (phase_ == GamePhase::Running) {
      const uint16_t count = static_cast<uint16_t>(rally_) * 3u < n
                                 ? static_cast<uint16_t>(rally_) * 3u : n;
      for (uint16_t i = 0; i < count; ++i) tail.set(i, kGreen);
    } else if (phase_ == GamePhase::Over) {
      const bool on = ((now_ms / 300u) & 1u) == 0;
      if (on) tail.fill(player_score_ > ai_score_ ? kGreen : kRed);
    }
  }

 private:
  int8_t right_paddle_y_() const { return (players_ == 2u) ? p2_y_ : ai_y_; }
  void move_ball_() {
    ball_x_ += ball_dx_;
    ball_y_ += ball_dy_;
    if (ball_y_ <= 0 || ball_y_ >= kH - 1) {
      if (ball_y_ <= 0) {
        ball_y_ = 0;
      }
      if (ball_y_ >= kH - 1) {
        ball_y_ = kH - 1;
      }
      ball_dy_ = static_cast<int8_t>(-ball_dy_);
    }
    // Player paddle plane (x=2) and right paddle plane (x=kW-3).
    if (ball_x_ <= 2 && ball_dx_ < 0) {
      if (paddle_covers_(player_y_, ball_y_)) {
        ball_x_ = 2;
        ball_dx_ = 1;
        ball_dy_ = ball_y_ < player_y_ ? -1 : ball_y_ > player_y_ ? 1 :
                   (rally_ & 1u) ? -1 : 1;
        rally_up_();
      } else if (ball_x_ == 0) {
        point_(false);
      }
    } else if (ball_x_ >= kW - 3 && ball_dx_ > 0) {
      if (paddle_covers_(right_paddle_y_(), ball_y_)) {
        ball_x_ = kW - 3;
        ball_dx_ = -1;
        ball_dy_ = ball_y_ < right_paddle_y_() ? -1 :
                   ball_y_ > right_paddle_y_() ? 1 : (rally_ & 1u) ? 1 : -1;
        rally_up_();
      } else if (ball_x_ >= kW - 1) {
        point_(true);
      }
    }
  }
  static bool paddle_covers_(int8_t center, int8_t y) {
    return y >= center - 1 && y <= center + 1;
  }
  void rally_up_() {
    ++rally_;
    if (rally_ % 4 == 0 && speed_ < speed_cap_) {
      ++speed_;
    }
  }
  void point_(bool player_won) {
    if (player_won) {
      ++player_score_;
    } else {
      ++ai_score_;
    }
    score_ = player_score_;
    if (player_score_ >= 5 || ai_score_ >= 5) {
      phase_ = GamePhase::Over;
      return;
    }
    serve_(player_won ? -1 : 1);
  }
  void serve_(int8_t dx) {
    ball_x_ = kW / 2;
    ball_y_ = static_cast<int8_t>(prng_next(&rng_) % kH);
    ball_dx_ = dx;
    ball_dy_ = (prng_next(&rng_) & 1u) ? 1 : -1;
    speed_ = (difficulty_ == GameDifficulty::Hard) ? 3u
             : (difficulty_ == GameDifficulty::Normal) ? 2u
                                                       : 1u;  // V-L009-3: per-difficulty serve
    rally_ = 0;
    rally_steps_ = 0;
  }
  uint32_t rng_{0xC0FFEEu};
  int8_t player_y_{4};
  int8_t ai_y_{4};
  int8_t p2_y_{4};        // LED-L010: P2's right paddle in the duel
  uint8_t players_{1};    // LED-L010: 1 = solo vs AI, 2 = P1 vs P2
  int8_t ball_x_{15};
  int8_t ball_y_{5};
  int8_t ball_dx_{1};
  int8_t ball_dy_{1};
  uint8_t speed_{2};
  uint8_t speed_cap_{4};  // V-L009-3: rally speedup ceiling per difficulty
  uint8_t ai_div_{2};     // V-L009-3: AI tracking cadence per difficulty
  uint16_t rally_steps_{0};
  uint8_t rally_{0};
  uint8_t player_score_{0};
  uint8_t ai_score_{0};
};

// --------------------------------------------------------------- Snake ----
class SnakeGame final : public ChromeGame {
 public:
  static constexpr uint16_t kCells = 300;  // full 30×10 grid bound
  const char* name() const override { return "snake"; }
  const char* label() const override { return "SNAKE"; }
  uint8_t winner() const override { return phase_ != GamePhase::Over ? 0 : won_ ? 1 : 2; }
  void render_tail(TailFramebuffer& tail, uint32_t) const override {
    tail.clear();
    // The rope is the snake's live length meter; the tip marks the next food
    // column so the second surface carries gameplay information, not decor.
    const uint16_t n = tail.length();
    const uint16_t filled = static_cast<uint16_t>((static_cast<uint32_t>(length_) * n) / kCells);
    for (uint16_t i = 0; i < filled; ++i) tail.set(i, kGreen);
    if (n) tail.set(static_cast<uint16_t>((static_cast<uint32_t>(food_x_) * (n - 1)) /
                                        (kW - 1)), kRed);
    if (phase_ == GamePhase::Over && won_) tail.fill(kGreen);
  }

  void reset(uint32_t now_ms) override {
    (void)now_ms;
    phase_ = GamePhase::Ready;
    score_ = 0;
    length_ = 3;
    won_ = false;
    for (uint16_t i = 0; i < length_; ++i) {
      xs_[i] = static_cast<uint8_t>(10 - i);  // head at (10,5) moving right
      ys_[i] = 5;
    }
    dir_x_ = 1;
    dir_y_ = 0;
    next_dx_ = 1;
    next_dy_ = 0;
    place_food_();
    // V-L009-3 difficulty: move interval starts at 360/270/180 ms per cell
    // (Easy/Normal/Hard multiples of the host step) and speeds up on food down
    // to a difficulty floor, so Easy stays forgiving even late-game.
    switch (difficulty_) {
      case GameDifficulty::Easy:
        tick_div_ = 4;
        tick_floor_ = 3;
        break;
      case GameDifficulty::Normal:
        tick_div_ = 3;
        tick_floor_ = 2;
        break;
      case GameDifficulty::Hard:
        tick_div_ = 2;
        tick_floor_ = 1;
        break;
    }
    tick_count_ = 0;
  }

  void on_input(uint8_t player, InputEvent ev, uint32_t now_ms) override {
    (void)player;
    (void)now_ms;
    // One queued direction change per event; 180° reversal rejected.
    if (ev == InputEvent::Up && dir_y_ == 0) {
      next_dx_ = 0;
      next_dy_ = -1;
    } else if (ev == InputEvent::Down && dir_y_ == 0) {
      next_dx_ = 0;
      next_dy_ = 1;
    } else if (ev == InputEvent::Left && dir_x_ == 0) {
      next_dx_ = -1;
      next_dy_ = 0;
    } else if (ev == InputEvent::Right && dir_x_ == 0) {
      next_dx_ = 1;
      next_dy_ = 0;
    }
  }

  void on_step(uint32_t now_ms) override {
    (void)now_ms;
    ++tick_count_;
    if (tick_count_ % tick_div_ != 0) {
      return;
    }
    dir_x_ = next_dx_;
    dir_y_ = next_dy_;
    const int16_t hx = static_cast<int16_t>(xs_[0]) + dir_x_;
    const int16_t hy = static_cast<int16_t>(ys_[0]) + dir_y_;
    if (hx < 0 || hx >= kW || hy < 0 || hy >= kH) {
      phase_ = GamePhase::Over;  // wall
      return;
    }
    if (occupies_(static_cast<uint8_t>(hx), static_cast<uint8_t>(hy), length_ - 1)) {
      phase_ = GamePhase::Over;  // self (tail cell vacates this step)
      return;
    }
    // Shift body, grow on food.
    if (static_cast<uint8_t>(hx) == food_x_ && static_cast<uint8_t>(hy) == food_y_) {
      // grow: shift opens a tail slot
      for (uint16_t i = length_; i > 0; --i) {
        xs_[i] = xs_[i - 1];
        ys_[i] = ys_[i - 1];
      }
      xs_[0] = static_cast<uint8_t>(hx);
      ys_[0] = static_cast<uint8_t>(hy);
      ++length_;
      ++score_;
      if (length_ == kCells) {
        won_ = true;
        phase_ = GamePhase::Over;
        return;
      }
      place_food_();
      if (score_ % 5 == 0 && tick_div_ > tick_floor_) {
        --tick_div_;  // speed up every 5 foods, bounded by the difficulty floor
      }
    } else {
      for (uint16_t i = length_ - 1; i > 0; --i) {
        xs_[i] = xs_[i - 1];
        ys_[i] = ys_[i - 1];
      }
      xs_[0] = static_cast<uint8_t>(hx);
      ys_[0] = static_cast<uint8_t>(hy);
    }
  }

 protected:
  void draw_running(FrameBuffer& frame, uint32_t now_ms) const override {
    (void)now_ms;
    frame.clear();
    for (uint16_t i = 0; i < length_; ++i) {
      frame.set(static_cast<int16_t>(xs_[i]), static_cast<int16_t>(ys_[i]),
                i == 0 ? kWhite : kGreen);
    }
    frame.set(static_cast<int16_t>(food_x_), static_cast<int16_t>(food_y_), kRed);
  }
  void draw_over(FrameBuffer& frame) const override {
    render_score_centered(frame, score_, kWhite);
    draw_border(frame, won_ ? kGreen : kRed);
  }

 private:
  bool occupies_(uint8_t x, uint8_t y, uint16_t up_to) const {
    for (uint16_t i = 0; i <= up_to && i < length_; ++i) {
      if (xs_[i] == x && ys_[i] == y) {
        return true;
      }
    }
    return false;
  }
  void place_food_() {
    for (uint16_t tries = 0; tries < kCells; ++tries) {
      const uint8_t fx = static_cast<uint8_t>(prng_next(&rng_) % kW);
      const uint8_t fy = static_cast<uint8_t>(prng_next(&rng_) % kH);
      if (!occupies_(fx, fy, length_ - 1)) {
        food_x_ = fx;
        food_y_ = fy;
        return;
      }
    }
    // Deterministic complete scan closes the rare PRNG-collision path.
    for (uint16_t cell = 0; cell < kCells; ++cell) {
      const uint8_t x = static_cast<uint8_t>(cell % kW);
      const uint8_t y = static_cast<uint8_t>(cell / kW);
      if (!occupies_(x, y, length_ - 1)) {
        food_x_ = x;
        food_y_ = y;
        return;
      }
    }
    won_ = true;
    phase_ = GamePhase::Over;
  }
  uint32_t rng_{0x51A8E001u};
  uint8_t xs_[kCells];
  uint8_t ys_[kCells];
  uint16_t length_{3};
  int8_t dir_x_{1};
  int8_t dir_y_{0};
  int8_t next_dx_{1};
  int8_t next_dy_{0};
  uint8_t food_x_{0};
  uint8_t food_y_{0};
  uint8_t tick_div_{3};
  uint8_t tick_floor_{2};  // V-L009-3: difficulty-bounded speedup floor
  uint32_t tick_count_{0};
  bool won_{false};
};

// ------------------------------------------------------------ Breakout ----
class BreakoutGame final : public ChromeGame {
 public:
  static constexpr uint8_t kRows = 2;
  static constexpr uint8_t kCols = 10;   // 3-px bricks across 30 px
  static constexpr uint8_t kBricks = kRows * kCols;
  const char* name() const override { return "breakout"; }
  const char* label() const override { return "BLOCK"; }
  uint8_t winner() const override { return phase_ != GamePhase::Over ? 0 : lives_ ? 1 : 2; }

  void reset(uint32_t now_ms) override {
    (void)now_ms;
    phase_ = GamePhase::Ready;
    score_ = 0;
    lives_ = 3;
    level_ = 1;
    launched_ = false;
    std::memset(bricks_, 1, sizeof(bricks_));
    // V-L009-3 difficulty: ball moves 1/2/3 cells per step and the Easy
    // paddle is wider (7 vs 5 cells) — forgiving serve returns when learning.
    switch (difficulty_) {
      case GameDifficulty::Easy:
        ball_moves_ = 1;
        paddle_w_ = 7;
        break;
      case GameDifficulty::Normal:
        ball_moves_ = 2;
        paddle_w_ = 5;
        break;
      case GameDifficulty::Hard:
        ball_moves_ = 3;
        paddle_w_ = 5;
        break;
    }
    paddle_x_ = static_cast<uint8_t>((kW - paddle_w_) / 2);
    ball_x_ = 15;    // served from above the paddle, rising toward the bricks
    ball_y_ = 8;
    ball_dx_ = 1;
    ball_dy_ = -1;
  }

  void on_input(uint8_t player, InputEvent ev, uint32_t now_ms) override {
    (void)player;
    (void)now_ms;
    if (ev == InputEvent::Left && paddle_x_ > 0) {
      --paddle_x_;
    }
    if (ev == InputEvent::Right && paddle_x_ + paddle_w_ < kW - 1) {
      ++paddle_x_;
    }
    if (ev == InputEvent::Confirm && phase_ == GamePhase::Running) {
      launched_ = true;
    }
  }

  void on_step(uint32_t now_ms) override {
    (void)now_ms;
    if (!launched_) {
      ball_x_ = static_cast<int8_t>(paddle_x_ + paddle_w_ / 2);
      ball_y_ = kH - 2;
      return;
    }
    for (uint8_t m = 0; m < ball_moves_; ++m) {  // V-L009-3: difficulty cadence
      move_ball_();
      if (phase_ != GamePhase::Running) {
        return;
      }
    }
  }

  void render_tail(TailFramebuffer& tail, uint32_t now_ms) const override {
    (void)now_ms;
    tail.clear();
    const uint16_t n = tail.length();
    for (uint8_t i = 0; i < lives_ && i < 3; ++i) {
      const uint16_t lo = static_cast<uint16_t>(i * n / 3u);
      const uint16_t hi = static_cast<uint16_t>((i + 1u) * n / 3u);
      for (uint16_t c = lo; c < hi; ++c) tail.set(c, Rgb{0, 24, 5});
    }
  }

 protected:
  void draw_running(FrameBuffer& frame, uint32_t now_ms) const override {
    (void)now_ms;
    frame.clear();
    for (uint8_t r = 0; r < kRows; ++r) {
      for (uint8_t c = 0; c < kCols; ++c) {
        if (bricks_[r][c]) {
          const int16_t bx = static_cast<int16_t>(c * 3);
          const Rgb color = (r == 0) ? kRed : kYellow;
          for (uint8_t k = 0; k < 3; ++k) {
            frame.set(static_cast<int16_t>(bx + k), static_cast<int16_t>(r), color);
          }
        }
      }
    }
    for (uint8_t k = 0; k < paddle_w_; ++k) {  // V-L009-3: 5-wide, 7 on Easy
      frame.set(static_cast<int16_t>(paddle_x_ + k), kH - 1, kGreen);
    }
    frame.set(static_cast<int16_t>(ball_x_), static_cast<int16_t>(ball_y_), kWhite);
  }

 private:
  void move_ball_() {
    ball_x_ += ball_dx_;
    ball_y_ += ball_dy_;
    if (ball_x_ <= 0) {
      ball_x_ = 0;
      ball_dx_ = 1;
    }
    if (ball_x_ >= kW - 1) {
      ball_x_ = kW - 1;
      ball_dx_ = -1;
    }
    // Bricks (rows 0..1; a brick hit flips the ball downward).
    if (ball_y_ >= 0 && ball_y_ < kRows) {
      const uint8_t col = static_cast<uint8_t>(ball_x_ / 3);
      if (bricks_[ball_y_][col]) {
        bricks_[ball_y_][col] = 0;
        ++score_;
        ball_dy_ = 1;
        bool any = false;
        for (uint8_t r = 0; r < kRows; ++r)
          for (uint8_t c = 0; c < kCols; ++c) any |= bricks_[r][c] != 0;
        if (!any) {
          if (level_ >= 3) phase_ = GamePhase::Over;
          else {
            ++level_;
            std::memset(bricks_, 1, sizeof(bricks_));
            launched_ = false;
            ball_x_ = static_cast<int8_t>(paddle_x_ + paddle_w_ / 2);
            ball_y_ = kH - 2;
          }
          return;
        }
      }
    }
    // Paddle (bottom row): bounce with angle by hit position.
    if (ball_y_ >= kH - 1) {
      if (ball_x_ >= paddle_x_ && ball_x_ < paddle_x_ + paddle_w_) {
        ball_y_ = kH - 2;
        ball_dy_ = -1;
        const int16_t rel = static_cast<int16_t>(ball_x_ - paddle_x_) -
                            static_cast<int16_t>(paddle_w_ / 2);  // centered hit = straight up
        ball_dx_ = (rel < 0) ? -1 : (rel > 0) ? 1 : 0;
      } else {
        if (lives_ > 0) --lives_;
        if (lives_ == 0) phase_ = GamePhase::Over;
        else {
          launched_ = false;
          ball_x_ = static_cast<int8_t>(paddle_x_ + paddle_w_ / 2);
          ball_y_ = kH - 2;
          ball_dx_ = 1;
          ball_dy_ = -1;
        }
        return;
      }
    }
    // Ceiling.
    if (ball_y_ <= 0 && ball_dy_ < 0) {
      ball_y_ = 0;
      ball_dy_ = 1;
    }
  }
  uint8_t bricks_[kRows][kCols];
  uint8_t paddle_x_{12};
  uint8_t paddle_w_{5};    // V-L009-3: 7 on Easy, 5 otherwise
  uint8_t ball_moves_{2};  // V-L009-3: ball cells per host step (1/2/3)
  uint8_t lives_{3};
  uint8_t level_{1};
  bool launched_{false};
  int8_t ball_x_{15};
  int8_t ball_y_{6};
  int8_t ball_dx_{1};
  int8_t ball_dy_{1};
};

// --------------------------------------------- Reaction (stop-at-zero) ----
class ReactionGame final : public ChromeGame {
 public:
  const char* name() const override { return "reaction"; }
  const char* label() const override { return "STOP0"; }
  void render_tail(TailFramebuffer& tail, uint32_t now_ms) const override {
    tail.clear();
    if (phase_ == GamePhase::Ready || window_ms_ == 0) return;
    const uint32_t remaining = phase_ == GamePhase::Over ? stopped_remaining_ : remaining_at_(now_ms);
    const uint16_t lit = static_cast<uint16_t>((static_cast<uint64_t>(remaining) * tail.length()) /
                                                window_ms_);
    for (uint16_t i = 0; i < lit; ++i) tail.set(i, remaining < 1000 ? kRed : kYellow);
    if (phase_ == GamePhase::Over && remaining <= 50) tail.set(0, kGreen);
  }

  void reset(uint32_t now_ms) override {
    (void)now_ms;
    phase_ = GamePhase::Ready;
    score_ = 0;
    // V-L009-3 difficulty: the visible countdown WINDOW differs (Easy gives
    // the longest approach to time the stop; Hard the shortest). The clock
    // itself stays exact-millisecond in every mode — difficulty never
    // distorts the displayed remaining time or the stop precision.
    switch (difficulty_) {
      case GameDifficulty::Easy:
        window_ms_ = 12000u;
        break;
      case GameDifficulty::Normal:
        window_ms_ = 10000u;
        break;
      case GameDifficulty::Hard:
        window_ms_ = 7000u;
        break;
    }
    start_ms_ = 0;
    stopped_remaining_ = 0;
    stopped_ = false;
  }

  void on_input(uint8_t player, InputEvent ev, uint32_t now_ms) override {
    (void)player;  // solo: any player slot may stop the clock
    if (ev != InputEvent::Confirm || stopped_) {
      return;
    }
    if (start_ms_ == 0) {
      start_ms_ = now_ms;  // event raced ahead of the first step
    }
    stopped_remaining_ = remaining_at_(now_ms);
    score_ = stopped_remaining_;  // lower is better (distance from 0.000)
    stopped_ = true;
    phase_ = GamePhase::Over;
  }

  void on_step(uint32_t now_ms) override {
    if (start_ms_ == 0) {
      start_ms_ = now_ms;  // countdown origin: first Running step
      return;
    }
    if (now_ms - start_ms_ >= window_ms_) {
      // Nobody stopped it: natural expiry at exactly 0.000 (distance 0 — a
      // perfect score by timing, without a press).
      stopped_remaining_ = 0;
      score_ = 0;
      stopped_ = true;
      phase_ = GamePhase::Over;
    }
  }

  uint32_t remaining_at(uint32_t now_ms) const { return remaining_at_(now_ms); }
  uint32_t window_ms() const { return window_ms_; }  // test/observation view

 protected:
  void draw_running(FrameBuffer& frame, uint32_t now_ms) const override {
    char buf[9];
    countdown_format(remaining_at_(now_ms), buf);
    render_centered_label(frame, buf, kWhite);
  }

  void draw_over(FrameBuffer& frame) const override {
    char buf[9];
    countdown_format(stopped_remaining_, buf);
    render_centered_label(frame, buf, kWhite);
    // Verdict colors by distance from zero: <50 ms green, <200 ms yellow, else red.
    draw_border(frame,
                stopped_remaining_ <= 50u ? kGreen
                                          : (stopped_remaining_ <= 200u ? kYellow : kRed));
  }

 private:
  uint32_t remaining_at_(uint32_t now_ms) const {
    const uint32_t elapsed = now_ms - start_ms_;  // wrap-safe
    return (elapsed >= window_ms_) ? 0u : window_ms_ - elapsed;
  }
  uint32_t window_ms_{10000u};  // V-L009-3: 12 s / 10 s / 7 s by difficulty
  uint32_t start_ms_{0};
  uint32_t stopped_remaining_{0};
  bool stopped_{false};
};

// -------------------------------------------------------------- Dodger ----
class DodgerGame final : public ChromeGame {
 public:
  static constexpr uint8_t kMaxObstacles = 8;
  const char* name() const override { return "dodger"; }
  const char* label() const override { return "DODGE"; }
  uint8_t winner() const override { return phase_ != GamePhase::Over ? 0 : won_ ? 1 : 2; }
  void render_tail(TailFramebuffer& tail, uint32_t) const override {
    tail.clear();
    const uint16_t n = tail.length();
    if (!n) return;
    // Green survival progress with red hazard markers for obstacles on the
    // player's column; proximity brightens as they approach the bottom.
    const uint16_t progress = static_cast<uint16_t>((static_cast<uint64_t>(tick_count_) * n) / 600u);
    for (uint16_t i = 0; i < progress && i < n; ++i) tail.set(i, kGreen);
    for (uint8_t i = 0; i < obstacle_count_; ++i) {
      if (ob_xy_[i][0] == player_x_ && ob_xy_[i][1] < kH) {
        const uint16_t at = static_cast<uint16_t>((static_cast<uint32_t>(ob_xy_[i][1]) * (n - 1)) /
                                                   (kH - 1));
        tail.set(at, kRed);
      }
    }
    if (phase_ == GamePhase::Over) tail.set(n - 1, won_ ? kGreen : kRed);
  }

  void reset(uint32_t now_ms) override {
    (void)now_ms;
    phase_ = GamePhase::Ready;
    score_ = 0;
    player_x_ = kW / 2;
    won_ = false;
    std::memset(ob_xy_, 0xFF, sizeof(ob_xy_));  // 0xFF = unused
    obstacle_count_ = 0;
    // V-L009-3 difficulty: obstacle cadence 3/2/1 host steps, a difficulty-bound
    // speedup floor, and slower spawning on Easy so reaction time stays sane.
    switch (difficulty_) {
      case GameDifficulty::Easy:
        tick_div_ = 3;
        tick_floor_ = 2;
        spawn_base_ = 4;
        break;
      case GameDifficulty::Normal:
        tick_div_ = 2;
        tick_floor_ = 1;
        spawn_base_ = 3;
        break;
      case GameDifficulty::Hard:
        tick_div_ = 1;
        tick_floor_ = 1;
        spawn_base_ = 1;
        break;
    }
    tick_count_ = 0;
    spawn_countdown_ = spawn_base_;
  }

  void on_input(uint8_t player, InputEvent ev, uint32_t now_ms) override {
    (void)player;
    (void)now_ms;
    if (phase_ != GamePhase::Running) return;
    if (ev == InputEvent::Left && player_x_ > 0) {
      --player_x_;
    }
    if (ev == InputEvent::Right && player_x_ < kW - 1) {
      ++player_x_;
    }
  }

  void on_step(uint32_t now_ms) override {
    (void)now_ms;
    ++tick_count_;
    score_ = tick_count_ * GameHost::kStepMs / 1000u;  // survived seconds
    if (tick_count_ >= 600u) {
      won_ = true;
      phase_ = GamePhase::Over;
      return;
    }
    if (tick_count_ % tick_div_ != 0) {
      return;
    }
    // Advance obstacles; recycle those past the bottom.
    for (uint8_t i = 0; i < obstacle_count_; ++i) {
      ++ob_xy_[i][1];
    }
    // Collision at the player row.
    for (uint8_t i = 0; i < obstacle_count_; ++i) {
      if (ob_xy_[i][1] == kH - 1 && ob_xy_[i][0] == player_x_) {
        phase_ = GamePhase::Over;
        return;
      }
    }
    uint8_t w = 0;
    for (uint8_t i = 0; i < obstacle_count_; ++i) {
      if (ob_xy_[i][1] < kH) {
        ob_xy_[w][0] = ob_xy_[i][0];
        ob_xy_[w][1] = ob_xy_[i][1];
        ++w;
      }
    }
    obstacle_count_ = w;
    // Spawn (bounded).
    if (--spawn_countdown_ == 0) {
      spawn_countdown_ = spawn_base_ + static_cast<uint8_t>(prng_next(&rng_) % 3u);
      if (obstacle_count_ < kMaxObstacles) {
        ob_xy_[obstacle_count_][0] = static_cast<uint8_t>(prng_next(&rng_) % kW);
        ob_xy_[obstacle_count_][1] = 0;
        ++obstacle_count_;
      }
    }
    if (tick_count_ % 400u == 0u && tick_div_ > tick_floor_) {
      --tick_div_;  // speed up over time, bounded by the difficulty floor
    }
  }

 protected:
  void draw_running(FrameBuffer& frame, uint32_t now_ms) const override {
    (void)now_ms;
    frame.clear();
    for (uint8_t i = 0; i < obstacle_count_; ++i) {
      frame.set(static_cast<int16_t>(ob_xy_[i][0]), static_cast<int16_t>(ob_xy_[i][1]), kRed);
    }
    frame.set(static_cast<int16_t>(player_x_), kH - 1, kGreen);
  }
  void draw_over(FrameBuffer& frame) const override {
    render_score_centered(frame, score_, kWhite);
    draw_border(frame, won_ ? kGreen : kRed);
  }

 private:
  uint32_t rng_{0xD0D6E42u};
  bool won_{false};
  uint8_t ob_xy_[kMaxObstacles][2];
  uint8_t obstacle_count_{0};
  uint8_t player_x_{15};
  uint8_t tick_div_{2};
  uint8_t tick_floor_{1};    // V-L009-3: difficulty-bounded speedup floor
  uint8_t spawn_base_{3};    // V-L009-3: spawn interval base per difficulty
  uint32_t tick_count_{0};
  uint8_t spawn_countdown_{3};
};

// Single static instances (no heap; exactly one game active at any time).
PongGame s_pong;
SnakeGame s_snake;
BreakoutGame s_breakout;
ReactionGame s_reaction;
DodgerGame s_dodger;

struct GameBinding {
  const char* name;
  Game* game;
};
constexpr GameBinding kGameBindings[] = {
    {"pong", &s_pong},         {"snake", &s_snake},   {"breakout", &s_breakout},
    {"reaction", &s_reaction}, {"dodger", &s_dodger},
};
constexpr uint8_t kGameBindingsCount =
    static_cast<uint8_t>(sizeof(kGameBindings) / sizeof(kGameBindings[0]));

}  // namespace

Game* game_by_name(const char* name) {
  if (name == nullptr) {
    return nullptr;
  }
  for (const GameBinding& b : kGameBindings) {
    if (std::strcmp(b.name, name) == 0) {
      return b.game;
    }
  }
  Game* tail_game = tail_game_by_name(name);
  return tail_game != nullptr ? tail_game : v41_game_by_name(name);
}

Game* game_by_index(uint8_t index) {
  if (index >= kGameCount) {
    return nullptr;
  }
  // Registry order: the five matrix bindings first, then the six tail games
  // (mirrors kGameNames; LED-L010).
  if (index < kGameBindingsCount) {
    return kGameBindings[index].game;
  }
  return game_by_name(kGameNames[index]);
}

}  // namespace games
}  // namespace ledwall
