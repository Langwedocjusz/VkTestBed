/*
=============================================================================================
    DXGI-VULKAN SWAPCHAIN INTEROP LIBRARY (dvk)
    VERSION 1.3.1
=============================================================================================

    A header-only C++ library for managing a DXGI swapchain within a Vulkan
    rendering context on Windows.

    Why use this?
    Vulkan's native Win32 presentation can sometimes struggle with precise frame pacing,
    latency control, and tearing behaviors. By delegating the swapchain and presentation
    to DXGI (DirectX Graphics Infrastructure) and rendering with Vulkan, you gain access
    to DXGI Waitable Objects for perfect CPU-to-display pacing and robust HDR/VRR support,
    while keeping your rendering pipeline purely in Vulkan.

=============================================================================================
    CHANGELOG
=============================================================================================

    v1.3.1
      - Increase robustness of the swapchain blit path
      - Ensure the blit path is selected if unordered access usage is required for the framebuffer

    v1.3.0
      - Add an option to copy to the present target instead of sharing the resource

    v1.2.1
      - Make failing to acquire the debug layers not a hard fail

    v1.2.0
      - Add support for changing the swap chain format

    v1.1.1
      - Minor type cleanup
      - Fix missing swap chain deletion

    v1.1.0
      - Add support for external swap chain images
      - Add a method to query the swap chain information

    v1.0.0
      - Initial release

=============================================================================================
    PREREQUISITES
=============================================================================================

    * VULKAN EXTENSIONS: Your Vulkan device MUST be created with the following extensions
        - VK_KHR_external_memory_win32
        - VK_KHR_external_semaphore_win32
        - VK_KHR_timeline_semaphore

    * TIMELINE SEMAPHORES: You must enable the `timelineSemaphore` feature when creating
      your Vulkan Logical Device.

=============================================================================================
*/

#ifndef DX_SWAP_VK_H
#  define DX_SWAP_VK_H 1

#  include <vulkan/vulkan.h>
#  include <vulkan/vulkan_win32.h>

#  ifndef DVK_PREPEND_MESSAGE
#    define DVK_PREPEND_MESSAGE "[DVK] "
#  endif
#  define DVK_PREPEND( str ) DVK_PREPEND_MESSAGE str

static const char * dvk_required_vulkan_extensions[] = { VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
                                                         VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME,
                                                         VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME };

struct IUnknown;
struct IDXGIFactory5;
struct IDXGISwapChain3;
struct ID3D12Device5;
struct ID3D12CommandAllocator;
struct ID3D12CommandQueue;
struct ID3D12GraphicsCommandList;
struct ID3D12Resource;
struct ID3D12Fence;
enum D3D_FEATURE_LEVEL : int;

typedef struct
{
  void * ( *allocate )( size_t size, size_t alignment, void * pContext );
  void ( *free )( void * pAddress, void * pContext );

  void * pContext;
} dvk_allocator_t;

typedef HRESULT( WINAPI * PFN_CREATE_DXGIFACTORY2 )( UINT, REFIID, void ** );
typedef HRESULT( WINAPI * PFN_D3D12_CREATE_DEVICE )( _In_opt_ IUnknown *, D3D_FEATURE_LEVEL, _In_ REFIID, _COM_Outptr_opt_ void ** );
typedef HRESULT( WINAPI * PFN_D3D12_GET_DEBUG_INTERFACE )( _In_ REFIID, _COM_Outptr_opt_ void ** );

typedef struct dvk_functions_t
{
  PFN_CREATE_DXGIFACTORY2       pCreateFactory     = nullptr;
  PFN_D3D12_CREATE_DEVICE       pCreateDevice      = nullptr;
  PFN_D3D12_GET_DEBUG_INTERFACE pGetDebugInterface = nullptr;

  PFN_vkCreateImage                       vkCreateImage                       = nullptr;
  PFN_vkGetMemoryWin32HandlePropertiesKHR vkGetMemoryWin32HandlePropertiesKHR = nullptr;
  PFN_vkGetImageMemoryRequirements        vkGetImageMemoryRequirements        = nullptr;
  PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties = nullptr;
  PFN_vkAllocateMemory                    vkAllocateMemory                    = nullptr;
  PFN_vkBindImageMemory                   vkBindImageMemory                   = nullptr;
  PFN_vkCreateSemaphore                   vkCreateSemaphore                   = nullptr;
  PFN_vkImportSemaphoreWin32HandleKHR     vkImportSemaphoreWin32HandleKHR     = nullptr;
  PFN_vkDestroyImage                      vkDestroyImage                      = nullptr;
  PFN_vkFreeMemory                        vkFreeMemory                        = nullptr;
  PFN_vkDestroySemaphore                  vkDestroySemaphore                  = nullptr;
  PFN_vkDeviceWaitIdle                    vkDeviceWaitIdle                    = nullptr;
} dvk_functions_t;

typedef struct dvk_vulkanContext_t
{
  VkPhysicalDevice        physicalDevice       = VK_NULL_HANDLE;
  VkDevice                device               = VK_NULL_HANDLE;
  VkAllocationCallbacks * pAllocationCallbacks = nullptr;
} dvk_vulkanContext_t;

typedef struct dvk_dxgiDeviceContext_t
{
  IDXGIFactory5 *             pDxgiFactory  = nullptr;
  ID3D12Device5 *             pD3D12Device  = nullptr;
  ID3D12CommandQueue *        pCommandQueue = nullptr;
  ID3D12GraphicsCommandList * pCommandList  = nullptr;
} dvk_dxgiDeviceContext_t;

typedef struct dvk_sharedImage_t
{
  ID3D12Resource * pImage   = nullptr;
  VkImage *        pVkImage = nullptr;
  VkDeviceMemory   memory   = VK_NULL_HANDLE;
  HANDLE           handle   = INVALID_HANDLE_VALUE;
} dvk_sharedImage_t;

typedef struct dvk_perFrameResources_t
{
  dvk_sharedImage_t        sharedImage       = {};
  ID3D12CommandAllocator * pCommandAllocator = nullptr;
  uint64_t                 lastSubmitValue   = 0;
  uint64_t                 lastBlitValue     = 0;
} dvk_perFrameResources_t;

typedef enum dvk_swapChainFlagBits_e
{
  DVK_SWAPCHAIN_FLAG_NONE            = 0x00000000,
  DVK_SWAPCHAIN_ALLOW_TEARING_FLAG   = 0x00000001,
  DVK_SWAPCHAIN_BACKBUFFER_BLIT_FLAG = 0x00000002,
} dvk_swapChainFlagBits_e;

typedef uint8_t dvk_swapChainFlags_e;

typedef struct dvk_dxgiSwapChain_t
{
  dvk_functions_t *         pFunctions     = nullptr;
  dvk_allocator_t *         pAllocator     = nullptr;
  dvk_dxgiDeviceContext_t * pDeviceContext = nullptr;
  dvk_vulkanContext_t *     pVulkanContext = nullptr;

  IDXGISwapChain3 * pSwapChain             = nullptr;
  HANDLE            presentationWaitHandle = NULL;

  dvk_perFrameResources_t * pPerFrameResources           = nullptr;
  size_t                    pImportedSwapChainImageCount = 0;

  VkImage           vkBlitImage = VK_NULL_HANDLE;
  dvk_sharedImage_t pBlitImage  = {};

  VkImage * pExternalSwapChainImages = nullptr;
  VkImage * pSwapChainImages         = nullptr;

  ID3D12Fence * pFence            = nullptr;
  HANDLE        fenceSharedHandle = INVALID_HANDLE_VALUE;
  VkSemaphore   semaphore         = VK_NULL_HANDLE;
  uint64_t      fenceValue        = 0;

  VkImageUsageFlags usage        = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  uint32_t          syncInterval = 1;

  dvk_swapChainFlags_e flags = DVK_SWAPCHAIN_FLAG_NONE;
} dvk_dxgiSwapChain_t;

