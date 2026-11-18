#pragma once

#include <array>
#include <cstddef>
#include <optional>

#include "editorUi.hpp"

namespace Cthulhu::Core
{
class Window;
}

namespace Cthulhu::Project
{
struct ProjectConfig;
}

namespace Cthulhu::Editor
{
class ProjectSettingsWindow
{
  public:
    [[nodiscard]] bool open(const Core::Window &parent, const std::filesystem::path &resources, const Project::ProjectConfig &config);
    void frame(float deltaTime);
    void close();

  private:
    void build(const Project::ProjectConfig &config);
    void selectPage(std::size_t index);

    Core::Window *window = nullptr;
    std::optional<EditorUi> ui;
    std::array<octogui::NodeHandle, 3> tabs{}, pages{};
    octogui::NodeHandle closeButton{};
};
} // namespace Cthulhu::Editor