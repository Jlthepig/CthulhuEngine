# How to use OctoGUI

This guide goes through a basic setup with GLFW and OpenGL. It happens in
five steps: set things up, build your UI, run it every frame, style it, and
shut it down. Everything is in the `octogui` namespace.

## 1. Setting up

Do your normal window setup first: create the GLFW window, make its OpenGL
context current, and load OpenGL. If you have your own GLFW callbacks, set
them up now too. Then hook up OctoGUI:

```cpp
#include "glad/gl.h"   // or your own loader
#include "GLFW/glfw3.h"
#include "OctoGui_OpenGL.h"
#include "OctoGui_GLFW.h"

using namespace octogui;

// after your own callbacks, so OctoGUI can pass input along to them
glfw::initInput(window);

OpenGLBackend renderer;
if (!renderer.init()) {
    // something went wrong with OpenGL
}

Context ctx;
ctx.setTextureBackend(&renderer); // lets OctoGUI upload its font and icons

(void)ctx.loadDefaultFont("Libraries/OctoGUI/assets/fonts/Roboto-SemiBold.ttf", 18);
(void)ctx.loadDefaultIconAtlas("Libraries/OctoGUI/assets/icons/octogui_icons.png");
ctx.setTheme(darkTheme());
```

The `Context` is your main way of talking to OctoGUI. You create widgets
through it, change them through it, and ask it what the user did.

The font and icon paths are relative to the folder your program runs from,
so change them to match your project.

## 2. Building your UI

You build the UI once, not every frame:

```cpp
NodeHandle root = ctx.ensureRoot();

NodeHandle panel = ctx.createPanel(root);
ctx.layoutStyle(panel).mode = LayoutMode::Vertical;  // stack children top to bottom
ctx.layoutStyle(panel).padding = Padding{16.0f};      // space inside the edges
ctx.layoutStyle(panel).gap = 8.0f;                    // space between children

NodeHandle label  = ctx.createLabel(panel, "Hello");
NodeHandle button = ctx.createButton(panel, "Click me");
```

Hang on to the handles for anything you want to change or check later. If a
widget gets deleted, its handle just stops working, so it's safe to keep old
handles around. `ctx.isValid(handle)` tells you if one is still good.

## 3. Every frame

Do these steps in this order every frame:

```cpp
glfwPollEvents();

glfw::updateInput(ctx, window);   // give OctoGUI this frame's input
ctx.beginFrame(deltaTime);
ctx.layout();                     // work out where everything goes

// react to the user
if (ctx.isClicked(button)) {
    ctx.setText(label, "Clicked!");
}

// or, if you prefer, go through the event queue
UIEvent e;
while (ctx.pollEvent(e)) {
    // ...
}

ctx.paint();                      // turn the UI into a list of things to draw

// draw your game first, then the UI on top
int fbW = 0, fbH = 0;
glfwGetFramebufferSize(window, &fbW, &fbH);
renderer.render(ctx.drawList(), ctx.input().displaySize,
                Vec2{static_cast<f32>(fbW), static_cast<f32>(fbH)});

ctx.endFrame();
glfwSwapBuffers(window);
```

Remember that drawing the UI changes some OpenGL settings: depth testing is
switched off and blending is switched on. Set your own settings back at the
start of each frame.

## 4. Making it look how you want

You can style every widget of one type at once, or just one widget:

```cpp
// every button you create after this gets rounder corners
ctx.defaultStyle(NodeType::Button).cornerRadius = 9.0f;

// just this one button gets the accent colour
ctx.style(button).normal.background  = ctx.theme().accent;
ctx.style(button).hovered.background = ctx.theme().accentHovered;
```

## 5. Shutting down

Shut things down in the reverse order you set them up:

```cpp
renderer.shutdown();
glfw::shutdownInput(window);  // gives your original callbacks back
```

That's everything. For bigger examples, have a look at the `examples` folder
in the OctoGUI repo.
