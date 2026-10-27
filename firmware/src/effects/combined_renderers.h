#pragma once

#include <cstdint>

#include "render/display_topology.h"
#include "render/framebuffer.h"

namespace ledwall {
namespace effects {

// Returns true when name is a V4.1 two-zone effect. Both surfaces are always
// rewritten from the same program-relative clock; the caller applies output
// limiting only after this logical frame is complete.
bool render_combined(const char* name, FrameBuffer& matrix,
                     TailFramebuffer& tail, uint32_t elapsed_ms);
bool combined_animation_name(const char* name);

}  // namespace effects
}  // namespace ledwall
