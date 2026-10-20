#include "VulkanContext.h"
#include "Pch.h"

#include "Frame.h"
#include "Swapchain.h"
#include "Vassert.h"
#include "VkUtils.h"

#include "VkBootstrap.h"

#include "volk.h"

struct VulkanContext::ExtraData {
    vkb::Instance Instance;
};

VulkanContext::VulkanContext(SystemWindow &window)
{
    if (volkInitialize() != VK_SUCCESS)
    {
        vpanic("Failed to initialize volk!");
    }

    // Instance creation:

    // Not requesting layers since their usage can be
    // configured as needed via vkconfig and that
    // doesn't require recompiling the whole program.
    auto vkbInstance = vkb::InstanceBuilder()
                           .set_app_name(window.Title.c_str())
                           .set_engine_name("No Engine")
                           .require_api_version(1, 3, 0)
                           .enable_extensions(Swapchain::GetRequiredInstanceExtensions())
                           .use_default_debug_messenger()
                           .build()
                           .value();

    mExtraData           = std::make_unique<ExtraData>();
    mExtraData->Instance = vkbInstance;

    Instance = vkbInstance.instance;
    volkLoadInstance(Instance);

    // Surface creation:
    Surface = window.CreateSurface(Instance);

    // Device selection:

    // To request required/desired device extensions:
    //.add_required_extension("VK_KHR_timeline_semaphore");
    //.add_desired_extension("VK_KHR_imageless_framebuffer");

    VkPhysicalDeviceFeatures features{};
    features.samplerAnisotropy = true;
    features.shaderInt16       = true;

    VkPhysicalDeviceVulkan11Features features11{};
    features11.storageBuffer16BitAccess = true;

    VkPhysicalDeviceVulkan12Features features12{};
    features12.scalarBlockLayout   = true;
    features12.descriptorIndexing  = true;
    features12.bufferDeviceAddress = true;
    features12.timelineSemaphore   = true;

    VkPhysicalDeviceVulkan13Features features13{};
    features13.dynamicRendering               = true;
    features13.synchronization2               = true;
    features13.shaderDemoteToHelperInvocation = true;

    std::vector<const char *> requiredExtensions{"VK_EXT_extended_dynamic_state3"};

    // Additional extensions potentially required by swapchain:
    for (auto ext : Swapchain::GetRequiredDeviceExtensions())
    {
        requiredExtensions.push_back(ext);
    }

    // PhysicalDevice
    auto physDevCandidates = vkb::PhysicalDeviceSelector(vkbInstance)
                                 .set_surface(Surface)
                                 .set_required_features(features)
                                 .set_required_features_11(features11)
                                 .set_required_features_12(features12)
                                 .set_required_features_13(features13)
                                 .add_required_extensions(requiredExtensions)
                                 .select_devices()
                                 .value();

    vkb::PhysicalDevice vkbPhysicalDevice{};

    for (const auto &device : physDevCandidates)
    {
        printf("Candidate device found: %s \n", device.name.c_str());

        if (device.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            vkbPhysicalDevice = device;
            break;
        }
    }

    printf("Selected device: %s \n", vkbPhysicalDevice.name.c_str());

    uint32_t apiVersion = vkbPhysicalDevice.properties.apiVersion;

    printf("Using Vulkan API Version: %u.%u.%u\n", VK_API_VERSION_MAJOR(apiVersion),
           VK_API_VERSION_MINOR(apiVersion), VK_API_VERSION_PATCH(apiVersion));

    // Enabling optional features:
    VkPhysicalDeviceFeatures optionalFeatures{};
    optionalFeatures.pipelineStatisticsQuery = true;

    vkbPhysicalDevice.enable_features_if_present(optionalFeatures);

    // Additional extension info:
    VkPhysicalDeviceExtendedDynamicState3FeaturesEXT dynamicState3Features{};
    dynamicState3Features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT;
    dynamicState3Features.extendedDynamicState3ColorBlendEnable = true;

    PhysicalDevice = vkbPhysicalDevice.physical_device;

    // Build logical device and load related function pointers through volk:
    auto vkbDevice = vkb::DeviceBuilder(vkbPhysicalDevice)
                         .add_pNext(&dynamicState3Features)
                         .build()
                         .value();

    Device = vkbDevice.device;
    volkLoadDevice(Device);

    // Create queues:
    auto CreateQueue = [&](vkb::QueueType type, Queue &out) {
        auto idx = vkbDevice.get_queue_index(type);

        if (!idx.has_value())
            vpanic(idx.error().message());

        auto handle = vkbDevice.get_queue(type);

        if (!handle.has_value())
            vpanic(handle.error().message());

        auto propVector = vkbPhysicalDevice.get_queue_families();
        auto properties = propVector[idx.value()];

        out = Queue{
            .Handle     = *handle,
            .Properties = properties,
            .Index      = *idx,
        };
    };

    CreateQueue(vkb::QueueType::graphics, Queues.Graphics);
    CreateQueue(vkb::QueueType::present, Queues.Present);

    // Vma Allocator creation:
    {
        VmaVulkanFunctions vulkanFunctions{};
        vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        vulkanFunctions.vkGetDeviceProcAddr   = vkGetDeviceProcAddr;

        VmaAllocatorCreateInfo allocatorCreateInfo = {};
        allocatorCreateInfo.vulkanApiVersion       = VK_API_VERSION_1_3;
        allocatorCreateInfo.physicalDevice         = PhysicalDevice;
        allocatorCreateInfo.device                 = Device;
        allocatorCreateInfo.instance               = Instance;
        allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
        allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;

        vmaCreateAllocator(&allocatorCreateInfo, &Allocator);
    }

    // Swapchain creation:
    Swapchain = std::make_unique<::Swapchain>(window, PhysicalDevice, Device, Surface,
                                              Queues.Graphics.Index, Queues.Present.Index,
                                              Queues.Present.Handle);
    Swapchain->Recreate();

    // Retrieve info about optional features:
    // Check for timestamp support:
    auto &limits          = vkbPhysicalDevice.properties.limits;
    auto  timestampPeriod = limits.timestampPeriod;

    OptionalFeatures.Timestamps      = (timestampPeriod != 0.0f);
    OptionalFeatures.TimestampPeriod = timestampPeriod;

    if (!limits.timestampComputeAndGraphics)
    {
        if (Queues.Graphics.Properties.timestampValidBits == 0)
        {
            OptionalFeatures.Timestamps = false;
        }
    }

    // Check for pipeline statistics support:
    OptionalFeatures.PipelineStatistics =
        vkbPhysicalDevice.features.pipelineStatisticsQuery;

    // Allocate command pools for immediate submit:
    mImmGraphicsCommandPool = vkutils::CreateCommandPool(*this, QueueType::Graphics);
}

VulkanContext::~VulkanContext()
{
    vkDestroyCommandPool(Device, mImmGraphicsCommandPool, nullptr);

    // Swapchain resources need to be freed at this precise point:
    Swapchain.reset();

    vmaDestroyAllocator(Allocator);

    vkDestroyDevice(Device, nullptr);
    vkDestroySurfaceKHR(Instance, Surface, nullptr);

    // Using vkb function here to also destroy
    // the library-supplied debug messenger:
    vkb::destroy_instance(mExtraData->Instance);
}

void VulkanContext::ImmediateSubmitGraphics(
    std::function<void(VkCommandBuffer)> &&function)
{
    VkCommandBuffer buffer =
        vkutils::AllocateCommandBuffer(*this, mImmGraphicsCommandPool);

    vkutils::BeginRecording(buffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    function(buffer);

    vkutils::EndRecording(buffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &buffer;

    vkQueueSubmit(Queues.Graphics.Handle, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(Queues.Graphics.Handle);

    vkFreeCommandBuffers(Device, mImmGraphicsCommandPool, 1, &buffer);
}