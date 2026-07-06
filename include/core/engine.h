#pragma once

#include <memory>
#include <string>
#include <vector>

#include "editorCamera.h"
#include "glm.hpp"
#include "physics.h"
#include "renderer.h"
#include "sceneLoader.h"

struct GLFWwindow;

namespace Cthulhu::Core
{
	class Window;
}

namespace Cthulhu::Scene
{
	class Camera;
	class Scene;
}

namespace Cthulhu
{
	class Engine
	{
	    public:
		using UpdateCallback = void (*)(void* context, float deltaTime);
		using RaycastCallback = void (*)(void* context, const Physics::RaycastHitInfo& hit);

		void init(const char* title, glm::vec2 resolution);
		void loadScene(const std::string& path);
		void setUpdateCallback(UpdateCallback callback, void* context = nullptr);
		void setRaycastCallback(RaycastCallback callback, void* context = nullptr);
		void processFixedUpdate(float fixedDt);
		void run();
		void shutdown();

		Scene::Camera* getCamera();
		Editor::EditorCamera* getEditorCamera() { return editorCamera; }
		void setEditorMode(bool isEditor);
    	bool isInEditorMode() const { return inEditorMode; }
		Core::Window* getWindow() { return window; }
		Rendering::Renderer& getRenderer() { return renderer; }
		Physics::PhysicsWorld& getPhysicsWorld() { return physicsWorld; }
		Scene::Scene& getScene() { return *scene; }

		float getDeltaTime() const { return deltaTime; }

		void triggerRaycastCallback(const Physics::RaycastHitInfo& hit)
		{
			if (raycastCallback)
			{
				raycastCallback(raycastContext, hit);
			}
		}

	    private:
		Rendering::Renderer renderer;
		Physics::PhysicsWorld physicsWorld;

		Scene::Camera* camera = nullptr;
		Editor::EditorCamera* editorCamera = nullptr;

		Core::Window* window = nullptr;
		GLFWwindow* glfwWindow = nullptr;

		std::unique_ptr<Scene::Scene> scene;
		Scene::SceneData sceneData;
		std::vector<Rendering::Renderable> frameRenderables;

		UpdateCallback updateCallback = nullptr;
		void* updateContext = nullptr;

		RaycastCallback raycastCallback = nullptr;
		void* raycastContext = nullptr;

		float deltaTime = 0.0f;
		double lastFrame = 0.0;
		double fpsTimer = 0.0;

		int frameCount = 0;
		float displayFPS = 0.0f;

		bool inEditorMode = true;
	};
}