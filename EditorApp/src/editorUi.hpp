#pragma once

#include <filesystem>

#include "OctoGui/Context.h"
#include "OctoGui_OpenGL.h"

struct GLFWwindow;

namespace Cthulhu::Editor
{
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

        private:
            GLFWwindow* window = nullptr;
            octogui::Context ctx;
            octogui::OpenGLBackend renderer;
            bool mouseOverUi = false;
            bool initialized = false;

    };
}