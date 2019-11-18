#ifndef PGW_VISUAL_VK_INSTANCE_UTILS_HPP
#define PGW_VISUAL_VK_INSTANCE_UTILS_HPP

#include <cstdint>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include "visual-common.hpp"

// This file provides helper functions for generating vk instances and related
// configurations.

namespace pgw {
namespace vk_util {

inline const std::vector< const char* > device_extensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};


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
    VkInstanceCreateInfo ci {};
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &app_info;

    std::uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;

    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    ci.enabledExtensionCount = glfwExtensionCount;
    ci.ppEnabledExtensionNames = glfwExtensions;

    if(enable_validation_layer) {
        ci.enabledLayerCount = default_validation_layers.size();
        ci.ppEnabledLayerNames = default_validation_layers.data();
    } else {
        ci.enabledLayerCount = 0;
    }

    // Create instance
    if (vkCreateInstance(&ci, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create instance.");
    }

    return instance;
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

// Queue families
//-----------------------------------------------------------------------------
struct QueueFamilyIndices {
    std::optional<std::uint32_t> graphics_family;
    std::optional<std::uint32_t> present_family;

    // Check if all the queue families are available
    bool is_complete() const {
        return graphics_family.has_value()
            && present_family.has_value();
    }
};
inline auto find_queue_families(
    VkPhysicalDevice phy_dev,
    VkSurfaceKHR     surface
) {
    QueueFamilyIndices indices;

    std::uint32_t qf_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phy_dev, &qf_count, nullptr);

    std::vector< VkQueueFamilyProperties > qf(qf_count);
    vkGetPhysicalDeviceQueueFamilyProperties(phy_dev, &qf_count, qf.data());

    for(int i = 0; i < qf.size(); ++i) {
        if(qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphics_family = i;
        }
        {
            VkBool32 present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(phy_dev, i, surface, &present_support);
            if(present_support) {
                indices.present_family = i;
            }
        }
    }

    return indices;
}

// Swap chain support
//-----------------------------------------------------------------------------
struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR          capabilities;
    std::vector< VkSurfaceFormatKHR > formats;
    std::vector< VkPresentModeKHR >   present_modes;
};
inline auto query_swap_chain_support(
    VkPhysicalDevice device,
    VkSurfaceKHR     surface
) {
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    {
        std::uint32_t fm_cnt;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &fm_cnt, nullptr);
        if(fm_cnt) {
            details.formats.resize(fm_cnt);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &fm_cnt, details.formats.data());
        }
    }

    {
        std::uint32_t pm_cnt;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &pm_cnt, nullptr);
        if(pm_cnt) {
            details.present_modes.resize(pm_cnt);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &pm_cnt, details.present_modes.data());
        }
    }

    return details;
}

// Physical devices
//-----------------------------------------------------------------------------
inline auto check_physical_device_extension_support(VkPhysicalDevice device) {
    std::uint32_t ext_count;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &ext_count, nullptr);
    std::vector< VkExtensionProperties > available_exts(ext_count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &ext_count, available_exts.data());

    std::set< std::string > unsupported_exts(device_extensions.begin(), device_extensions.end());
    for(const auto& ext : available_exts) {
        unsupported_exts.erase(ext.extensionName);
    }

    return unsupported_exts.empty();
}

inline auto is_physical_device_suitable(
    VkPhysicalDevice device,
    VkSurfaceKHR     surface
) {
    const auto indices = find_queue_families(device, surface);

    const bool extensions_supported = check_physical_device_extension_support(device);

    bool swap_chain_adequate = false;
    if(extensions_supported) {
        const auto sc = query_swap_chain_support(device, surface);
        swap_chain_adequate = !sc.formats.empty() && !sc.present_modes.empty();
    }

    return indices.is_complete() && extensions_supported && swap_chain_adequate;
}

inline auto pick_physical_device(
    VkInstance   instance,
    VkSurfaceKHR surface
) {
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
        if(is_physical_device_suitable(d, surface)) {
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
inline auto create_logical_device(
    VkPhysicalDevice phy_dev,
    VkSurfaceKHR     surface
) {
    VkDevice dev;
    VkQueue  graphics_queue;
    VkQueue  present_queue;

    const auto indices = find_queue_families(phy_dev, surface);

    // Create queues
    std::vector< VkDeviceQueueCreateInfo > queue_cis;
    float queue_priority = 1.0f;
    {
        std::set< std::uint32_t > qfs {
            indices.graphics_family.value(),
            indices.present_family.value()
        };

        queue_cis.reserve(qfs.size());
        for(auto qf : qfs) {
            VkDeviceQueueCreateInfo queue_ci {};
            queue_ci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_ci.queueFamilyIndex = qf;
            queue_ci.queueCount = 1;
            queue_ci.pQueuePriorities = &queue_priority;
            queue_cis.push_back(queue_ci);
        }
    }

    // Device features
    VkPhysicalDeviceFeatures device_features {};

    // Create info for logical device
    VkDeviceCreateInfo ci {};
    ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.pQueueCreateInfos = queue_cis.data();
    ci.queueCreateInfoCount = queue_cis.size();
    ci.pEnabledFeatures = &device_features;

    ci.enabledExtensionCount = device_extensions.size();
    ci.ppEnabledExtensionNames = device_extensions.data();

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
    vkGetDeviceQueue(dev, indices.present_family.value(),  0, &present_queue);

    return std::tuple(dev, graphics_queue, present_queue);
}


} // namespace vk_util
} // namespace pgw

#endif
