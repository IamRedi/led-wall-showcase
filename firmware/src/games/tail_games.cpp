// LED-L010 tail lane games (ADR-LED-L010 D6; expansion brief §19). Six curated,
// deterministic games on the 1D tail surface:
//   color_chain (1P, Zuma-like), light_race (1P/2P), reaction_stop (1P),
//   color_catch (1P, matrix target + tail pulses), memory (1P, Simon),
//   tug_of_war (1P/2P).
//
// Shared rules (the tail game engine contract):
// - Gameplay positions are LOGICAL cells 0..lane-1 (never raw driver LEDs).
//   The lane length is injected by the host (set_lane) from the configured
//   tail zone; host tests inject synthetic lanes. Renders clamp to the actual
//   TailFramebuffer length, so a shorter rope can never be overrun.
// - Deterministic: xorshift PRNG with fixed seeds, GameHost fixed-cadence
//   stepping, no heap, bounded static state.
// - The matrix is HUD/chrome only (Ready/Pause/Over labels, scores, target
//   chips); the tail is the playing field.
// - Difficulty scales the documented parameters at reset only (V-L009-3
//   semantics); player mode applies at reset (LED-L010).

#include "games/game_host.h"

#include <cstdio>
#include <cstring>

#include "render/color.h"
#include "render/framebuffer.h"
#include "text/text_renderer.h"

namespace ledwall {
namespace games {

namespace {

constexpr uint8_t kW = kMatrixWidth;
constexpr uint8_t kH = kMatrixHeight;

constexpr Rgb kWhite{255, 255, 255};
constexpr Rgb kGreen{40, 220, 70};
constexpr Rgb kOrange{240, 170, 30};
constexpr Rgb kRed{230, 50, 40};
constexpr Rgb kBlue{50, 90, 240};
constexpr Rgb kYellow{235, 220, 40};
constexpr Rgb kDim{40, 44, 50};

// Fallback lane for callers that never injected one (host tests usually do).
constexpr uint16_t kDefaultLane = 60;

// Bounded playable minimum for every tail lane game (repair cycle 1,
// M-L010-2/3): below this the physics/zone geometry degenerates, so the API
// rejects tail games on shorter configured lanes (game_min_lane) instead of
// silently substituting a fake lane. Mirrored publicly in games/game.h as
// kTailGameMinLane for the registry/dispatcher validation.
constexpr uint16_t kMinLane = 12;

// reaction_stop default zone anchor on a full-size lane (clamped to the
// actual lane everywhere it is set — never used as an absolute position).
constexpr uint16_t kZoneStartDefault = 20;

// Shared chrome (mirrors games_impl.cpp ChromeGame): ready/pause blink labels
// on the matrix, over screen with score.
void draw_chrome(FrameBuffer& frame, GamePhase phase, const char* label, uint32_t score,
                 uint32_t now_ms, Rgb over_color) {
  switch (phase) {
    case GamePhase::Ready:
      render_blink_label(frame, label, now_ms, kWhite);
      break;
    case GamePhase::Paused:
      render_blink_label(frame, "PAUSE", now_ms, kYellow);
      break;
    case GamePhase::Over:
      render_score_centered(frame, score, kWhite);
      for (uint8_t x = 0; x < kW; ++x) {
        frame.set(static_cast<int16_t>(x), 0, over_color);
        frame.set(static_cast<int16_t>(x), kH - 1, over_color);
      }
      break;
    case GamePhase::Running:
      frame.clear();
      break;
  }
}

// A small 1-cell-pitch numeric HUD digit (3×5) for in-game scores — big text
// would flood the HUD; this stays readable at panel scale.
void draw_hud_digit(FrameBuffer& frame, int16_t x, int16_t y, uint8_t digit, Rgb color) {
  static const uint8_t kDigits[10][5] = {
      {0x7, 0x5, 0x5, 0x5, 0x7},  // 0
      {0x2, 0x6, 0x2, 0x2, 0x7},  // 1
      {0x7, 0x1, 0x7, 0x4, 0x7},  // 2
      {0x7, 0x1, 0x7, 0x1, 0x7},  // 3
      {0x5, 0x5, 0x7, 0x1, 0x1},  // 4
      {0x7, 0x4, 0x7, 0x1, 0x7},  // 5
      {0x7, 0x4, 0x7, 0x5, 0x7},  // 6
      {0x7, 0x1, 0x2, 0x2, 0x2},  // 7
      {0x7, 0x5, 0x7, 0x5, 0x7},  // 8
      {0x7, 0x5, 0x7, 0x1, 0x7},  // 9
  };
  if (digit > 9) {
    return;
  }
  for (uint8_t row = 0; row < 5; ++row) {
    for (uint8_t col = 0; col < 3; ++col) {
      if (kDigits[digit][row] & (0x4 >> col)) {
        frame.set(static_cast<int16_t>(x + col), static_cast<int16_t>(y + row), color);
      }
    }
  }
}

void draw_hud_number(FrameBuffer& frame, int16_t x, int16_t y, uint32_t value, Rgb color) {
  char buf[11];
  std::snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(value));
  int16_t cx = x;
  for (const char* p = buf; *p != '\0' && p - buf < 5; ++p) {
    draw_hud_digit(frame, cx, y, static_cast<uint8_t>(*p - '0'), color);
    cx += 4;
  }
}

// The four Simon/catch palette colors (indices are wire-stable).
constexpr uint8_t kLaneColorCount = 4;
constexpr Rgb kLaneColors[kLaneColorCount] = {kRed, kGreen, kBlue, kYellow};

// ------------------------------------------------------- color_chain ----
// Zuma-like: a chain of colored balls advances from the spawn end toward the
// MAW at the far end. Confirm inserts the "next" color (matrix HUD chip) at
// the insertion cell (60 % of the lane, Left/Right fine-tune ±6). Runs of 3+
// same-colored balls clear (chain-closing cascades re-check the gap; combo
// multiplies). Score = 10 × cleared × combo. Lose when the front ball reaches
// the maw; a full clear levels up (fresh chain, faster). Difficulty: 3 colors
// on Easy vs 4, chain advance 500/300/200 ms; every insertion also advances
// the chain one cell (Zuma pressure).
class ColorChainGame final : public Game {
 public:
  static constexpr uint8_t kMaxBalls = 30;
  const char* name() const override { return "color_chain"; }
  const char* label() const override { return "CHAIN"; }
  uint8_t winner() const override { return phase_ == GamePhase::Over ? 2 : 0; }

