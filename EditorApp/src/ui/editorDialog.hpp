#pragma once

#include <array>
#include <string_view>
#include <span>
#include <string>
#include <vector>

#include "OctoGui/Context.h"

namespace Cthulhu::Editor
{
enum class DialogChoice { None, Primary, Secondary, Cancel };

class EditorDialog
{
  public:
    void build(octogui::Context &ctx, octogui::NodeHandle root, octogui::NodeHandle editorContent);
    void show(octogui::Context &ctx, std::string_view title, std::string_view message, std::string_view primary, std::string_view secondary);
    void setDetails(octogui::Context &ctx, std::span<const std::string> lines);
    void setNotice(octogui::Context &ctx, std::string_view message);
    void setPrimaryEnabled(octogui::Context &ctx, bool enabled);
    void hide(octogui::Context &ctx);
    void setError(octogui::Context &ctx, std::string_view message);

    [[nodiscard]] bool isOpen(const octogui::Context &ctx) const noexcept
    {
        return ctx.isVisible(backdrop);
    }

    [[nodiscard]] DialogChoice pollChoice(octogui::Context &ctx) const;

  private:
    octogui::NodeHandle content{}, backdrop{}, titleLabel{}, messageLabel{}, errorLabel{};
    std::array<octogui::NodeHandle, 3> buttons{};
    octogui::NodeHandle details{}, noticeLabel{}, previousFocus{};
    std::vector<octogui::NodeHandle> detailRows;
};
} // namespace Cthulhu::Editor
