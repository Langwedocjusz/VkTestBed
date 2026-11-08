#include "Swapchain.h"
#include "Pch.h"

#include "Vassert.h"

#include "VkBootstrap.h"
#include "volk.h"

// === DXGI interop swapchain implementation: ========

#ifdef VULKAN_ON_DXGI
#define NOMINMAX
#include <windows.h>

#define DX_SWAP_VK_IMPLEMENTATION
#include "vk_dxgi.h"

static void *dxgiAlloc(size_t size, size_t alignment, void *)
{
    return _aligned_malloc(size, alignment);
}

static void dxgiFree(void *address, void *)
{
    _aligned_free(address);
}

struct Swapchain::DxgiContext {
    dvk_dxgiDeviceContext_t Context;
    dvk_functions_t         Functions;
    dvk_vulkanContext_t     VkContext;
    dvk_allocator_t         Allocator;
    HWND                    Window;
};

struct Swapchain::DxgiImpl {
    dvk_dxgiSwapChain_t Swapchain;
    dvk_swapChainInfo_t Info;
    // Presentation info returned
    // by last frame's acquire image:
    dvk_presentationInfo_t LastPresentInfo;
    bool                   UsesBlit;

    DxgiImpl(Swapchain::DxgiContext &ctx, VkExtent2D extent, bool useBlit)
        : UsesBlit(useBlit)
    {
        dvk_createSwapChainParameters_t swapChainParameters{};

        auto width  = static_cast<uint16_t>(extent.width);
        auto height = static_cast<uint16_t>(extent.height);

        swapChainParameters.pFunctions     = &ctx.Functions;
        swapChainParameters.pAllocator     = &ctx.Allocator;
        swapChainParameters.pDeviceContext = &ctx.Context;
        swapChainParameters.pVulkanContext = &ctx.VkContext;
        swapChainParameters.window         = ctx.Window;
        swapChainParameters.width          = width;
        swapChainParameters.height         = height;
        swapChainParameters.format         = VK_FORMAT_R8G8B8A8_UNORM;
        swapChainParameters.imageCount     = FrameInfo::MaxInFlight;
        swapChainParameters.maximumLatency = FrameInfo::MaxInFlight;
        swapChainParameters.syncInterval   = 1;

        if (useBlit)
        {
            swapChainParameters.flags = DVK_SWAPCHAIN_BACKBUFFER_BLIT_FLAG;
        }

        swapChainParameters.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        {
            auto res = dvk_createSwapChain(swapChainParameters, &Swapchain);

            if (res.code != DVK_OK)
            {
                auto msg = std::string("Failed to create dvk swapchain! ") + res.message;
                vpanic(msg);
            }
        }

        {
            auto res = dvk_getSwapChainInfo(Swapchain, &Info);
            vassert(res.code == DVK_OK);
        }
    }

    ~DxgiImpl()
    {
        dvk_destroySwapChain(&Swapchain);
    }
};

void Swapchain::InitDxgi(SystemWindow &window)
{
    vassert(mDxgiCtx == nullptr);

    // Set up the function table:
    const auto getMemoryWin32HandleProperties = reinterpret_cast<PFN_vkGetMemoryWin32HandlePropertiesKHR>(
        vkGetDeviceProcAddr(mCtx.Device, "vkGetMemoryWin32HandlePropertiesKHR")
    );

    const auto importSemaphoreWin32Handle = reinterpret_cast<PFN_vkImportSemaphoreWin32HandleKHR>(
        vkGetDeviceProcAddr(mCtx.Device, "vkImportSemaphoreWin32HandleKHR")
    );

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
    idProperties.sType                        = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;

    VkPhysicalDeviceProperties2 properties = {};
    properties.sType                       = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties.pNext                       = &idProperties;

    vkGetPhysicalDeviceProperties2(mCtx.PhysicalDevice, &properties);

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
    auto                    result = dvk_createDevice(deviceParameters, &dxgiContext);

    vassert(result.code == DVK_OK);

    dvk_vulkanContext_t dxgiVkContext{
        .physicalDevice = mCtx.PhysicalDevice,
        .device         = mCtx.Device,
    };

    dvk_allocator_t dxgiAllocator{
        .allocate = dxgiAlloc,
        .free     = dxgiFree,
    };

    mDxgiCtx = std::make_unique<DxgiContext>();

    *mDxgiCtx = DxgiContext{
        .Context   = dxgiContext,
        .Functions = dvkFunctions,
        .VkContext = dxgiVkContext,
        .Allocator = dxgiAllocator,
        .Window    = window.GetNativeHandle(),
    };
}

