#include "VulkanContext.h"
#include "Pch.h"

#include "Frame.h"
#include "Vassert.h"
#include "VkInit.h"
#include "VkUtils.h"

#include "VkBootstrap.h"

#include "volk.h"

#ifdef VULKAN_ON_DXGI

#define NOMINMAX
#include <windows.h>

#define DX_SWAP_VK_IMPLEMENTATION
#include "vk_dxgi.h"

#include <format>

struct VulkanContext::DxgiData {
    dvk_dxgiDeviceContext_t Context;
    dvk_functions_t         Functions;
    dvk_vulkanContext_t     VkContext;
    dvk_allocator_t         Allocator;         
    HWND                    Window;
    dvk_dxgiSwapChain_t     Swapchain;
    // Presentation info returned
    // by last frame's acquire image:
    dvk_presentationInfo_t LastPresentInfo;
};

static void *dxgiAlloc(size_t size, size_t alignment, void *)
{
    return _aligned_malloc(size, alignment);
}

static void dxgiFree(void *address, void *)
{
    _aligned_free(address);
}

#endif

static VkQueue CreateQueue(VulkanContext &ctx, vkb::QueueType type,
                           VkQueueFamilyProperties &properties)
{
    auto idx = ctx.Device.get_queue_index(type);

    auto queue = ctx.Device.get_queue(type);

    if (!queue.has_value())
        vpanic(queue.error().message());

    auto propVector = ctx.PhysicalDevice.get_queue_families();
    properties      = propVector[idx.value()];

    return queue.value();
}