typedef struct dvk_createDeviceParameters_t
{
  dvk_functions_t * pFunctions = nullptr;

  LUID deviceLuid        = {};
  bool enableDebugLayers = false;

} dvk_createDeviceParameters_t;

typedef struct dvk_createSwapChainParameters_t
{
  dvk_functions_t *         pFunctions     = nullptr;
  dvk_allocator_t *         pAllocator     = nullptr;
  dvk_dxgiDeviceContext_t * pDeviceContext = nullptr;
  dvk_vulkanContext_t *     pVulkanContext = nullptr;

  HWND      window                   = {};
  VkImage * pExternalSwapChainImages = nullptr;

  uint16_t width  = 1;
  uint16_t height = 1;

  VkFormat          format = VK_FORMAT_R8G8B8A8_UNORM;
  VkImageUsageFlags usage  = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  uint8_t imageCount     = 2;
  uint8_t maximumLatency = 1;

  uint32_t             syncInterval = 1;
  dvk_swapChainFlags_e flags        = DVK_SWAPCHAIN_FLAG_NONE;
} dvk_createSwapChainParameters_t;

typedef struct dvk_presentationInfo_t
{
  IDXGISwapChain3 *           pSwapChain      = nullptr;
  ID3D12CommandQueue *        pCommandQueue   = nullptr;
  ID3D12GraphicsCommandList * pCommandList    = nullptr;
  dvk_perFrameResources_t *   pFrameResources = nullptr;

  ID3D12Fence * pFence          = nullptr;
  uint32_t      imageIndex      = 0;
  VkSemaphore   waitSemaphore   = VK_NULL_HANDLE;
  uint64_t      waitValue       = 0;
  VkSemaphore   submitSemaphore = VK_NULL_HANDLE;
  uint64_t      submitValue     = 0;
  uint64_t      blitValue       = 0;

  bool     allowTearing = false;
  uint32_t syncInterval = 1;
} dvk_presentationInfo_t;

typedef struct dvk_presentParameters_t
{
  IDXGISwapChain3 *           pSwapChain      = nullptr;
  ID3D12CommandQueue *        pCommandQueue   = nullptr;
  ID3D12GraphicsCommandList * pCommandList    = nullptr;
  dvk_perFrameResources_t *   pFrameResources = nullptr;

  ID3D12Fence * pFence      = nullptr;
  uint64_t      submitValue = 0;
  uint64_t      blitValue   = 0;

  bool     allowTearing = false;
  uint32_t syncInterval = 1;

} dvk_presentParameters_t;

typedef struct dvk_swapChainInfo_t
{
  uint8_t imageCount;

  uint16_t width;
  uint16_t height;

  VkFormat format;
} dvk_swapChainInfo_t;

typedef enum dvk_result_e
{
  DVK_OK,
  DVK_ERROR,
  DVK_ERROR_VULKAN,
  DVK_ERROR_DXGI,
  DVK_ERROR_DX12,
  DVK_ERROR_ALLOCATOR,
} dvk_result_e;

typedef struct dvk_result_t
{
  dvk_result_e code;
  char         message[128];
} dvk_result_t;

[[nodiscard]] static bool dvk_isError( const dvk_result_t & result ) noexcept
{
  return result.code >= DVK_ERROR;
}

[[nodiscard]] static dvk_result_t dvk_createDevice( const dvk_createDeviceParameters_t & parameters, dvk_dxgiDeviceContext_t * pContext ) noexcept;
static void                       dvk_destroyDevice( const dvk_dxgiDeviceContext_t & context ) noexcept;

[[nodiscard]] static dvk_result_t dvk_createSwapChain( const dvk_createSwapChainParameters_t & parameters, dvk_dxgiSwapChain_t * pSwapChain ) noexcept;
static void                       dvk_destroySwapChain( dvk_dxgiSwapChain_t * pSwapChain ) noexcept;

[[nodiscard]] static dvk_result_t dvk_resizeSwapChain( dvk_dxgiSwapChain_t * pSwapChain, uint16_t width, uint16_t height ) noexcept;
[[nodiscard]] static dvk_result_t dvk_changeSwapChainFormat( dvk_dxgiSwapChain_t * pSwapChain, VkFormat format ) noexcept;

[[nodiscard]] static dvk_result_t dvk_acquireImage( dvk_dxgiSwapChain_t * pSwapChain, dvk_presentationInfo_t * pPresentationInfo ) noexcept;
[[nodiscard]] static dvk_result_t dvk_presentImage( const dvk_presentParameters_t & presentParameters ) noexcept;

[[nodiscard]] static dvk_result_t dvk_getSwapChainInfo( const dvk_dxgiSwapChain_t & swapChain, dvk_swapChainInfo_t * pInfo ) noexcept;

#endif

#ifdef DX_SWAP_VK_IMPLEMENTATION

#  include <d3d12.h>
#  include <dxgi1_6.h>
#  include <stdio.h>

// =======================
// Internal functions
static dvk_result_t         __dvk_createSwapChainObjects( dvk_dxgiSwapChain_t * pSwapChain ) noexcept;
static void                 __dvk_destroySwapChainObjects( dvk_dxgiSwapChain_t * pSwapChain ) noexcept;
static dvk_result_t         __dvk_createSharedImage( const dvk_dxgiSwapChain_t * pSwapChain, dvk_sharedImage_t * pSharedImage ) noexcept;
static void                 __dvk_destroySharedImage( const dvk_dxgiSwapChain_t * pSwapChain, dvk_sharedImage_t * pSharedImage ) noexcept;
inline DXGI_FORMAT          __dvk_toDxgiFormat( VkFormat format ) noexcept;
inline VkFormat             __dvk_toVkFormat( DXGI_FORMAT format ) noexcept;
inline DXGI_USAGE           __dvk_toDxgiUsage( VkImageUsageFlags usage ) noexcept;
inline D3D12_RESOURCE_FLAGS __dvk_toD3D12Usage( VkImageUsageFlags usage ) noexcept;
inline VkImageUsageFlags    __dvk_toVkUsage( DXGI_USAGE usage ) noexcept;
static dvk_result_t         __dvk_waitIdle( dvk_dxgiSwapChain_t * pSwapChain ) noexcept;
static bool                 __dvk_hasSwapChainFlag( dvk_swapChainFlags_e flags, dvk_swapChainFlagBits_e testFlag ) noexcept;

// =======================

