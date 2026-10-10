#include <fstream>
#include <string>
#include <vector>

#include "Internal/WidgetMetrics.h"
#include "Systems/TextSystem.h"

namespace octogui {

namespace {

std::vector<u8> readFileBytes(const std::string &path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);

  if (!file) {
    return {};
  }

  const std::streamsize fileSize = file.tellg();

  if (fileSize <= 0) {
    return {};
  }

  file.seekg(0, std::ios::beg);

  std::vector<u8> data(static_cast<usize>(fileSize));

  if (!file.read(reinterpret_cast<char *>(data.data()), fileSize)) {
    return {};
  }

  return data;
}

} // namespace

void TextSystem::setTextureBackend(TextureBackend *backend) noexcept {
  textureBackend = backend;
}

bool TextSystem::loadDefaultFont(std::string_view path, u32 pixelSize) {
  if (!textureBackend) {
    return false;
  }

  std::vector<u8> bytes = readFileBytes(std::string(path));

  if (bytes.empty()) {
    return false;
  }

  return defaultFont.load(bytes.data(), bytes.size(), pixelSize,
                          *textureBackend);
}

Font &TextSystem::font() noexcept { return defaultFont; }

const Font &TextSystem::font() const noexcept { return defaultFont; }

void TextSystem::updateLayoutHints(Tree &tree, NodeHandle root) {
  if (!defaultFont.isLoaded() || !tree.isValid(root)) {
    return;
  }

  std::vector<NodeHandle> stack;
  stack.push_back(root);

  while (!stack.empty()) {
    const NodeHandle node = stack.back();
    stack.pop_back();

    if (!tree.isValid(node)) {
      continue;
    }

    const NodeType type = tree.type(node);

    const bool isSpecialSized =
      type == NodeType::Root || type == NodeType::Checkbox ||
      type == NodeType::RadioButton || type == NodeType::TextInput ||
      type == NodeType::NumericInput || type == NodeType::ComboBox;

    if (!isSpecialSized) {
      const std::string_view nodeText = tree.text(node);

      if (!nodeText.empty()) {
        LayoutStyle *style = tree.layoutStyle(node);

        if (style) {
          const Vec2 textSize = defaultFont.measureText(nodeText);
          const f32 extraWidth = 8.0f;
          const f32 desiredWidth =
              textSize.x + style->padding.horizontal() + extraWidth;
          const f32 desiredHeight = textSize.y + style->padding.vertical();

          bool changed = false;

          if (style->preferredSize.x != desiredWidth) {
            style->preferredSize.x = desiredWidth;
            changed = true;
          }

          if (desiredHeight > style->preferredSize.y) {
            style->preferredSize.y = desiredHeight;
            changed = true;
          }

          if (changed) {
            tree.markLayoutDirty(node);
          }
        }
      }
    }

    if (type == NodeType::Checkbox || type == NodeType::RadioButton) {
      const std::string_view nodeText = tree.text(node);

      f32 textWidth = 0.0f;
      f32 textHeight = 0.0f;

      if (!nodeText.empty()) {
        const Vec2 textSize = defaultFont.measureText(nodeText);

        textWidth = textSize.x;
        textHeight = textSize.y;
      }

      const bool isCheckbox = type == NodeType::Checkbox;
      const f32 boxSize =
          isCheckbox ? detail::CheckboxBoxSize : detail::RadioButtonBoxSize;
      const f32 spacing =
          isCheckbox ? detail::CheckboxSpacing : detail::RadioButtonSpacing;

      LayoutStyle *style = tree.layoutStyle(node);

      if (style) {
        const f32 desiredWidth =
            boxSize + (nodeText.empty() ? 0.0f : spacing + textWidth) +
            style->padding.horizontal();
        const f32 desiredHeight =
            (textHeight > boxSize ? textHeight : boxSize) +
            style->padding.vertical();

        bool changed = false;

        if (style->preferredSize.x != desiredWidth) {
          style->preferredSize.x = desiredWidth;
          changed = true;
        }

        if (desiredHeight > style->preferredSize.y) {
          style->preferredSize.y = desiredHeight;
          changed = true;
        }

        if (changed) {
          tree.markLayoutDirty(node);
        }
      }
    }

    NodeHandle child = tree.firstChild(node);

    while (tree.isValid(child)) {
      stack.push_back(child);
      child = tree.nextSibling(child);
    }
  }
}

} // namespace octogui