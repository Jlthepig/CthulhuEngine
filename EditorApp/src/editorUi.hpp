#pragma once

#include <filesystem>

#include "OctoGui/Context.h"
#include "OctoGui_OpenGL.h"

struct GLFWwindow;

namespace Cthulhu::Editor
{
    struct PixelRect
    {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
    };
    class EditorUi
    {
        public:
            bool init(GLFWwindow* window, const std::filesystem::path &resourceRoot);
            void shutdown();

            void beginFrame(float deltaTime);
            void endFrame();

            [[nodiscard]] octogui::Context &context() noexcept
            {
                return ctx;
            }

            [[nodiscard]] bool wantsMouse() const noexcept
            {
                return mouseOverUi;
            }

            [[nodiscard]] bool wantsKeyboard() const noexcept;

            void setSceneNode(octogui::NodeHandle node) noexcept
            {
                sceneNode = node;
            }

            [[nodiscard]] PixelRect pixelRect(octogui::NodeHandle node) const;

        private:
            GLFWwindow* window = nullptr;
            octogui::Context ctx;
            octogui::OpenGLBackend renderer;
            octogui::NodeHandle sceneNode{};
            bool mouseOverUi = false;
            bool initialized = false;

    };
}