  void reset(uint32_t now_ms) override {
    (void)now_ms;
    phase_ = GamePhase::Ready;
    score_ = 0;
    cleared_level_ = 0;
    combo_ = 1;
    rng_ = 0xC4A11A08u;
    level_ = 1;
    len_ = 0;
    front_pos_ = 0;
    insert_offset_ = 0;
    switch (difficulty_) {
      case GameDifficulty::Easy:
        advance_div_ = 5;  // × 90 ms host step = 450 ms per cell
        color_count_ = 4;
        break;
      case GameDifficulty::Normal:
        advance_div_ = 3;  // 300 ms per cell
        color_count_ = 4;
        break;
      case GameDifficulty::Hard:
        advance_div_ = 2;  // 200 ms per cell
        color_count_ = 4;
        break;
    }
    tick_count_ = 0;
    spawn_initial_();
    next_color_ = static_cast<uint8_t>(prng_next(&rng_) % color_count_);
    reserve_color_ = static_cast<uint8_t>(prng_next(&rng_) % color_count_);
  }

  void set_lane(uint16_t cells) override {
    lane_ = (cells >= 30) ? cells : kDefaultLane;  // 30 visible game cells minimum
  }

  void on_input(uint8_t player, InputEvent ev, uint32_t now_ms) override {
    (void)player;
    (void)now_ms;
    if (phase_ != GamePhase::Running) {
      return;
    }
    if (ev == InputEvent::Confirm) {
      insert_next_();
    } else if (ev == InputEvent::ActionB) {
      const uint8_t swap = next_color_;
      next_color_ = reserve_color_;
      reserve_color_ = swap;
    } else if (ev == InputEvent::Left && insert_offset_ > -18) {
      --insert_offset_;
    } else if (ev == InputEvent::Right && insert_offset_ < 11) {
      ++insert_offset_;
    }
  }

  void on_step(uint32_t now_ms) override {
    (void)now_ms;
    if (phase_ != GamePhase::Running) {
      return;
    }
    ++tick_count_;
    if (tick_count_ % advance_div_ == 0) {
      advance_chain_(1);
    }
  }

  void set_difficulty(GameDifficulty difficulty) override { difficulty_ = difficulty; }
  GamePhase phase() const override { return phase_; }
  void set_phase(GamePhase phase) override { phase_ = phase; }
  uint32_t score() const override { return score_; }

  void render(FrameBuffer& frame, uint32_t now_ms) const override {
    draw_chrome(frame, phase_, "CHAIN", score_, now_ms, kRed);
    if (phase_ != GamePhase::Running) {
      return;
    }
    // HUD: current and reserve color chips, score and level.
    frame.fill_rect(0, 0, 3, 3, kLaneColors[next_color_ % kLaneColorCount]);
    frame.fill_rect(5, 0, 3, 3, kLaneColors[reserve_color_ % kLaneColorCount]);
    draw_hud_number(frame, 16, 0, score_, kWhite);
    for (uint8_t i = 0; i < level_ && i < 10; ++i) {
      frame.set(static_cast<int16_t>(2 + i * 3), kH - 1, kDim);
    }
    for (uint8_t i = 1; i < combo_ && i < 6; ++i)
      frame.set(static_cast<int16_t>(20 + i), 7, kYellow);
  }

  void render_tail(TailFramebuffer& tail, uint32_t now_ms) const override {
    (void)now_ms;
    tail.clear();
    const uint16_t n = tail.length();
    const uint16_t group = n / 30u;
    if (group == 0) return;
    // Game cell 0 is the free end; 29 is the anchor/maw. On a 150-LED
    // product lane each ball occupies exactly five physical LEDs.
    for (uint16_t led = 0; led < group; ++led) tail.set(led, kRed);
    for (uint16_t i = 0; i < len_; ++i) {
      const int32_t cell = static_cast<int32_t>(front_pos_) - static_cast<int32_t>(len_ - 1 - i);
      if (cell >= 0 && cell < 29) {
        for (uint16_t led = 0; led < group; ++led) {
          const uint16_t pos = static_cast<uint16_t>(n - 1 - cell * group - led);
          tail.set(pos, kLaneColors[balls_[i] % kLaneColorCount]);
        }
      }
    }
    // Insertion cell marker (blink).
    if ((now_ms / 300) % 2 == 0) {
      const int32_t ins = insert_cell_();
      if (ins >= 0 && ins < 29) {
        for (uint16_t led = 0; led < group; ++led)
          tail.set(static_cast<uint16_t>(n - 1 - ins * group - led), kWhite);
      }
    }
  }

 private:
  int32_t insert_cell_() const {
    return 18 + insert_offset_;
  }

  uint16_t maw_pos_() const { return 29; }

  void spawn_initial_() {
    len_ = 0;
    for (uint8_t i = 0; i < 10; ++i) {
      push_back_();
    }
    front_pos_ = 10;
  }

  void push_back_() {
    if (len_ >= kMaxBalls) {
      return;
    }
    balls_[len_++] = static_cast<uint8_t>(prng_next(&rng_) % color_count_);
  }

  void advance_chain_(uint8_t cells) {
    front_pos_ = static_cast<uint16_t>(front_pos_ + cells);
    // Spawn room at the back grows as the chain marches forward.
    if (front_pos_ >= len_ && len_ < kMaxBalls) {
      push_back_();
    }
    if (front_pos_ >= maw_pos_()) {
      phase_ = GamePhase::Over;  // the chain reached the maw
    }
  }

  void insert_next_() {
    if (len_ + 1 > kMaxBalls) {
      return;  // bounded: a full chain cannot take another ball
    }
    // Insertion INDEX from the target cell: index counts back from the front.
    int32_t idx = static_cast<int32_t>(len_) - 1 -
                  (static_cast<int32_t>(front_pos_) - insert_cell_());
    if (idx < 0) {
      idx = 0;
    }
    if (idx > static_cast<int32_t>(len_)) {
      idx = static_cast<int32_t>(len_);
    }
    for (uint16_t i = len_; i > static_cast<uint16_t>(idx); --i) {
      balls_[i] = balls_[i - 1];
    }
    balls_[idx] = next_color_;
    ++len_;
    if (idx == static_cast<int32_t>(len_ - 1)) {
      ++front_pos_;  // inserted at the front: the chain grew forward
    }
    resolve_matches_(static_cast<uint16_t>(idx));
    if (phase_ != GamePhase::Running) {
      return;
    }
    next_color_ = reserve_color_;
    reserve_color_ = static_cast<uint8_t>(prng_next(&rng_) % color_count_);
    advance_chain_(1);  // insertion pressure: the chain lunges forward
  }

  void resolve_matches_(uint16_t around) {
    uint16_t cleared_total = 0;
    uint8_t combo = 0;
    uint16_t cleared = resolve_run_(around);
    while (cleared > 0) {
      if (combo < 5) ++combo;
      cleared_total += cleared;
      cleared_level_ += cleared;
      score_ += 10u * cleared * combo;
      cleared = longest_run_collapse_();
    }
    if (cleared_total == 0) {
      return;
    }
    combo_ = combo > 0 ? combo : 1;
    if (cleared_level_ >= 50 || len_ == 0) {
      if (cleared_level_ >= 50) {
        ++level_;
        cleared_level_ = 0;
      }
      if (advance_div_ > 2) {
        --advance_div_;  // faster chains per level, bounded
      }
      spawn_initial_();
    }
  }