VulkanContext::VulkanContext(uint32_t width, uint32_t height, const std::string &appName,
                             SystemWindow &window)
    : RequestedWidth(width), RequestedHeight(height)
{
    if (volkInitialize() != VK_SUCCESS)
    {
        vpanic("Failed to initialize volk!");
    }

    // Instance creation:

    // Not requesting layers since their usage can be
    // configured as needed via vkconfig and that
    // doesn't require recompiling the whole program.
    Instance = vkb::InstanceBuilder()
                   .set_app_name(appName.c_str())
                   .set_engine_name("No Engine")
                   .require_api_version(1, 3, 0)
                   #ifdef VULKAN_ON_DXGI
                   .enable_extension("VK_KHR_win32_surface")
                   #endif
                   .use_default_debug_messenger()
                   .build()
                   .value();

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

    std::vector<const char*> requiredExtensions{"VK_EXT_extended_dynamic_state3"};

    // Additional extensions for vk_dxgi on Windows:
    #ifdef VULKAN_ON_DXGI
    for (auto ext : dvk_required_vulkan_extensions)
        requiredExtensions.push_back(ext);
    #endif

    //PhysicalDevice 
        auto physDevCandidates =
        vkb::PhysicalDeviceSelector(Instance)
                         .set_surface(Surface)
                         .set_required_features(features)
                         .set_required_features_11(features11)
                         .set_required_features_12(features12)
                         .set_required_features_13(features13)
                         .add_required_extensions(requiredExtensions)
                         .select_devices()
                         .value();

            //.select()
            //.value();

    for (const auto &device : physDevCandidates)
    {
        printf("Candidate device found: %s \n", device.name.c_str());
    
        if (device.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            PhysicalDevice = device;
            break;
        }
    }

    printf("Selected device: %s \n", PhysicalDevice.name.c_str());

    uint32_t apiVersion = PhysicalDevice.properties.apiVersion;

    printf("Using Vulkan API Version: %u.%u.%u\n", VK_API_VERSION_MAJOR(apiVersion),
           VK_API_VERSION_MINOR(apiVersion), VK_API_VERSION_PATCH(apiVersion));

    // Enabling optional features:
    VkPhysicalDeviceFeatures optionalFeatures{};
    optionalFeatures.pipelineStatisticsQuery = true;

    PhysicalDevice.enable_features_if_present(optionalFeatures);

    // Additional extension info:
    VkPhysicalDeviceExtendedDynamicState3FeaturesEXT dynamicState3Features{};
    dynamicState3Features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT;
    dynamicState3Features.extendedDynamicState3ColorBlendEnable = true;

    // Build logical device and load related function pointers through volk:
    Device = vkb::DeviceBuilder(PhysicalDevice)
                 .add_pNext(&dynamicState3Features)
                 .build()
                 .value();

    volkLoadDevice(Device);

    // Create queues:
    Queues.Graphics =
        CreateQueue(*this, vkb::QueueType::graphics, QueueProperties.Graphics);

    Queues.Present = CreateQueue(*this, vkb::QueueType::present, QueueProperties.Present);

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

    // On Windows, initialize vk_dxgi library:
    #ifdef VULKAN_ON_DXGI
    // Set up the function table:
    const auto getMemoryWin32HandleProperties =
        reinterpret_cast<PFN_vkGetMemoryWin32HandlePropertiesKHR>(
            vkGetDeviceProcAddr(Device, "vkGetMemoryWin32HandlePropertiesKHR"));

    const auto importSemaphoreWin32Handle = reinterpret_cast<PFN_vkImportSemaphoreWin32HandleKHR>(
            vkGetDeviceProcAddr(Device, "vkImportSemaphoreWin32HandleKHR"));

    dvk_functions_t dvkFunctions = { 
        .pCreateFactory                      = CreateDXGIFactory2,
        .pCreateDevice                       = D3D12CreateDevice,
        .pGetDebugInterface                  = D3D12GetDebugInterface,
        .vkCreateImage                       = vkCreateImage,
        .vkGetMemoryWin32HandlePropertiesKHR = getMemoryWin32HandleProperties,
        .vkGetImageMemoryRequirements        = vkGetImageMemoryRequirements,
        .vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties,
        .vkAllocateMemory                    = vkAllocateMemory,
        .vkBindImageMemory                   = vkBindImageMemory,
        .vkCreateSemaphore                   = vkCreateSemaphore,
        .vkImportSemaphoreWin32HandleKHR     = importSemaphoreWin32Handle,
        .vkDestroyImage                      = vkDestroyImage,
        .vkFreeMemory                        = vkFreeMemory,
        .vkDestroySemaphore                  = vkDestroySemaphore,
        .vkDeviceWaitIdle                    = vkDeviceWaitIdle,
    };

    // Grab device LUID:
    VkPhysicalDeviceIDProperties idProperties = {};
    idProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;

    VkPhysicalDeviceProperties2 properties = {};
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties.pNext = &idProperties;
    
    vkGetPhysicalDeviceProperties2(PhysicalDevice, &properties);

    // TODO: Maybe fallback to the default vulkan swapchain in this case?
    // Since FIFO doesn't work consistently in this case, maybe switch
    // to letterbox if that's the case.
    vassert(idProperties.deviceLUIDValid);

    LUID deviceLuid{};
    static_assert(sizeof(deviceLuid) == VK_LUID_SIZE);
    std::memcpy(&deviceLuid, idProperties.deviceLUID, sizeof(deviceLuid));

    // Initialize D3D Device:
    dvk_createDeviceParameters_t deviceParameters = { 
        .pFunctions = &dvkFunctions, 
        .deviceLuid = deviceLuid,
    };

    dvk_dxgiDeviceContext_t dxgiContext{};
    auto result = dvk_createDevice(deviceParameters, &dxgiContext);

    vassert(result.code == DVK_OK);

    dvk_vulkanContext_t dxgiVkContext{
        .physicalDevice = PhysicalDevice,
        .device         = Device,
    };

    dvk_allocator_t dxgiAllocator{
        .allocate = dxgiAlloc,
        .free     = dxgiFree,
    };

    mDxgiData = std::make_unique<DxgiData>(DxgiData{
        .Context   = dxgiContext, 
        .Functions = dvkFunctions,
        .VkContext = dxgiVkContext,
        .Allocator = dxgiAllocator,
        .Window    = window.GetNativeHandle(),
    });

    #endif

    // Swapchain creation:
    CreateSwapchain(true);

    // Allocate command pools for immediate submit:
    mImmGraphicsCommandPool = vkinit::CreateCommandPool(*this, vkb::QueueType::graphics);
}

