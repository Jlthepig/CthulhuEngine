#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

#include "editorUi.hpp"

namespace Cthulhu
{
class Engine;
}

namespace Cthulhu::Scribe
{
class Session;
}

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
    void frame(Engine &engine, Scribe::Session &session);
    void close();

  private:
    void build(const Project::ProjectConfig &config);
    void selectPage(std::size_t index);
    void applyDetails(Scribe::Session &session);
    void pickMainScene(Engine &engine, Scribe::Session &session);
    void showMainSceneMessage(std::string_view message);

    Core::Window *window = nullptr;
    std::optional<EditorUi> ui;
    std::array<octogui::NodeHandle, 3> tabs{}, pages{};
    octogui::NodeHandle closeButton{}, mainSceneValue{}, chooseMainScene{}, clearMainScene{}, mainSceneNotice{};
    octogui::NodeHandle projectName{}, windowWidth{}, windowHeight{}, applyButton{}, detailsNotice{};
};
} // namespace Cthulhu::Editor