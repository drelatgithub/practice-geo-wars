#ifndef PGW_VISUAL_WINDOW_HPP
#define PGW_VISUAL_WINDOW_HPP

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility> // move

#include "glfw-utils.hpp"
#include "visual-common.hpp"
#include "vk-utils.hpp"

namespace pgw {

struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;

    static auto get_binding_desc() {
        VkVertexInputBindingDescription bd {};

        bd.binding = 0;
        bd.stride = sizeof(Vertex);
        bd.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bd;
    }

    static auto get_attr_desc() {
        std::array< VkVertexInputAttributeDescription, 2 > ad {};

        ad[0].binding = 0;
        ad[0].location = 0;
        ad[0].format = VK_FORMAT_R32G32_SFLOAT;
        ad[0].offset = offsetof(Vertex, pos);
        ad[1].binding = 0;
        ad[1].location = 1;
        ad[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        ad[1].offset = offsetof(Vertex, color);
        return ad;
    }
};
inline const std::vector< Vertex > vertices {
    {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, 0.5f }, {0.0f, 1.0f, 0.0f}},
    {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
};

class Window {
public:

    constexpr static int max_frames_in_flight = 2;

    Window(int width, int height) {
        glfw_init_(width, height);
        vulkan_init_();
    }

    ~Window() {
        vulkan_destroy_();
        glfw_destroy_();
    }

    void mainloop() {
        std::size_t current_frame = 0;

        while(!glfwWindowShouldClose(window_)) {
            glfwPollEvents();
            draw_frame_(current_frame);
            current_frame = (current_frame + 1) % max_frames_in_flight;
        }

        vkDeviceWaitIdle(device_);
    }

private:
    static void callback_framebuffer_resize_(GLFWwindow* window, int width, int height) {
        auto p_window = static_cast< Window* >(glfwGetWindowUserPointer(window));
        p_window->framebuffer_resized_ = true;
    }

    void glfw_init_(int width, int height) {

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        window_ = glfwCreateWindow(width, height, window_title, nullptr, nullptr);
        glfwSetWindowUserPointer(window_, this);
        glfwSetFramebufferSizeCallback(window_, callback_framebuffer_resize_);
    }

    void glfw_destroy_() {
        glfwDestroyWindow(window_);

    }

    void vulkan_init_() {
        instance_ = vk_util::create_instance();
        surface_  = vk_util::create_surface(instance_, window_);
        physical_device_ = vk_util::pick_physical_device(instance_, surface_);

        std::tie(
            device_,
            graphics_queue_,
            present_queue_
        ) = vk_util::create_logical_device(physical_device_, surface_);

        command_pool_ = vk_util::create_command_pool(device_, physical_device_, surface_);

        std::tie(
            vertex_buffer_,
            vertex_buffer_memory_
        ) = vk_util::create_vertex_buffer(
            physical_device_,
            device_,
            command_pool_,
            graphics_queue_,
            vertices
        );

        vulkan_swap_chain_init_();

        std::tie(
            image_available_semaphores_,
            render_finished_semaphores_,
            in_flight_fences_,
            images_in_flight_
        ) = vk_util::create_sync_objs< max_frames_in_flight >(device_, swap_chain_images_);
    }

    void vulkan_destroy_() {
        for(std::size_t i = 0; i < max_frames_in_flight; ++i) {
            vkDestroySemaphore(device_, render_finished_semaphores_[i], nullptr);
            vkDestroySemaphore(device_, image_available_semaphores_[i], nullptr);
            vkDestroyFence(device_, in_flight_fences_[i], nullptr);
        }

        vulkan_swap_chain_destroy_();

        vkDestroyBuffer(device_, vertex_buffer_, nullptr);
        vkFreeMemory(device_, vertex_buffer_memory_, nullptr);

        vkDestroyCommandPool(device_, command_pool_, nullptr);

        vkDestroyDevice(device_, nullptr);
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        vkDestroyInstance(instance_, nullptr);
    }

    void vulkan_swap_chain_init_() {

        const auto [width, height] = glfw_util::get_framebuffer_size(window_);

        std::tie(
            swap_chain_,
            swap_chain_images_,
            swap_chain_image_format_,
            swap_chain_extent_
        ) = vk_util::create_swap_chain(physical_device_, surface_, device_, width, height);
        swap_chain_image_views_ = vk_util::create_image_views(
            device_,
            swap_chain_images_,
            swap_chain_image_format_
        );

        render_pass_ = vk_util::create_render_pass(device_, swap_chain_image_format_);
        std::tie(
            pipeline_layout_,
            graphics_pipeline_
        ) = vk_util::create_graphics_pipeline(
            device_,
            swap_chain_extent_,
            render_pass_,
            Vertex::get_binding_desc(),
            Vertex::get_attr_desc()
        );

        swap_chain_framebuffers_ = vk_util::create_framebuffers(
            device_,
            swap_chain_image_views_,
            swap_chain_extent_,
            render_pass_
        );

        command_buffers_ = vk_util::create_command_buffers(
            device_,
            swap_chain_extent_,
            render_pass_,
            graphics_pipeline_,
            swap_chain_framebuffers_,
            command_pool_,
            vertex_buffer_,
            vertices.size()
        );
    }