VulkanContext::~VulkanContext()
{
    vkDestroyCommandPool(Device, mImmGraphicsCommandPool, nullptr);

    #ifdef VULKAN_ON_DXGI
    dvk_destroySwapChain(&mDxgiData->Swapchain);
    dvk_destroyDevice(mDxgiData->Context);

    for (auto imgView : SwapchainImageViews)
    {
        vkDestroyImageView(Device, imgView, nullptr);
    }
    #else
    Swapchain.destroy_image_views(SwapchainImageViews);
    vkb::destroy_swapchain(Swapchain);
    #endif

    vmaDestroyAllocator(Allocator);

    vkb::destroy_device(Device);
    vkb::destroy_surface(Instance, Surface);
    vkb::destroy_instance(Instance);
}

void VulkanContext::CreateSwapchain(bool firstRun)
{
    VkExtent2D extent{.width = RequestedWidth, .height = RequestedHeight};

    #ifdef VULKAN_ON_DXGI
    dvk_swapChainInfo_t dvkInfo{};

    if (firstRun)
    {
        dvk_createSwapChainParameters_t swapChainParameters{};

        swapChainParameters.pFunctions     = &mDxgiData->Functions;
        swapChainParameters.pAllocator     = &mDxgiData->Allocator;
        swapChainParameters.pDeviceContext = &mDxgiData->Context;
        swapChainParameters.pVulkanContext = &mDxgiData->VkContext;
        swapChainParameters.window         = mDxgiData->Window;
        swapChainParameters.width          = static_cast<uint16_t>(extent.width);
        swapChainParameters.height         = static_cast<uint16_t>(extent.height);
        swapChainParameters.format         = VK_FORMAT_R8G8B8A8_UNORM;
        swapChainParameters.imageCount     = FrameInfo::MaxInFlight;
        swapChainParameters.maximumLatency = FrameInfo::MaxInFlight;
        swapChainParameters.syncInterval   = 1;
        //swapChainParameters.flags        = DVK_SWAPCHAIN_BACKBUFFER_BLIT_FLAG;

        swapChainParameters.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        {
            auto res = dvk_createSwapChain(swapChainParameters, &mDxgiData->Swapchain);

            if (res.code != DVK_OK)
            {
                auto msg = std::format("Failed to create dvk swapchain! {}", res.message);
                vpanic(msg);
            }
        }

        {
            auto res = dvk_getSwapChainInfo(mDxgiData->Swapchain, &dvkInfo);
            vassert(res.code == DVK_OK);
        }
    }
    else
    {
        for (auto imageView : SwapchainImageViews)
            vkDestroyImageView(Device, imageView, nullptr);

        SwapchainImages.clear();
        SwapchainImageViews.clear();

        auto res = dvk_resizeSwapChain(&mDxgiData->Swapchain, static_cast<uint16_t>(extent.width), static_cast<uint16_t>(extent.height));

        if (res.code != DVK_OK)
        {
            auto msg = std::format("Failed to resize dvk swapchain! {}", res.message);
            vpanic(msg);
        }

        {
            auto res = dvk_getSwapChainInfo(mDxgiData->Swapchain, &dvkInfo);
            vassert(res.code == DVK_OK);
        }
    }
    
    const auto imageCount = dvkInfo.imageCount;

    SwapchainFormat = dvkInfo.format;
    SwapchainExtent = extent;
    SwapchainImages.resize(imageCount);
    SwapchainImageViews.resize(imageCount);

    for (uint32_t index = 0; index < imageCount; index++)
    {
        auto image = mDxgiData->Swapchain.pSwapChainImages[index];

        SwapchainImages[index] = image;
    }

    for (uint32_t index = 0; index < imageCount; index++)
    {
        VkImageViewCreateInfo imageViewInfo{};
        imageViewInfo.sType                 = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewInfo.image                 = SwapchainImages[index];
        imageViewInfo.viewType              = VK_IMAGE_VIEW_TYPE_2D;
        imageViewInfo.format                = dvkInfo.format;
        imageViewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewInfo.subresourceRange.baseMipLevel   = 0;
        imageViewInfo.subresourceRange.levelCount     = 1;
        imageViewInfo.subresourceRange.baseArrayLayer = 0;
        imageViewInfo.subresourceRange.layerCount     = 1;

        auto ret = vkCreateImageView(Device, &imageViewInfo, nullptr, &SwapchainImageViews[index]);

        vassert(ret == VK_SUCCESS);
    }

    #else

    if (!firstRun)
        Swapchain.destroy_image_views(SwapchainImageViews);

    // To manually specify format:
    //.set_desired_format(VkSurfaceFormatKHR)

    auto swapRet = vkb::SwapchainBuilder(Device)
                       .set_old_swapchain(Swapchain)
                       .set_desired_extent(RequestedWidth, RequestedHeight)
                       .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
                       // To enable blit from secondary render target:
                       .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                       .build();

    if (!swapRet.has_value())
        vpanic(swapRet.error().message());

    vkb::destroy_swapchain(Swapchain);

    Swapchain = swapRet.value();

    SwapchainFormat = Swapchain.image_format; 
    SwapchainExtent = extent;

    SwapchainImages     = Swapchain.get_images().value();
    SwapchainImageViews = Swapchain.get_image_views().value();
    #endif
}

