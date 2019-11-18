#ifndef PGW_VISUAL_VK_INSTANCE_UTILS_HPP
#define PGW_VISUAL_VK_INSTANCE_UTILS_HPP

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <tuple>

#include "visual-common.hpp"

// This file provides helper functions for generating vk instances and related
// configurations.

namespace pgw {
namespace vk_util {

// VkInstance creation
//-----------------------------------------------------------------------------
// The instance should be properly destroyed after use.
inline auto create_instance() {
    VkInstance instance;

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
    if (vkCreateInstance(&create_info, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create instance.");
    }

    return instance;
}

// Queue families
//-----------------------------------------------------------------------------
struct QueueFamilyIndices {
    std::optional<std::uint32_t> graphics_family;

    // Check if all the queue families are available
    bool is_complete() const {
        return graphics_family.has_value();
    }
};
inline auto find_queue_families(VkPhysicalDevice phy_dev) {
    QueueFamilyIndices indices;

    std::uint32_t qf_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phy_dev, &qf_count, nullptr);

    std::vector< VkQueueFamilyProperties > qf(qf_count);
    vkGetPhysicalDeviceQueueFamilyProperties(phy_dev, &qf_count, qf.data());

    for(int i = 0; i < qf.size(); ++i) {
        if(qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphics_family = i;
        }
    }

    return indices;
}

// Physical devices
//-----------------------------------------------------------------------------
inline auto is_physical_device_suitable(VkPhysicalDevice device) {
    auto indices = find_queue_families(device);

    return indices.is_complete();
}

inline auto pick_physical_device(VkInstance instance) {
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;

    // Find physical devices
    std::uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
    if (device_count == 0) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support.");
    }

    // Find suitable physical devices
    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, devices.data());
    for(const auto& d : devices) {
        if(is_physical_device_suitable(d)) {
            physical_device = d;
            break;
        }
    }
    if(physical_device == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to find a suitable GPU.");
    }

    return physical_device;
}

// Logical devices
//-----------------------------------------------------------------------------
// The logical device should be properly destroyed after use.
inline auto create_logical_device(VkPhysicalDevice phy_dev) {
    VkDevice dev;
    VkQueue  graphics_queue;

    auto indices = find_queue_families(phy_dev);

    // Create queue
    VkDeviceQueueCreateInfo queue_ci {};
    queue_ci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_ci.queueFamilyIndex = indices.graphics_family.value();
    queue_ci.queueCount = 1;
    float queue_priority = 1.0f;
    queue_ci.pQueuePriorities = &queue_priority;

    // Device features
    VkPhysicalDeviceFeatures device_features {};

    // Create info for logical device
    VkDeviceCreateInfo ci {};
    ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.pQueueCreateInfos = &queue_ci;
    ci.queueCreateInfoCount = 1;
    ci.pEnabledFeatures = &device_features;

    ci.enabledExtensionCount = 0;

    if(enable_validation_layer) {
        ci.enabledLayerCount = static_cast<std::uint32_t>(default_validation_layers.size());
        ci.ppEnabledLayerNames = default_validation_layers.data();
    } else {
        ci.enabledLayerCount = 0;
    }

    // Create logical device
    if(vkCreateDevice(phy_dev, &ci, nullptr, &dev) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device.");
    }

    // Get queue handle
    vkGetDeviceQueue(dev, indices.graphics_family.value(), 0, &graphics_queue);

    return std::tuple(dev, graphics_queue);
}

// Window surfaces
//-----------------------------------------------------------------------------
inline auto create_surface(VkInstance instance, GLFWwindow* window) {
    VkSurfaceKHR surface;

    if(glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface.");
    }

    return surface;
}

} // namespace vk_util
} // namespace pgw

#endif
