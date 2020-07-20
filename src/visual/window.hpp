#ifndef PGW_VISUAL_WINDOW_HPP
#define PGW_VISUAL_WINDOW_HPP

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility> // move

#include "glfw-utils.hpp"
#include "visual-common.hpp"
#include "vk-swap-chain-manager.hpp"
#include "vk-utils.hpp"
#include "vk-vertex-buffer-manager.hpp"

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
        std::vector< VkVertexInputAttributeDescription > ad(2);

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

    template<
        typename BeforeRender
    >
    void mainloop(BeforeRender&& before_render) {
        std::size_t current_frame = 0;

        while(!glfwWindowShouldClose(window_)) {
            glfwPollEvents();

            before_render();

            draw_frame_(current_frame);
            current_frame = (current_frame + 1) % max_frames_in_flight;
        }

        vkDeviceWaitIdle(device_);
    }

    // Utilities
    //---------------------------------
    void copy_vertex_data(const std::vector< Vertex >& vs) {
        op_vertex_buffer_manager_.value().copy_data(vs);
    }

    // Accessors
    //---------------------------------
    auto      & glfw_callbacks()       { return glfw_callbacks_; }
    const auto& glfw_callbacks() const { return glfw_callbacks_; }

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

        // Set callbacks
        glfwSetKeyCallback(
            window_,
            [](GLFWwindow* window, int key, int scancode, int action, int mods) -> void {
                const auto pw = static_cast< Window* >(glfwGetWindowUserPointer(window));
                if(const auto& cb = pw->glfw_callbacks_.key_callback) {
                    cb(key, scancode, action, mods);
                }
            }
        );
    }

    void glfw_destroy_() {
        glfwDestroyWindow(window_);

    }

    void vulkan_init_() {
        instance_ = vk_util::create_instance();
        surface_  = vk_util::create_surface(instance_, window_);
        physical_device_ = vk_util::pick_physical_device(instance_, surface_);
        qf_indices_ = vk_util::find_queue_families(physical_device_, surface_);

        std::tie(
            device_,
            graphics_queue_,
            present_queue_,
            transfer_queue_
        ) = vk_util::create_logical_device(physical_device_, surface_);

        transfer_command_pool_ = vk_util::create_transfer_command_pool(device_, qf_indices_);

        op_vertex_buffer_manager_.emplace(
            physical_device_,
            device_,
            transfer_command_pool_,
            transfer_queue_
        );

        const auto [width, height] = glfw_util::get_framebuffer_size(window_);
        op_swap_chain_manager_.emplace(
            physical_device_,
            surface_,
            qf_indices_,
            device_,
            width, height,
            Vertex::get_binding_desc(),
            Vertex::get_attr_desc()
        );

        std::tie(
            image_available_semaphores_,
            render_finished_semaphores_,
            in_flight_fences_,
            images_in_flight_
        ) = vk_util::create_sync_objs< max_frames_in_flight >(
            device_,
            op_swap_chain_manager_->num_images()
        );
    }

    void vulkan_destroy_() {
        for(std::size_t i = 0; i < max_frames_in_flight; ++i) {
            vkDestroySemaphore(device_, render_finished_semaphores_[i], nullptr);
            vkDestroySemaphore(device_, image_available_semaphores_[i], nullptr);
            vkDestroyFence(device_, in_flight_fences_[i], nullptr);
        }

        op_swap_chain_manager_.reset();

        op_vertex_buffer_manager_.reset();

        vkDestroyCommandPool(device_, transfer_command_pool_, nullptr);

        vkDestroyDevice(device_, nullptr);
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        vkDestroyInstance(instance_, nullptr);
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

        const auto [width, height] = glfw_util::get_framebuffer_size(window_);
        op_swap_chain_manager_.value().recreate(
            width, height,
            Vertex::get_binding_desc(),
            Vertex::get_attr_desc()
        );
    }

    void draw_frame_(std::size_t frame) {
        // Fences
        vkWaitForFences(device_, 1, &in_flight_fences_[frame], VK_TRUE, UINT64_MAX);

        // Acquire image from swap chain
        std::uint32_t image_index;
        {
            const auto result = vkAcquireNextImageKHR(
                device_,
                op_swap_chain_manager_->swap_chain(),
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

        // Reset and record the command buffer
        //-----------------------------
        vkResetCommandPool(device_, op_swap_chain_manager_->command_pools()[image_index], 0);

        vk_util::record_graphics_command_buffer(
            op_swap_chain_manager_->swap_chain_extent(),
            op_swap_chain_manager_->render_pass(),
            op_swap_chain_manager_->graphics_pipeline(),
            op_swap_chain_manager_->framebuffers()[image_index],
            op_swap_chain_manager_->command_buffers()[image_index],
            op_vertex_buffer_manager_->buffer(),
            op_vertex_buffer_manager_->num_vertices()
        );

        // Set up semaphores and get ready to submit
        //-----------------------------
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
        si.pCommandBuffers = &(op_swap_chain_manager_->command_buffers())[image_index];
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

        VkSwapchainKHR swap_chains[] { op_swap_chain_manager_->swap_chain() };
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

    glfw_util::CallbackContainer glfw_callbacks_;

    // Vulkan instance
    VkInstance       instance_;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;

    VkSurfaceKHR     surface_;
    vk_util::QueueFamilyIndices qf_indices_;

    VkDevice         device_;
    VkQueue          graphics_queue_;
    VkQueue          present_queue_;
    VkQueue          transfer_queue_;

    std::optional< vk_util::SwapChainManager > op_swap_chain_manager_;

    VkCommandPool    transfer_command_pool_;

    std::optional< vk_util::VertexBufferManager > op_vertex_buffer_manager_;

    std::array< VkSemaphore, max_frames_in_flight > image_available_semaphores_;
    std::array< VkSemaphore, max_frames_in_flight > render_finished_semaphores_;
    std::array< VkFence, max_frames_in_flight > in_flight_fences_;
    std::vector< VkFence > images_in_flight_;

    // States
    bool framebuffer_resized_ = false;
};


} // namespace pgw

#endif
