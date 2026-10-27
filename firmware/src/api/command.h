#pragma once

#include <cstddef>
#include <cstdint>

#include "api/registry.h"
#include "core/brightness.h"
#include "core/mode_machine.h"
#include "text/text_style.h"

namespace ledwall {

// Text Studio semantic values live in ledwall::text; the wire-facing API model
// uses them directly (re-exported here for readability).
using text::TextStyle;
using text::kDefaultTextBg;
using text::kDefaultTextFg;

// Text protocol cap (frozen spec §6): 128 characters + NUL. Defined here —
// the text program model is API-owned and public_state consumes it.
constexpr size_t kTextMaxLen = 128 + 1;

// Typed error vocabulary (frozen spec §9). Wire names are the exact strings.
// LED-L009 adds UnknownGame for the games registry.
enum class ErrorCode : uint8_t {
  None = 0,
  TextTooWide,
  UnknownScene,
  UnknownAnimation,
  UnknownExpression,
  InvalidParameter,
  UnsupportedCapability,
  LedUnavailable,
  InternalError,
  UnknownGame,
};
constexpr uint8_t kErrorCodeCount = 10;
const char* error_code_name(ErrorCode code);

constexpr uint32_t kTextProtocolCap = 128;              // frozen spec §6
constexpr uint32_t kScrollSpeedMinMsPerPx = 40;         // frozen spec §6
constexpr uint32_t kScrollSpeedMaxMsPerPx = 500;        // frozen spec §6
constexpr uint32_t kScrollSpeedDefaultMsPerPx = 140;    // heritage default (§6)
constexpr uint32_t kWordDwellMinMs = 200;               // LED-L009 word-by-word bounds
constexpr uint32_t kWordDwellMaxMs = 60000;
constexpr uint32_t kWordDwellDefaultMs = 2000;          // brief example: "2 seconds per word"

// §4 text {current, scrolling} + render parameters. Content is kept per slot
// (base vs transient) so expiry returns the reported text to the base program
// without mutation — selection happens at state-build time.
// LED-L007: started_ms stamps the program's activation for program-relative
// render timing; done_reported is render-side bookkeeping for the §9
// text.done event (one completion per program activation; the base and
// transient slots are tracked independently). Neither field is public wire
// state.
// LED-L009 Text Studio: mode (fixed / marquee / words), marquee direction
// (RTL default = the normal reading direction; heritage LTR preserved as the
// explicit reverse), loop (marquee loops by construction; words loop vs
// one-shot), word dwell, semantic style + palette indices. The `scrolling`
// flag remains as the legacy-compat projection (mode == Marquee) so accepted
// clients keep their exact wire view.
enum class TextMode : uint8_t { Fixed = 0, Marquee, Words };
constexpr uint8_t kTextModeCount = 3;
constexpr const char* const kTextModeNames[kTextModeCount] = {"fixed", "marquee", "words"};
const char* text_mode_name(TextMode mode);
bool text_mode_from_name(const char* name, TextMode* out);

enum class TextTailMode : uint8_t { Off = 0, Solid, RainbowFlow, Pulse, Progress, TextSynced, Custom };
constexpr const char* const kTextTailModeNames[] = {
    "off", "solid", "rainbow_flow", "pulse", "progress", "text_synced", "custom"};
bool text_tail_mode_from_name(const char* name, TextTailMode* out);

struct TextProgram {
  char current[kTextMaxLen]{};
  bool scrolling{false};       // legacy projection: mode == Marquee
  bool scroll_explicit{false};
  uint32_t speed_ms_per_px{kScrollSpeedDefaultMsPerPx};
  uint32_t started_ms{0};
  bool done_reported{false};
  // LED-L009 Text Studio fields:
  TextMode mode{TextMode::Fixed};
  bool rtl{true};              // true = right→left (normal reading), false = heritage left→right
  bool loop{true};             // words: loop vs one-shot (marquee always loops)
  uint32_t word_dwell_ms{kWordDwellDefaultMs};
  TextStyle style{TextStyle::Solid};
  uint8_t fg_color{kDefaultTextFg};  // palette index (heritage_green default)
  uint8_t bg_color{kDefaultTextBg};  // palette index (black default)
  TextTailMode tail_mode{TextTailMode::Off};
  uint8_t tail_color{kDefaultTextFg};
  uint32_t duration_ms{0};
};

struct TextState {
  TextProgram base;       // applies while baseMode == text
  TextProgram transient;  // applies while a text transient is active
};

// LED-L009 notification payload (semantic mode; transient with automatic
// restoration via the accepted §11 transient semantics).
struct NotifyState {
  char text[kTextMaxLen]{};
  bool high_priority{false};  // true = alert border styling
  bool active{false};         // mirrors a live notification transient
};

constexpr size_t kMaxErrorMessageLen = 128;
constexpr size_t kMaxAvailableListLen = 512;  // LED-L010: 36-name animation registry grew

// LED-L009 visualizer band count: one band per matrix column.
constexpr uint8_t kSpectrumBands = 30;

struct SemanticCommand {
  enum class Capability : uint8_t {
    TextShow = 0,
    TextClear,
    SceneSet,
    AnimationSet,
    ExpressionSet,
    Clear,
    BrightnessSet,
    StateRead,
    // LED-L009 additive capabilities:
    TimerSet,
    TimerStart,
    TimerPause,
    TimerResume,
    TimerReset,
    TimerStop,
    InputSend,
    MenuSet,
    GameSet,
    GameCommand,
    GameDifficulty,  // V-L009-3: engine-enforced easy/normal/hard
    VisualizerStart,
    VisualizerStop,
    VisualizerData,
    Notify,
    StandbySet,
    TailPoint,
    TailPointOff,
  };

