#ifndef PGW_VISUAL_VK_SWAP_CHAIN_MANAGER_HPP
#define PGW_VISUAL_VK_SWAP_CHAIN_MANAGER_HPP

#include "vk-utils.hpp"

namespace pgw {
namespace vk_util {

class SwapChainManager {
public:
    SwapChainManager(
        VkPhysicalDevice phys_dev,
        VkSurfaceKHR     surface,
        const QueueFamilyIndices& qf_indices,
        VkDevice         device,
        int              width,
        int              height,
        VkVertexInputBindingDescription bind_desc,
        const std::vector< VkVertexInputAttributeDescription >& attr_desc
    ) :
        phys_dev_(phys_dev),
        surface_(surface),
        qf_indices_(qf_indices),
        device_(device)
    {
        init_(
            width,
            height,
            bind_desc,
            attr_desc
        );
    }

    ~SwapChainManager() {
        destroy_();
    }

    void recreate(
        int width,
        int height,
        VkVertexInputBindingDescription bind_desc,
        const std::vector< VkVertexInputAttributeDescription >& attr_desc
    ) {
        vkDeviceWaitIdle(device_);

        destroy_();
        init_(
            width,
            height,
            bind_desc,
            attr_desc
        );
    }


    // Accessors
    auto num_images() const { return swap_chain_images_.size(); }
    auto swap_chain() const { return swap_chain_; }
    auto swap_chain_extent() const { return swap_chain_extent_; }

    auto render_pass() const { return render_pass_; }
    auto graphics_pipeline() const { return graphics_pipeline_; }

    const auto& framebuffers() const { return framebuffers_; }
    const auto& command_pools() const { return command_pools_; }
    const auto& command_buffers() const { return command_buffers_; }

private:
    void init_(
        int width,
        int height,
        VkVertexInputBindingDescription bind_desc,
        const std::vector< VkVertexInputAttributeDescription >& attr_desc
    ) {

        std::tie(
            swap_chain_,
            swap_chain_images_,
            swap_chain_image_format_,
            swap_chain_extent_
        ) = vk_util::create_swap_chain(phys_dev_, surface_, device_, width, height);
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
            bind_desc,
            attr_desc
        );

        framebuffers_ = vk_util::create_framebuffers(
            device_,
            swap_chain_image_views_,
            swap_chain_extent_,
            render_pass_
        );

        std::tie(
            command_pools_,
            command_buffers_
        ) = vk_util::create_graphics_command_pools_and_buffers(
            device_,
            qf_indices_,
            framebuffers_
        );
    }

    void destroy_() {
        for(auto command_pool : command_pools_) {
            vkDestroyCommandPool(device_, command_pool, nullptr);
        }

        for(auto framebuffer : framebuffers_) {
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


    // Environment (not changed)
    VkPhysicalDevice   phys_dev_;
    VkSurfaceKHR       surface_;
    QueueFamilyIndices qf_indices_;
    VkDevice           device_;

    // Swap chain managed objects
    VkSwapchainKHR     swap_chain_;
    std::vector< VkImage > swap_chain_images_;
    VkFormat           swap_chain_image_format_;
    VkExtent2D         swap_chain_extent_;

    std::vector< VkImageView > swap_chain_image_views_;

    VkRenderPass       render_pass_;
    VkPipelineLayout   pipeline_layout_;
    VkPipeline         graphics_pipeline_;

    std::vector< VkFramebuffer > framebuffers_;

    std::vector< VkCommandPool > command_pools_;
    std::vector< VkCommandBuffer > command_buffers_;
};

} // namespace vk_util
} // namespace pgw

#endif