    void vulkan_swap_chain_destroy_() {
        vkFreeCommandBuffers(device_, command_pool_, command_buffers_.size(), command_buffers_.data());

        for(auto framebuffer : swap_chain_framebuffers_) {
            vkDestroyFramebuffer(device_, framebuffer, nullptr);
        }
        vkDestroyPipeline(device_, graphics_pipeline_, nullptr);
        vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
        vkDestroyRenderPass(device_, render_pass_, nullptr);
        for (auto view : swap_chain_image_views_) {
            vkDestroyImageView(device_, view, nullptr);
        }
        vkDestroySwapchainKHR(device_, swap_chain_, nullptr);
    }

    void vulkan_swap_chain_recreate_() {
        for(
            auto [width, height] = glfw_util::get_framebuffer_size(window_);
            width == 0 || height == 0;
            glfwGetFramebufferSize(window_, &width, &height)
        ) {
            glfwWaitEvents();
        }

        vkDeviceWaitIdle(device_);

        vulkan_swap_chain_destroy_();
        vulkan_swap_chain_init_();
    }

    void draw_frame_(std::size_t frame) {
        // Fences
        vkWaitForFences(device_, 1, &in_flight_fences_[frame], VK_TRUE, UINT64_MAX);

        // Acquire image from swap chain
        std::uint32_t image_index;
        {
            const auto result = vkAcquireNextImageKHR(
                device_,
                swap_chain_,
                UINT64_MAX,
                image_available_semaphores_[frame],
                VK_NULL_HANDLE,
                &image_index
            );

            switch(result) {

            case VK_ERROR_OUT_OF_DATE_KHR:
                vulkan_swap_chain_recreate_();
                return;

            case VK_SUCCESS:
            case VK_SUBOPTIMAL_KHR:
                break;

            default:
                throw std::runtime_error("Failed to acquire swap chain image.");
            }
        }

        // Check if a previous frame is using this image
        if(images_in_flight_[image_index] != VK_NULL_HANDLE) {
            vkWaitForFences(device_, 1, &images_in_flight_[image_index], VK_TRUE, UINT64_MAX);
        }
        // Mark the image as now being in use by this frame
        images_in_flight_[image_index] = in_flight_fences_[frame];

        VkSemaphore wait_semaphores[] { image_available_semaphores_[frame] };
        VkSemaphore signal_semaphores[] { render_finished_semaphores_[frame] };

        // Submit command buffer
        VkSubmitInfo si {};
        si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkPipelineStageFlags wait_stages[] { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        si.waitSemaphoreCount = 1;
        si.pWaitSemaphores = wait_semaphores;
        si.pWaitDstStageMask = wait_stages;
        si.commandBufferCount = 1;
        si.pCommandBuffers = &command_buffers_[image_index];
        si.signalSemaphoreCount = 1;
        si.pSignalSemaphores = signal_semaphores;

        vkResetFences(device_, 1, &in_flight_fences_[frame]);
        if(vkQueueSubmit(graphics_queue_, 1, &si, in_flight_fences_[frame]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit draw command buffer.");
        }

        // Presentation
        VkPresentInfoKHR pi {};
        pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        pi.waitSemaphoreCount = 1;
        pi.pWaitSemaphores = signal_semaphores;

        VkSwapchainKHR swap_chains[] { swap_chain_ };
        pi.swapchainCount = 1;
        pi.pSwapchains = swap_chains;
        pi.pImageIndices = &image_index;
        pi.pResults = nullptr;

        {
            const auto result = vkQueuePresentKHR(present_queue_, &pi);

            bool need_swap_chain_recreate = framebuffer_resized_;

            switch(result) {

            case VK_ERROR_OUT_OF_DATE_KHR:
            case VK_SUBOPTIMAL_KHR:
                need_swap_chain_recreate = true;
                break;

            case VK_SUCCESS:
                break;

            default:
                throw std::runtime_error("Failed to present swap chain image.");
            }

            if(need_swap_chain_recreate) {
                framebuffer_resized_ = false;
                vulkan_swap_chain_recreate_();
            }
        }
    }


    // Member variables
    //-------------------------------------------------------------------------

    glfw_util::EnvGuard glfw_env_guard_;

    // GLFW window
    GLFWwindow*      window_ = nullptr;

    // Vulkan instance
    VkInstance       instance_;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;

    VkSurfaceKHR     surface_;

    VkDevice         device_;
    VkQueue          graphics_queue_;
    VkQueue          present_queue_;

    VkSwapchainKHR   swap_chain_;
    std::vector< VkImage > swap_chain_images_;
    VkFormat         swap_chain_image_format_;
    VkExtent2D       swap_chain_extent_;

    std::vector< VkImageView > swap_chain_image_views_;

    VkRenderPass     render_pass_;
    VkPipelineLayout pipeline_layout_;
    VkPipeline       graphics_pipeline_;

    std::vector< VkFramebuffer > swap_chain_framebuffers_;

    VkCommandPool    command_pool_;
    std::vector< VkCommandBuffer > command_buffers_;

    VkDeviceMemory   vertex_buffer_memory_;
    VkBuffer         vertex_buffer_;

    std::array< VkSemaphore, max_frames_in_flight > image_available_semaphores_;
    std::array< VkSemaphore, max_frames_in_flight > render_finished_semaphores_;
    std::array< VkFence, max_frames_in_flight > in_flight_fences_;
    std::vector< VkFence > images_in_flight_;

    // States
    bool framebuffer_resized_ = false;
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