  uint16_t resolve_run_(uint16_t idx) {
    if (len_ == 0) {
      return 0;
    }
    const uint8_t color = balls_[idx];
    uint16_t lo = idx;
    while (lo > 0 && balls_[lo - 1] == color) {
      --lo;
    }
    uint16_t hi = idx;
    while (hi + 1 < len_ && balls_[hi + 1] == color) {
      ++hi;
    }
    const uint16_t run = hi - lo + 1;
    return (run >= 3) ? collapse_(lo, run) : 0;
  }

  // Finds the longest same-color run; clears it if 3+ (chain-closing), else 0.
  uint16_t longest_run_collapse_() {
    uint16_t best_lo = 0;
    uint16_t best_run = 1;
    uint16_t run = 1;
    for (uint16_t i = 1; i < len_; ++i) {
      if (balls_[i] == balls_[i - 1]) {
        ++run;
      } else {
        if (run > best_run) {
          best_run = run;
          best_lo = i - run;
        }
        run = 1;
      }
    }
    if (run > best_run) {
      best_run = run;
      best_lo = len_ - run;
    }
    return (best_run >= 3) ? collapse_(best_lo, best_run) : 0;
  }

  uint16_t collapse_(uint16_t lo, uint16_t run) {
    for (uint16_t i = lo; i + run < len_; ++i) {
      balls_[i] = balls_[i + run];
    }
    len_ = static_cast<uint16_t>(len_ - run);
    // The segment beyond the cleared gap slides back toward the free-end
    // source. Resolve this before testing the anchor, so a matching shot can
    // rescue a chain whose front was one step from the maw.
    front_pos_ = front_pos_ > run ? static_cast<uint16_t>(front_pos_ - run) : 0;
    return run;
  }

  uint32_t rng_{0xC4A11A08u};
  GamePhase phase_{GamePhase::Ready};
  uint32_t score_{0};
  GameDifficulty difficulty_{GameDifficulty::Easy};
  uint8_t balls_[kMaxBalls]{};
  uint16_t len_{0};
  uint16_t front_pos_{0};
  uint16_t lane_{kDefaultLane};
  uint8_t next_color_{0};
  uint8_t reserve_color_{0};
  uint8_t color_count_{3};
  uint8_t combo_{1};
  uint16_t cleared_level_{0};
  uint8_t advance_div_{5};
  uint8_t level_{1};
  int8_t insert_offset_{0};
  uint32_t tick_count_{0};
};

// --------------------------------------------------------- light_race ----
// Symmetric 2P sprint: adjacent centers -> opposite outer ends. Each player
// alternates A/B press edges; the same throttle applies to both. Round result
// is resolved on the next fixed step so arrival in the same tick is a draw.
class LightRaceGame final : public Game {
 public:
  const char* name() const override { return "light_race"; }
  const char* label() const override { return "RACE"; }

  void reset(uint32_t now_ms) override {
    (void)now_ms;
    phase_ = GamePhase::Ready;
    score_ = 0;
    p1_wins_ = p2_wins_ = 0;
    round_ = 1;
    reset_round_();
    debounce_ms_ = difficulty_ == GameDifficulty::Easy ? 160u :
                   difficulty_ == GameDifficulty::Normal ? 120u : 100u;
  }

  void set_lane(uint16_t cells) override { lane_ = cells >= kMinLane ? cells : kDefaultLane; }
  void set_players(uint8_t /*players*/) override {}  // 2P-only family
  uint8_t players() const override { return 2; }
  void set_difficulty(GameDifficulty difficulty) override { difficulty_ = difficulty; }

  void on_input(uint8_t player, InputEvent ev, uint32_t now_ms) override {
    if (phase_ != GamePhase::Running || player < 1 || player > 2) return;
    const uint8_t slot = player - 1;
    const InputEvent expected = next_a_[slot] ? InputEvent::Confirm : InputEvent::ActionB;
    if (ev != expected || (seen_[slot] && now_ms - last_press_ms_[slot] < debounce_ms_)) return;
    seen_[slot] = true;
    last_press_ms_[slot] = now_ms;
    next_a_[slot] = !next_a_[slot];
    uint16_t& pos = slot == 0 ? p1_ : p2_;
    if (pos < finish_()) ++pos;
  }

  void on_step(uint32_t now_ms) override {
    (void)now_ms;
    if (phase_ != GamePhase::Running) return;
    const bool p1_done = p1_ >= finish_(), p2_done = p2_ >= finish_();
    if (!p1_done && !p2_done) return;
    if (p1_done && !p2_done) ++p1_wins_;
    if (p2_done && !p1_done) ++p2_wins_;
    score_ = p1_wins_;
    if (p1_wins_ >= 5 || p2_wins_ >= 5) phase_ = GamePhase::Over;
    else {
      if (!(p1_done && p2_done)) ++round_;
      reset_round_();  // a draw repeats the same round
    }
  }

  GamePhase phase() const override { return phase_; }
  void set_phase(GamePhase phase) override { phase_ = phase; }
  uint32_t score() const override { return score_; }
  uint32_t player_score(uint8_t player) const override { return player == 1 ? p1_wins_ : player == 2 ? p2_wins_ : 0; }
  uint8_t winner() const override { return phase_ != GamePhase::Over ? 0 : p1_wins_ > p2_wins_ ? 1 : p2_wins_ > p1_wins_ ? 2 : 3; }

  void render(FrameBuffer& frame, uint32_t now_ms) const override {
    draw_chrome(frame, phase_, "RACE", score_, now_ms, kGreen);
    if (phase_ == GamePhase::Over) {
      render_centered_label(frame, p1_wins_ == p2_wins_ ? "DRAW" :
                                  p1_wins_ > p2_wins_ ? "P1" : "P2", kWhite);
      return;
    }
    if (phase_ != GamePhase::Running) return;
    draw_bar_(frame, 1, p1_, kGreen);
    draw_bar_(frame, kH - 2, p2_, kOrange);
    for (uint8_t i = 0; i < p1_wins_; ++i) frame.set(14 + i * 2, 3, kGreen);
    for (uint8_t i = 0; i < p2_wins_; ++i) frame.set(14 + i * 2, 6, kOrange);
  }

  void render_tail(TailFramebuffer& tail, uint32_t now_ms) const override {
    (void)now_ms;
    tail.clear();
    const uint16_t n = tail.length();
    if (n < 4) {
      return;
    }
    const uint16_t half = finish_();
    const uint16_t left_center = static_cast<uint16_t>(n / 2 - 1);
    const uint16_t right_center = static_cast<uint16_t>(n / 2);
    tail.set(0, kGreen);
    tail.set(static_cast<uint16_t>(right_center + half - 1), kOrange);
    tail.set(static_cast<uint16_t>(left_center - (p1_ < half ? p1_ : half - 1)), kWhite);
    tail.set(static_cast<uint16_t>(right_center + (p2_ < half ? p2_ : half - 1)), kWhite);
  }

