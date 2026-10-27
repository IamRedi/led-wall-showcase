#include <cstring>

#include "effects/expression_map.h"
#include "test_framework.h"

// §8 expression mapping (data-driven). Expected values are transcribed from
// the frozen spec examples (thinking→slow wave, alert→border alert palette,
// sleeping→dim idle) and the ADR-007 vocabulary — never from the
// implementation under test.

void test_expression_table_covers_frozen_vocabulary_exactly() {
  // Registry order and byte-equality with the frozen 8-intent vocabulary.
  constexpr const char* kFrozen[] = {
      "idle", "listening", "thinking", "speaking", "happy", "alert", "error", "sleeping",
  };
  for (const char* name : kFrozen) {
    const ledwall::effects::ExpressionVisual* v =
        ledwall::effects::expression_visual_by_name(name);
    CHECK(v != nullptr);
    if (v != nullptr) {
      CHECK_STR_EQ(v->expression, name);
    }
  }
  // Unknown intents: rejected upstream by the dispatcher; the mapping returns
  // nothing (fail closed).
  CHECK(ledwall::effects::expression_visual_by_name("curious") == nullptr);
  CHECK(ledwall::effects::expression_visual_by_name("") == nullptr);
  CHECK(ledwall::effects::expression_visual_by_name(nullptr) == nullptr);
  CHECK(ledwall::effects::expression_visual_by_name("Alert") == nullptr);  // case-sensitive
}

void test_expression_frozen_examples_mapping() {
  // sleeping → dim idle (rest/blank family): not animated.
  const ledwall::effects::ExpressionVisual* sleeping =
      ledwall::effects::expression_visual_by_name("sleeping");
  CHECK(sleeping != nullptr);
  CHECK_EQ_INT(static_cast<int>(sleeping->kind),
               static_cast<int>(ledwall::effects::ExpressionVisual::Kind::Rest));
  CHECK(!sleeping->animated);

  // idle → rest/blank as well.
  const ledwall::effects::ExpressionVisual* idle =
      ledwall::effects::expression_visual_by_name("idle");
  CHECK_EQ_INT(static_cast<int>(idle->kind),
               static_cast<int>(ledwall::effects::ExpressionVisual::Kind::Rest));

  // thinking → slow wave (slower than the heritage demo divisor 80).
  const ledwall::effects::ExpressionVisual* thinking =
      ledwall::effects::expression_visual_by_name("thinking");
  CHECK_EQ_INT(static_cast<int>(thinking->kind),
               static_cast<int>(ledwall::effects::ExpressionVisual::Kind::Wave));
  CHECK(thinking->param_a > 80);  // slower than the registered wave
  CHECK(thinking->animated);

  // alert → border with an alert (red, hue 0) palette, animated pulse.
  const ledwall::effects::ExpressionVisual* alert =
      ledwall::effects::expression_visual_by_name("alert");
  CHECK_EQ_INT(static_cast<int>(alert->kind),
               static_cast<int>(ledwall::effects::ExpressionVisual::Kind::BorderPulse));
  CHECK_EQ_INT(alert->hue, 0);  // alert palette = red
  CHECK(alert->animated);

  // error: static red border (distinct from the pulsing alert).
  const ledwall::effects::ExpressionVisual* error =
      ledwall::effects::expression_visual_by_name("error");
  CHECK_EQ_INT(static_cast<int>(error->kind),
               static_cast<int>(ledwall::effects::ExpressionVisual::Kind::StaticBorder));
  CHECK_EQ_INT(error->hue, 0);
  CHECK(!error->animated);

  // speaking: distinct wave realization (faster, warm hue offset).
  const ledwall::effects::ExpressionVisual* speaking =
      ledwall::effects::expression_visual_by_name("speaking");
  CHECK_EQ_INT(static_cast<int>(speaking->kind),
               static_cast<int>(ledwall::effects::ExpressionVisual::Kind::Wave));
  CHECK(speaking->param_a < 80);   // faster than the registered wave
  CHECK_EQ_INT(speaking->hue_offset, 48);
}
