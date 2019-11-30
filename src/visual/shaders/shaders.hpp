#ifndef PGW_VISUAL_SHADERS_SHADERS_HPP
#define PGW_VISUAL_SHADERS_SHADERS_HPP

#include <iterator>
#include <vector>

namespace pgw {

namespace vertex_shader {
#include "shader.vert.spv.hpp"
const std::vector< unsigned char > shader(std::begin(value), std::end(value));
} // namespace vertex_shader

namespace fragment_shader {
#include "shader.frag.spv.hpp"
const std::vector< unsigned char > shader(std::begin(value), std::end(value));
} // namespace fragment_shader

} // namespace pgw

#endif