 private:
  void draw_bar_(FrameBuffer& frame, uint8_t y, uint16_t pos, Rgb color) const {
    const uint8_t cells = static_cast<uint8_t>(
        (static_cast<uint32_t>(pos) * (kW - 2)) / finish_());
    for (uint8_t x = 0; x < cells && x < kW; ++x) {
      frame.set(static_cast<int16_t>(x), static_cast<int16_t>(y), color);
    }
  }
  uint16_t finish_() const { return static_cast<uint16_t>(lane_ / 2 - 1); }
  void reset_round_() {
    p1_ = p2_ = 0;
    next_a_[0] = next_a_[1] = true;
    seen_[0] = seen_[1] = false;
  }
  GamePhase phase_{GamePhase::Ready};
  uint32_t score_{0};
  GameDifficulty difficulty_{GameDifficulty::Easy};
  uint16_t p1_{0};
  uint16_t p2_{0};
  uint16_t lane_{kDefaultLane};
  uint8_t round_{1}, p1_wins_{0}, p2_wins_{0};
  uint16_t debounce_ms_{120};
  uint32_t last_press_ms_[2]{};
  bool seen_[2]{}, next_a_[2]{true, true};
};

// ------------------------------------------------------ reaction_stop ----
// A light bounces along the rope; stop it (Confirm) inside the highlighted
// target zone. Each hit scores 10×round and shrinks the zone (min 1) while
// the light accelerates over time. Difficulty: zone 5/3/2 cells, light speed
// 40/25/10 ms per cell (physics on the host cadence).
class ReactionStopGame final : public Game {
 public:
  const char* name() const override { return "reaction_stop"; }
  const char* label() const override { return "STOP"; }

  void reset(uint32_t now_ms) override {
    (void)now_ms;
    phase_ = GamePhase::Ready;
    score_ = 0;
    scores_[0] = scores_[1] = 0;
    attempts_[0] = attempts_[1] = 0;
    active_player_ = 1;
    rng_ = 0x5EA105EDu;
    pos_ = 0;
    dir_ = 1;
    moving_ = false;
    stop_cell_ = 0;
    ticks_ = 0;
    switch (difficulty_) {
      case GameDifficulty::Easy:
        speed_div_ = 4;   // light advances every 4th host step (40 ms/cell)
        zone_w_ = 5;
        break;
      case GameDifficulty::Normal:
        speed_div_ = 2;   // 20 ms/cell
        zone_w_ = 3;
        break;
      case GameDifficulty::Hard:
        speed_div_ = 1;   // every step
        zone_w_ = 2;
        break;
    }
    // M-L010-3 (repair cycle 1): the zone start is lane-derived, never a
    // hard-coded 20. The host applies set_lane BEFORE reset, so the previous
    // fixed value overrode the lane clamp — on lanes below ~27 the zone sat
    // outside the light's bounce range and every stop lost the round.
    zone_start_ = kZoneStartDefault;
    clamp_zone_();
  }

  void set_lane(uint16_t cells) override {
    lane_ = (cells >= kMinLane) ? cells : kDefaultLane;
    clamp_zone_();
  }

  void set_difficulty(GameDifficulty difficulty) override { difficulty_ = difficulty; }
  void set_players(uint8_t players) override { players_ = players == 2 ? 2 : 1; }
  uint8_t players() const override { return players_; }
  void on_input(uint8_t player, InputEvent ev, uint32_t now_ms) override {
    (void)now_ms;
    if (ev != InputEvent::Confirm || phase_ != GamePhase::Running || !moving_ ||
        player != active_player_) {
      return;
    }
    moving_ = false;
    stop_cell_ = pos_;
    if (pos_ >= zone_start_ && pos_ < zone_start_ + zone_w_) {
      const uint16_t center = static_cast<uint16_t>(zone_start_ + (zone_w_ - 1u) / 2u);
      const uint16_t distance = pos_ > center ? pos_ - center : center - pos_;
      const uint16_t radius = static_cast<uint16_t>((zone_w_ + 1u) / 2u);
      scores_[active_player_ - 1] += 100u - 100u * distance / radius;
    }
    finish_attempt_();
  }

  void on_step(uint32_t now_ms) override {
    (void)now_ms;
    if (!moving_) {
      return;
    }
    ++ticks_;
    if (ticks_ >= 120) { finish_attempt_(); return; }  // 12 s attempt cap
    if (ticks_ % speed_div_ != 0) {
      return;  // light speed cadence (difficulty)
    }
    // Deterministic bounce across the lane.
    pos_ = static_cast<uint16_t>(pos_ + dir_);
    if (pos_ >= lane_ - 1) {
      pos_ = static_cast<uint16_t>(lane_ - 2);
      dir_ = -1;
    } else if (pos_ == 0) {
      dir_ = 1;
    }
    if (ticks_ % 100 == 0 && speed_div_ > 1) {
      --speed_div_;  // the light accelerates over time, bounded
    }
  }

  GamePhase phase() const override { return phase_; }
  void set_phase(GamePhase phase) override {
    phase_ = phase;
    if (phase == GamePhase::Running && !moving_) {
      moving_ = true;  // the light starts as the round starts
    }
  }
  uint32_t score() const override { return score_; }
  uint32_t player_score(uint8_t player) const override { return player >= 1 && player <= 2 ? scores_[player - 1] : 0; }
  uint8_t winner() const override { return phase_ != GamePhase::Over ? 0 : players_ == 1 ? (scores_[0] ? 1 : 2) : scores_[0] > scores_[1] ? 1 : scores_[1] > scores_[0] ? 2 : 3; }

  void render(FrameBuffer& frame, uint32_t now_ms) const override {
    draw_chrome(frame, phase_, "STOP", score_, now_ms, kOrange);
    if (phase_ != GamePhase::Running) {
      return;
    }
    draw_hud_number(frame, 1, 0, score_, kWhite);
    if (players_ == 2) draw_hud_number(frame, 20, 0, scores_[1], kOrange);
    for (uint8_t i = 0; i < attempts_[active_player_ - 1] && i < 5; ++i) {
      frame.set(static_cast<int16_t>(kW - 1 - i), 0, kGreen);
    }
    frame.set(active_player_ == 1 ? 1 : 29, 9, kWhite);
  }

  void render_tail(TailFramebuffer& tail, uint32_t now_ms) const override {
    (void)now_ms;
    tail.clear();
    const uint16_t n = tail.length();
    if (n < 4) {
      return;
    }
    // Target zone (green block), clamped to the physical rope.
    const uint16_t zs = (zone_start_ < n) ? zone_start_ : 0;
    for (uint16_t c = zs; c < zs + zone_w_ && c < n; ++c) {
      tail.set(c, kGreen);
    }
    if (!moving_) {
      if (stop_cell_ < n) {
        tail.set(stop_cell_, kWhite);  // stopped light
      }
      return;
    }
    // Moving light (physics position) + a one-cell trail.
    if (pos_ < n) {
      tail.set(pos_, kWhite);
    }
    if (pos_ >= 1 && pos_ - 1 < n) {
      tail.set(static_cast<uint16_t>(pos_ - 1), Rgb{70, 70, 80});
    }
  }

