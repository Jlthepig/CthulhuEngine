#pragma once

#include "testCommon.hpp"

namespace Cthulhu::Validation
{
inline void validateRuntimeApi(Engine& engine, Results& r)
{
	if (!engine.loadScene(SAVE_PATH))
	{
		check(r, false, "RT0 runtime API setup");
		return;
	}

	auto* scene = engine.getActiveScene();
	auto& world = scene->getWorld();
	auto inSync = [&]() {
		return scene->getEntityCount() == static_cast<size_t>(world.count<CS::EntityIdentityComponent>());
	};

	const auto weaponId = scene->findEntityByName("V_SavedWeapon");
	check(r, weaponId && !scene->findEntityByName("V_NoSuchEntity"), "RT1 findEntityByName finds by name");

	int frame = 0;
	flecs::entity bullet;
	bool duplicateRefused = false;
	auto spawner = world.system("V_Spawner").kind(flecs::OnUpdate).run([&](flecs::iter&) {
		++frame;
		if (frame == 1)
		{
			bullet = scene->createEntity("V_Bullet");
			bullet.set(CS::WeaponComponent{3.0f, 30.0f});
			scene->createEntity("V_Flash").destruct(); // spawned and gone in the same frame
		}
		else if (frame == 2)
		{
			scene->destroyEntity(idOf(bullet));
			duplicateRefused = weaponId && !scene->duplicateEntity(*weaponId);
		}
	});

	world.progress(0.0f);
	const auto bulletId = scene->findEntityByName("V_Bullet");
	const auto bulletEntity = bulletId ? scene->findEntity(*bulletId) : std::nullopt;
	const auto* weapon = bulletEntity ? bulletEntity->try_get<CS::WeaponComponent>() : nullptr;
	check(r, weapon && weapon->fireRate == 3.0f && bulletEntity->has<CS::WeaponRuntimeComponent>(),
		  "RT2 spawning inside a system works (components arrive after the frame)");
	check(r, !scene->findEntityByName("V_Flash") && inSync(),
		  "RT3 created and destroyed in the same frame leaves nothing behind");

	world.progress(0.0f);
	check(r, bulletId && !scene->isEntityAlive(*bulletId) && inSync(), "RT4 destroying inside a system works");
	check(r, duplicateRefused, "RT5 duplicate refuses to run inside a system");

	spawner.destruct();

	flecs::entity plain = scene->createEntity("V_PlainFlecs");
	const CS::EntityId plainId = idOf(plain);
	plain.destruct();
	check(r, !scene->isEntityAlive(plainId) && inSync(), "RT6 plain flecs destruct keeps the scene in sync");

	engine.loadScene(SAVE_PATH);
}
} // namespace Cthulhu::Validation