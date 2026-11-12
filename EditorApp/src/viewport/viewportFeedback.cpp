#include <algorithm>
#include <cmath>
#include <limits>

#include <gtc/constants.hpp>

#include "viewportPanel.hpp"

namespace Cthulhu::Editor
{
namespace
{
struct GuidePainter
{
    octogui::DrawList &draw;
    glm::dmat4 viewProjection;
    octogui::Rect viewport;

    void line(const glm::dvec3 &from, const glm::dvec3 &to, octogui::Color color, float width) const
    {
        glm::dvec4 a = viewProjection * glm::dvec4(from, 1.0);
        glm::dvec4 b = viewProjection * glm::dvec4(to, 1.0);
        for (int component = 0; component < 4; ++component)
        {
            if (!std::isfinite(a[component]) || !std::isfinite(b[component]))
            {
                return;
            }
        }
        constexpr glm::dvec4 planes[]{{1, 0, 0, 1},
                                      {-1, 0, 0, 1},
                                      {0, 1, 0, 1},
                                      {0, -1, 0, 1},
                                      {0, 0, 1, 1},
                                      {0, 0, -1, 1}};
        for (const auto &plane : planes)
        {
            const double da = glm::dot(a, plane);
            const double db = glm::dot(b, plane);
            if (da < 0.0 && db < 0.0)
            {
                return;
            }
            if ((da < 0.0) != (db < 0.0))
            {
                const glm::dvec4 intersection = a + (b - a) * (da / (da - db));
                if (da < 0.0)
                {
                    a = intersection;
                }
                else
                {
                    b = intersection;
                }
            }
        }
        if (a.w <= 0.0 || b.w <= 0.0)
        {
            return;
        }
        const glm::dvec2 start{viewport.x + (a.x / a.w + 1.0) * 0.5 * viewport.w,
                               viewport.y + (1.0 - a.y / a.w) * 0.5 * viewport.h};
        const glm::dvec2 end{viewport.x + (b.x / b.w + 1.0) * 0.5 * viewport.w,
                             viewport.y + (1.0 - b.y / b.w) * 0.5 * viewport.h};
        for (int axis = 0; axis < 2; ++axis)
        {
            if (!std::isfinite(start[axis]) || !std::isfinite(end[axis]) ||
                std::abs(start[axis]) > std::numeric_limits<float>::max() ||
                std::abs(end[axis]) > std::numeric_limits<float>::max())
            {
                return;
            }
        }
        const octogui::Vec2 p{static_cast<float>(start.x), static_cast<float>(start.y)};
        const octogui::Vec2 q{static_cast<float>(end.x), static_cast<float>(end.y)};
        draw.addLine(p, q, color, width);
    }
};
} // namespace

void ViewportPanel::drawGizmoFeedback(octogui::Context &ctx) const
{
    if (!gizmoDragActive() || (drag.mode != GizmoMode::Move && drag.mode != GizmoMode::Rotate))
    {
        return;
    }
    const auto &style = gizmoSystem.style;
    const auto &viewport = drag.viewport;
    if (viewport.w <= 0.0f || viewport.h <= 0.0f || !std::isfinite(style.axisLengthPixels) ||
        style.axisLengthPixels <= 0.0f || !std::isfinite(drag.frame.axisLengthWorld) ||
        drag.frame.axisLengthWorld <= 0.0f)
    {
        return;
    }
    const float top = ctx.isValid(header) ? std::max(viewport.y, ctx.tree().rect(header).y + ctx.tree().rect(header).h)
                                          : viewport.y;
    const octogui::Rect clip{viewport.x, top, viewport.w, viewport.y + viewport.h - top};
    if (clip.h <= 0.0f)
    {
        return;
    }

    auto &draw = ctx.drawList();
    const GuidePainter painter{draw, glm::dmat4(drag.viewProjection), viewport};
    const glm::dvec3 pivot(drag.frame.pivot);
    const auto white = octogui::Color::fromRGBA8(255, 255, 255, 190);
    const float width = style.guideWidthPixels;
    draw.pushClip(clip);

    if (drag.mode == GizmoMode::Move)
    {
        painter.line(pivot, pivot + drag.translation, white, width);
    }
    else if (drag.mode == GizmoMode::Rotate)
    {
        const auto &rotate = drag.rotate;
        const double radius = static_cast<double>(drag.frame.axisLengthWorld) * style.rotationRadius;
        const double sweep = std::fmod(rotate.appliedAngle, glm::two_pi<double>());
        const int segments = static_cast<int>(std::clamp(std::ceil(std::abs(sweep) * 16.0), 1.0, 128.0));
        const auto radial = [&](double angle) {
            return rotate.u * std::cos(angle) + rotate.v * std::sin(angle);
        };
        for (int i = 0; i < segments; ++i)
        {
            const double a = rotate.startAngle + sweep * i / segments;
            const double b = rotate.startAngle + sweep * (i + 1) / segments;
            painter.line(pivot + radial(a) * radius, pivot + radial(b) * radius, white, width);
        }
        painter.line(pivot, pivot + radial(rotate.startAngle + sweep) * radius, white, width);
    }
    draw.popClip();
}
} // namespace Cthulhu::Editor