 private:
  void finish_attempt_() {
    ++attempts_[active_player_ - 1];
    score_ = scores_[0];
    if (attempts_[0] >= 5 && (players_ == 1 || attempts_[1] >= 5)) {
      moving_ = false;
      phase_ = GamePhase::Over;
      return;
    }
    if (players_ == 2) active_player_ = active_player_ == 1 ? 2 : 1;
    new_round_();
  }
  void new_round_() {
    moving_ = true;
    pos_ = 0;
    dir_ = 1;
    ticks_ = 0;
    // New zone position: deterministic spread over the middle of the lane.
    const uint16_t span = static_cast<uint16_t>(lane_ > 14 ? lane_ - 14 : 1);
    zone_start_ = static_cast<uint16_t>(7 + (prng_next(&rng_) % span));
    clamp_zone_();
  }
  // M-L010-3 (repair cycle 1): keep the target zone inside the light's bounce
  // range [0, lane-2] for every supported lane — shared by reset(), set_lane()
  // and new_round_() so no path can reintroduce an unreachable zone.
  void clamp_zone_() {
    if (zone_start_ + zone_w_ > lane_ - 2) {
      zone_start_ = static_cast<uint16_t>((lane_ - 2 > zone_w_) ? lane_ - 2 - zone_w_ : 1);
    }
  }
  uint32_t rng_{0x5EA105EDu};
  GamePhase phase_{GamePhase::Ready};
  uint32_t score_{0};
  GameDifficulty difficulty_{GameDifficulty::Easy};
  uint16_t lane_{kDefaultLane};
  uint16_t pos_{0};
  int8_t dir_{1};
  uint16_t zone_start_{kZoneStartDefault};
  uint8_t zone_w_{5};
  uint8_t speed_div_{4};
  uint8_t players_{1};
  uint8_t active_player_{1};
  uint8_t attempts_[2]{};
  uint32_t scores_[2]{};
  bool moving_{false};
  uint16_t stop_cell_{0};
  uint32_t ticks_{0};
};

// --------------------------------------------------------- color_catch ----
// The matrix shows the TARGET color; colored pulses run along the rope. Press
// Confirm exactly when a TARGET-colored pulse crosses the catch gate (white):
// hit = +10×combo (combo grows per hit, resets on a miss). Pressing on a wrong
// color or letting a target pulse pass = a life lost. Three lives. Difficulty:
// pulse cadence 1200/900/600 ms, pulse speed 30/20/10 ms per cell.
class ColorCatchGame final : public Game {
 public:
  static constexpr uint8_t kMaxPulses = 6;
  const char* name() const override { return "color_catch"; }
  const char* label() const override { return "CATCH"; }

  void reset(uint32_t now_ms) override {
    (void)now_ms;
    phase_ = GamePhase::Ready;
    score_ = 0;
    score_p1_ = score_p2_ = 0;
    turn_ = 1;
    turn_ticks_ = 0;
    rng_ = 0xCA07C40Cu;
    lives_ = 3;
    combo_ = 1;
    target_ = 0;
    spawns_ = 0;
    tick_count_ = 0;
    switch (difficulty_) {
      case GameDifficulty::Easy:
        spawn_div_ = 12;  // × 90 ms
        speed_div_ = 3;   // 30 ms per cell
        break;
      case GameDifficulty::Normal:
        spawn_div_ = 9;
        speed_div_ = 2;
        break;
      case GameDifficulty::Hard:
        spawn_div_ = 6;
        speed_div_ = 1;
        break;
    }
    std::memset(pulse_active_, 0, sizeof(pulse_active_));
  }

  void set_lane(uint16_t cells) override {
    lane_ = (cells >= kMinLane) ? cells : kDefaultLane;
    catch_pos_ = static_cast<uint16_t>((static_cast<uint32_t>(lane_) * 7u) / 10u);
  }

  void set_difficulty(GameDifficulty difficulty) override { difficulty_ = difficulty; }
  void set_players(uint8_t players) override { players_ = players == 2 ? 2 : 1; }
  uint8_t players() const override { return players_; }
  void on_input(uint8_t player, InputEvent ev, uint32_t now_ms) override {
    (void)now_ms;
    if (ev != InputEvent::Confirm || phase_ != GamePhase::Running ||
        (players_ == 2 && player != turn_)) {
      return;
    }
    // Catch check: a pulse AT the gate (±1 cell), fastest first.
    int best = -1;
    uint16_t best_dist = 0xFFFF;
    for (uint8_t i = 0; i < kMaxPulses; ++i) {
      if (!pulse_active_[i]) {
        continue;
      }
      const int32_t d = static_cast<int32_t>(catch_pos_) - static_cast<int32_t>(pulse_pos_[i]);
      if (d >= -1 && d <= 1) {
        const uint16_t dist = static_cast<uint16_t>(d < 0 ? -d : d);
        if (dist < best_dist) {
          best_dist = dist;
          best = static_cast<int>(i);
        }
      }
    }
    if (best >= 0) {
      if (pulse_color_[best] == target_) {
        score_ += 10u;
        if (combo_ < 9) {
          ++combo_;
        }
      } else {
        combo_ = 1;
        penalize_();  // pressed on a wrong color
      }
      pulse_active_[best] = false;
    } else {
      combo_ = 1;
      penalize_();  // whiffed press
    }
  }

  void on_step(uint32_t now_ms) override {
    (void)now_ms;
    if (phase_ != GamePhase::Running) {
      return;
    }
    ++tick_count_;
    if (players_ == 2 && ++turn_ticks_ >= 300) {
      if (turn_ == 1) {
        score_p1_ = score_;
        turn_ = 2;
        turn_ticks_ = 0;
        score_ = 0;
        target_ = 0;
        spawns_ = 0;
        tick_count_ = 0;
        rng_ = 0x8B63D512u;  // independent but same cadence and color distribution
        std::memset(pulse_active_, 0, sizeof(pulse_active_));
      } else {
        score_p2_ = score_;
        score_ = score_p1_;
        phase_ = GamePhase::Over;
      }
      return;
    }
    for (uint8_t i = 0; i < kMaxPulses; ++i) {
      if (!pulse_active_[i]) {
        continue;
      }
      ++ticks_[i];
      if (ticks_[i] % speed_div_ == 0) {
        ++pulse_pos_[i];
      }
      if (pulse_pos_[i] > catch_pos_ + 4) {
        if (pulse_color_[i] == target_) {
          penalize_();  // a target pulse slipped past uncaptured
          combo_ = 1;
        }
        pulse_active_[i] = false;
      }
    }
    if (tick_count_ % spawn_div_ == 0) {
      for (uint8_t i = 0; i < kMaxPulses; ++i) {
        if (!pulse_active_[i]) {
          pulse_active_[i] = true;
          pulse_pos_[i] = 0;
          ticks_[i] = 0;
          pulse_color_[i] = static_cast<uint8_t>(prng_next(&rng_) % kLaneColorCount);
          break;
        }
      }
      ++spawns_;
      if (spawns_ % 5 == 0) {
        uint8_t next = target_;
        while (next == target_) {
          next = static_cast<uint8_t>(prng_next(&rng_) % kLaneColorCount);
        }
        target_ = next;  // the target re-rolls every 5 spawns
      }
    }
  }

