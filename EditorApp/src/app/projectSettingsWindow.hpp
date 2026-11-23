#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

#include "editorUi.hpp"
#include "projectInputEditor.hpp"

namespace Cthulhu
{
class Engine;
}

namespace Cthulhu::Scribe
{
class Session;
struct Result;
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

[[nodiscard]] Scribe::Result pickProjectMainScene(octogui::Context &ctx, const Core::Window &owner, Engine &engine, Scribe::Session &session);
class ProjectSettingsWindow
{
  public:
    [[nodiscard]] bool open(const Core::Window &parent, const std::filesystem::path &resources, const Project::ProjectConfig &config);
    void frame(Engine &engine, Scribe::Session &session);
    void close();

    [[nodiscard]] bool deferQuit();
    [[nodiscard]] bool waitingForQuit() const { return quitDeferred; }

  private:
    void build(const Project::ProjectConfig &config);
    void updateControls(octogui::Context &ctx, GLFWwindow *native, Engine &engine, Scribe::Session &session, bool canEdit);
    void selectPage(std::size_t index);
    void applyDetails(Scribe::Session &session);
    void showMainSceneMessage(std::string_view message);

    Core::Window *window = nullptr;
    std::optional<EditorUi> ui;
    static constexpr std::size_t inputPage = 3;
    std::array<octogui::NodeHandle, 4> tabs{}, pages{};
    std::vector<octogui::NodeHandle> focusOrder;
    ProjectInputEditor inputEditor;
    EditorDialog inputDialog;
    bool closePending = false;
    bool closePrompt = false;
    bool quitDeferred = false;
    octogui::NodeHandle closeButton{}, mainSceneValue{}, chooseMainScene{}, clearMainScene{}, mainSceneNotice{};
    octogui::NodeHandle projectName{}, windowWidth{}, windowHeight{}, applyButton{}, detailsNotice{};
};
} // namespace Cthulhu::Editor