#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>

#include "assetType.hpp"
#include "audio.hpp"
#include "characterController.hpp"
#include "components.hpp"
#include "engine.hpp"
#include "fileReader.hpp"
#include "scene.hpp"
#include "log_utils.hpp"

namespace Cthulhu::Validation
{
namespace CS = Cthulhu::Scene;
using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

inline constexpr std::string_view TEST_AUDIO = "res://assets/audio/gunshot.wav";

inline constexpr std::string_view SAVE_PATH = "res://.cthulhu/validation.scene";
inline constexpr std::string_view BROKEN_PATH = "res://.cthulhu/validation_broken.scene";

struct Results
{
	int passed = 0;
	int failed = 0;
};

inline void check(Results& r, bool condition, const std::string& name)
{
	if (condition)
	{
		++r.passed;
		Log::Print("PASS  " + name, "Validation", LogType::LOG_SUCCESS);
	}
	else
	{
		++r.failed;
		Log::Print("FAIL  " + name, "Validation", LogType::LOG_ERROR);
	}
}

inline CS::EntityId idOf(flecs::entity e)
{
	const auto* identity = e.try_get<CS::EntityIdentityComponent>();
	return identity ? identity->id : CS::EntityId{};
}

inline uint32_t bodies(Engine& engine) { return engine.getPhysicsWorld().getBodyCount(); }
inline int characters() { return Physics::CharacterController::getLiveCharacterCount(); }
inline size_t sounds() { return Core::Audio::getActiveSoundCount(); }

inline void checkRefInvariant(Engine& engine, Results& r, const std::string& label)
{
	auto* scene = engine.getActiveScene();
	const int meshUsers = scene ? scene->getWorld().count<CS::MeshRuntimeComponent>() : 0;
	const int audioUsers = scene ? scene->getWorld().count<CS::AudioSourceRuntimeComponent>() : 0;

	auto& assets = engine.getAssetManager();
	check(r, assets.getTotalModelRefCount() == static_cast<uint32_t>(meshUsers) &&
			 assets.getTotalAudioClipRefCount() == static_cast<uint32_t>(audioUsers),
		  "F1 asset refs == runtime users (" + label + ")");
}
} // namespace Cthulhu::Validation
