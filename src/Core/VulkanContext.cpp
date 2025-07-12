#include "Pch.h"
#include "VulkanContext.h"

#include "VkBootstrap.h"
#include "VkUtils.h"

#include <libassert/assert.hpp>

#include <format>
#include <vulkan/vulkan_core.h>

static VkQueue CreateQueue(VulkanContext &ctx, vkb::QueueType type,
                           VkQueueFamilyProperties &properties)
{
    auto idx = ctx.Device.get_queue_index(type);

    auto queue = ctx.Device.get_queue(type);

    ASSERT(queue.has_value(),
           std::format("Failed to get a queue: {}", queue.error().message()));

    auto propVector = ctx.PhysicalDevice.get_queue_families();
    properties = propVector[idx.value()];

    return queue.value();
}

VulkanContext::VulkanContext(uint32_t width, uint32_t height, const std::string &appName,
                             SystemWindow &window)
    : RequestedWidth(width), RequestedHeight(height)
{
    // Initialization done using vk-bootstrap, docs available at
    // https://github.com/charles-lunarg/vk-bootstrap/blob/main/docs/getting_started.md

    // Retrieve system info
    auto systemInfoRet = vkb::SystemInfo::get_system_info();

    ASSERT(systemInfoRet, systemInfoRet.error().message());

    const auto &systemInfo = systemInfoRet.value();

    // Instance creation:
    auto instBuilder = vkb::InstanceBuilder();

    if (systemInfo.is_extension_available("VK_EXT_debug_utils"))
        instBuilder.enable_extension("VK_EXT_debug_utils");

    auto instRet = instBuilder.set_app_name(appName.c_str())
                       .set_engine_name("No Engine")
                       .require_api_version(1, 3, 0)
                       .request_validation_layers()
                       .use_default_debug_messenger()
                       .build();

    ASSERT(instRet, instRet.error().message());

    Instance = instRet.value();
    Surface = window.CreateSurface(Instance);

    SetDebugUtilsObjectName = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(
        Instance, "vkSetDebugUtilsObjectNameEXT");

    // Device selection:

    // To request required/desired device extensions:
    //.add_required_extension("VK_KHR_timeline_semaphore");
    //.add_desired_extension("VK_KHR_imageless_framebuffer");

    VkPhysicalDeviceFeatures features{};
    features.samplerAnisotropy = true;

    VkPhysicalDeviceVulkan12Features features12{};
    features12.descriptorIndexing = true;
    features12.bufferDeviceAddress = true;

    VkPhysicalDeviceVulkan13Features features13{};
    features13.dynamicRendering = true;
    features13.synchronization2 = true;

    auto physDeviceRet = vkb::PhysicalDeviceSelector(Instance)
                             .set_surface(Surface)
                             .set_required_features(features)
                             .set_required_features_12(features12)
                             .set_required_features_13(features13)
                             .select();

    ASSERT(physDeviceRet, physDeviceRet.error().message());

    PhysicalDevice = physDeviceRet.value();

    auto deviceRet = vkb::DeviceBuilder(PhysicalDevice).build();

    ASSERT(deviceRet, deviceRet.error().message());

    Device = deviceRet.value();

    // Create queues:
    Queues.Graphics =
        CreateQueue(*this, vkb::QueueType::graphics, QueueProperties.Graphics);

    Queues.Present = CreateQueue(*this, vkb::QueueType::present, QueueProperties.Present);

    // Vma Allocator creation:
    VmaAllocatorCreateInfo allocatorCreateInfo = {};
    allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    allocatorCreateInfo.physicalDevice = PhysicalDevice;
    allocatorCreateInfo.device = Device;
    allocatorCreateInfo.instance = Instance;
    allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

    vmaCreateAllocator(&allocatorCreateInfo, &Allocator);

    // Swapchain creation:
    CreateSwapchain(true);

    // Allocate command pools for immediate submit:
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = Device.get_queue_index(vkb::QueueType::graphics).value();
    // To allow resetting individual buffers:
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    auto ret = vkCreateCommandPool(Device, &poolInfo, nullptr, &mImmGraphicsCommandPool);

    ASSERT(ret == VK_SUCCESS, "Failed to create an immediate submit command pool!");
}

VulkanContext::~VulkanContext()
{
    vkDestroyCommandPool(Device, mImmGraphicsCommandPool, nullptr);

    Swapchain.destroy_image_views(SwapchainImageViews);
    vkb::destroy_swapchain(Swapchain);

    vmaDestroyAllocator(Allocator);

    vkb::destroy_device(Device);
    vkb::destroy_surface(Instance, Surface);
    vkb::destroy_instance(Instance);
}

void VulkanContext::CreateSwapchain(bool firstRun)
{
    if (!firstRun)
        Swapchain.destroy_image_views(SwapchainImageViews);

    // To manually specify format and present mode:
    //.set_desired_format(VkSurfaceFormatKHR)
    //.set_desired_present_mode(VkPresentModeKHR)
    auto swapRet = vkb::SwapchainBuilder(Device)
                       .set_old_swapchain(Swapchain)
                       .set_desired_extent(RequestedWidth, RequestedHeight)
                       // To enable blit from secondary render target:
                       .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                       .build();

    ASSERT(swapRet, swapRet.error().message());

    vkb::destroy_swapchain(Swapchain);

    Swapchain = swapRet.value();

    SwapchainImages = Swapchain.get_images().value();
    SwapchainImageViews = Swapchain.get_image_views().value();
}

void VulkanContext::ImmediateSubmitGraphics(
    std::function<void(VkCommandBuffer)> &&function)
{
    VkCommandBuffer buffer;

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = mImmGraphicsCommandPool;
    allocInfo.commandBufferCount = 1;

    vkAllocateCommandBuffers(Device, &allocInfo, &buffer);

    vkutils::BeginRecording(buffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    function(buffer);

    vkutils::EndRecording(buffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &buffer;

    vkQueueSubmit(Queues.Graphics, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(Queues.Graphics);

    vkFreeCommandBuffers(Device, mImmGraphicsCommandPool, 1, &buffer);
}