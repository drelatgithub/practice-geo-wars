#ifndef PGW_VISUAL_VISUAL_COMMON_HPP
#define PGW_VISUAL_VISUAL_COMMON_HPP

#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

namespace pgw {

#ifdef NDEBUG
constexpr bool visual_debug = false;
#else
constexpr bool visual_debug = true;
#endif


constexpr const char* window_title = "Geometry Wars";
constexpr const char* app_name     = "GeoWars";


// Validation layer
const std::vector< const char* > default_validation_layers {
    "VK_LAYER_KHRONOS_validation"
};
constexpr bool enable_validation_layer = visual_debug;

} // namespace pgw

#endif