static dvk_result_t dvk_createDevice( const dvk_createDeviceParameters_t & parameters, dvk_dxgiDeviceContext_t * pContext ) noexcept
{
    HRESULT result = parameters.pFunctions->pCreateFactory(
        DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&pContext->pDxgiFactory));
  if ( FAILED( result ) )
  {
    dvk_destroyDevice( *pContext );
    return dvk_result_t{
      .code    = DVK_ERROR_DXGI,
      .message = DVK_PREPEND( "Failed to create the DXGI factory" ),
    };
  }

  if ( parameters.enableDebugLayers )
  {
    ID3D12Debug * pDebugController;
    result = parameters.pFunctions->pGetDebugInterface( IID_PPV_ARGS( &pDebugController ) );
    if ( SUCCEEDED( result ) )
    {
      ID3D12Debug1 * pDebugController1;
      result = pDebugController->QueryInterface( IID_PPV_ARGS( &pDebugController1 ) );
      if ( SUCCEEDED( result ) )
      {
        pDebugController1->EnableDebugLayer();
        pDebugController1->SetEnableGPUBasedValidation( true );
        pDebugController1->Release();
      }

      pDebugController->Release();
    }
  }

  IDXGIAdapter * pDxgiAdapter = nullptr;
  result                      = pContext->pDxgiFactory->EnumAdapterByLuid( parameters.deviceLuid, IID_PPV_ARGS( &pDxgiAdapter ) );
  if ( FAILED( result ) )
  {
    dvk_destroyDevice( *pContext );
    return dvk_result_t{
      .code    = DVK_ERROR_DXGI,
      .message = DVK_PREPEND( "Failed to get the adapter by LUID" ),
    };
  }

  result = parameters.pFunctions->pCreateDevice( pDxgiAdapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS( &pContext->pD3D12Device ) );
  if ( FAILED( result ) )
  {
    dvk_destroyDevice( *pContext );
    pDxgiAdapter->Release();
    return dvk_result_t{
      .code    = DVK_ERROR_DX12,
      .message = DVK_PREPEND( "Failed to create the DX12 device" ),
    };
  }
  pDxgiAdapter->Release();

  constexpr D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {
    D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_QUEUE_PRIORITY_NORMAL, D3D12_COMMAND_QUEUE_FLAG_NONE, 0
  };

  result = pContext->pD3D12Device->CreateCommandQueue( &commandQueueDesc, IID_PPV_ARGS( &pContext->pCommandQueue ) );
  if ( FAILED( result ) )
  {
    dvk_destroyDevice( *pContext );
    return dvk_result_t{
      .code    = DVK_ERROR_DX12,
      .message = DVK_PREPEND( "Failed to create the DX12 command queue" ),
    };
  }

  result =
    pContext->pD3D12Device->CreateCommandList1( 0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS( &pContext->pCommandList ) );
  if ( FAILED( result ) )
  {
    dvk_destroyDevice( *pContext );
    return dvk_result_t{
      .code    = DVK_ERROR_DX12,
      .message = DVK_PREPEND( "Failed to create the DX12 command list" ),
    };
  }

  return dvk_result_t{ .code = DVK_OK };
}

static void dvk_destroyDevice( const dvk_dxgiDeviceContext_t & context ) noexcept
{
  if ( context.pCommandList != nullptr )
  {
    context.pCommandList->Release();
  }
  if ( context.pCommandQueue != nullptr )
  {
    context.pCommandQueue->Release();
  }
  if ( context.pD3D12Device != nullptr )
  {
    context.pD3D12Device->Release();
  }
  if ( context.pDxgiFactory != nullptr )
  {
    context.pDxgiFactory->Release();
  }
}

static dvk_result_t dvk_createSwapChain( const dvk_createSwapChainParameters_t & parameters, dvk_dxgiSwapChain_t * pSwapChain ) noexcept
{
  dvk_swapChainFlags_e modifiedFlags = parameters.flags;
  // DX12 forbids unordered access to presentable resources, if the application requires this usage, force blit mode
  if ( parameters.usage & VK_IMAGE_USAGE_STORAGE_BIT )
  {
    modifiedFlags |= DVK_SWAPCHAIN_BACKBUFFER_BLIT_FLAG;
  }

  pSwapChain->pFunctions               = parameters.pFunctions;
  pSwapChain->pAllocator               = parameters.pAllocator;
  pSwapChain->pDeviceContext           = parameters.pDeviceContext;
  pSwapChain->pVulkanContext           = parameters.pVulkanContext;
  pSwapChain->syncInterval             = parameters.syncInterval;
  pSwapChain->flags                    = modifiedFlags;
  pSwapChain->pExternalSwapChainImages = parameters.pExternalSwapChainImages;
  pSwapChain->usage                    = parameters.usage;

  const DXGI_FORMAT format = __dvk_toDxgiFormat( parameters.format );
  if ( format == DXGI_FORMAT_UNKNOWN )
  {
    return dvk_result_t{ .code = DVK_ERROR, .message = DVK_PREPEND( "Invalid swap chain format" ) };
  }

  const DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {
    .Width  = parameters.width,
    .Height = parameters.height,
    .Format = format,
    .Stereo = false,
    .SampleDesc =
      DXGI_SAMPLE_DESC{
        .Count   = 1,
        .Quality = 0,
      },
    .BufferUsage =
      __dvk_hasSwapChainFlag( pSwapChain->flags, DVK_SWAPCHAIN_BACKBUFFER_BLIT_FLAG ) ? DXGI_USAGE_RENDER_TARGET_OUTPUT : __dvk_toDxgiUsage( parameters.usage ),
    .BufferCount = parameters.imageCount,
    .Scaling     = DXGI_SCALING_NONE,
    .SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD,
    .AlphaMode   = DXGI_ALPHA_MODE_IGNORE,
    .Flags       = ( __dvk_hasSwapChainFlag( parameters.flags, DVK_SWAPCHAIN_ALLOW_TEARING_FLAG ) ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u ) |
             DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT,
  };

  HRESULT result = parameters.pDeviceContext->pDxgiFactory->CreateSwapChainForHwnd(
    parameters.pDeviceContext->pCommandQueue, parameters.window, &swapChainDesc, NULL, NULL, (IDXGISwapChain1 **)&pSwapChain->pSwapChain );

  if ( FAILED( result ) )
  {
    printf("CreateSwapChainForHwnd failed: 0x%08X\n", (unsigned)result);

    dvk_destroySwapChain( pSwapChain );
    return dvk_result_t{
      .code    = DVK_ERROR_DX12,
      .message = DVK_PREPEND( "Failed to create the DX12 swap chain" ),
    };
  }

  pSwapChain->presentationWaitHandle = pSwapChain->pSwapChain->GetFrameLatencyWaitableObject();
  // The failure case is defined as `NULL` in the specs:
  // https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_3/nf-dxgi1_3-idxgiswapchain2-getframelatencywaitableobject#return-value
  if ( pSwapChain->presentationWaitHandle == NULL )
  {
    dvk_destroySwapChain( pSwapChain );
    return dvk_result_t{
      .code    = DVK_ERROR_DX12,
      .message = DVK_PREPEND( "Failed to get the presentation waitable object" ),
    };
  }

  result = pSwapChain->pSwapChain->SetMaximumFrameLatency( parameters.maximumLatency );
  if ( FAILED( result ) )
  {
    dvk_destroySwapChain( pSwapChain );
    return dvk_result_t{
      .code    = DVK_ERROR_DX12,
      .message = DVK_PREPEND( "Failed to set the maximum frame latency" ),
    };
  }

  result = parameters.pDeviceContext->pDxgiFactory->MakeWindowAssociation( parameters.window, DXGI_MWA_NO_WINDOW_CHANGES );
  if ( FAILED( result ) )
  {
    dvk_destroySwapChain( pSwapChain );
    return dvk_result_t{
      .code    = DVK_ERROR_DX12,
      .message = DVK_PREPEND( "Failed to set the window association" ),
    };
  }

  const dvk_result_t dvkResult = __dvk_createSwapChainObjects( pSwapChain );
  if ( dvk_isError( dvkResult ) )
  {
    dvk_destroySwapChain( pSwapChain );
    return dvkResult;
  }

  result = parameters.pDeviceContext->pD3D12Device->CreateFence( 0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS( &pSwapChain->pFence ) );
  if ( FAILED( result ) )
  {
    dvk_destroySwapChain( pSwapChain );
    return dvk_result_t{
      .code    = DVK_ERROR_DX12,
      .message = DVK_PREPEND( "Failed to create the Fence object" ),
    };
  }

  VkSemaphoreTypeCreateInfo typeInfo = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO, .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE };

  VkSemaphoreCreateInfo semaphoreCreateInfo = {
    VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
  };
  typeInfo.pNext            = semaphoreCreateInfo.pNext;
  semaphoreCreateInfo.pNext = &typeInfo;

  VkResult vulkanResult = pSwapChain->pFunctions->vkCreateSemaphore( parameters.pVulkanContext->device, &semaphoreCreateInfo, nullptr, &pSwapChain->semaphore );
  if ( vulkanResult != VK_SUCCESS )
  {
    dvk_destroySwapChain( pSwapChain );
    return dvk_result_t{
      .code    = DVK_ERROR_VULKAN,
      .message = DVK_PREPEND( "Failed to create the semaphore object" ),
    };
  }

  result = parameters.pDeviceContext->pD3D12Device->CreateSharedHandle( pSwapChain->pFence, nullptr, GENERIC_ALL, nullptr, &pSwapChain->fenceSharedHandle );
  if ( FAILED( result ) )
  {
    dvk_destroySwapChain( pSwapChain );
    return dvk_result_t{
      .code    = DVK_ERROR_DX12,
      .message = DVK_PREPEND( "Failed to get the fence shared handle" ),
    };
  }

  VkImportSemaphoreWin32HandleInfoKHR importSemaphoreInfo = {
    .sType      = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,
    .semaphore  = pSwapChain->semaphore,
    .handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT,
    .handle     = pSwapChain->fenceSharedHandle,
  };
  vulkanResult = pSwapChain->pFunctions->vkImportSemaphoreWin32HandleKHR( parameters.pVulkanContext->device, &importSemaphoreInfo );
  if ( vulkanResult != VK_SUCCESS )
  {
    dvk_destroySwapChain( pSwapChain );
    return dvk_result_t{
      .code    = DVK_ERROR_VULKAN,
      .message = DVK_PREPEND( "Failed to import the fence shared handle" ),
    };
  }

  return dvk_result_t{ .code = DVK_OK };
}

