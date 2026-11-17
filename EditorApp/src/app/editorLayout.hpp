#pragma once

#include <array>

namespace Cthulhu::Editor
{
struct EditorLayout
{
    // workspace, browser height, hierarchy width, browser folders
    std::array<float, 4> ratios{0.80f, 0.72f, 0.18f, 0.22f};
    bool hierarchy = true;
    bool inspector = true;
    bool browser = true;

    bool operator==(const EditorLayout &) const = default;
};

EditorLayout loadEditorLayout();
void saveEditorLayout(const EditorLayout &layout);
}