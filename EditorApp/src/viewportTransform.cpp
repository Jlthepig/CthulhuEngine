#include <algorithm>
#include <cmath>

#include "engine.hpp"
#include "log_utils.hpp"
#include "scene.hpp"
#include "transformUtils.hpp"
#include "viewportPanel.hpp"

namespace Cthulhu::Editor
{
bool ViewportPanel::prepareDragParents(Engine &engine)
{
    auto *scene = engine.getActiveScene();
    if (!scene)
    {
        return false;
    }
    drag.inverseParents.assign(drag.values.size(), glm::dmat3(1.0));
    drag.followsSelection.assign(drag.values.size(), 0);

    for (std::size_t i = 0; i < drag.values.size(); ++i)
    {
        if (!scene->isEntityAlive(drag.values[i].id))
        {
            return false;
        }

        const auto parentId = scene->getParent(drag.values[i].id);
        auto ancestor = parentId;
        while (ancestor)
        {
            const auto found = std::lower_bound(drag.values.begin(),
                                                drag.values.end(),
                                                *ancestor,
                                                [](const Scribe::TransformValue &value, Scene::EntityId id) {
                                                    return value.id.high < id.high ||
                                                           (value.id.high == id.high && value.id.low < id.low);
                                                });

            if (found != drag.values.end() && found->id == *ancestor)
            {
                drag.followsSelection[i] = 1;
                break;
            }

            ancestor = scene->getParent(*ancestor);
        }

        if (drag.followsSelection[i] || !parentId)
        {
            continue;
        }

        const auto parent = scene->findEntity(*parentId);
        if (!parent || !parent->has<Scene::TransformComponent>())
        {
            return false;
        }

        const glm::dmat3 linear(Scene::getWorldTransform(*parent).matrix);
        const double volume = glm::length(linear[0]) * glm::length(linear[1]) * glm::length(linear[2]);
        const double determinant = glm::determinant(linear);

        if (!std::isfinite(volume) || !std::isfinite(determinant) || volume == 0.0 ||
            std::abs(determinant) <= volume * 1.0e-10)
        {
            KalaHeaders::KalaLog::Log::Print("GIZMO PARENT TRANSFORM IS SINGULAR OR ILL-CONDITIONED",
                                             "Gizmo",
                                             KalaHeaders::KalaLog::LogType::LOG_WARNING);
            return false;
        }

        const glm::dmat3 inverse = glm::inverse(linear);
        for (int column = 0; column < 3; ++column)
        {
            for (int row = 0; row < 3; ++row)
            {
                if (!std::isfinite(inverse[column][row]))
                {
                    return false;
                }
            }
        }

        drag.inverseParents[i] = inverse;
    }

    return true;
}
} // namespace Cthulhu::Editor
