#pragma once

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif

#include "GLFW/glfw3.h"

#include "OctoGui/Context.h"

namespace octogui 
{
namespace glfw 
{

void initInput(GLFWwindow* window);
void shutdownInput(GLFWwindow* window);
void updateInput(Context& ctx, GLFWwindow* window);

} // glfw
} // octogui