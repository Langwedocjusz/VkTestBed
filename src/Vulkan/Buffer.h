#pragma once

#include "OpaqueBuffer.h"
#include "VulkanContext.h"

#include "volk.h"

#include <string>

class Buffer {
  public:
    struct Info;
    struct ThinInfo;
  public:
    Buffer(VulkanContext &ctx, const Info &info);
    ~Buffer();
    Buffer(const Buffer &)            = delete;
    Buffer &operator=(const Buffer &) = delete;
    Buffer(Buffer &&) noexcept;
    Buffer &operator=(Buffer &&) = delete;

    // Factory functions for common types:
    static Buffer MakeStaging(VulkanContext &ctx, const ThinInfo &info);
    static Buffer MakeMappedUniform(VulkanContext &ctx, const ThinInfo &info);
    static Buffer MakeTransferDST(VulkanContext &ctx, const Info &info, const void *data);
    static Buffer MakeVertex(VulkanContext &ctx, const std::string &name, const OpaqueBuffer &buf);
    static Buffer MakeVertexStorage(VulkanContext &ctx, const std::string &name, const OpaqueBuffer &buf);
    static Buffer MakeIndex(VulkanContext &ctx, const std::string &name, const OpaqueBuffer &buf);

    // Buffer operations:
    void Upload(const void *data, VkDeviceSize size);
    // Will only worked on mapped buffers:
    void UploadMapped(const void *data, VkDeviceSize size);

    static void CopyContents(VkCommandBuffer cmd, const Buffer &src, Buffer &dst,
                             size_t size);

  public:
    struct Info {
        std::string              Name;
        VkDeviceSize             Size  = 0;
        VkBufferUsageFlags       Usage = 0;
        VmaAllocationCreateFlags Flags = 0;
    };

    struct ThinInfo{
        std::string              Name;
        VkDeviceSize             Size  = 0;
    };

  public:
    VkBuffer          Handle;
    VkDeviceSize      Size;
    VmaAllocation     Allocation;
    VmaAllocationInfo AllocInfo;

  private:
    VulkanContext &mCtx;
};