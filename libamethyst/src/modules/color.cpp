#include "modules/color.h"

#include "logging/log.h"
#include "utils/packing.h"

#include <algorithm>

namespace Amethyst {

bool gradientEqual(const Gradient &a, const Gradient &b)
{
    return a == b;
}

std::shared_ptr<const Gradient> Gradient::linear(float angle, std::initializer_list<GradientColorStop> stops)
{
    auto grad = std::make_shared<Gradient>();
    grad->type = GradientType::LINEAR;
    grad->angleDegrees = angle;

    uint32_t count = static_cast<uint32_t>(stops.size());
    if (count > MAX_GRADIENT_STOPS) {
        AM_LOG_WARN("Gradient given {} stops, clamping to {}", count, MAX_GRADIENT_STOPS);
        count = MAX_GRADIENT_STOPS;
    }

    uint32_t i = 0;
    for (const GradientColorStop &stop : stops) {
        if (i >= count) {
            break;
        }
        grad->stops[i] = GradientStop{packColor(stop.color), clamp(stop.t, 0.0f, 1.0f)};
        ++i;
    }
    grad->stopCount = count;

    std::sort(grad->stops.begin(), grad->stops.begin() + count,
              [](const GradientStop &lhs, const GradientStop &rhs) { return lhs.t < rhs.t; });

    return grad;
}

} // namespace Amethyst
