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
            draw_frame_();
        }

        vkDeviceWaitIdle(device_);
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
        instance_ = vk_util::create_instance();
        surface_  = vk_util::create_surface(instance_, window_);
        physical_device_ = vk_util::pick_physical_device(instance_, surface_);

        std::tie(
            device_,
            graphics_queue_,
            present_queue_
        ) = vk_util::create_logical_device(physical_device_, surface_);

        std::tie(
            swap_chain_,
            swap_chain_images_,
            swap_chain_image_format_,
            swap_chain_extent_
        ) = vk_util::create_swap_chain(physical_device_, surface_, device_, width_, height_);

        swap_chain_image_views_ = vk_util::create_image_views(
            device_,
            swap_chain_images_,
            swap_chain_image_format_
        );

        render_pass_ = vk_util::create_render_pass(device_, swap_chain_image_format_);
        std::tie(
            pipeline_layout_,
            graphics_pipeline_
        ) = vk_util::create_graphics_pipeline(device_, swap_chain_extent_, render_pass_);

        swap_chain_framebuffers_ = vk_util::create_framebuffers(device_, swap_chain_image_views_, swap_chain_extent_, render_pass_);

        command_pool_ = vk_util::create_command_pool(device_, physical_device_, surface_);
        command_buffers_ = vk_util::create_command_buffers(
            device_,
            swap_chain_extent_,
            render_pass_,
            graphics_pipeline_,
            swap_chain_framebuffers_,
            command_pool_
        );

        std::tie(
            image_available_semaphore_,
            render_finished_semaphore_
        ) = vk_util::create_semaphores(device_);
    }

    void draw_frame_() {
        // Acquire image from swap chain
        std::uint32_t image_index;
        vkAcquireNextImageKHR(
            device_,
            swap_chain_,
            UINT64_MAX,
            image_available_semaphore_,
            VK_NULL_HANDLE,
            &image_index
        );

        VkSemaphore wait_semaphores[] { image_available_semaphore_ };
        VkSemaphore signal_semaphores[] { render_finished_semaphore_ };

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

        if(vkQueueSubmit(graphics_queue_, 1, &si, VK_NULL_HANDLE) != VK_SUCCESS) {
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

        vkQueuePresentKHR(present_queue_, &pi);
    }

    void vulkan_destroy_() {
        vkDestroySemaphore(device_, render_finished_semaphore_, nullptr);
        vkDestroySemaphore(device_, image_available_semaphore_, nullptr);
        vkDestroyCommandPool(device_, command_pool_, nullptr);
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
        vkDestroyDevice(device_, nullptr);
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        vkDestroyInstance(instance_, nullptr);
    }

    // Member variables
    //-------------------------------------------------------------------------

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

    VkSemaphore image_available_semaphore_;
    VkSemaphore render_finished_semaphore_;

    // Settings
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