  GamePhase phase() const override { return phase_; }
  void set_phase(GamePhase phase) override { phase_ = phase; }
  uint32_t score() const override { return score_; }
  uint32_t player_score(uint8_t player) const override {
    if (player == 1) return players_ == 2 && turn_ == 2 ? score_p1_ : score_;
    return player == 2 && players_ == 2 ? (turn_ == 2 && phase_ != GamePhase::Over ? score_ : score_p2_) : 0;
  }
  uint8_t winner() const override { return phase_ != GamePhase::Over ? 0 : players_ == 1 ? (lives_ ? 1 : 2) : score_p1_ > score_p2_ ? 1 : score_p2_ > score_p1_ ? 2 : 3; }

  void render(FrameBuffer& frame, uint32_t now_ms) const override {
    draw_chrome(frame, phase_, "CATCH", score_, now_ms, kRed);
    if (phase_ != GamePhase::Running) {
      if (phase_ == GamePhase::Over && players_ == 2) {
        render_centered_label(frame, score_p1_ == score_p2_ ? "DRAW" :
                                      score_p1_ > score_p2_ ? "P1" : "P2", kWhite);
      }
      return;
    }
    // HUD: target color field (top strip), lives as pips, combo dots.
    frame.fill_rect(0, 0, kW, 2, kLaneColors[target_ % kLaneColorCount]);
    if (players_ == 2) frame.set(0, 4, turn_ == 1 ? kGreen : kOrange);
    for (uint8_t i = 0; i < lives_; ++i) {
      frame.fill_rect(static_cast<int16_t>(1 + i * 3), kH - 2, 2, 1, kGreen);
    }
    for (uint8_t i = 0; i + 1 < combo_; ++i) {
      frame.set(static_cast<int16_t>(kW - 2 - i), kH - 2, kYellow);
    }
  }

  void render_tail(TailFramebuffer& tail, uint32_t now_ms) const override {
    (void)now_ms;
    tail.clear();
    const uint16_t n = tail.length();
    // Catch gate (white, 2 wide), clamped.
    const uint16_t cp = (catch_pos_ + 1 < n) ? catch_pos_ : static_cast<uint16_t>(n - 2);
    tail.set(cp, kWhite);
    tail.set(static_cast<uint16_t>(cp + 1), kWhite);
    for (uint8_t i = 0; i < kMaxPulses; ++i) {
      if (!pulse_active_[i]) {
        continue;
      }
      if (pulse_pos_[i] < n) {
        tail.set(pulse_pos_[i], kLaneColors[pulse_color_[i] % kLaneColorCount]);
      }
    }
  }

 private:
  void penalize_() {
    if (players_ == 2) score_ = score_ >= 5 ? score_ - 5 : 0;
    else lose_life_();
  }
  void lose_life_() {
    if (lives_ > 0) {
      --lives_;
    }
    if (lives_ == 0) {
      phase_ = GamePhase::Over;
    }
  }
  uint32_t rng_{0xCA07C40Cu};
  uint32_t score_p1_{0}, score_p2_{0};
  uint16_t turn_ticks_{0};
  uint8_t players_{1}, turn_{1};
  GamePhase phase_{GamePhase::Ready};
  uint32_t score_{0};
  GameDifficulty difficulty_{GameDifficulty::Easy};
  uint16_t lane_{kDefaultLane};
  bool pulse_active_[kMaxPulses]{};
  uint16_t pulse_pos_[kMaxPulses]{};
  uint32_t ticks_[kMaxPulses]{};
  uint8_t pulse_color_[kMaxPulses]{};
  uint8_t lives_{3};
  uint8_t combo_{1};
  uint8_t target_{0};
  uint8_t spawn_div_{12};
  uint8_t speed_div_{3};
  uint16_t catch_pos_{42};
  uint32_t tick_count_{0};
  uint32_t spawns_{0};
};

// ------------------------------------------------------------- memory ----
// Simon on the rope: the tail is four color zones. A sequence of zone flashes
// plays back; the player repeats it (confirm = zone 0 red, back = zone 1
// green, left = zone 2 blue, right = zone 3 yellow). A completed round grows
// the sequence by one; one wrong press ends the game (score = completed
// rounds). Difficulty: flash 500/380/260 ms, starting length 1/2/3.
class MemoryGame final : public Game {
 public:
  static constexpr uint8_t kMaxSeq = 16;
  const char* name() const override { return "memory"; }
  const char* label() const override { return "MEM"; }

  void reset(uint32_t now_ms) override {
    (void)now_ms;
    phase_ = GamePhase::Ready;
    won_ = false;
    score_ = 0;
    scores_[0] = scores_[1] = 0;
    round_ = 1;
    turn_in_round_ = 0;
    active_player_ = 1;
    seq_len_ = 0;
    play_idx_ = 0;
    input_idx_ = 0;
    playing_ = false;
    flash_active_ = false;
    flash_zone_ = 0;
    phase_timer_ = 0;
    last_step_ = now_ms;
    switch (difficulty_) {
      case GameDifficulty::Easy:
        flash_ms_ = 500;
        start_len_ = 3;
        break;
      case GameDifficulty::Normal:
        flash_ms_ = 380;
        start_len_ = 3;
        break;
      case GameDifficulty::Hard:
        flash_ms_ = 260;
        start_len_ = 3;
        break;
    }
  }

  void set_difficulty(GameDifficulty difficulty) override { difficulty_ = difficulty; }
  void set_players(uint8_t players) override { players_ = players == 2 ? 2 : 1; }
  uint8_t players() const override { return players_; }
  void on_input(uint8_t player, InputEvent ev, uint32_t now_ms) override {
    if (phase_ != GamePhase::Running || playing_ || player != active_player_) {
      return;  // input only during the repeat phase
    }
    uint8_t zone = 0xFF;
    switch (ev) {
      case InputEvent::Up:
      case InputEvent::Confirm: zone = 0; break;
      case InputEvent::Right:
      case InputEvent::ActionB: zone = 1; break;
      case InputEvent::Down: zone = 2; break;
      case InputEvent::Left: zone = 3; break;
      default: return;
    }
    flash_zone_ = zone;
    flash_active_ = true;  // echo the press on the rope
    if (zone != seq_[input_idx_]) {
      if (players_ == 1) phase_ = GamePhase::Over;
      else end_turn_(now_ms);
      return;
    }
    ++scores_[active_player_ - 1];
    score_ = scores_[0];
    ++input_idx_;
    if (input_idx_ >= seq_len_) {
      if (players_ == 2) end_turn_(now_ms);
      else if (seq_len_ >= kMaxSeq) { won_ = true; phase_ = GamePhase::Over; }
      else begin_turn_(now_ms);
    }
  }