  Capability capability{Capability::StateRead};

  // text.show / notify
  char text[kTextMaxLen]{};
  bool has_scroll{false};
  bool scroll{false};
  bool has_speed{false};
  uint32_t speed_ms_per_px{kScrollSpeedDefaultMsPerPx};
  bool has_duration{false};
  uint32_t duration_ms{0};
  // LED-L009 text studio:
  bool has_mode{false};
  TextMode mode{TextMode::Fixed};
  bool has_direction{false};
  bool rtl{true};
  bool has_loop{false};
  bool loop{true};
  bool has_word_dwell{false};
  uint32_t word_dwell_ms{kWordDwellDefaultMs};
  bool has_style{false};
  TextStyle style{TextStyle::Solid};
  bool has_fg_color{false};
  uint8_t fg_color{kDefaultTextFg};
  bool has_bg_color{false};
  uint8_t bg_color{kDefaultTextBg};
  bool has_tail_mode{false};
  TextTailMode tail_mode{TextTailMode::Off};
  bool has_tail_color{false};
  uint8_t tail_color{kDefaultTextFg};

  // scene.set / animation.set / expression.set / game.set
  char name[kMaxProgramNameLen]{};
  // LED-L010: commanded display zone for scene/animation programs
  // ("matrix" | "tail" | "both"; absent = the animation's default).
  bool has_target{false};
  char target[8]{};

  // brightness.set
  bool has_level{false};
  uint32_t level{0};
  bool has_tail_index{false};
  uint16_t tail_index{0};

  // LED-L009:
  bool has_on{false};       // standby.set
  bool on{false};
  bool has_action{false};   // menu.set (enter/exit) / game.command (pause/resume/restart/exit)
  char action[16]{};
  bool has_difficulty{false};  // game.difficulty (easy|normal|hard) — V-L009-3
  char difficulty[8]{};
  bool has_input{false};    // input.send
  char input[16]{};
  bool has_player{false};   // input.send player slot (1|2; default 1) — LED-L010
  uint8_t player{1};
  bool has_players{false};  // game.set player mode (1|2) — LED-L010
  uint8_t players{1};
  uint8_t bands[kSpectrumBands]{};  // visualizer.data
  bool has_priority{false};         // notify
  char priority[8]{};               // "normal" | "high"
};

struct CommandResponse {
  bool ok{false};
  ErrorCode error{ErrorCode::None};  // None iff ok
  char message[kMaxErrorMessageLen]{};
  char available[kMaxAvailableListLen]{};  // populated for unknownX errors
  bool state_changed{false};               // true → broadcast state.changed
  // LED-L007: display.text.show acks report the §6 skippedCharacters count
  // (unsupported characters rendered as space).
  bool has_skipped_characters{false};
  uint8_t skipped_characters{0};
};

// Bounded §9 event channel: state.changed / mode.changed / transient.expired,
// plus the LED-L007 render-completion event text.done and the LED-L009
// timer.done (countdown reached zero).
enum class EventKind : uint8_t {
  StateChanged = 0,
  ModeChanged,
  TransientExpired,
  TextDone,
  TimerDone,
};
constexpr uint8_t kMaxPendingEvents = 8;

struct Event {
  EventKind kind;
};

class EventSink {
 public:
  void push(EventKind kind) {
    if (count_ >= kMaxPendingEvents) {
      return;  // bounded: drop excess (broadcast coalesces anyway)
    }
    items_[count_].kind = kind;
    ++count_;
  }
  uint8_t pending() const { return count_; }
  // Drain up to `max` events; returns how many were taken.
  uint8_t drain(Event* out, uint8_t max) {
    uint8_t taken = 0;
    while (taken < max && count_ > 0) {
      out[taken] = items_[0];
      --count_;
      for (uint8_t i = 0; i < count_; ++i) {
        items_[i] = items_[i + 1];  // shift-left (small bounded array)
      }
      ++taken;
    }
    return taken;
  }

 private:
  Event items_[kMaxPendingEvents]{};
  uint8_t count_{0};
};

// Executes §5 core capability semantics against the accepted LED-L002 mode
// machine and brightness (single monotonic timebase, wrap-safe). Malformed or
// unknown commands mutate nothing and return typed errors (§11).
// LED-L009: the dispatcher additionally coordinates the timer, input, menu,
// games, visualizer, notification and standby subsystems through the
// DeviceServices interface (host-testable with a fake).
class DeviceServices;  // fwd

class CommandDispatcher {
 public:
  void begin(ModeMachine* machine, Brightness* brightness, TextState* text, EventSink* events);
  // LED-L009: optional services hook (set before any extended command is
  // executed; nullptr extended commands fail closed with internalError).
  void set_services(DeviceServices* services);

  CommandResponse execute(const SemanticCommand& cmd, uint32_t now_ms);

 private:
  CommandResponse execute_inner_(const SemanticCommand& cmd, uint32_t now_ms);
  CommandResponse apply_program_(ModeKind kind, const char* name, const SemanticCommand& cmd,
                                 uint32_t now_ms);
  // LED-L010: target-aware application + zone-target resolution for
  // scene/animation programs (validation incl. tail availability).
  CommandResponse apply_program_(ModeKind kind, const char* name, const SemanticCommand& cmd,
                                 uint32_t now_ms, ZoneTarget target);
  CommandResponse resolve_zone_target_(const SemanticCommand& cmd, ZoneTarget* out);
  CommandResponse ok_(bool state_changed);
  CommandResponse error_(ErrorCode code, const char* message, const char* available);

  ModeMachine* machine_{nullptr};
  Brightness* brightness_{nullptr};
  TextState* text_{nullptr};
  EventSink* events_{nullptr};
  DeviceServices* services_{nullptr};
};

}  // namespace ledwall
