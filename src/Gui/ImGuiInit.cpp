#include "ImGuiInit.h"
#include "Pch.h"

#include "CppUtils.h"
#include "Vassert.h"
#include "VulkanContext.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#include <array>
#include <iostream>

// Storage for window handle:
static GLFWwindow *sWindow = nullptr;

static void ImGuiStyleCustom();
[[maybe_unused]] static void check_vk_result(VkResult err);

void iminit::InitImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    io.Fonts->AddFontFromFileTTF("assets/fonts/Roboto/Roboto-Regular.ttf", 16.0f);

    ImGui::StyleColorsDark();
    ImGuiStyleCustom();
}

VkDescriptorPool iminit::CreateDescriptorPool(VulkanContext &ctx)
{
    VkDescriptorPool pool;

    std::array<VkDescriptorPoolSize, 1> poolSizes{
        {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10}},
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 10;
    pool_info.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    pool_info.pPoolSizes = poolSizes.data();

    auto res = vkCreateDescriptorPool(ctx.Device, &pool_info, nullptr, &pool);

    vassert(res == VK_SUCCESS, "Failed to create Imgui Descriptor Pool!");

    return pool;
}

void iminit::InitGlfwBackend(GLFWwindow *window)
{
    ImGui_ImplGlfw_InitForVulkan(window, false);
    sWindow = window;
}

void iminit::InitVulkanBackend(VulkanContext &ctx, VkDescriptorPool descriptorPool,
                               uint32_t framesInFlight)
{
    ImGui_ImplVulkan_InitInfo init_info = {};

    init_info.Instance = ctx.Instance;
    init_info.PhysicalDevice = ctx.PhysicalDevice;
    init_info.Device = ctx.Device;

    init_info.Queue = ctx.Queues.Graphics;
    init_info.DescriptorPool = descriptorPool;
    init_info.MinImageCount = framesInFlight;
    init_info.ImageCount = framesInFlight;

    init_info.UseDynamicRendering = true;
    init_info.CheckVkResultFn = check_vk_result;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    // Dynamic rendering data
    init_info.PipelineRenderingCreateInfo = {};
    init_info.PipelineRenderingCreateInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats =
        &ctx.Swapchain.image_format;

    // init_info.PipelineRenderingCreateInfo.depthAttachmentFormat =
    //     vkutils::FindDepthFormat(ctx);

    ImGui_ImplVulkan_Init(&init_info);
}

void iminit::DestroyImGui()
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void iminit::BeginGuiFrame()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void iminit::FinalizeGuiFrame()
{
    ImGui::Render();
}

void iminit::RecordImguiToCommandBuffer(VkCommandBuffer cmd)
{
    ImDrawData *draw_data = ImGui::GetDrawData();

    if (draw_data)
        ImGui_ImplVulkan_RenderDrawData(draw_data, cmd);
}

void iminit::ImGuiHandleEvent(Event::EventVariant event)
{
    // clang-format off
    std::visit(overloaded{
        []([[maybe_unused]] Event::FramebufferResize arg) {},
        [](Event::Focus arg) {ImGui_ImplGlfw_WindowFocusCallback(sWindow, arg.Focused);},
        [](Event::CursorEnter arg) {ImGui_ImplGlfw_CursorEnterCallback(sWindow, arg.Entered);},
        [](Event::CursorPos arg) {ImGui_ImplGlfw_CursorPosCallback(sWindow, arg.XPos, arg.YPos);},
        [](Event::MouseButton arg) {ImGui_ImplGlfw_MouseButtonCallback(sWindow, arg.Button, arg.Action, arg.Mods);},
        [](Event::Scroll arg) {ImGui_ImplGlfw_ScrollCallback(sWindow, arg.XOffset, arg.YOffset);},
        [](Event::Key arg) {ImGui_ImplGlfw_KeyCallback(sWindow, arg.Keycode, arg.Scancode, arg.Action,arg.Mods);},
        [](Event::Char arg) {ImGui_ImplGlfw_CharCallback(sWindow, arg.Codepoint); }
    }, event);
    // clang-format on

    // Currently not handled:
    // ImGui_ImplGlfw_MonitorCallbac;
}