  void on_step(uint32_t now_ms) override {
    if (phase_ != GamePhase::Running) {
      return;
    }
    const uint32_t dt = now_ms - last_step_;
    last_step_ = now_ms;
    phase_timer_ += dt;
    if (playing_) {
      // Playback: flash each item flash_ms_, then a 140 ms dark gap.
      if (flash_active_ && phase_timer_ >= flash_ms_) {
        flash_active_ = false;
        phase_timer_ = 0;
      } else if (!flash_active_ && phase_timer_ >= 140) {
        phase_timer_ = 0;
        ++play_idx_;
        if (play_idx_ >= seq_len_) {
          playing_ = false;
          input_idx_ = 0;
        } else {
          flash_zone_ = seq_[play_idx_];
          flash_active_ = true;
        }
      }
    } else if (flash_active_ && phase_timer_ >= 120) {
      flash_active_ = false;  // input echo decays
    }
  }

  GamePhase phase() const override { return phase_; }
  void set_phase(GamePhase phase) override {
    phase_ = phase;
    if (phase == GamePhase::Running && seq_len_ == 0) {
      begin_turn_(last_step_);  // the first round builds and plays
    }
  }
  uint32_t score() const override { return score_; }
  uint32_t player_score(uint8_t player) const override { return player >= 1 && player <= 2 ? scores_[player - 1] : 0; }
  uint8_t winner() const override { return phase_ != GamePhase::Over ? 0 : players_ == 1 ? (won_ ? 1 : 2) : scores_[0] > scores_[1] ? 1 : scores_[1] > scores_[0] ? 2 : 3; }

  void render(FrameBuffer& frame, uint32_t now_ms) const override {
    draw_chrome(frame, phase_, "MEM", score_, now_ms, kBlue);
    if (phase_ != GamePhase::Running) {
      return;
    }
    // HUD: round number + phase indicator (yellow = watch, green = repeat).
    draw_hud_number(frame, 1, 0, static_cast<uint32_t>(round_), kWhite);
    frame.fill_rect(kW - 4, 0, 3, 2, playing_ ? kYellow : kGreen);
    frame.set(active_player_ == 1 ? 1 : 28, 9, kWhite);
  }

  void render_tail(TailFramebuffer& tail, uint32_t now_ms) const override {
    (void)now_ms;
    tail.clear();
    const uint16_t n = tail.length();
    if (n < 8) {
      return;
    }
    // Four zone tints (very dim) + the flashing zone bright.
    for (uint8_t z = 0; z < 4; ++z) {
      const uint16_t lo = static_cast<uint16_t>(static_cast<uint32_t>(z) * n / 4u);
      const uint16_t hi = static_cast<uint16_t>(static_cast<uint32_t>(z + 1) * n / 4u);
      const bool lit = flash_active_ && flash_zone_ == z;
      const Rgb base = kLaneColors[z % kLaneColorCount];
      const Rgb drawn = lit ? base
                            : Rgb{static_cast<uint8_t>(base.r / 8),
                                  static_cast<uint8_t>(base.g / 8),
                                  static_cast<uint8_t>(base.b / 8)};
      for (uint16_t c = lo; c < hi && c < n; ++c) {
        tail.set(c, drawn);
      }
    }
  }

 private:
  void begin_turn_(uint32_t now_ms) {
    if (players_ == 1) {
      seq_len_ = seq_len_ == 0 ? start_len_ : static_cast<uint8_t>(seq_len_ + 1);
      round_ = static_cast<uint8_t>(seq_len_ - start_len_ + 1);
      active_player_ = 1;
    } else {
      seq_len_ = static_cast<uint8_t>(start_len_ + round_ - 1);
      active_player_ = (round_ & 1u) ? (turn_in_round_ == 0 ? 1 : 2)
                                     : (turn_in_round_ == 0 ? 2 : 1);
    }
    // Per-player, per-round independent sequence with equal length and timing.
    rng_ = 0x6E0B7E1Du ^ (static_cast<uint32_t>(round_) * 0x9E3779B9u) ^
           (static_cast<uint32_t>(active_player_) * 0x7F4A7C15u);
    for (uint8_t i = 0; i < seq_len_; ++i) {
      seq_[i] = static_cast<uint8_t>(prng_next(&rng_) & 3u);
    }
    play_idx_ = 0;
    playing_ = true;
    flash_zone_ = seq_[0];
    flash_active_ = true;
    phase_timer_ = 0;
    last_step_ = now_ms;
  }
  void end_turn_(uint32_t now_ms) {
    ++turn_in_round_;
    if (turn_in_round_ >= 2) {
      turn_in_round_ = 0;
      ++round_;
    }
    if (round_ > 5) { phase_ = GamePhase::Over; return; }
    begin_turn_(now_ms);
  }
  uint32_t rng_{0x6E0B7E1Du};
  GamePhase phase_{GamePhase::Ready};
  uint32_t score_{0};
  GameDifficulty difficulty_{GameDifficulty::Easy};
  uint8_t seq_[kMaxSeq]{};
  uint8_t seq_len_{0};
  uint8_t play_idx_{0};
  uint8_t input_idx_{0};
  uint8_t round_{0};
  uint8_t players_{1};
  uint8_t active_player_{1};
  uint8_t turn_in_round_{0};
  bool won_{false};
  uint32_t scores_[2]{};
  uint16_t flash_ms_{500};
  uint8_t start_len_{1};
  bool playing_{false};
  bool flash_active_{false};
  uint8_t flash_zone_{0};
  uint32_t phase_timer_{0};
  uint32_t last_step_{0};
};

// ---------------------------------------------------------- tug_of_war ----
// One balance marker; P1 pulls toward cell 0 (green side), P2 toward the far
// end (orange side). Every Confirm press pulls one cell. 2P: raw duel. 1P:
// the deterministic AI pulls on a cadence (360/180/90 ms by difficulty).
// First side to drag the marker past its goal wins the round; the round win
// count is the score.
class TugOfWarGame final : public Game {
 public:
  const char* name() const override { return "tug_of_war"; }
  const char* label() const override { return "TUG"; }

