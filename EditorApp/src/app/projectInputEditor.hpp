#pragma once

#include <array>
#include <optional>
#include <vector>

#include "editorDialog.hpp"
#include "projectInput.hpp"

struct GLFWwindow;

namespace Cthulhu::Scribe { class Session; }

namespace Cthulhu::Editor
{
class ProjectInputEditor
{
  public:
    void build(octogui::Context &ctx, octogui::NodeHandle page, octogui::NodeHandle overlay, 
              const Project::ProjectInputMap &input);
    void update(octogui::Context &ctx, GLFWwindow *window, EditorDialog &dialog,
                octogui::NodeHandle activated, octogui::NodeHandle entered, bool canEdit);
    void setEditable(octogui::Context &ctx, bool canEdit);
    void appendControls(std::vector<octogui::NodeHandle> &controls) const;
    [[nodiscard]] bool dirty(const octogui::Context &ctx) const;
    [[nodiscard]] bool renameValid() const { return !invalidRename; }
    [[nodiscard]] bool cancelNameEdit(octogui::Context &ctx);
    [[nodiscard]] std::string apply(octogui::Context &ctx, Scribe::Session &session);
    void deleteSelected(octogui::Context &ctx);
    void showNotice(octogui::Context &ctx, std::string_view text) const;

    [[nodiscard]] bool capturing() const { return captureWindow != nullptr; }
    void finishCapture(octogui::Context &ctx, bool canCapture);
    void cancelCapture();

  private:
    using Handle = octogui::NodeHandle;
    using Context = octogui::Context;
    struct BindingRow {Handle replace{}, remove{}; };
    struct ActionRow
    {
      Project::InputActionId id{};
      Handle label{}, addBinding{}, remove{};
      std::vector<BindingRow> bindings;
    };

    void refresh(Context &ctx);
    void commitName(Context &ctx);
    void startRename(Context &ctx);
    void endRename(Context &ctx);
    void setRenameError(Context &ctx, std::string_view text);
    void focusSelected(Context &ctx, bool binding);
    void startCapture(Context &ctx, GLFWwindow *window, std::optional<std::size_t> index);
    void showCapture(Context &ctx, std::string_view input, std::string_view message, bool error);
    void stopCapture(Context &ctx);
    [[nodiscard]] bool renamePending(const Context &ctx) const;
    [[nodiscard]] std::size_t selectedIndex() const;

    Project::ProjectInputMap saved, draft;
    Project::InputActionId selected{}, lastClicked{};
    std::array<Handle, Project::inputControlCount> assignments{}, noBindings{};
    std::vector<ActionRow> actionRows;
    Handle filter{}, newName{}, addAction{}, newNameError{}, revert{}, notice{};
    Handle actions{}, actionList{}, name{}, nameError{};
    Handle captureBackdrop{}, captureTitle{}, captureInput{}, captureMessage{};
    std::string filterText, rejectedName, renameError;
    float clickAge = 1.0f;
    bool renaming = false;
    bool invalidRename = false;

    inline static ProjectInputEditor *listener = nullptr;
    GLFWwindow *captureWindow = nullptr;
    void (*previousKey)(GLFWwindow *, int, int, int, int) = nullptr;
    void (*previousMouse)(GLFWwindow *, int, int, int) = nullptr;
    void (*previousFocus)(GLFWwindow *, int) = nullptr;
    std::optional<std::size_t> replacing;
    std::optional<Project::InputBinding> captured;
    bool lostFocus = false;
};
} // namespace Cthulhu::Editor