void Swapchain::RecreateDxgi(VkExtent2D extent)
{
    vassert(mDxgiCtx != nullptr);

    if (mDxgiImpl == nullptr)
    {
        mDxgiImpl = std::make_unique<DxgiImpl>(*mDxgiCtx, extent, Cfg.Dxgi.UseBlit);
    }
    else if (mDxgiImpl->UsesBlit != Cfg.Dxgi.UseBlit)
    {
        mDxgiImpl.reset();
        mDxgiImpl = std::make_unique<DxgiImpl>(*mDxgiCtx, extent, Cfg.Dxgi.UseBlit);
    }
    else
    {
        Images.clear();
        DestroyImageViews();

        auto width  = static_cast<uint16_t>(extent.width);
        auto height = static_cast<uint16_t>(extent.height);

        auto res = dvk_resizeSwapChain(&mDxgiImpl->Swapchain, width, height);

        if (res.code != DVK_OK)
        {
            auto msg = std::string("Failed to resize dvk swapchain! ") + res.message;
            vpanic(msg);
        }

        {
            auto res = dvk_getSwapChainInfo(mDxgiImpl->Swapchain, &mDxgiImpl->Info);
            vassert(res.code == DVK_OK);
        }
    }

    const auto imageCount = mDxgiImpl->Info.imageCount;

    Format = mDxgiImpl->Info.format;
    Extent = extent;

    Images.resize(imageCount);
    ImageViews.resize(imageCount);

    for (uint32_t index = 0; index < imageCount; index++)
    {
        auto image    = mDxgiImpl->Swapchain.pSwapChainImages[index];
        Images[index] = image;
    }

    for (uint32_t index = 0; index < imageCount; index++)
    {
        VkImageViewCreateInfo imageViewInfo{};
        imageViewInfo.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewInfo.image            = Images[index];
        imageViewInfo.viewType         = VK_IMAGE_VIEW_TYPE_2D;
        imageViewInfo.format           = mDxgiImpl->Info.format;
        imageViewInfo.subresourceRange = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1,
        };

        auto ret = vkCreateImageView(mCtx.Device, &imageViewInfo, nullptr, &ImageViews[index]);

        vassert(ret == VK_SUCCESS);
    }
}

PresentationInfo Swapchain::AcquireDxgi(uint32_t &imageIndex)
{
    dvk_presentationInfo_t presentationInfo{};
    auto                   res = dvk_acquireImage(&mDxgiImpl->Swapchain, &presentationInfo);

    if (dvk_isError(res))
    {
        Ok = false;
    }

    imageIndex                 = presentationInfo.imageIndex;
    mDxgiImpl->LastPresentInfo = presentationInfo;

    return PresentationInfo{
        .WaitSemaphore   = presentationInfo.waitSemaphore,
        .WaitValue       = presentationInfo.waitValue,
        .SubmitSemaphore = presentationInfo.submitSemaphore,
        .SubmitValue     = presentationInfo.submitValue,
    };
}

void Swapchain::PresentDxgi()
{
    auto &presentInfo = mDxgiImpl->LastPresentInfo;

    dvk_presentParameters_t params{};

    params.pSwapChain      = presentInfo.pSwapChain;
    params.pCommandQueue   = presentInfo.pCommandQueue;
    params.pCommandList    = presentInfo.pCommandList;
    params.pFrameResources = presentInfo.pFrameResources;
    params.pFence          = presentInfo.pFence;
    params.submitValue     = presentInfo.submitValue;
    params.blitValue       = presentInfo.blitValue;
    params.allowTearing    = presentInfo.allowTearing;
    params.syncInterval    = presentInfo.syncInterval;

    auto res = dvk_presentImage(params);

    vassert(res.code == DVK_OK);
}

#endif

// === Native vulkan swapchain implementation: =======

struct Swapchain::NativeImpl {
    vkb::Swapchain Swapchain;

    NativeImpl(Swapchain::Context &ctx, VkExtent2D extent, VkPresentModeKHR presentMode)
    {
        // To manually specify format:
        //.set_desired_format(VkSurfaceFormatKHR)

        auto builder = vkb::SwapchainBuilder(
            ctx.PhysicalDevice, ctx.Device, ctx.Surface, ctx.GraphicsQueueIdx, ctx.PresentQueueIdx
        );

        auto swapRet = builder.set_old_swapchain(Swapchain)
                           .set_desired_extent(extent.width, extent.height)
                           .set_desired_present_mode(presentMode)
                           // To enable blit from secondary render target:
                           .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                           .build();

        if (!swapRet.has_value())
            vpanic(swapRet.error().message());

        Swapchain = swapRet.value();
    }

    ~NativeImpl()
    {
        vkb::destroy_swapchain(Swapchain);
    }
};

#ifdef VULKAN_ON_DXGI
bool Swapchain::IsDxgi() const
{
    return mDxgiImpl != nullptr;
}
#endif

