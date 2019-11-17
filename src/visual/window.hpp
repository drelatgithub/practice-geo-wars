#ifndef PGW_VISUAL_WINDOW_HPP
#define PGW_VISUAL_WINDOW_HPP

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility> // move

#include "visual-common.hpp"
#include "vk-instance-utils.hpp"

namespace pgw {

class Window {
public:

    Window(int width, int height) :
        width_(width),
        height_(height)
    {
        glfw_init_();
        vulkan_init_();
    }

    ~Window() {
        vulkan_destroy_();
        glfw_destroy_();
    }

    void mainloop() {
        while(!glfwWindowShouldClose(window_)) {
            glfwPollEvents();
        }
    }

private:
    void glfw_init_() {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        window_ = glfwCreateWindow(width_, height_, window_title, nullptr, nullptr);
    }

    void glfw_destroy_() {
        glfwDestroyWindow(window_);

        glfwTerminate();
    }

    void vulkan_init_() {
        vkm::create_instance(&instance_);
        physical_device_ = vkm::pick_physical_device(instance_);
        vkm::create_logical_device(&device_, &graphics_queue_, physical_device_);
    }

    void vulkan_destroy_() {
        vkDestroyDevice(device_, nullptr);
        vkDestroyInstance(instance_, nullptr);
    }

    // Member variables
    GLFWwindow*      window_ = nullptr;

    VkInstance       instance_;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice         device_;
    VkQueue          graphics_queue_;

    int width_;
    int height_;
};

inline void window() {
    Window w(800, 600);

    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    std::cout << extensionCount << " extensions supported" << std::endl;

    glm::mat4 matrix;
    glm::vec4 vec;
    auto test = matrix * vec;

    w.mainloop();
}

} // namespace pgw

#endif