static void dvk_destroySwapChain( dvk_dxgiSwapChain_t * pSwapChain ) noexcept
{
  if ( pSwapChain == nullptr )
  {
    return;
  }

  if ( pSwapChain->pFence )
  {
    __dvk_waitIdle( pSwapChain );
  }

  if ( pSwapChain->semaphore != VK_NULL_HANDLE )
  {
    pSwapChain->pFunctions->vkDestroySemaphore( pSwapChain->pVulkanContext->device, pSwapChain->semaphore, nullptr );
    pSwapChain->semaphore = VK_NULL_HANDLE;
  }
  if ( pSwapChain->fenceSharedHandle != INVALID_HANDLE_VALUE )
  {
    CloseHandle( pSwapChain->fenceSharedHandle );
    pSwapChain->fenceSharedHandle = INVALID_HANDLE_VALUE;
  }
  if ( pSwapChain->pFence != nullptr )
  {
    pSwapChain->pFence->Release();
    pSwapChain->pFence = nullptr;
  }
  if ( pSwapChain->presentationWaitHandle != NULL )
  {
    CloseHandle( pSwapChain->presentationWaitHandle );
    pSwapChain->presentationWaitHandle = NULL;
  }

  __dvk_destroySwapChainObjects( pSwapChain );

  if ( pSwapChain->pSwapChain != nullptr )
  {
    pSwapChain->pSwapChain->Release();
    pSwapChain->pSwapChain = nullptr;
  }
}

static dvk_result_t dvk_resizeSwapChain( dvk_dxgiSwapChain_t * pSwapChain, uint16_t width, uint16_t height ) noexcept
{
  const dvk_result_t dvkResult = __dvk_waitIdle( pSwapChain );
  if ( dvk_isError( dvkResult ) )
  {
    return dvkResult;
  }

  DXGI_SWAP_CHAIN_DESC swapChainDesc;
  HRESULT              result = pSwapChain->pSwapChain->GetDesc( &swapChainDesc );
  if ( FAILED( result ) )
  {
    return dvk_result_t{
      .code    = DVK_ERROR_DX12,
      .message = DVK_PREPEND( "Failed to get the swap chain description" ),
    };
  }
  const uint32_t    swapChainImageCount = swapChainDesc.BufferCount;
  const DXGI_FORMAT swapChainFormat     = swapChainDesc.BufferDesc.Format;

  __dvk_destroySwapChainObjects( pSwapChain );

  result = pSwapChain->pSwapChain->ResizeBuffers( swapChainImageCount, width, height, swapChainFormat, swapChainDesc.Flags );
  if ( FAILED( result ) )
  {
    return dvk_result_t{
      .code    = DVK_ERROR_DXGI,
      .message = DVK_PREPEND( "Failed to resize the DXGI buffer" ),
    };
  }

  return __dvk_createSwapChainObjects( pSwapChain );
}

inline dvk_result_t dvk_changeSwapChainFormat( dvk_dxgiSwapChain_t * pSwapChain, VkFormat format ) noexcept
{
  const dvk_result_t dvkResult = __dvk_waitIdle( pSwapChain );
  if ( dvk_isError( dvkResult ) )
  {
    return dvkResult;
  }

  DXGI_SWAP_CHAIN_DESC swapChainDesc;
  HRESULT              result = pSwapChain->pSwapChain->GetDesc( &swapChainDesc );
  if ( FAILED( result ) )
  {
    return dvk_result_t{
      .code    = DVK_ERROR_DX12,
      .message = DVK_PREPEND( "Failed to get the swap chain description" ),
    };
  }
  const uint32_t    swapChainImageCount = swapChainDesc.BufferCount;
  const DXGI_FORMAT swapChainFormat     = __dvk_toDxgiFormat( format );
  if ( swapChainFormat == DXGI_FORMAT_UNKNOWN )
  {
    return dvk_result_t{ .code = DVK_ERROR, .message = DVK_PREPEND( "Invalid swap chain format" ) };
  }

  __dvk_destroySwapChainObjects( pSwapChain );

  result = pSwapChain->pSwapChain->ResizeBuffers(
    swapChainImageCount, swapChainDesc.BufferDesc.Width, swapChainDesc.BufferDesc.Height, swapChainFormat, swapChainDesc.Flags );
  if ( FAILED( result ) )
  {
    return dvk_result_t{
      .code    = DVK_ERROR_DXGI,
      .message = DVK_PREPEND( "Failed to resize the DXGI buffer" ),
    };
  }

  return __dvk_createSwapChainObjects( pSwapChain );
}

