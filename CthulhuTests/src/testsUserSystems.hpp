#pragma once

#include "systemRegistry.hpp"
#include "testCommon.hpp"

namespace Cthulhu::Validation
{
inline int validationGameplayTicks = 0;
inline int validationAlwaysTicks = 0;
inline int validationRegistrations = 0;
inline void* validationContext = nullptr;

inline void registerValidationSystems(void* context, flecs::world& world, Engine&)
{
	++validationRegistrations;
	validationContext = context;

	world.system("V_GameplayTicker")
		.kind(flecs::OnUpdate)
		.run([](flecs::iter&) { ++validationGameplayTicks; })
		.add<CS::GameplaySystem>();

	world.system("V_AlwaysTicker").kind(flecs::OnUpdate).run([](flecs::iter&) { ++validationAlwaysTicks; });
}

inline void validateUserSystems(Engine& engine, Results& r)
{
	if (!engine.loadScene(SAVE_PATH))
	{
		check(r, false, "U0 user systems setup");
		return;
	}

	auto tick = [&]() {
		validationGameplayTicks = 0;
		validationAlwaysTicks = 0;
		engine.getActiveScene()->getWorld().progress(0.0f);
	};

	int marker = 0;
	engine.addSystemRegistration(registerValidationSystems, &marker);
	check(r, validationRegistrations == 1 && engine.getActiveScene()->getWorld().lookup("V_GameplayTicker"),
		  "U1 registration runs right away on the active scene");
	check(r, validationContext == &marker, "U1 registration receives its context");

	tick();
	check(r, validationGameplayTicks == 1 && validationAlwaysTicks == 1, "U2 in Play both kinds of system run");

	engine.setWorldMode(WorldMode::Edit);
	tick();
	check(r, validationGameplayTicks == 0 && validationAlwaysTicks == 1, "U2 in Edit only untagged systems run");

	const int before = validationRegistrations;
	check(r, engine.loadScene(SAVE_PATH) && validationRegistrations > before &&
				 engine.getActiveScene()->getWorld().lookup("V_AlwaysTicker"),
		  "U3 every newly loaded scene gets the systems");

	engine.play();
	tick();
	check(r, validationGameplayTicks == 1, "U4 the play copy gets the systems and runs gameplay");

	engine.stop();
	tick();
	check(r, engine.getActiveScene()->getWorld().lookup("V_GameplayTicker") && validationGameplayTicks == 0 &&
				 validationAlwaysTicks == 1,
		  "U4 after Stop the edit scene has the systems with gameplay off");

	engine.setWorldMode(WorldMode::Play);
	engine.loadScene(SAVE_PATH);
}
} // namespace Cthulhu::Validation