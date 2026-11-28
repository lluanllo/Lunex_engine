#include "stpch.h"

// Vulkan first: the GLFW backend declares stand-in Vulkan types when vulkan.h was not seen
#include "backends/imgui_impl_vulkan.cpp"

#define IMGUI_IMPL_OPENGL_LOADER_GLAD
#include "backends/imgui_impl_opengl3.cpp"
#include "backends/imgui_impl_glfw.cpp"