static dvk_result_t dvk_acquireImage( dvk_dxgiSwapChain_t * pSwapChain, dvk_presentationInfo_t * pPresentationInfo ) noexcept
{
  if ( pPresentationInfo == nullptr )
  {
    return dvk_result_t{ .code = DVK_ERROR, .message = DVK_PREPEND( "Presentation info cannot be null" ) };
  }

  const DWORD result = WaitForSingleObjectEx( pSwapChain->presentationWaitHandle, 1000, true );
  if ( result != WAIT_OBJECT_0 )
  {
    if ( result == WAIT_TIMEOUT )
    {
      return dvk_result_t{ .code = DVK_ERROR, .message = DVK_PREPEND( "Timeout while waiting for the presentation event" ) };
    }
    return dvk_result_t{ .code = DVK_ERROR, .message = DVK_PREPEND( "Error while waiting for the presentation event" ) };
  }

  const uint32_t imageIndex = pSwapChain->pSwapChain->GetCurrentBackBufferIndex();

  dvk_perFrameResources_t * pFrameResources = &pSwapChain->pPerFrameResources[imageIndex];

  uint64_t waitFenceValue   = 0;
  uint64_t submitFenceValue = 0;
  uint64_t blitFenceValue   = 0;

  if ( __dvk_hasSwapChainFlag( pSwapChain->flags, DVK_SWAPCHAIN_BACKBUFFER_BLIT_FLAG ) )
  {
    if ( pFrameResources->lastBlitValue != 0 && pSwapChain->pFence->GetCompletedValue() < pFrameResources->lastBlitValue )
    {
      HRESULT dxResult = pSwapChain->pFence->SetEventOnCompletion( pFrameResources->lastBlitValue, nullptr );
      if ( FAILED( dxResult ) )
      {
        return dvk_result_t{ .code = DVK_ERROR_DX12, .message = DVK_PREPEND( "Error waiting for the blit fence to be signaled" ) };
      }
    }

    HRESULT dxResult = pFrameResources->pCommandAllocator->Reset();
    if ( FAILED( dxResult ) )
    {
      return dvk_result_t{ .code = DVK_ERROR_DX12, .message = DVK_PREPEND( "Failed to reset the frame command allocator" ) };
    }

    dxResult = pSwapChain->pDeviceContext->pCommandList->Reset( pFrameResources->pCommandAllocator, nullptr );
    if ( FAILED( dxResult ) )
    {
      return dvk_result_t{ .code = DVK_ERROR_DX12, .message = DVK_PREPEND( "Failed to reset the command list" ) };
    }

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.pResource   = pFrameResources->sharedImage.pImage;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_DEST;
    pSwapChain->pDeviceContext->pCommandList->ResourceBarrier( 1, &barrier );

    D3D12_TEXTURE_COPY_LOCATION dst = {}, src = {};
    dst.Type      = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    src.Type      = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.pResource = pFrameResources->sharedImage.pImage;
    src.pResource = pSwapChain->pBlitImage.pImage;
    pSwapChain->pDeviceContext->pCommandList->CopyTextureRegion( &dst, 0, 0, 0, &src, nullptr );

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
    pSwapChain->pDeviceContext->pCommandList->ResourceBarrier( 1, &barrier );

    dxResult = pSwapChain->pDeviceContext->pCommandList->Close();
    if ( FAILED( dxResult ) )
    {
      return dvk_result_t{
        .code    = DVK_ERROR_DX12,
        .message = DVK_PREPEND( "Failed to close the command list" ),
      };
    }

    // When bliting the image to the backbuffer the synchronization is sequential, waiting on the single blit buffer
    waitFenceValue   = pSwapChain->fenceValue;
    submitFenceValue = ++pSwapChain->fenceValue;
    blitFenceValue   = ++pSwapChain->fenceValue;
  }
  else
  {
    // When sharing the memory, the synchronization is parallel: each present waits on its own back buffer
    waitFenceValue                   = pFrameResources->lastSubmitValue;
    submitFenceValue                 = ++pSwapChain->fenceValue;
    pFrameResources->lastSubmitValue = submitFenceValue;
  }

  *pPresentationInfo = dvk_presentationInfo_t{ .pSwapChain      = pSwapChain->pSwapChain,
                                               .pCommandQueue   = pSwapChain->pDeviceContext->pCommandQueue,
                                               .pCommandList    = __dvk_hasSwapChainFlag( pSwapChain->flags, DVK_SWAPCHAIN_BACKBUFFER_BLIT_FLAG )
                                                                  ? pSwapChain->pDeviceContext->pCommandList
                                                                  : nullptr,
                                               .pFrameResources = pFrameResources,
                                               .pFence          = pSwapChain->pFence,
                                               .imageIndex      = imageIndex,
                                               .waitSemaphore   = pSwapChain->semaphore,
                                               .waitValue       = waitFenceValue,
                                               .submitSemaphore = pSwapChain->semaphore,
                                               .submitValue     = submitFenceValue,
                                               .blitValue       = blitFenceValue,
                                               .allowTearing    = __dvk_hasSwapChainFlag( pSwapChain->flags, DVK_SWAPCHAIN_ALLOW_TEARING_FLAG ),
                                               .syncInterval    = pSwapChain->syncInterval };

  return dvk_result_t{ .code = DVK_OK };
}

static dvk_result_t dvk_presentImage( const dvk_presentParameters_t & presentParameters ) noexcept
{
  HRESULT result = presentParameters.pCommandQueue->Wait( presentParameters.pFence, presentParameters.submitValue );
  if ( FAILED( result ) )
  {
    return dvk_result_t{ .code = DVK_ERROR_DX12, .message = DVK_PREPEND( "Failed to wait for the VK submit fence" ) };
  }

  // A non null command list here means that we are doing a blit to back buffer
  if ( presentParameters.pCommandList != nullptr )
  {
    presentParameters.pCommandQueue->ExecuteCommandLists( 1, ( (ID3D12CommandList **)&presentParameters.pCommandList ) );

    result = presentParameters.pCommandQueue->Signal( presentParameters.pFence, presentParameters.blitValue );
    if ( FAILED( result ) )
    {
      return dvk_result_t{ .code = DVK_ERROR_DX12, .message = DVK_PREPEND( "Failed to signal the blit fence" ) };
    }
  }
  presentParameters.pFrameResources->lastBlitValue = presentParameters.blitValue;

  const bool vsyncEnabled = presentParameters.syncInterval != 0;
  const UINT flags        = ( !vsyncEnabled && presentParameters.allowTearing ) ? DXGI_PRESENT_ALLOW_TEARING : 0x0;

  result = presentParameters.pSwapChain->Present( presentParameters.syncInterval, flags );
  if ( FAILED( result ) )
  {
    return dvk_result_t{ .code = DVK_ERROR_DX12, .message = DVK_PREPEND( "Failed to present the swap chain buffer" ) };
  }

  return dvk_result_t{ .code = DVK_OK };
}

