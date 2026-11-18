#pragma once

#include <memory>

#include <glm.hpp>

#include <GLFW/glfw3.h>
#include <glad.h>

using std::make_unique;
using std::unique_ptr;

namespace Cthulhu::Scene
{
class Camera;
}

namespace Cthulhu::Core
{
enum class WindowMode
{
    Windowed,
    Borderless,
    ExclusiveFullscreen
};

struct WindowConfig
{
    glm::vec2 resolution = glm::vec2(1920.0f, 1080.0f);
    WindowMode initialMode = WindowMode::Windowed;
    bool vSync = false;
    bool visible = true;
    glm::ivec2 minimumSize{0, 0};
};

class Window
{
  public:
    Window() = default;
    ~Window();
    Window(const Window &) = delete;
    Window &operator=(const Window &) = delete;

    static Window *createWindow(const WindowConfig &config, const char *windowTitle, const Window *centerOver = nullptr);
    static void destroyWindow(Window *window);
    static void destroyAll();

    [[nodiscard]] bool shouldClose() const;
    GLFWwindow *getWindow() const;
    float getWidth();
    float getHeight();

    void show();
    void focus();
    void setWindowMode(WindowMode newMode);
    void setCamera(Cthulhu::Scene::Camera *cam);

    static void mouse_callback(GLFWwindow *window, double xposIn, double yposIn);
    static void mouse_button_callback(GLFWwindow *window, int button, int action, int mods);

  private:
    void centerOn(const Window &parent);
    GLFWmonitor *getCurrentMonitor() const;

    GLFWwindow *glfWwindow = nullptr;
    Cthulhu::Scene::Camera *camera = nullptr;
    glm::vec2 windowSize = glm::vec2(1920.0f, 1080.0f);
    bool firstMouse = true;
    float lastX = windowSize.x / 2.0f;
    float lastY = windowSize.y / 2.0f;

    WindowMode currentMode = WindowMode::Windowed;
    int cachedX = 0;
    int cachedY = 0;
    int cachedWidth = 0;
    int cachedHeight = 0;
};
} // namespace Cthulhu::Core