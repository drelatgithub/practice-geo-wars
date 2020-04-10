#ifndef PGW_VISUAL_VK_VERTEX_BUFFER_MANAGER_HPP
#define PGW_VISUAL_VK_VERTEX_BUFFER_MANAGER_HPP

#include "visual/vk-utils.hpp"

namespace pgw {
namespace vk_util {

class VertexBufferManager {
public:

    struct CopyDataResult {
        bool buffer_reallocated = false;
    };

    VertexBufferManager(
        VkPhysicalDevice phys_dev,
        VkDevice         device,
        VkCommandPool    command_pool,
        VkQueue          transfer_queue,
        VkDeviceSize     initial_size = 1024
    ) :
        phys_dev_(phys_dev),
        device_(device),
        command_pool_(command_pool),
        transfer_queue_(transfer_queue),
        buffer_size_(initial_size)
    {
        create_buffers_();
    }

    ~VertexBufferManager() {
        destroy_buffers_();
    }

    template< typename Vertex >
    CopyDataResult copy_data(const std::vector< Vertex >& vertex_data) {
        CopyDataResult res {};

        const auto new_num_vertices = vertex_data.size();
        const auto new_used_size    = new_num_vertices * sizeof(Vertex);

        if(new_used_size > buffer_size_) {
            // Need reallocation
            vkDeviceWaitIdle(device_);

            destroy_buffers_();

            // Current strategy: expand the buffer size by a factor of 2
            while(buffer_size_ < new_used_size) buffer_size_ *= 2;
            create_buffers_();

            res.buffer_reallocated = true;
        }

        // Copy data to staging buffer
        {
            void* p_data;
            vkMapMemory(device_, staging_memory_, 0, new_used_size, 0, &p_data);
            std::memcpy(p_data, vertex_data.data(), new_used_size);
            vkUnmapMemory(device_, staging_memory_);
        }

        // Transfer data from staging buffer to device buffer
        copy_buffer(
            device_,
            command_pool_,
            transfer_queue_,
            staging_buffer_,
            buffer_,
            new_used_size
        );

        // Set variables
        used_size_ = new_used_size;
        num_vertices_ = new_num_vertices;

        return res;
    }

    // Accessors
    auto capacity() const { return buffer_size_; }
    auto num_vertices() const { return num_vertices_; }
    auto size() const { return used_size_; }
    auto buffer() const { return buffer_; }

private:

    void create_buffers_() {
        std::tie(
            staging_buffer_,
            staging_memory_
        ) = create_buffer(
            phys_dev_,
            device_,
            buffer_size_,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );

        std::tie(
            buffer_,
            memory_
        ) = create_buffer(
            phys_dev_,
            device_,
            buffer_size_,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
    }

    void destroy_buffers_() {
        vkDestroyBuffer(device_, buffer_, nullptr);
        vkFreeMemory(device_, memory_, nullptr);

        vkDestroyBuffer(device_, staging_buffer_, nullptr);
        vkFreeMemory(device_, staging_memory_, nullptr);
    }


    // The environment (not changed)
    VkPhysicalDevice phys_dev_;
    VkDevice         device_;
    VkCommandPool    command_pool_; // Used for buffer copying
    VkQueue          transfer_queue_; // Used for buffer copying

    // The transfer vertex buffer
    VkBuffer       staging_buffer_;
    VkDeviceMemory staging_memory_;
    VkDeviceSize   buffer_size_; // The capacity of the buffer

    // The device vertex buffer (actual storage)
    VkBuffer       buffer_;
    VkDeviceMemory memory_;

    // The actual data stored
    VkDeviceSize   used_size_ = 0;
    std::uint32_t  num_vertices_ = 0;
};

} // namespace vk_util
} // namespace pgw

#endif