static dvk_result_t dvk_getSwapChainInfo( const dvk_dxgiSwapChain_t & swapChain, dvk_swapChainInfo_t * pInfo ) noexcept
{
  if ( pInfo == nullptr )
  {
    return dvk_result_t{ .code = DVK_ERROR, .message = "Swap chain info cannot be null" };
  }

  DXGI_SWAP_CHAIN_DESC swapChainDesc;
  HRESULT              result = swapChain.pSwapChain->GetDesc( &swapChainDesc );
  if ( FAILED( result ) )
  {
    return dvk_result_t{
      .code    = DVK_ERROR_DX12,
      .message = DVK_PREPEND( "Failed to get the swap chain description" ),
    };
  }

  *pInfo = dvk_swapChainInfo_t{
    .imageCount = (uint8_t)swapChainDesc.BufferCount,
    .width      = (uint16_t)swapChainDesc.BufferDesc.Width,
    .height     = (uint16_t)swapChainDesc.BufferDesc.Height,
    .format     = __dvk_toVkFormat( swapChainDesc.BufferDesc.Format ),
  };

  return dvk_result_t{ .code = DVK_OK };
}

static dvk_result_t __dvk_createSwapChainObjects( dvk_dxgiSwapChain_t * pSwapChain ) noexcept
{
  DXGI_SWAP_CHAIN_DESC swapChainDesc;
  HRESULT              result = pSwapChain->pSwapChain->GetDesc( &swapChainDesc );
  if ( FAILED( result ) )
  {
    return dvk_result_t{
      .code    = DVK_ERROR_DX12,
      .message = DVK_PREPEND( "Failed to get the swap chain description" ),
    };
  }
  const uint32_t swapChainImageCount       = swapChainDesc.BufferCount;
  pSwapChain->pImportedSwapChainImageCount = swapChainImageCount;

  // If we are not using an external array of swap chain images, allocate one
  if ( pSwapChain->pExternalSwapChainImages == nullptr )
  {
    pSwapChain->pSwapChainImages =
      (VkImage *)pSwapChain->pAllocator->allocate( swapChainImageCount * sizeof( VkImage ), 16u, pSwapChain->pAllocator->pContext );
    if ( pSwapChain->pSwapChainImages == nullptr )
    {
      return dvk_result_t{
        .code    = DVK_ERROR_ALLOCATOR,
        .message = DVK_PREPEND( "Failed to allocate the memory for the vulkan swap chain images" ),
      };
    }
    for ( size_t idx = 0; idx < swapChainImageCount; ++idx )
    {
      pSwapChain->pSwapChainImages[idx] = VK_NULL_HANDLE;
    }
  }

  {
    pSwapChain->pPerFrameResources = (dvk_perFrameResources_t *)pSwapChain->pAllocator->allocate(
      swapChainImageCount * sizeof( dvk_perFrameResources_t ), 16u, pSwapChain->pAllocator->pContext );
    if ( pSwapChain->pPerFrameResources == nullptr )
    {
      return dvk_result_t{
        .code    = DVK_ERROR_ALLOCATOR,
        .message = DVK_PREPEND( "Failed to allocate the memory for the dx12 swap chain images" ),
      };
    }

    for ( size_t idx = 0; idx < swapChainImageCount; ++idx )
    {
      pSwapChain->pPerFrameResources[idx] = dvk_perFrameResources_t{};
    }
  }

  // Create the blit image used for rendering and copy to the swapchain
  if ( __dvk_hasSwapChainFlag( pSwapChain->flags, DVK_SWAPCHAIN_BACKBUFFER_BLIT_FLAG ) )
  {
    D3D12_RESOURCE_DESC blitImageDesc = {};
    blitImageDesc.Width               = swapChainDesc.BufferDesc.Width;
    blitImageDesc.Height              = swapChainDesc.BufferDesc.Height;
    blitImageDesc.Format              = swapChainDesc.BufferDesc.Format;
    blitImageDesc.Flags               = __dvk_toD3D12Usage( pSwapChain->usage );
    blitImageDesc.SampleDesc.Count    = 1;
    blitImageDesc.DepthOrArraySize    = 1;
    blitImageDesc.MipLevels           = 1;
    blitImageDesc.Layout              = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    blitImageDesc.Dimension           = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type                  = D3D12_HEAP_TYPE_DEFAULT;

    result = pSwapChain->pDeviceContext->pD3D12Device->CreateCommittedResource(
      &heapProps, D3D12_HEAP_FLAG_SHARED, &blitImageDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS( &pSwapChain->pBlitImage.pImage ) );
    if ( FAILED( result ) )
    {
      __dvk_destroySwapChainObjects( pSwapChain );
      return dvk_result_t{
        .code    = DVK_ERROR_DX12,
        .message = DVK_PREPEND( "Failed to create the blit image" ),
      };
    }

    pSwapChain->pBlitImage.pVkImage = &pSwapChain->vkBlitImage;
    const dvk_result_t dvkResult    = __dvk_createSharedImage( pSwapChain, &pSwapChain->pBlitImage );
    if ( dvk_isError( dvkResult ) )
    {
      __dvk_destroySwapChainObjects( pSwapChain );
      return dvkResult;
    }
  }

  for ( size_t idx = 0; idx < swapChainImageCount; ++idx )
  {
    dvk_perFrameResources_t * pPerFrameResources = &pSwapChain->pPerFrameResources[idx];
    result                                       = pSwapChain->pSwapChain->GetBuffer( (uint32_t)idx, IID_PPV_ARGS( &pPerFrameResources->sharedImage.pImage ) );
    if ( FAILED( result ) )
    {
      __dvk_destroySwapChainObjects( pSwapChain );
      return dvk_result_t{
        .code    = DVK_ERROR_DX12,
        .message = DVK_PREPEND( "Failed to get the swap chain images" ),
      };
    }

    wchar_t imageName[32];
    _snwprintf_s( imageName, _countof( imageName ), _TRUNCATE, L"SwapChainImage-%zu", idx );
    result = pPerFrameResources->sharedImage.pImage->SetName( imageName );
    if ( FAILED( result ) )
    {
      __dvk_destroySwapChainObjects( pSwapChain );
      return dvk_result_t{
        .code    = DVK_ERROR_DX12,
        .message = DVK_PREPEND( "Failed to set the swap chain image name" ),
      };
    }

    if ( __dvk_hasSwapChainFlag( pSwapChain->flags, DVK_SWAPCHAIN_BACKBUFFER_BLIT_FLAG ) )
    {
      if ( pSwapChain->pExternalSwapChainImages != nullptr )
      {
        pSwapChain->pExternalSwapChainImages[idx] = pSwapChain->vkBlitImage;
      }

      result = pSwapChain->pDeviceContext->pD3D12Device->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                                                 IID_PPV_ARGS( &pPerFrameResources->pCommandAllocator ) );
      if ( FAILED( result ) )
      {
        __dvk_destroySwapChainObjects( pSwapChain );
        return dvk_result_t{
          .code    = DVK_ERROR_DX12,
          .message = DVK_PREPEND( "Failed to create the frame command allocator" ),
        };
      }
    }
    else
    {
      if ( pSwapChain->pExternalSwapChainImages != nullptr )
      {
        pPerFrameResources->sharedImage.pVkImage = &pSwapChain->pExternalSwapChainImages[idx];
      }
      else
      {
        pPerFrameResources->sharedImage.pVkImage = &pSwapChain->pSwapChainImages[idx];
      }

      const dvk_result_t dvkResult = __dvk_createSharedImage( pSwapChain, &pPerFrameResources->sharedImage );
      if ( dvk_isError( dvkResult ) )
      {
        __dvk_destroySwapChainObjects( pSwapChain );
        return dvkResult;
      }
    }
  }

  return dvk_result_t{ .code = DVK_OK };
}

