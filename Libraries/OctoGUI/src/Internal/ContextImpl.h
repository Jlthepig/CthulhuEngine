#pragma once

#include "OctoGui/Context.h"

#include "Systems/EventSystem.h"
#include "Systems/InteractionSystem.h"
#include "Systems/LayoutSystem.h"
#include "Systems/PaintSystem.h"
#include "Systems/StyleSystem.h"
#include "Systems/TextSystem.h"
#include "Systems/WidgetSystem.h"

#include "Internal/IconAtlas.h"

namespace octogui {

struct Context::Impl {
  InputState input;
  Tree tree;

  LayoutSystem layout;
  InteractionSystem interaction;
  PaintSystem paint;
  StyleSystem styles;
  TextSystem text;
  IconAtlas icons;
  WidgetSystem widgets;
  EventSystem events;

  NodeHandle root{};

  u64 frameCounter = 0;
};

} // namespace octogui