PresentationInfo VulkanContext::AcquireSwapchainImage(
    [[maybe_unused]] FrameResources &frameData, [[maybe_unused]] uint32_t &imageIndex)
{
    if (!SwapchainOk)
        return {};

    #ifdef VULKAN_ON_DXGI
    dvk_presentationInfo_t presentationInfo{};
    auto res = dvk_acquireImage(&mDxgiData->Swapchain, &presentationInfo);

    if (dvk_isError(res))
    {
        SwapchainOk = false;
    }

    imageIndex = presentationInfo.imageIndex;
    mDxgiData->LastPresentInfo = presentationInfo;

    return PresentationInfo{
        .WaitSemaphore   = presentationInfo.waitSemaphore,
        .WaitValue       = presentationInfo.waitValue,
        .SubmitSemaphore = presentationInfo.submitSemaphore,
        .SubmitValue     = presentationInfo.submitValue,
    };

    #else
    VkResult result = vkAcquireNextImageKHR(Device, Swapchain, UINT64_MAX,
                                            frameData.ImageAcquiredSemaphore,
                                            VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        SwapchainOk = false;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        vpanic("Failed to acquire swapchain image!");
    }

    return {};
    #endif
}

void VulkanContext::Present([[maybe_unused]] SwapchainResources &swapResources,
                            [[maybe_unused]] uint32_t           &imageIndex)
{
    #ifdef VULKAN_ON_DXGI
    auto &presentInfo = mDxgiData->LastPresentInfo;

    dvk_presentParameters_t presentParameters{};

    presentParameters.pSwapChain      = presentInfo.pSwapChain;
    presentParameters.pCommandQueue   = presentInfo.pCommandQueue;
    presentParameters.pCommandList    = presentInfo.pCommandList;
    presentParameters.pFrameResources = presentInfo.pFrameResources;
    presentParameters.pFence          = presentInfo.pFence;
    presentParameters.submitValue     = presentInfo.submitValue;
    presentParameters.blitValue       = presentInfo.blitValue;
    presentParameters.allowTearing    = presentInfo.allowTearing;
    presentParameters.syncInterval    = presentInfo.syncInterval;

    auto res = dvk_presentImage(presentParameters);

    vassert(res.code == DVK_OK);

    #else
    VkPresentInfoKHR present_info   = {};
    present_info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores    = &swapResources.RenderCompletedSemaphore;
    present_info.swapchainCount     = 1;
    present_info.pSwapchains        = &Swapchain.swapchain;
    present_info.pImageIndices      = &imageIndex;

    VkResult result = vkQueuePresentKHR(Queues.Present, &present_info);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        SwapchainOk = false;
        return;
    }
    else if (result != VK_SUCCESS)
    {
        vpanic("Failed to present swapchain image!");
    }
    #endif
}

void VulkanContext::ImmediateSubmitGraphics(
    std::function<void(VkCommandBuffer)> &&function)
{
    VkCommandBuffer buffer =
        vkinit::AllocateCommandBuffer(*this, mImmGraphicsCommandPool);

    vkutils::BeginRecording(buffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    function(buffer);

    vkutils::EndRecording(buffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &buffer;

    vkQueueSubmit(Queues.Graphics, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(Queues.Graphics);

    vkFreeCommandBuffers(Device, mImmGraphicsCommandPool, 1, &buffer);
}