static void __dvk_destroySwapChainObjects( dvk_dxgiSwapChain_t * pSwapChain ) noexcept
{
  if ( pSwapChain->pPerFrameResources != nullptr )
  {
    for ( size_t idx = 0; idx < pSwapChain->pImportedSwapChainImageCount; ++idx )
    {
      dvk_perFrameResources_t * pImportedSwapChainImage = &pSwapChain->pPerFrameResources[idx];
      __dvk_destroySharedImage( pSwapChain, &pImportedSwapChainImage->sharedImage );

      if ( pImportedSwapChainImage->pCommandAllocator != nullptr )
      {
        pImportedSwapChainImage->pCommandAllocator->Release();
        pImportedSwapChainImage->pCommandAllocator = nullptr;
      }
    }
  }

  __dvk_destroySharedImage( pSwapChain, &pSwapChain->pBlitImage );

  pSwapChain->pAllocator->free( pSwapChain->pPerFrameResources, pSwapChain->pAllocator->pContext );
  pSwapChain->pPerFrameResources = nullptr;
  if ( pSwapChain->pSwapChainImages != nullptr )
  {
    pSwapChain->pAllocator->free( pSwapChain->pSwapChainImages, pSwapChain->pAllocator->pContext );
    pSwapChain->pSwapChainImages = nullptr;
  }
  pSwapChain->pImportedSwapChainImageCount = 0;
}

inline dvk_result_t __dvk_createSharedImage( const dvk_dxgiSwapChain_t * pSwapChain, dvk_sharedImage_t * pSharedImage ) noexcept
{
  // This function expects the D3D12 and Vulkan images to be valid
  if ( pSharedImage->pImage == nullptr || pSharedImage->pVkImage == nullptr )
  {
    return dvk_result_t{
      .code    = DVK_ERROR,
      .message = DVK_PREPEND( "One of the required shared image resource was invalid" ),
    };
  }

  VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
  pSwapChain->pFunctions->vkGetPhysicalDeviceMemoryProperties( pSwapChain->pVulkanContext->physicalDevice, &deviceMemoryProperties );

  const D3D12_RESOURCE_DESC imageDesc = pSharedImage->pImage->GetDesc();

  VkExternalMemoryImageCreateInfo externalMemoryImageCreateInfo = {
    .sType       = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
    .pNext       = nullptr,
    .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT,
  };
  VkImageCreateInfo imageCreateInfo{
    .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .pNext         = &externalMemoryImageCreateInfo,
    .imageType     = VK_IMAGE_TYPE_2D,
    .format        = __dvk_toVkFormat( imageDesc.Format ),
    .extent        = VkExtent3D{ .width = (uint32_t)imageDesc.Width, .height = (uint32_t)imageDesc.Height, .depth = 1 },
    .mipLevels     = 1,
    .arrayLayers   = 1,
    .samples       = VK_SAMPLE_COUNT_1_BIT,
    .tiling        = VK_IMAGE_TILING_OPTIMAL,
    .usage         = pSwapChain->usage,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };
  VkResult vkResult = pSwapChain->pFunctions->vkCreateImage(
    pSwapChain->pVulkanContext->device, &imageCreateInfo, pSwapChain->pVulkanContext->pAllocationCallbacks, pSharedImage->pVkImage );
  if ( vkResult != VK_SUCCESS )
  {
    __dvk_destroySharedImage( pSwapChain, pSharedImage );
    return dvk_result_t{
      .code    = DVK_ERROR_VULKAN,
      .message = DVK_PREPEND( "Failed to create corresponding Vulkan image" ),
    };
  }

  HRESULT result = pSwapChain->pDeviceContext->pD3D12Device->CreateSharedHandle( pSharedImage->pImage, nullptr, GENERIC_ALL, nullptr, &pSharedImage->handle );
  if ( FAILED( result ) )
  {
    __dvk_destroySharedImage( pSwapChain, pSharedImage );
    return dvk_result_t{
      .code    = DVK_ERROR_DX12,
      .message = DVK_PREPEND( "Failed to create swap chain image shared handle" ),
    };
  }

  VkMemoryWin32HandlePropertiesKHR dxgiMemoryRequirements{ .sType          = VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR,
                                                           .pNext          = nullptr,
                                                           .memoryTypeBits = 0xffffffff };
  vkResult = pSwapChain->pFunctions->vkGetMemoryWin32HandlePropertiesKHR(
    pSwapChain->pVulkanContext->device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT, pSharedImage->handle, &dxgiMemoryRequirements );
  if ( vkResult != VK_SUCCESS )
  {
    __dvk_destroySharedImage( pSwapChain, pSharedImage );
    return dvk_result_t{
      .code    = DVK_ERROR_VULKAN,
      .message = DVK_PREPEND( "Failed to get memory win32 handle properties" ),
    };
  }

  VkMemoryRequirements vulkanMemoryRequirements;
  pSwapChain->pFunctions->vkGetImageMemoryRequirements( pSwapChain->pVulkanContext->device, *pSharedImage->pVkImage, &vulkanMemoryRequirements );

  dxgiMemoryRequirements.memoryTypeBits &= vulkanMemoryRequirements.memoryTypeBits;

  uint32_t memoryTypeIndex = UINT32_MAX;
  for ( uint32_t im = 0; im < deviceMemoryProperties.memoryTypeCount; ++im )
  {
    const uint32_t currentBit = 0x1 << im;
    if ( ( dxgiMemoryRequirements.memoryTypeBits & currentBit ) == currentBit )
    {
      if ( deviceMemoryProperties.memoryTypes[im].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT )
      {
        memoryTypeIndex = im;
        break;
      }
    }
  }
  if ( memoryTypeIndex == UINT32_MAX )
  {
    __dvk_destroySharedImage( pSwapChain, pSharedImage );
    return dvk_result_t{
      .code    = DVK_ERROR,
      .message = DVK_PREPEND( "Failed to get find suitable memory type" ),
    };
  }

  const VkMemoryDedicatedAllocateInfoKHR dedicatedAllocateInfo{
    .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO, .pNext = nullptr, .image = *pSharedImage->pVkImage, .buffer = VK_NULL_HANDLE
  };
  const VkImportMemoryWin32HandleInfoKHR importMemoryHandle{ .sType      = VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR,
                                                             .pNext      = &dedicatedAllocateInfo,
                                                             .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT_KHR,
                                                             .handle     = pSharedImage->handle,
                                                             .name       = nullptr };
  const VkMemoryAllocateInfo             memoryAllocateInfo{ .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                                             .pNext           = &importMemoryHandle,
                                                             .allocationSize  = vulkanMemoryRequirements.size,
                                                             .memoryTypeIndex = memoryTypeIndex };

  vkResult = pSwapChain->pFunctions->vkAllocateMemory(
    pSwapChain->pVulkanContext->device, &memoryAllocateInfo, pSwapChain->pVulkanContext->pAllocationCallbacks, &pSharedImage->memory );
  if ( vkResult != VK_SUCCESS )
  {
    __dvk_destroySharedImage( pSwapChain, pSharedImage );
    return dvk_result_t{
      .code    = DVK_ERROR_VULKAN,
      .message = DVK_PREPEND( "Failed to allocate vulkan memory for the swap chain image" ),
    };
  }

  vkResult = pSwapChain->pFunctions->vkBindImageMemory( pSwapChain->pVulkanContext->device, *pSharedImage->pVkImage, pSharedImage->memory, 0 );
  if ( vkResult != VK_SUCCESS )
  {
    __dvk_destroySharedImage( pSwapChain, pSharedImage );
    return dvk_result_t{
      .code    = DVK_ERROR_VULKAN,
      .message = DVK_PREPEND( "Failed to bind vulkan image memory" ),
    };
  }

  return dvk_result_t{ .code = DVK_OK };
}

