#ifndef PGW_VISUAL_VK_INSTANCE_UTILS_HPP
#define PGW_VISUAL_VK_INSTANCE_UTILS_HPP

#include <algorithm> // clamp
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include "visual-common.hpp"
#include "visual/shaders/shaders.hpp"

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
// Caller must ensure that the vector is not empty.
inline auto choose_swap_surface_format(const std::vector< VkSurfaceFormatKHR >& fms) {
    for(const auto& fm : fms) {
        if(fm.format == VK_FORMAT_B8G8R8A8_UNORM && fm.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return fm;
        }
    }
    return fms[0];
}
inline auto choose_swap_present_mode(const std::vector< VkPresentModeKHR >& pms) {
    for(const auto& pm : pms) {
        if(pm == VK_PRESENT_MODE_MAILBOX_KHR) {
            return pm;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR; // Default option
}
inline auto choose_swap_extent(
    const VkSurfaceCapabilitiesKHR& capabilities,
    std::uint32_t                   width,
    std::uint32_t                   height
) {
    if(capabilities.currentExtent.width != std::numeric_limits< std::uint32_t >::max()) {
        return capabilities.currentExtent;
    } else {
        return VkExtent2D {
            std::clamp(width,  capabilities.minImageExtent.width,  capabilities.maxImageExtent.width ),
            std::clamp(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
        };
    }
}
inline auto create_swap_chain(
    VkPhysicalDevice phy_dev,
    VkSurfaceKHR     surface,
    VkDevice         dev,
    std::uint32_t    width,
    std::uint32_t    height
) {
    VkSwapchainKHR sc;
    std::vector< VkImage > sc_images;

    const auto sc_support = query_swap_chain_support(phy_dev, surface);

    const auto fm = choose_swap_surface_format(sc_support.formats);
    const auto pm = choose_swap_present_mode(sc_support.present_modes);
    const auto extent = choose_swap_extent(sc_support.capabilities, width, height);

    auto image_cnt = sc_support.capabilities.minImageCount + 1;
    if(sc_support.capabilities.maxImageCount > 0 && image_cnt > sc_support.capabilities.maxImageCount) {
        image_cnt = sc_support.capabilities.maxImageCount;
    }

    // Create info for swap chain
    VkSwapchainCreateInfoKHR ci {};
    ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface = surface;
    ci.minImageCount = image_cnt;
    ci.imageFormat = fm.format;
    ci.imageColorSpace = fm.colorSpace;
    ci.imageExtent = extent;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    const auto indices = find_queue_families(phy_dev, surface);
    std::uint32_t queue_family_indices[] {
        indices.graphics_family.value(),
        indices.present_family.value()
    };
    if(indices.graphics_family == indices.present_family) {
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ci.queueFamilyIndexCount = 0; // Optional
        ci.pQueueFamilyIndices = nullptr; // Optional
    } else {
        ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        ci.queueFamilyIndexCount = 2;
        ci.pQueueFamilyIndices = queue_family_indices;
    }

    ci.preTransform = sc_support.capabilities.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = pm;
    ci.clipped = VK_TRUE;
    ci.oldSwapchain = VK_NULL_HANDLE;

    // Create the swap chain
    if(vkCreateSwapchainKHR(dev, &ci, nullptr, &sc) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swap chain.");
    }

    // Retrieve images
    vkGetSwapchainImagesKHR(dev, sc, &image_cnt, nullptr);
    sc_images.resize(image_cnt);
    vkGetSwapchainImagesKHR(dev, sc, &image_cnt, sc_images.data());

    return std::tuple(
        sc,
        sc_images,
        fm.format,
        extent
    );
}

// Image views
//-----------------------------------------------------------------------------
inline auto create_image_views(
    VkDevice                      dev,
    const std::vector< VkImage >& images,
    VkFormat                      format
) {
    const auto num_images = images.size();

    std::vector< VkImageView > image_views(num_images);

    for (std::size_t i = 0; i < num_images; ++i) {
        VkImageViewCreateInfo ci {};
        ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ci.image = images[i];
        ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ci.format = format;
        ci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        ci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        ci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        ci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ci.subresourceRange.baseMipLevel = 0;
        ci.subresourceRange.levelCount = 1;
        ci.subresourceRange.baseArrayLayer = 0;
        ci.subresourceRange.layerCount = 1;

        if (vkCreateImageView(dev, &ci, nullptr, &image_views[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image views.");
        }
    }

    return image_views;
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

inline bool is_physical_device_suitable(
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


// Render passes
//-----------------------------------------------------------------------------
inline auto create_render_pass(
    VkDevice dev,
    VkFormat swap_chain_image_format
) {
    VkRenderPass render_pass;

    VkAttachmentDescription color_attachment {};
    color_attachment.format = swap_chain_image_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref {};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    VkSubpassDependency dep {};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rp_ci {};
    rp_ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp_ci.attachmentCount = 1;
    rp_ci.pAttachments = &color_attachment;
    rp_ci.subpassCount = 1;
    rp_ci.pSubpasses = &subpass;
    rp_ci.dependencyCount = 1;
    rp_ci.pDependencies = &dep;

    if(vkCreateRenderPass(dev, &rp_ci, nullptr, &render_pass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass.");
    }

    return render_pass;
}


// Graphics pipelines
//-----------------------------------------------------------------------------
inline auto create_shader_module(
    VkDevice                          dev,
    const std::vector<unsigned char>& code
) {
    VkShaderModule res;

    VkShaderModuleCreateInfo ci {};
    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = code.size();
    ci.pCode = reinterpret_cast<const std::uint32_t*>(code.data());

    if(vkCreateShaderModule(dev, &ci, nullptr, &res) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module.");
    }

    return res;
}
inline auto create_graphics_pipeline(
    VkDevice     dev,
    VkExtent2D   swap_chain_extent,
    VkRenderPass render_pass,
    VkVertexInputBindingDescription
                 vi_binding_desc,
    std::array< VkVertexInputAttributeDescription, 2 >
                 vi_attr_desc
) {
    VkPipelineLayout pipeline_layout;
    VkPipeline       graphics_pipeline;

    const auto vert_sm = create_shader_module(dev, vertex_shader::shader);
    const auto frag_sm = create_shader_module(dev, fragment_shader::shader);

    VkPipelineShaderStageCreateInfo vert_ci {};
    vert_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_ci.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_ci.module = vert_sm;
    vert_ci.pName = "main";

    VkPipelineShaderStageCreateInfo frag_ci {};
    frag_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_ci.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_ci.module = frag_sm;
    frag_ci.pName = "main";

    VkPipelineShaderStageCreateInfo ss_ci[] {
        vert_ci,
        frag_ci
    };

    VkPipelineVertexInputStateCreateInfo vertex_input_ci {};
    vertex_input_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_ci.vertexBindingDescriptionCount = 1;
    vertex_input_ci.pVertexBindingDescriptions = &vi_binding_desc;
    vertex_input_ci.vertexAttributeDescriptionCount = std::size(vi_attr_desc);
    vertex_input_ci.pVertexAttributeDescriptions = vi_attr_desc.data();

    VkPipelineInputAssemblyStateCreateInfo input_asm_ci {};
    input_asm_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_asm_ci.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_asm_ci.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = swap_chain_extent.width;
    viewport.height = swap_chain_extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor {};
    scissor.offset = {0, 0};
    scissor.extent = swap_chain_extent;

    VkPipelineViewportStateCreateInfo vp_ci {};
    vp_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp_ci.viewportCount = 1;
    vp_ci.pViewports = &viewport;
    vp_ci.scissorCount = 1;
    vp_ci.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer_ci {};
    rasterizer_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer_ci.depthClampEnable = VK_FALSE;
    rasterizer_ci.rasterizerDiscardEnable = VK_FALSE;
    rasterizer_ci.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer_ci.lineWidth = 1.0f;
    rasterizer_ci.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer_ci.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer_ci.depthBiasEnable = VK_FALSE;
    rasterizer_ci.depthBiasConstantFactor = 0.0f;
    rasterizer_ci.depthBiasClamp = 0.0f;
    rasterizer_ci.depthBiasSlopeFactor = 0.0f;

    VkPipelineMultisampleStateCreateInfo multisample_ci {};
    multisample_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_ci.sampleShadingEnable = VK_FALSE;
    multisample_ci.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisample_ci.minSampleShading = 1.0f;
    multisample_ci.pSampleMask = nullptr;
    multisample_ci.alphaToCoverageEnable = VK_FALSE;
    multisample_ci.alphaToOneEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState cba_state {};
    cba_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    cba_state.blendEnable = VK_FALSE;
    cba_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    cba_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    cba_state.colorBlendOp = VK_BLEND_OP_ADD;
    cba_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    cba_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    cba_state.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo cb_ci {};
    cb_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb_ci.logicOpEnable = VK_FALSE;
    cb_ci.logicOp = VK_LOGIC_OP_COPY;
    cb_ci.attachmentCount = 1;
    cb_ci.pAttachments = &cba_state;
    cb_ci.blendConstants[0] = 0.0f;
    cb_ci.blendConstants[1] = 0.0f;
    cb_ci.blendConstants[2] = 0.0f;
    cb_ci.blendConstants[3] = 0.0f;

    VkDynamicState dynamic_states[] {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_LINE_WIDTH
    };
    VkPipelineDynamicStateCreateInfo ds_ci {};
    ds_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    ds_ci.dynamicStateCount = 2;
    ds_ci.pDynamicStates = dynamic_states;

    VkPipelineLayoutCreateInfo pl_ci {};
    pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl_ci.setLayoutCount = 0;
    pl_ci.pSetLayouts = nullptr;
    pl_ci.pushConstantRangeCount = 0;
    pl_ci.pPushConstantRanges = nullptr;

    if(vkCreatePipelineLayout(dev, &pl_ci, nullptr, &pipeline_layout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout.");
    }

    VkGraphicsPipelineCreateInfo pipeline_ci {};
    pipeline_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_ci.stageCount = 2;
    pipeline_ci.pStages = ss_ci;
    pipeline_ci.pVertexInputState = &vertex_input_ci;
    pipeline_ci.pInputAssemblyState = &input_asm_ci;
    pipeline_ci.pViewportState = &vp_ci;
    pipeline_ci.pRasterizationState = &rasterizer_ci;
    pipeline_ci.pMultisampleState = &multisample_ci;
    pipeline_ci.pDepthStencilState = nullptr;
    pipeline_ci.pColorBlendState = &cb_ci;
    pipeline_ci.pDynamicState = nullptr; // &ds_ci;
    pipeline_ci.layout = pipeline_layout;
    pipeline_ci.renderPass = render_pass;
    pipeline_ci.subpass = 0;
    pipeline_ci.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_ci.basePipelineIndex = -1;

    if(vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipeline_ci, nullptr, &graphics_pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline.");
    }

    vkDestroyShaderModule(dev, frag_sm, nullptr);
    vkDestroyShaderModule(dev, vert_sm, nullptr);

    return std::tuple(pipeline_layout, graphics_pipeline);
}


// Framebuffers
//-----------------------------------------------------------------------------
inline auto create_framebuffers(
    VkDevice     dev,
    const std::vector< VkImageView >& swap_chain_image_views,
    VkExtent2D   swap_chain_extent,
    VkRenderPass render_pass
) {
    std::vector< VkFramebuffer > swap_chain_framebuffers(swap_chain_image_views.size());

    for(size_t i = 0; i < swap_chain_image_views.size(); ++i) {
        VkImageView attachments[] {
            swap_chain_image_views[i]
        };

        VkFramebufferCreateInfo ci {};
        ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        ci.renderPass = render_pass;
        ci.attachmentCount = 1;
        ci.pAttachments = attachments;
        ci.width = swap_chain_extent.width;
        ci.height = swap_chain_extent.height;
        ci.layers = 1;

        if(vkCreateFramebuffer(dev, &ci, nullptr, &swap_chain_framebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer.");
        }
    }

    return swap_chain_framebuffers;
}


// Vertex buffer
//-----------------------------------------------------------------------------

inline std::uint32_t find_memory_type(
    VkPhysicalDevice      phy_dev,
    std::uint32_t         type_filter,
    VkMemoryPropertyFlags prop_f
) {
    VkPhysicalDeviceMemoryProperties mem_prop;
    vkGetPhysicalDeviceMemoryProperties(phy_dev, &mem_prop);

    for(std::uint32_t i = 0; i < mem_prop.memoryTypeCount; ++i) {
        if(type_filter & (1 << i) && (mem_prop.memoryTypes[i].propertyFlags & prop_f) == prop_f) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find a suitable memory type.");
}

template< typename VType >
inline auto create_vertex_buffer(
    VkPhysicalDevice phy_dev,
    VkDevice         device,
    const std::vector< VType >& vertex_data
) {
    VkBuffer       vertex_buffer;
    VkDeviceMemory vertex_buffer_memory;

    // Create vertex buffer
    VkBufferCreateInfo buf_ci {};
    buf_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf_ci.size = vertex_data.size() * sizeof(VType);
    buf_ci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    buf_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if(vkCreateBuffer(device, &buf_ci, nullptr, &vertex_buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create vertex buffer.");
    }

    // Allocate vertex buffer memory
    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(device, vertex_buffer, &mem_req);

    VkMemoryAllocateInfo alloc_info {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_req.size;
    alloc_info.memoryTypeIndex = find_memory_type(
        phy_dev,
        mem_req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    if(vkAllocateMemory(device, &alloc_info, nullptr, &vertex_buffer_memory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate vertex buffer memory.");
    }

    vkBindBufferMemory(device, vertex_buffer, vertex_buffer_memory, 0);

    // Copy vertex data
    void* data;
    vkMapMemory(device, vertex_buffer_memory, 0, buf_ci.size, 0, &data);
    std::memcpy(data, vertex_data.data(), buf_ci.size);
    vkUnmapMemory(device, vertex_buffer_memory);

    return std::tuple(
        vertex_buffer,
        vertex_buffer_memory
    );
}


// Command pool and buffers
//-----------------------------------------------------------------------------
inline auto create_command_pool(
    VkDevice         dev,
    VkPhysicalDevice phys_dev,
    VkSurfaceKHR     surface
) {
    VkCommandPool command_pool;

    const auto qf_indices = find_queue_families(phys_dev, surface);

    VkCommandPoolCreateInfo ci {};
    ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    ci.queueFamilyIndex = qf_indices.graphics_family.value();
    ci.flags = 0;

    if(vkCreateCommandPool(dev, &ci, nullptr, &command_pool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool.");
    }

    return command_pool;
}

inline auto create_command_buffers(
    VkDevice      dev,
    VkExtent2D    swap_chain_extent,
    VkRenderPass  render_pass,
    VkPipeline    graphics_pipeline,
    const std::vector< VkFramebuffer >& swap_chain_framebuffers,
    VkCommandPool command_pool,
    VkBuffer      vertex_buffer,
    std::size_t   num_vertices
) {
    std::vector< VkCommandBuffer > command_buffers(swap_chain_framebuffers.size());

    VkCommandBufferAllocateInfo ai {};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = command_pool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = command_buffers.size();

    if(vkAllocateCommandBuffers(dev, &ai, command_buffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers.");
    }

    for(size_t i = 0; i < command_buffers.size(); ++i) {
        // Starting command buffer recording
        VkCommandBufferBeginInfo cb_bi {};
        cb_bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cb_bi.flags = 0;
        cb_bi.pInheritanceInfo = nullptr;

        if(vkBeginCommandBuffer(command_buffers[i], &cb_bi) != VK_SUCCESS) {
            throw std::runtime_error("Failed to begin recording command buffer.");
        }

        VkRenderPassBeginInfo rp_bi {};
        rp_bi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rp_bi.renderPass = render_pass;
        rp_bi.framebuffer = swap_chain_framebuffers[i];
        rp_bi.renderArea.offset = {0, 0};
        rp_bi.renderArea.extent = swap_chain_extent;

        VkClearValue clear_color = {0.0f, 0.0f, 0.0f, 1.0f};
        rp_bi.clearValueCount = 1;
        rp_bi.pClearValues = &clear_color;
        vkCmdBeginRenderPass(command_buffers[i], &rp_bi, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);

        VkBuffer vertex_buffers[] { vertex_buffer };
        VkDeviceSize offsets[] { 0 };
        vkCmdBindVertexBuffers(command_buffers[i], 0, 1, vertex_buffers, offsets);

        vkCmdDraw(command_buffers[i], num_vertices, 1, 0, 0);

        // End command buffer recording
        vkCmdEndRenderPass(command_buffers[i]);
        if(vkEndCommandBuffer(command_buffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to record command buffer.");
        }
    }

    return command_buffers;
}


// Command pool and buffers
//-----------------------------------------------------------------------------
template< std::size_t max_frames >
inline auto create_sync_objs(
    VkDevice dev,
    const std::vector< VkImage > swap_chain_images
) {
    std::array< VkSemaphore, max_frames > image_available_semaphores;
    std::array< VkSemaphore, max_frames > render_finished_semaphores;
    std::array< VkFence, max_frames > in_flight_fences;
    std::vector< VkFence > images_in_flight(swap_chain_images.size());

    VkSemaphoreCreateInfo ci {};
    ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_ci {};
    fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for(std::size_t i = 0; i < max_frames; ++i) {
        if(
            vkCreateSemaphore(dev, &ci, nullptr, &image_available_semaphores[i]) != VK_SUCCESS
            || vkCreateSemaphore(dev, &ci, nullptr, &render_finished_semaphores[i]) != VK_SUCCESS
            || vkCreateFence(dev, &fence_ci, nullptr, &in_flight_fences[i]) != VK_SUCCESS
        ) {
            throw std::runtime_error("Failed to create synchronization objects for a frame.");
        }
    }

    return std::tuple(
        image_available_semaphores,
        render_finished_semaphores,
        in_flight_fences,
        images_in_flight
    );
}


} // namespace vk_util
} // namespace pgw

#endif