void Swapchain::RecreateNative(VkExtent2D extent)
{
    DestroyImageViews();

    mNativeImpl.reset();
    mNativeImpl = std::make_unique<NativeImpl>(mCtx, extent, Cfg.Native.PresentMode);

    Format = mNativeImpl->Swapchain.image_format;
    Extent = extent;

    Images     = mNativeImpl->Swapchain.get_images().value();
    ImageViews = mNativeImpl->Swapchain.get_image_views().value();
}

PresentationInfo Swapchain::AcquireNative(FrameResources &frameData, uint32_t &imageIndex)
{
    VkResult result = vkAcquireNextImageKHR(
        mCtx.Device, mNativeImpl->Swapchain, UINT64_MAX, frameData.ImageAcquiredSemaphore, VK_NULL_HANDLE, &imageIndex
    );

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        Ok = false;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        vpanic("Failed to acquire swapchain image!");
    }

    return {};
}

void Swapchain::PresentNative(SwapchainResources &swapResources, uint32_t &imageIndex)
{
    VkPresentInfoKHR present_info   = {};
    present_info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores    = &swapResources.RenderCompletedSemaphore;
    present_info.swapchainCount     = 1;
    present_info.pSwapchains        = &mNativeImpl->Swapchain.swapchain;
    present_info.pImageIndices      = &imageIndex;

    VkResult result = vkQueuePresentKHR(mCtx.PresentQueueHandle, &present_info);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        Ok = false;
        return;
    }
    else if (result != VK_SUCCESS)
    {
        vpanic("Failed to present swapchain image!");
    }
}

void Swapchain::DestroyImageViews()
{
    for (auto imageView : ImageViews)
    {
        vkDestroyImageView(mCtx.Device, imageView, nullptr);
    }

    ImageViews.clear();
}

// === Common wrapper around both: ===================

Swapchain::Swapchain(
    SystemWindow    &window,
    VkPhysicalDevice physicalDevice,
    VkDevice         device,
    VkSurfaceKHR     surface,
    uint32_t         graphicsQueueIdx,
    uint32_t         presentQueueIdx,
    VkQueue          presentQueueHandle
)
{
    mCtx = {
        .PhysicalDevice     = physicalDevice,
        .Device             = device,
        .Surface            = surface,
        .GraphicsQueueIdx   = graphicsQueueIdx,
        .PresentQueueIdx    = presentQueueIdx,
        .PresentQueueHandle = presentQueueHandle,
    };

    {
        int32_t width, height;
        window.GetWindowSize(width, height);

        Cfg.RequestedWidth  = width;
        Cfg.RequestedHeight = height;
    }

#ifdef VULKAN_ON_DXGI
    InitDxgi(window);
#endif
}

Swapchain::~Swapchain()
{
    mNativeImpl.reset();

#ifdef VULKAN_ON_DXGI
    {
        mDxgiImpl.reset();
        dvk_destroyDevice(mDxgiCtx->Context);
    }
#endif

    DestroyImageViews();
}

void Swapchain::Recreate()
{
    VkExtent2D extent{
        .width  = Cfg.RequestedWidth,
        .height = Cfg.RequestedHeight,
    };

#ifdef VULKAN_ON_DXGI
    if (Cfg.UseDxgi)
    {
        mNativeImpl.reset();
        RecreateDxgi(extent);
    }
    else
    {
        mDxgiImpl.reset();
        RecreateNative(extent);
    }
#else
    RecreateNative(extent);
#endif
}

PresentationInfo Swapchain::Acquire(FrameResources &frameData, uint32_t &imageIndex)
{
    if (Ok)
    {
        if (mNativeImpl)
        {
            return AcquireNative(frameData, imageIndex);
        }
#ifdef VULKAN_ON_DXGI
        else if (mDxgiImpl)
        {
            return AcquireDxgi(imageIndex);
        }
#endif
        else
        {
            vpanic("No swapchain implementation ready to acquire image!");
        }
    }

    return {};
}

void Swapchain::Present(SwapchainResources &swapResources, uint32_t &imageIndex)
{
    if (mNativeImpl)
    {
        return PresentNative(swapResources, imageIndex);
    }
#ifdef VULKAN_ON_DXGI
    else if (mDxgiImpl)
    {
        return PresentDxgi();
    }
#endif
    else
    {
        vpanic("No swapchain implementation ready to present!");
    }
}

std::vector<const char *> Swapchain::GetRequiredInstanceExtensions()
{
#ifdef VULKAN_ON_DXGI
    return {"VK_KHR_win32_surface"};
#else
    return {};
#endif
}

std::span<const char *> Swapchain::GetRequiredDeviceExtensions()
{
#ifdef VULKAN_ON_DXGI
    return dvk_required_vulkan_extensions;
#else
    return {};
#endif
}