static void ImGuiStyleCustom()
{
    auto &style = ImGui::GetStyle();

    style.WindowRounding = 2.0f;
    style.ChildRounding = 2.0f;
    style.FrameRounding = 0.0f;
    style.PopupRounding = 2.0f;
    style.ScrollbarRounding = 2.0f;
    style.GrabRounding = 1.0f;
    style.TabRounding = 2.0f;

    ImVec4 *colors = ImGui::GetStyle().Colors;
    // clang-format off
    colors[ImGuiCol_Text]                      = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled]              = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg]                  = ImVec4(0.06f, 0.06f, 0.06f, 0.94f);
    colors[ImGuiCol_ChildBg]                   = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_PopupBg]                   = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
    colors[ImGuiCol_Border]                    = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    colors[ImGuiCol_BorderShadow]              = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                   = ImVec4(0.33f, 0.32f, 0.32f, 0.54f);
    colors[ImGuiCol_FrameBgHovered]            = ImVec4(0.58f, 0.58f, 0.58f, 0.40f);
    colors[ImGuiCol_FrameBgActive]             = ImVec4(0.58f, 0.58f, 0.58f, 0.67f);
    colors[ImGuiCol_TitleBg]                   = ImVec4(0.48f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_TitleBgActive]             = ImVec4(0.63f, 0.21f, 0.21f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]          = ImVec4(0.33f, 0.11f, 0.11f, 0.51f);
    colors[ImGuiCol_MenuBarBg]                 = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]               = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab]             = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]      = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]       = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    colors[ImGuiCol_CheckMark]                 = ImVec4(0.98f, 0.98f, 0.98f, 1.00f);
    colors[ImGuiCol_SliderGrab]                = ImVec4(0.88f, 0.24f, 0.24f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]          = ImVec4(0.98f, 0.26f, 0.26f, 1.00f);
    colors[ImGuiCol_Button]                    = ImVec4(0.98f, 0.26f, 0.26f, 0.40f);
    colors[ImGuiCol_ButtonHovered]             = ImVec4(0.98f, 0.26f, 0.26f, 1.00f);
    colors[ImGuiCol_ButtonActive]              = ImVec4(0.98f, 0.06f, 0.06f, 1.00f);
    colors[ImGuiCol_Header]                    = ImVec4(0.98f, 0.26f, 0.26f, 0.31f);
    colors[ImGuiCol_HeaderHovered]             = ImVec4(0.98f, 0.26f, 0.26f, 0.80f);
    colors[ImGuiCol_HeaderActive]              = ImVec4(0.98f, 0.26f, 0.26f, 1.00f);
    colors[ImGuiCol_Separator]                 = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    colors[ImGuiCol_SeparatorHovered]          = ImVec4(0.75f, 0.10f, 0.10f, 0.78f);
    colors[ImGuiCol_SeparatorActive]           = ImVec4(0.75f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_ResizeGrip]                = ImVec4(0.98f, 0.26f, 0.26f, 0.20f);
    colors[ImGuiCol_ResizeGripHovered]         = ImVec4(0.98f, 0.26f, 0.26f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]          = ImVec4(0.98f, 0.26f, 0.26f, 0.95f);
    colors[ImGuiCol_TabHovered]                = ImVec4(0.98f, 0.26f, 0.26f, 0.80f);
    colors[ImGuiCol_Tab]                       = ImVec4(0.58f, 0.18f, 0.18f, 0.86f);
    colors[ImGuiCol_TabSelected]               = ImVec4(0.68f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_TabSelectedOverline]       = ImVec4(0.98f, 0.26f, 0.26f, 1.00f);
    colors[ImGuiCol_TabDimmed]                 = ImVec4(0.07f, 0.10f, 0.15f, 0.97f);
    colors[ImGuiCol_TabDimmedSelected]         = ImVec4(0.42f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_DockingPreview]            = ImVec4(0.98f, 0.26f, 0.26f, 0.70f);
    colors[ImGuiCol_DockingEmptyBg]            = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_PlotLines]                 = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]          = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram]             = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]      = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg]             = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]         = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
    colors[ImGuiCol_TableBorderLight]          = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
    colors[ImGuiCol_TableRowBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]             = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
    colors[ImGuiCol_TextLink]                  = ImVec4(0.98f, 0.26f, 0.26f, 1.00f);
    colors[ImGuiCol_TextSelectedBg]            = ImVec4(0.98f, 0.26f, 0.26f, 0.35f);
    colors[ImGuiCol_DragDropTarget]            = ImVec4(1.00f, 1.00f, 1.00f, 0.90f);
    colors[ImGuiCol_NavHighlight]              = ImVec4(0.98f, 0.26f, 0.26f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight]     = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]         = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]          = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
    // clang-format on
}

[[maybe_unused]] static void check_vk_result(VkResult err)
{
    if (err == VK_SUCCESS)
        return;

    std::cerr << "Vulkan Error: Vkresult = " << err << '\n';

    if (err < 0)
        abort();
}