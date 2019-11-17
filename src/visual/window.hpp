#ifndef PGW_VISUAL_WINDOW_HPP
#define PGW_VISUAL_WINDOW_HPP

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility> // move

#include "visual-common.hpp"

constexpr const char* window_title = "Geometry Wars";
constexpr const char* app_name     = "GeoWars";

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
        const auto create_instance = [](VkInstance* p_instance) {
            // App info
            VkApplicationInfo app_info {};
            app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            app_info.pApplicationName = app_name;
            app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
            app_info.pEngineName = "No Engine";
            app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
            app_info.apiVersion = VK_API_VERSION_1_0;

            // Create info
            VkInstanceCreateInfo create_info {};
            create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            create_info.pApplicationInfo = &app_info;

            std::uint32_t glfwExtensionCount = 0;
            const char** glfwExtensions;

            glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

            create_info.enabledExtensionCount = glfwExtensionCount;
            create_info.ppEnabledExtensionNames = glfwExtensions;
            create_info.enabledLayerCount = 0;

            // Create instance
            if (vkCreateInstance(&create_info, nullptr, p_instance) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create instance.");
            }
        };

        const auto pick_physical_device = [](const VkInstance& instance) {
            VkPhysicalDevice physical_device = VK_NULL_HANDLE;

            // Find physical devices
            std::uint32_t device_count = 0;
            vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
            if (device_count == 0) {
                throw std::runtime_error("Failed to find GPUs with Vulkan support.");
            }

            // Find suitable physical devices
            const auto is_device_suitable = [](VkPhysicalDevice device) {
                return true;
            };
            std::vector<VkPhysicalDevice> devices(device_count);
            vkEnumeratePhysicalDevices(instance, &device_count, devices.data());
            for(const auto& d : devices) {
                if(is_device_suitable(d)) {
                    physical_device = d;
                    break;
                }
            }
            if(physical_device == VK_NULL_HANDLE) {
                throw std::runtime_error("Failed to find a suitable GPU.");
            }

            return physical_device;
        };

        create_instance(&instance_);
        physical_device_ = pick_physical_device(instance_);
    }

    void vulkan_destroy_() {
        vkDestroyInstance(instance_, nullptr);
    }

    // State variables
    GLFWwindow* window_ = nullptr;
    VkInstance instance_;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;

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
#endif
