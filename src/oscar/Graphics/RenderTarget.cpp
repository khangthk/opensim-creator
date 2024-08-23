#include "RenderTarget.h"

#include <oscar/Utils/Assertions.h>


void osc::RenderTarget::validate_or_throw() const
{
    const auto required_dimensions = dimensions();
    const auto required_aa_level = depth_ ? depth_->buffer.anti_aliasing_level() : colors_.front().color_buffer.anti_aliasing_level();

    // validate other buffers against the first
    for (const auto& color_attachment : colors_) {
        OSC_ASSERT_ALWAYS(color_attachment.color_buffer.dimensions() == required_dimensions);
        OSC_ASSERT_ALWAYS(color_attachment.color_buffer.anti_aliasing_level() == required_aa_level);
    }
    if (depth_) {
        OSC_ASSERT_ALWAYS(depth_->buffer.dimensions() == required_dimensions);
        OSC_ASSERT_ALWAYS(depth_->buffer.anti_aliasing_level() == required_aa_level);
    }
}