  void reset(uint32_t now_ms) override {
    (void)now_ms;
    phase_ = GamePhase::Ready;
    score_ = 0;
    // M-L010-2 (repair cycle 1): the start marker is lane-derived (center),
    // never the hard-coded 29 — the host applies set_lane BEFORE reset, so
    // the previous fixed value overrode the lane-proportional start (wrong
    // start for any lane != 60; instant round loss possible below lane 31).
    marker_ = static_cast<int16_t>(lane_ / 2);
    p1_wins_ = p2_wins_ = 0;
    round_ = 1;
    round_steps_ = 0;
    seen_[0] = seen_[1] = false;
    tick_count_ = 0;
    switch (difficulty_) {
      case GameDifficulty::Easy:
        ai_every_ = 4;  // AI pull every 4th host step (400 ms)
        break;
      case GameDifficulty::Normal:
        ai_every_ = 2;  // 200 ms
        break;
      case GameDifficulty::Hard:
        ai_every_ = 1;  // every step (90 ms)
        break;
    }
  }

  void set_lane(uint16_t cells) override {
    lane_ = (cells >= kMinLane) ? cells : kDefaultLane;
    marker_ = static_cast<int16_t>(lane_ / 2);
  }

  void set_players(uint8_t players) override { players_ = (players == 2u) ? 2u : 1u; }
  uint8_t players() const override { return players_; }

  void set_difficulty(GameDifficulty difficulty) override { difficulty_ = difficulty; }
  void on_input(uint8_t player, InputEvent ev, uint32_t now_ms) override {
    if (ev != InputEvent::Confirm || phase_ != GamePhase::Running) {
      return;
    }
    if (player < 1 || player > players_) return;
    const uint8_t slot = player - 1;
    if (seen_[slot] && now_ms - last_press_ms_[slot] < 120u) return;
    seen_[slot] = true;
    last_press_ms_[slot] = now_ms;
    if (player == kPlayer2 && players_ == 2u) {
      ++marker_;  // P2 pulls toward the far end
    } else {
      --marker_;  // P1 pulls toward cell 0
    }
    check_win_();
  }

  void on_step(uint32_t now_ms) override {
    (void)now_ms;
    if (phase_ != GamePhase::Running) {
      return;
    }
    ++tick_count_;
    if (players_ == 1u && tick_count_ % ai_every_ == 0) {
      ++marker_;  // the AI (orange) pulls toward the far end
      check_win_();
    }
    if (phase_ == GamePhase::Running && ++round_steps_ >= 200u) {
      const int16_t center = static_cast<int16_t>(lane_ / 2);
      finish_round_(marker_ < center ? 1 : marker_ > center ? 2 : 0);
    }
  }

  GamePhase phase() const override { return phase_; }
  void set_phase(GamePhase phase) override { phase_ = phase; }
  uint32_t score() const override { return score_; }
  uint32_t player_score(uint8_t player) const override { return player == 1 ? p1_wins_ : player == 2 ? p2_wins_ : 0; }
  uint8_t winner() const override { return phase_ != GamePhase::Over ? 0 : p1_wins_ > p2_wins_ ? 1 : p2_wins_ > p1_wins_ ? 2 : 3; }

  void render(FrameBuffer& frame, uint32_t now_ms) const override {
    draw_chrome(frame, phase_, "TUG", score_, now_ms, kGreen);
    if (phase_ == GamePhase::Over) {
      render_centered_label(frame, p1_wins_ > p2_wins_ ? "P1" : "P2", kWhite);
      return;
    }
    if (phase_ != GamePhase::Running) {
      return;
    }
    // HUD: marker mirror bar (P1 green left, P2/AI orange right) + P1 wins.
    const int16_t cells = static_cast<int16_t>(
        (static_cast<int32_t>(marker_) * (kW - 2)) / static_cast<int32_t>(lane_));
    for (uint8_t x = 0; x < kW; ++x) {
      frame.set(static_cast<int16_t>(x), kH - 1,
                x < cells ? kOrange : (x == cells ? kWhite : kGreen));
    }
    draw_hud_number(frame, 1, 0, p1_wins_, kGreen);
    draw_hud_number(frame, 25, 0, p2_wins_, kOrange);
  }

  void render_tail(TailFramebuffer& tail, uint32_t now_ms) const override {
    (void)now_ms;
    tail.clear();
    const uint16_t n = tail.length();
    if (n < 6) {
      return;
    }
    // Sides tinted + the marker (bright white) + goal posts.
    const uint16_t m = (marker_ >= 0 && marker_ < n) ? static_cast<uint16_t>(marker_)
                                                     : static_cast<uint16_t>(n - 1);
    for (uint16_t c = 0; c < n; ++c) {
      tail.set(c, c < m ? Rgb{14, 40, 16} : Rgb{40, 26, 8});
    }
    tail.set(m, kWhite);
    tail.set(0, kGreen);
    tail.set(static_cast<uint16_t>(n - 1), kOrange);
  }

 private:
  void check_win_() {
    if (marker_ <= 0) {
      finish_round_(1);
    } else if (marker_ >= static_cast<int16_t>(lane_ - 1)) {
      finish_round_(2);
    }
  }
  void finish_round_(uint8_t winner) {
    if (winner == 1) ++p1_wins_;
    if (winner == 2) ++p2_wins_;
    score_ = p1_wins_;
    if (p1_wins_ >= 5 || p2_wins_ >= 5) {
      phase_ = GamePhase::Over;
      return;
    }
    if (winner != 0) ++round_;  // middle tie repeats the round
    marker_ = static_cast<int16_t>(lane_ / 2);
    round_steps_ = 0;
    seen_[0] = seen_[1] = false;
  }
  GamePhase phase_{GamePhase::Ready};
  uint32_t score_{0};
  GameDifficulty difficulty_{GameDifficulty::Easy};
  uint8_t players_{1};
  uint16_t lane_{kDefaultLane};
  int16_t marker_{kDefaultLane / 2};  // lane center (M-L010-2); reset() re-derives
  uint8_t ai_every_{4};
  uint32_t tick_count_{0};
  uint8_t p1_wins_{0};
  uint8_t p2_wins_{0};
  uint8_t round_{1};
  uint16_t round_steps_{0};
  uint32_t last_press_ms_[2]{};
  bool seen_[2]{};
};

// Single static instances (no heap; exactly one game active at any time).
ColorChainGame s_color_chain;
LightRaceGame s_light_race;
ReactionStopGame s_reaction_stop;
ColorCatchGame s_color_catch;
MemoryGame s_memory;
TugOfWarGame s_tug_of_war;

struct TailGameBinding {
  const char* name;
  Game* game;
};

constexpr TailGameBinding kTailGameBindings[] = {
    {"color_chain", &s_color_chain},
    {"light_race", &s_light_race},
    {"reaction_stop", &s_reaction_stop},
    {"color_catch", &s_color_catch},
    {"memory", &s_memory},
    {"tug_of_war", &s_tug_of_war},
};

}  // namespace

Game* tail_game_by_name(const char* name) {
  if (name == nullptr) {
    return nullptr;
  }
  for (const TailGameBinding& b : kTailGameBindings) {
    if (std::strcmp(b.name, name) == 0) {
      return b.game;
    }
  }
  return nullptr;  // fail closed
}

}  // namespace games
}  // namespace ledwall
