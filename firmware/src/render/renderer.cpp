#include "render/renderer.h"

#include <cstdio>

#include "effects/animation_renderers.h"
#include "effects/combined_renderers.h"
#include "effects/expression_map.h"
#include "effects/tail_renderers.h"
#include "render/color.h"
#include "text/text_renderer.h"

namespace ledwall {
namespace renderer {

namespace {

// Heritage text palette: CHSV(96, 200, 255) (frozen §6 "default heritage
// palette"; heritage demoScroll hueBase 96), through the deterministic
// converter.
Rgb text_color() { return hsv2rgb(96, 200, 255); }

constexpr Rgb kWhite{255, 255, 255};
constexpr Rgb kDimGray{70, 70, 80};
constexpr Rgb kPauseYellow{220, 190, 40};

void draw_border(Rgb color, FrameBuffer& frame) {
  for (uint8_t x = 0; x < kMatrixWidth; ++x) {
    frame.set(static_cast<int16_t>(x), 0, color);
    frame.set(static_cast<int16_t>(x), kMatrixHeight - 1, color);
  }
  for (uint8_t y = 1; y < kMatrixHeight - 1; ++y) {
    frame.set(0, static_cast<int16_t>(y), color);
    frame.set(kMatrixWidth - 1, static_cast<int16_t>(y), color);
  }
}

// Legacy projection: a program built with only the §6 scroll flag
// (mode Fixed + scrolling) IS a marquee in the heritage LEFT→RIGHT direction
// — the rtl field only exists for programs created through the LED-L009
// Text Studio (explicit mode = marquee honors rtl). This keeps every
// LED-L007 golden and legacy client behavior byte-exact.
TextMode effective_text_mode(const TextProgram& text) {
  return (text.mode == TextMode::Fixed && text.scrolling) ? TextMode::Marquee : text.mode;
}

bool effective_text_rtl(const TextProgram& text) {
  return effective_text_mode(text) == TextMode::Marquee && text.mode == TextMode::Fixed
             ? false  // scroll-flag projection: heritage direction
             : text.rtl;
}

void render_text(FrameBuffer& frame, const TextProgram& text, uint32_t started_ms,
                 uint32_t now_ms) {
  const uint32_t elapsed = now_ms - started_ms;  // wrap-safe unsigned difference
  // LED-L007 legacy path (flat heritage color) preserved for simple programs;
  // the Text Studio realization covers all modes/styles.
  const TextMode em = effective_text_mode(text);
  if (text.style == TextStyle::Solid && text.fg_color == kDefaultTextFg &&
      text.bg_color == kDefaultTextBg && em != TextMode::Words) {
    if (em == TextMode::Fixed) {
      text::draw_static(frame, text.current, text_color());
      return;
    }
    if (em == TextMode::Marquee && !effective_text_rtl(text)) {
      // Legacy scroll flag with the heritage color keeps the exact
      // LED-L007 goldens for the heritage LTR direction.
      text::draw_scrolling(frame, text.current, text_color(), elapsed,
                           text.speed_ms_per_px);
      return;
    }
  }
  text::draw_text_program(frame, text, elapsed);
}

// Frozen §8 expression realization (data-driven mapping to canonical
// renderers — ADR-007). Unchanged from the accepted LED-L007 behavior.
void render_expression(FrameBuffer& frame, const effects::ExpressionVisual& visual,
                       uint32_t started_ms, uint32_t now_ms) {
  const uint32_t elapsed = now_ms - started_ms;
  frame.clear();
  switch (visual.kind) {
    case effects::ExpressionVisual::Kind::Rest:
      // Blank/rest frame (idle; sleeping = "dim idle" — dimness comes from
      // the user brightness pipeline, frozen §8/§13).
      break;
    case effects::ExpressionVisual::Kind::BorderPulse: {
      const uint32_t period_ms = visual.param_a;
      const uint8_t phase = static_cast<uint8_t>(((elapsed % period_ms) * 256u) / period_ms);
      const uint8_t pulse =
          static_cast<uint8_t>(40u + (static_cast<uint16_t>(sin_u8(phase)) * 215u) / 255u);
      const Rgb c = hsv2rgb(visual.hue, 255, pulse);
      draw_border(c, frame);
      break;
    }
    case effects::ExpressionVisual::Kind::StaticBorder: {
      // Non-animated solid border (error): fixed value, no pulse.
      draw_border(hsv2rgb(visual.hue, 255, 200), frame);
      break;
    }
    case effects::ExpressionVisual::Kind::Wave:
      effects::render_wave_frame(frame, elapsed, visual.param_a, visual.hue_offset);
      break;
    case effects::ExpressionVisual::Kind::Plasma:
      effects::render_plasma_frame(frame, elapsed, visual.param_a);
      break;
  }
}

void render_timer(FrameBuffer& frame, const CountdownTimer& timer, uint32_t now_ms) {
  char buf[9];
  countdown_format(timer.remaining_ms(now_ms), buf);
  frame.clear();
  switch (timer.state()) {
    case CountdownTimer::State::Idle:
      // Armed but not started: dim display of the stored duration.
      text::draw_static(frame, buf, kDimGray);
      break;
    case CountdownTimer::State::Running:
      text::draw_static(frame, buf, kWhite);
      break;
    case CountdownTimer::State::Paused:
      text::draw_static(frame, buf, kWhite);
      draw_border(kPauseYellow, frame);
      break;
    case CountdownTimer::State::Finished: {
      // Stable finished state with the visual alert (pulsing red border).
      text::draw_static(frame, buf, kWhite);
      const uint8_t pulse =
          90u + (static_cast<uint16_t>(sin_u8(static_cast<uint8_t>((now_ms / 60) & 0xFF))) *
                 165u) /
                    255u;
      draw_border(hsv2rgb(0, 255, pulse), frame);
      break;
    }
  }
}

// LED-L009 notification: text (auto-marquee when wider than the canvas) on
// black; high priority adds a pulsing red border.
void render_notification(FrameBuffer& frame, const NotifyState& notify, uint32_t started_ms,
                         uint32_t now_ms) {
  TextProgram view;
  std::snprintf(view.current, sizeof(view.current), "%s", notify.text);
  view.mode = text::fits_static(notify.text) ? TextMode::Fixed : TextMode::Marquee;
  view.rtl = true;
  view.speed_ms_per_px = 140;
  view.style = TextStyle::Solid;
  view.fg_color = 1;  // white
  view.bg_color = 0;  // black
  text::draw_text_program(frame, view, now_ms - started_ms);
  if (notify.high_priority) {
    const uint8_t pulse =
        90u + (static_cast<uint16_t>(sin_u8(static_cast<uint8_t>((now_ms / 70) & 0xFF))) * 165u) /
                  255u;
    draw_border(hsv2rgb(0, 255, pulse), frame);
  }
}

// LED-L009 local menu rendering: centered label (selection marker blink),
// auto-marquee for long list names, bottom progress dots.
void render_menu(FrameBuffer& frame, const MenuState& menu, uint32_t now_ms) {
  frame.clear();
  char label[kMaxProgramNameLen];

  if (menu.screen() == MenuScreen::Root) {
    static const char* const kRootLabels[kMenuRootItems] = {
        "TIMER", "ANIMS", "EXPRS", "GAMES", "BRI", "STBY", "EXIT",
    };
    if (menu.root_index() == 4) {
      std::snprintf(label, sizeof(label), "BRI %u",
                    static_cast<unsigned>(menu.brightness_mirror));
    } else {
      std::snprintf(label, sizeof(label), "%s", kRootLabels[menu.root_index()]);
    }
  } else {
    uint8_t idx = menu.list_index();
    const char* name = "";
    if (menu.screen() == MenuScreen::Animations) {
      name = (idx < kAnimationCount) ? kAnimationNames[idx] : "";
    } else if (menu.screen() == MenuScreen::Expressions) {
      name = (idx < kExpressionCount) ? kExpressionNames[idx] : "";
    } else if (menu.screen() == MenuScreen::Games) {
      name = (idx < kGameCount) ? kGameNames[idx] : "";
    }
    std::snprintf(label, sizeof(label), "%s", name);
    for (char* p = label; *p != '\0'; ++p) {
      if (*p >= 'a' && *p <= 'z') {
        *p = static_cast<char>(*p - 'a' + 'A');  // panel labels render uppercase
      }
    }
  }

  TextProgram view;
  std::snprintf(view.current, sizeof(view.current), "%s", label);
  if (text::fits_static(label)) {
    view.mode = TextMode::Fixed;
    text::draw_text_program(frame, view, 0);
  } else {
    // Long registry names auto-marquee inside the menu (RTL, heritage speed).
    view.mode = TextMode::Marquee;
    view.rtl = true;
    view.speed_ms_per_px = 110;
    text::draw_text_program(frame, view, now_ms - 0);
  }

  // Selection marker: blinking '>' column to the left of short labels.
  const bool blink_on = ((now_ms / 450) & 1u) == 0u;
  if (blink_on) {
    const int16_t width = static_cast<int16_t>(text::string_width_px(label));
    if (width <= kMatrixWidth - 12) {
      const int16_t cx = static_cast<int16_t>((kMatrixWidth - width) / 2);
      frame.set(static_cast<int16_t>(cx - 4), 4, kWhite);
      frame.set(static_cast<int16_t>(cx - 3), 5, kWhite);
      frame.set(static_cast<int16_t>(cx - 5), 3, kWhite);
    }
  }

  // Position dots on the bottom row.
  if (menu.screen() == MenuScreen::Root) {
    for (uint8_t i = 0; i < kMenuRootItems; ++i) {
      const int16_t x = static_cast<int16_t>(2 + i * 4);
      frame.set(x, kMatrixHeight - 1, (i == menu.root_index()) ? kWhite : kDimGray);
    }
  } else {
    uint8_t count = 0;
    if (menu.screen() == MenuScreen::Animations) {
      count = kAnimationCount;
    } else if (menu.screen() == MenuScreen::Expressions) {
      count = kExpressionCount;
    } else if (menu.screen() == MenuScreen::Games) {
      count = kGameCount;
    }
    if (count > 0) {
      // Progress bar proportional to the list position (bounded to the width).
      const int16_t fill = static_cast<int16_t>(1 + (kMatrixWidth - 2) * menu.list_index() / count);
      for (int16_t x = 1; x <= fill && x < kMatrixWidth - 1; ++x) {
        frame.set(x, kMatrixHeight - 1, kWhite);
      }
    }
  }
}

// LED-L009 visualizer: one bar per column, rainbow hue by column, height by
// band level; peak pixel brighter.
void render_spectrum(FrameBuffer& frame, const SpectrumInput& spectrum, uint32_t now_ms) {
  frame.clear();
  if (!spectrum.active(now_ms)) {
    return;  // expired silence renders blank (fail-safe)
  }
  for (uint8_t x = 0; x < kSpectrumBands; ++x) {
    const uint8_t level = spectrum.band(x, now_ms);
    const uint8_t height = static_cast<uint8_t>(static_cast<uint16_t>(level) * kMatrixHeight /
                                                255u);
    const Rgb color = hsv2rgb(static_cast<uint8_t>(80 + x * 4), 255, 255);
    for (uint8_t i = 0; i < height; ++i) {
      frame.set(static_cast<int16_t>(x), static_cast<int16_t>(kMatrixHeight - 1 - i), color);
    }
  }
}

}  // namespace

// ---- LED-L010 tail pass (ADR-LED-L010 D3) ----
// Determines what the 1D surface shows for the current program. The tail is a
// semantic surface: scene/animation programs follow their commanded
// ZoneTarget, and the integrated semantics (timer progress, notification
// accent, expression wash, spectrum bass) drive it exactly like the accepted
// border-pulse styling drives the matrix. Blank = safe default everywhere.
namespace {

void render_timer_tail(TailFramebuffer& tail, const CountdownTimer& timer, uint32_t now_ms) {
  // Live progress drain: remaining/duration, green → orange → red, pulsing at
  // finished (same palette as the countdown_progress animation).
  const uint32_t remaining = timer.remaining_ms(now_ms);
  const uint32_t duration = timer.duration_ms();
  uint8_t frac = 0;
  if (duration > 0) {
    frac = static_cast<uint8_t>((static_cast<uint64_t>(remaining) * 255u) / duration);
  }
  Rgb fg{60, 230, 90};
  if (frac < 100) {
    fg = Rgb{230, 60, 50};
  } else if (frac < 150) {
    fg = Rgb{240, 160, 40};
  }
  if (timer.state() == CountdownTimer::State::Finished && (now_ms / 250) % 2 == 0) {
    tail.clear();
    return;
  }
  effects::tail_progress_core(tail, frac, fg, Rgb{12, 30, 16});
}

void render_text_tail(TailFramebuffer& tail, const RenderInput& input) {
  tail.clear();
  if (input.text == nullptr) return;
  const TextProgram& t = *input.text;
  const uint32_t elapsed = input.now_ms - input.started_ms;
  const Rgb color = text::text_palette_color(t.tail_color);
  switch (t.tail_mode) {
    case TextTailMode::Off: return;
    case TextTailMode::Solid: tail.fill(color); return;
    case TextTailMode::RainbowFlow:
      for (uint16_t i = 0; i < tail.length(); ++i) {
        tail.set(i, hsv2rgb(static_cast<uint8_t>(i * 3u + elapsed / 30u), 230, 160));
      }
      return;
    case TextTailMode::Pulse: {
      const uint8_t phase = static_cast<uint8_t>((elapsed % 1400u) * 256u / 1400u);
      const uint8_t scale = static_cast<uint8_t>(35u + (sin_u8(phase) * 175u) / 255u);
      tail.fill(Rgb{static_cast<uint8_t>(color.r * scale / 255u),
                    static_cast<uint8_t>(color.g * scale / 255u),
                    static_cast<uint8_t>(color.b * scale / 255u)});
      return;
    }
    case TextTailMode::Progress: {
      uint32_t total = t.duration_ms;
      const TextMode mode = effective_text_mode(t);
      if (total == 0 && mode == TextMode::Marquee) {
        total = text::scroll_span_px(t.current) * t.speed_ms_per_px;
      } else if (total == 0 && mode == TextMode::Words) {
        total = text::words_total_ms(t);
      }
      if (total == 0) return;  // static text has no invented progress
      const uint32_t position = t.duration_ms ? (elapsed > total ? total : elapsed)
                                            : elapsed % total;
      effects::tail_progress_core(tail, static_cast<uint8_t>(
          (static_cast<uint64_t>(position) * 255u) / total), color, kRgbBlack);
      return;
    }
    case TextTailMode::TextSynced: {
      uint32_t boundary = 0;
      if (effective_text_mode(t) == TextMode::Words) boundary = t.word_dwell_ms;
      else if (effective_text_mode(t) == TextMode::Marquee)
        boundary = text::scroll_span_px(t.current) * t.speed_ms_per_px;
      else if (t.duration_ms != 0) boundary = t.duration_ms;
      if (boundary == 0) return;
      const uint32_t age = elapsed % boundary;
      if (age >= 350u) return;
      const uint16_t count = tail.length();
      const uint16_t center = static_cast<uint16_t>((static_cast<uint64_t>(age) * count) / 350u);
      for (uint16_t i = center; i < count && i < center + 5u; ++i) tail.set(i, color);
      return;
    }
    case TextTailMode::Custom:
      if (input.pixel != nullptr) input.pixel->render_tail(tail);
      return;
  }
}

void render_expression_tail(TailFramebuffer& tail, const effects::ExpressionVisual& visual,
                            uint32_t started_ms, uint32_t now_ms) {
  const uint32_t elapsed = now_ms - started_ms;
  switch (visual.kind) {
    case effects::ExpressionVisual::Kind::Rest:
      tail.clear();  // idle/sleeping: dark rope
      break;
    case effects::ExpressionVisual::Kind::BorderPulse:
      effects::tail_pulse_core(tail, elapsed, visual.hue, 30, 9);
      break;
    case effects::ExpressionVisual::Kind::StaticBorder:
      tail.fill(hsv2rgb(visual.hue, 255, 200));  // error: solid red rope
      break;
    case effects::ExpressionVisual::Kind::Wave:
    case effects::ExpressionVisual::Kind::Plasma:
      effects::tail_pulse_core(tail, elapsed, visual.hue_offset, 20, 36);  // slow wash
      break;
  }
}

void render_spectrum_tail(TailFramebuffer& tail, const SpectrumInput& spectrum,
                          uint32_t now_ms) {
  tail.clear();
  if (!spectrum.active(now_ms)) {
    return;
  }
  // Bass-band pulse: the low end of the feed drives a warm rope wash.
  uint32_t bass = 0;
  for (uint8_t x = 0; x < 10; ++x) {
    bass += spectrum.band(x, now_ms);
  }
  bass /= 10u;
  const uint8_t val = static_cast<uint8_t>(30u + (bass * 225u) / 255u);
  tail.fill(hsv2rgb(static_cast<uint8_t>(20 + bass / 4), 240, val));
}

void render_tail_pass(TailFramebuffer& tail, const RenderInput& input) {
  switch (input.kind) {
    case ModeKind::Scene:
    case ModeKind::Animation: {
      const effects::TailRenderer r = effects::tail_animation_by_name(input.name);
      if (r != nullptr && input.target != ZoneTarget::Matrix) {
        r(tail, input.now_ms - input.started_ms);
      } else {
        tail.clear();  // matrix-target program or unknown name: dark rope
      }
      break;
    }
    case ModeKind::Expression: {
      const effects::ExpressionVisual* visual =
          effects::expression_visual_by_name(input.name);
      if (visual != nullptr) {
        render_expression_tail(tail, *visual, input.started_ms, input.now_ms);
      } else {
        tail.clear();
      }
      break;
    }
    case ModeKind::Timer:
      if (input.timer != nullptr) {
        render_timer_tail(tail, *input.timer, input.now_ms);
      } else {
        tail.clear();
      }
      break;
    case ModeKind::Text:
      render_text_tail(tail, input);
      break;
    case ModeKind::Notification:
      if (input.notify != nullptr && input.notify->active) {
        if (input.notify->high_priority) {
          effects::tail_chase_core(tail, input.now_ms - input.started_ms, 0);  // red chase
        } else {
          tail.fill(hsv2rgb(140, 200, 120));  // calm static accent
        }
      } else {
        tail.clear();
      }
      break;
    case ModeKind::Spectrum:
      if (input.spectrum != nullptr) {
        render_spectrum_tail(tail, *input.spectrum, input.now_ms);
      } else {
        tail.clear();
      }
      break;
    case ModeKind::Game:
      // M-L010-1 (repair cycle 1): the rope is cleared FIRST, then the game
      // may draw its own lane. Matrix-only games inherit the base-class
      // no-op render_tail, so the rope now renders the safe dark state while
      // they run instead of freezing on the previous program's last frame
      // (tail games clear again internally — idempotent).
      tail.clear();
      if (input.game != nullptr) {
        input.game->render_tail(tail, input.now_ms);
      }
      break;
    default:
      tail.clear();  // idle/text/menu/standby/paint: dark rope
      break;
  }
}

}  // namespace

void render(FrameBuffer& frame, TailFramebuffer* tail, const RenderInput& input) {
  frame.set_frame_id(input.frame_id);
  if (tail != nullptr) tail->set_frame_id(input.frame_id);
  if (input.kind == ModeKind::Paint) {
    if (input.pixel != nullptr) {
      input.pixel->render(frame, tail);
    } else {
      frame.clear();
      if (tail != nullptr) tail->clear();
    }
    return;
  }
  if (tail != nullptr && input.target == ZoneTarget::Both &&
      (input.kind == ModeKind::Scene || input.kind == ModeKind::Animation) &&
      effects::render_combined(input.name, frame, *tail,
                               input.now_ms - input.started_ms)) {
    return;
  }
  // Matrix surface: exactly the accepted per-kind selection. When the commanded
  // target is Tail-only, the matrix renders the safe blank (the program chose
  // the rope); Both renders the both-capable matrix counterpart.
  if (input.target == ZoneTarget::Tail &&
      (input.kind == ModeKind::Scene || input.kind == ModeKind::Animation)) {
    frame.clear();  // tail-only program: matrix dark (API rejects this when tail is disabled)
  } else {
    switch (input.kind) {
    case ModeKind::Idle:
      frame.clear();  // safe blank exactly as accepted (§11 CLEAR / boot)
      break;
    case ModeKind::Text:
      if (input.text != nullptr) {
        render_text(frame, *input.text, input.started_ms, input.now_ms);
      } else {
        frame.clear();  // malformed: text mode without a program → blank
      }
      break;
    case ModeKind::Scene:
    case ModeKind::Animation: {
      // §7: animation-class entries are usable as scenes and animations; the
      // same renderer realizes both kinds (frozen 8 + LED-L009 + LED-L010
      // additions). Unknown names render blank (fail closed).
      const effects::AnimationRenderer r = effects::animation_by_name(input.name);
      if (r != nullptr) {
        r(frame, input.now_ms - input.started_ms);
      } else {
        frame.clear();  // unknown registry name (impossible via API): blank
      }
      break;
    }
    case ModeKind::Expression: {
      const effects::ExpressionVisual* visual =
          effects::expression_visual_by_name(input.name);
      if (visual != nullptr) {
        render_expression(frame, *visual, input.started_ms, input.now_ms);
      } else {
        frame.clear();  // unknown expression (impossible via API): blank
      }
      break;
    }
    case ModeKind::Timer:
      if (input.timer != nullptr) {
        render_timer(frame, *input.timer, input.now_ms);
      } else {
        frame.clear();  // fail closed without the subsystem view
      }
      break;
    case ModeKind::Menu:
      if (input.menu != nullptr && input.menu->active()) {
        render_menu(frame, *input.menu, input.now_ms);
      } else {
        frame.clear();
      }
      break;
    case ModeKind::Game:
      if (input.game != nullptr) {
        input.game->render(frame, input.now_ms);
      } else {
        frame.clear();
      }
      break;
    case ModeKind::Notification:
      if (input.notify != nullptr && input.notify->active) {
        render_notification(frame, *input.notify, input.started_ms, input.now_ms);
      } else {
        frame.clear();
      }
      break;
    case ModeKind::Spectrum:
      if (input.spectrum != nullptr) {
        render_spectrum(frame, *input.spectrum, input.now_ms);
      } else {
        frame.clear();
      }
      break;
    case ModeKind::Standby:
      frame.clear();  // display-off: blank panel, network preserved
      break;
    case ModeKind::Paint:
      // Diagnostic capability without a renderer — fail closed visually.
      frame.clear();
      break;
    default:
      frame.clear();  // out-of-range kind: blank
      break;
    }
  }

  // Tail surface (skipped entirely when disabled — nullptr).
  if (tail != nullptr) {
    render_tail_pass(*tail, input);
  }
}

void render(FrameBuffer& frame, const RenderInput& input) {
  render(frame, nullptr, input);
}

bool program_is_time_varying(const RenderInput& input) {
  switch (input.kind) {
    case ModeKind::Idle:
    case ModeKind::Standby:
    case ModeKind::Paint:
      return false;
    case ModeKind::Text: {
      if (input.text == nullptr) {
        return false;
      }
      const TextProgram& t = *input.text;
      const TextMode em = effective_text_mode(t);
      if (em == TextMode::Marquee || em == TextMode::Words) {
        return true;
      }
      // Fixed: animated only under a rainbow style.
      return t.style == TextStyle::RainbowText || t.style == TextStyle::RainbowBg ||
             t.tail_mode == TextTailMode::RainbowFlow || t.tail_mode == TextTailMode::Pulse ||
             t.tail_mode == TextTailMode::Progress || t.tail_mode == TextTailMode::TextSynced;
    }
    case ModeKind::Scene:
    case ModeKind::Animation:
      return effects::animation_is_time_varying(input.name);
    case ModeKind::Expression: {
      const effects::ExpressionVisual* visual =
          effects::expression_visual_by_name(input.name);
      return visual != nullptr && visual->animated;
    }
    case ModeKind::Timer:
      return input.timer != nullptr &&
             (input.timer->state() == CountdownTimer::State::Running ||
              input.timer->state() == CountdownTimer::State::Finished);
    case ModeKind::Menu:
      return input.menu != nullptr && input.menu->active();
    case ModeKind::Game:
      return input.game != nullptr;
    case ModeKind::Notification:
      return input.notify != nullptr && input.notify->active &&
             !text::fits_static(input.notify->text);  // marquee notification
    case ModeKind::Spectrum:
      return input.spectrum != nullptr && input.spectrum->active(input.now_ms);
    default:
      return false;
  }
}

bool text_program_complete(const TextProgram& text, uint32_t now_ms, bool frame_shown) {
  if (text.current[0] == '\0') {
    return false;  // no program
  }
  const uint32_t elapsed = now_ms - text.started_ms;  // wrap-safe
  switch (effective_text_mode(text)) {
    case TextMode::Fixed:
      return frame_shown;  // complete when actually displayed once
    case TextMode::Marquee: {
      const uint32_t cycle_ms = text.speed_ms_per_px * text::scroll_span_px(text.current);
      return elapsed >= cycle_ms;  // one full wrap traversal completed
    }
    case TextMode::Words:
      // One-shot: complete after the last word's dwell. Looping programs
      // never complete (continuous repetition by definition).
      return !text.loop && elapsed >= text::words_total_ms(text);
  }
  return false;
}

bool text_frame_content_changed(const TextProgram& text, uint32_t now_ms,
                                uint32_t last_shown_ms) {
  // Rainbow styles drift with elapsed time in every mode — always re-render.
  if (text.style == TextStyle::RainbowText || text.style == TextStyle::RainbowBg) {
    return true;
  }
  const TextMode em = effective_text_mode(text);
  if (em == TextMode::Marquee) {
    if (text.current[0] == '\0') {
      return false;  // nothing to scroll
    }
    uint32_t speed = text.speed_ms_per_px == 0 ? 1u : text.speed_ms_per_px;
    const int16_t width = static_cast<int16_t>(text::string_width_px(text.current));
    const int16_t span = static_cast<int16_t>(width + kMatrixWidth);
    if (span <= 0) {
      return false;
    }
    const uint32_t elapsed_now = now_ms - text.started_ms;      // wrap-safe
    const uint32_t elapsed_shown = last_shown_ms - text.started_ms;
    const int16_t off_now =
        static_cast<int16_t>((elapsed_now / speed) % static_cast<uint32_t>(span));
    const int16_t off_shown =
        static_cast<int16_t>((elapsed_shown / speed) % static_cast<uint32_t>(span));
    return off_now != off_shown;
  }
  if (em == TextMode::Words) {
    // Flat words: the frame changes only when the active word (or the one-shot
    // completed blank) changes — once per dwell, not once per throttle frame.
    // Program-relative, exactly like the renderer's word index.
    const uint32_t dwell = text.word_dwell_ms == 0 ? 2000u : text.word_dwell_ms;
    return ((now_ms - text.started_ms) / dwell) != ((last_shown_ms - text.started_ms) / dwell);
  }
  return false;  // Fixed with a flat style never changes
}

}  // namespace renderer
}  // namespace ledwall