inline void __dvk_destroySharedImage( const dvk_dxgiSwapChain_t * pSwapChain, dvk_sharedImage_t * pSharedImage ) noexcept
{
  if ( pSharedImage->pVkImage != nullptr && pSharedImage->pVkImage != VK_NULL_HANDLE )
  {
    pSwapChain->pFunctions->vkDestroyImage( pSwapChain->pVulkanContext->device, *pSharedImage->pVkImage, pSwapChain->pVulkanContext->pAllocationCallbacks );
    *pSharedImage->pVkImage = VK_NULL_HANDLE;
  }
  if ( pSharedImage->memory != VK_NULL_HANDLE )
  {
    pSwapChain->pFunctions->vkFreeMemory( pSwapChain->pVulkanContext->device, pSharedImage->memory, pSwapChain->pVulkanContext->pAllocationCallbacks );
    pSharedImage->memory = VK_NULL_HANDLE;
  }
  if ( pSharedImage->handle != INVALID_HANDLE_VALUE )
  {
    CloseHandle( pSharedImage->handle );
    pSharedImage->handle = INVALID_HANDLE_VALUE;
  }
  if ( pSharedImage->pImage != nullptr )
  {
    pSharedImage->pImage->Release();
    pSharedImage->pImage = nullptr;
  }
}

inline DXGI_FORMAT __dvk_toDxgiFormat( VkFormat format ) noexcept
{
  switch ( format )
  {
    case VK_FORMAT_R8G8B8A8_UNORM    : return DXGI_FORMAT_R8G8B8A8_UNORM;
    case VK_FORMAT_R8G8B8A8_SRGB     : return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case VK_FORMAT_B8G8R8A8_UNORM    : return DXGI_FORMAT_B8G8R8A8_UNORM;
    case VK_FORMAT_B8G8R8A8_SRGB     : return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    case VK_FORMAT_R16G16B16A16_UNORM: return DXGI_FORMAT_R16G16B16A16_UNORM;
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
      return DXGI_FORMAT_R10G10B10A2_UNORM;  // :TP: No this is not a mistake, Vulkan names PACK32 MSB-to-LSB, DXGI names LSB-to-MSB
    default: return DXGI_FORMAT_UNKNOWN;
  }
}

inline VkFormat __dvk_toVkFormat( DXGI_FORMAT format ) noexcept
{
  switch ( format )
  {
    case DXGI_FORMAT_R8G8B8A8_UNORM     : return VK_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
    case DXGI_FORMAT_B8G8R8A8_UNORM     : return VK_FORMAT_B8G8R8A8_UNORM;
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return VK_FORMAT_B8G8R8A8_SRGB;
    case DXGI_FORMAT_R16G16B16A16_UNORM : return VK_FORMAT_R16G16B16A16_UNORM;
    case DXGI_FORMAT_R10G10B10A2_UNORM:
      return VK_FORMAT_A2B10G10R10_UNORM_PACK32;  // :TP: No this is not a mistake, Vulkan names PACK32 MSB-to-LSB, DXGI names LSB-to-MSB
    default: return VK_FORMAT_UNDEFINED;
  }
}

inline DXGI_USAGE __dvk_toDxgiUsage( VkImageUsageFlags usage ) noexcept
{
  DXGI_USAGE dxgiUsage = 0;

  if ( usage & VK_IMAGE_USAGE_SAMPLED_BIT )
  {
    dxgiUsage |= DXGI_USAGE_SHADER_INPUT;
  }
  if ( usage & VK_IMAGE_USAGE_STORAGE_BIT )
  {
    dxgiUsage |= DXGI_USAGE_UNORDERED_ACCESS;
  }
  if ( usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT )
  {
    dxgiUsage |= DXGI_USAGE_RENDER_TARGET_OUTPUT;
  }
  if ( usage & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT )
  {
    dxgiUsage |= DXGI_USAGE_SHADER_INPUT;
  }
  return dxgiUsage;
}

inline D3D12_RESOURCE_FLAGS __dvk_toD3D12Usage( VkImageUsageFlags usage ) noexcept
{
  D3D12_RESOURCE_FLAGS d3d12Usage = D3D12_RESOURCE_FLAG_NONE;

  if ( usage & VK_IMAGE_USAGE_STORAGE_BIT )
  {
    d3d12Usage |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
  }
  if ( usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT )
  {
    d3d12Usage |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
  }
  return d3d12Usage;
}

inline VkImageUsageFlags __dvk_toVkUsage( DXGI_USAGE usage ) noexcept
{
  VkImageUsageFlags vkUsage = 0;

  if ( usage & DXGI_USAGE_SHADER_INPUT )
  {
    vkUsage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    vkUsage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
  }
  if ( usage & DXGI_USAGE_RENDER_TARGET_OUTPUT )
  {
    vkUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  }
  if ( usage & DXGI_USAGE_UNORDERED_ACCESS )
  {
    vkUsage |= VK_IMAGE_USAGE_STORAGE_BIT;
  }
  return vkUsage;
}

static dvk_result_t __dvk_waitIdle( dvk_dxgiSwapChain_t * pSwapChain ) noexcept
{
  const VkResult vulkanResult = pSwapChain->pFunctions->vkDeviceWaitIdle( pSwapChain->pVulkanContext->device );
  if ( vulkanResult != VK_SUCCESS )
  {
    return dvk_result_t{ .code = DVK_ERROR_VULKAN, .message = DVK_PREPEND( "Error waiting for vulkan device idle" ) };
  }

  HRESULT result = pSwapChain->pDeviceContext->pCommandQueue->Signal( pSwapChain->pFence, ++pSwapChain->fenceValue );
  if ( FAILED( result ) )
  {
    return dvk_result_t{ .code = DVK_ERROR_DX12, .message = DVK_PREPEND( "Error signaling the D3D12 fence" ) };
  }
  if ( pSwapChain->pFence->GetCompletedValue() < pSwapChain->fenceValue )
  {
    result = pSwapChain->pFence->SetEventOnCompletion( pSwapChain->fenceValue, nullptr );
    if ( FAILED( result ) )
    {
      return dvk_result_t{ .code = DVK_ERROR_DX12, .message = DVK_PREPEND( "Error waiting for fence event" ) };
    }
  }
  return dvk_result_t{ .code = DVK_OK };
}

inline bool __dvk_hasSwapChainFlag( dvk_swapChainFlags_e flags, dvk_swapChainFlagBits_e testFlag ) noexcept
{
  return ( ( flags & testFlag ) != 0x00 );
}
#endif
