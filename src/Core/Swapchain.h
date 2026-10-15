#pragma once

#include "Frame.h"
#include "SystemWindow.h"

#include <memory>
#include <span>

struct PresentationInfo {
	VkSemaphore WaitSemaphore = VK_NULL_HANDLE;
	uint64_t    WaitValue = 0;
	VkSemaphore SubmitSemaphore = VK_NULL_HANDLE;
	uint64_t    SubmitValue = 0;
};

class Swapchain {
public:
	Swapchain(
		SystemWindow &window,
		VkPhysicalDevice physicalDevice,
		VkDevice device,
		VkSurfaceKHR surface,
		uint32_t graphicsQueueIdx,
		uint32_t presentQueueIdx,
		VkQueue presentQueueHandle
	);
	~Swapchain();

	void Recreate();
	PresentationInfo Acquire(FrameResources& frameData, uint32_t& imageIndex);
	void Present(SwapchainResources& swapResources, uint32_t& imageIndex);

	#ifdef VULKAN_ON_DXGI
	[[nodiscard]] bool IsDxgi() const;
	#endif

	static std::vector<const char*> GetRequiredInstanceExtensions();
	static std::span<const char*> GetRequiredDeviceExtensions();

public:
	VkFormat   Format;
	VkExtent2D Extent;

	std::vector<VkImage>     Images;
	std::vector<VkImageView> ImageViews;

	bool Ok = true;

	struct {
		uint32_t RequestedWidth  = 800;
		uint32_t RequestedHeight = 600;

		struct {
			VkPresentModeKHR PresentMode = VK_PRESENT_MODE_FIFO_KHR; 
		} Native;

		#ifdef VULKAN_ON_DXGI
		struct {
			bool UseBlit = true;
		} Dxgi;

		bool UseDxgi = true;
		#endif	
	} Cfg;

private:
	struct Context {
		VkPhysicalDevice PhysicalDevice;
		VkDevice         Device;
		VkSurfaceKHR     Surface;
		uint32_t         GraphicsQueueIdx;
		uint32_t         PresentQueueIdx;
		VkQueue          PresentQueueHandle;
	} mCtx;

	struct NativeImpl;
	std::unique_ptr<NativeImpl> mNativeImpl;

	void RecreateNative(VkExtent2D extent);
	PresentationInfo AcquireNative(FrameResources& frameData, uint32_t& imageIndex);
	void PresentNative(SwapchainResources& swapResources, uint32_t& imageIndex);

	#ifdef VULKAN_ON_DXGI
	struct DxgiContext;
	std::unique_ptr<DxgiContext> mDxgiCtx;

	struct DxgiImpl;
	std::unique_ptr<DxgiImpl> mDxgiImpl;

	void InitDxgi(SystemWindow& window);
	void RecreateDxgi(VkExtent2D extent);
	PresentationInfo AcquireDxgi(uint32_t& imageIndex);
	void PresentDxgi();
	#endif

	void DestroyImageViews();
};