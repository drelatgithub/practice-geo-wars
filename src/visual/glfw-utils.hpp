#ifndef PGW_VISUAL_GLFW_UTILS_HPP
#define PGW_VISUAL_GLFW_UTILS_HPP

#include <cstddef>
#include <functional>

#include "visual-common.hpp"

namespace pgw {
namespace glfw_util {

// The global GLFW environment guard
// Initializes GLFW when needed, and destroys GLFW when no EnvGuard is present.
//
// As per GLFW's specification, this class can only be used in the "main"
// thread, and is not thread-safe.
class EnvGuard {
public:
    EnvGuard() { init_(); }
    EnvGuard(const EnvGuard& ) { init_(); }
    EnvGuard(      EnvGuard&&) { init_(); }
    ~EnvGuard() { destroy_(); }

private:
    static void init_() {
        if(env_counter_ == 0) glfwInit();
        ++env_counter_;
    }
    static void destroy_() {
        --env_counter_;
        if(env_counter_ == 0) glfwTerminate();
    }

    inline static std::size_t env_counter_ = 0;

};

// Wrapper function for getting window framebuffer size
struct FramebufferSize {
    int width;
    int height;
};
inline auto get_framebuffer_size(GLFWwindow* window) {
    FramebufferSize res;
    glfwGetFramebufferSize(window, &res.width, &res.height);
    return res;
}

} // namespace glfw_util
} // namespace pgw

#endif
