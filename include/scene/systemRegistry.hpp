#pragma once

#include <flecs.h>
namespace Cthulhu
{
class Engine;
}

namespace Cthulhu::Scene
{
struct GameplaySystem
{
};

// register core systems to flecs world
void RegisterCoreSystems(flecs::world &world, Cthulhu::Engine *engineContext);
} // namespace Cthulhu::Scene