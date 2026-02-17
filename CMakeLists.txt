cmake_minimum_required(VERSION 3.30.2)

project(VkTestBed)

option(USE_VALIDATION_LAYERS "Wether or not to use the Vulkan Validation Layers" ON)

#=Global setup============================================================================

#Enable parallel compilation for MSVC:
if(MSVC)
    add_compile_options(/MP)
endif()

#Export compile commands to enable usage of clang-tidy:
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")

#=Get dependencies=========================================================================

option(IMGUI_BACKEND_GLFW "" ON )
option(IMGUI_BACKEND_VULKAN "" ON )
option(IMGUI_VULKAN_NO_PROTOTYPES "" ON)
option(TRACY_ENABLE "" ON )

add_subdirectory(vendor/cpptrace)
add_subdirectory(vendor/efsw)
add_subdirectory(vendor/fastgltf)
add_subdirectory(vendor/glm)
add_subdirectory(vendor/glfw)
add_subdirectory(vendor/imgui)
add_subdirectory(vendor/ImGuizmo)
add_subdirectory(vendor/mikktspace)
add_subdirectory(vendor/stb_image)
add_subdirectory(vendor/tinyexr)
add_subdirectory(vendor/KTX-Software/lib ktx)
add_subdirectory(vendor/tracy)
add_subdirectory(vendor/vk-bootstrap)
add_subdirectory(vendor/volk)
add_subdirectory(vendor/VulkanMemoryAllocator)

#Treat some external headers as system, since they trigger warnings...
get_target_property(_inc GPUOpen::VulkanMemoryAllocator INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(VulkanMemoryAllocator SYSTEM INTERFACE ${_inc})

get_target_property(_inc fastgltf::fastgltf INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(fastgltf SYSTEM INTERFACE ${_inc})

#Some additional linking for dependencies:
target_link_libraries(imgui PRIVATE glfw)
target_link_libraries(imgui PRIVATE volk)
target_link_libraries(ImGuizmo PRIVATE imgui)

#=Setup the main executable=================================================================

add_executable(${PROJECT_NAME})

#Specify C++ standard:
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)

#Compile definitions:
target_compile_definitions(${PROJECT_NAME} PRIVATE GLM_FORCE_DEPTH_ZERO_TO_ONE)

if(USE_VALIDATION_LAYERS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_VALIDATION_LAYERS)
endif(USE_VALIDATION_LAYERS)

#Enable max warnings and warnings as errors:
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

#Output compilation timings with clang:
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -ftime-trace)
endif()

#Setup starting project and debugger working directory for MSVC:
if(MSVC)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
    set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()

#Add sources (lso listing headers to make them visible in IDEs):
target_sources(${PROJECT_NAME}
    PRIVATE
        src/Core/Application.h
        src/Core/Application.cpp
        src/Core/AssetManager.h
        src/Core/AssetManager.cpp
        src/Core/Camera.h
        src/Core/Camera.cpp
        src/Core/Event.h
        src/Core/Frame.h
        src/Core/Frame.h
        src/Core/GeometryData.h
        src/Core/GeometryData.cpp
        src/Core/ImageData.h
        src/Core/ImageData.cpp
        src/Core/Keycodes.h
        src/Core/ModelConfig.h
        src/Core/ModelLoader.h
        src/Core/ModelLoader.cpp
        src/Core/Scene.h
        src/Core/Scene.cpp
        src/Core/SceneEditor.h
        src/Core/SceneEditor.cpp
        src/Core/SceneGraph.h
        src/Core/SceneGraph.cpp
        src/Core/TangentsGenerator.h
        src/Core/TangentsGenerator.cpp
        src/Core/Primitives.h
        src/Core/Primitives.cpp
        src/Core/RenderContext.h
        src/Core/RenderContext.cpp
        src/Core/ShaderManager.h
        src/Core/ShaderManager.cpp
        src/Core/SystemWindow.h
        src/Core/SystemWindow.cpp
        src/Core/VertexLayout.h
        src/Core/VertexLayout.cpp
        src/Core/VmaImpl.cpp
        src/Core/VulkanContext.h
        src/Core/VulkanContext.cpp
        src/Core/VulkanStatistics.h
        src/Core/VulkanStatistics.cpp
        src/Cpp/Vassert.h
        src/Cpp/Vassert.cpp
        src/Cpp/Bitflags.h
        src/Cpp/CppUtils.h
        src/Cpp/OpaqueBuffer.h
        src/Cpp/SyncQueue.h
        src/Cpp/ThreadPool.h
        src/Cpp/Timer.h
        src/EntryPoint.cpp
        src/Gui/FilesystemBrowser.h
        src/Gui/FilesystemBrowser.cpp
        src/Gui/ImGuiInit.h
        src/Gui/ImGuiInit.cpp
        src/Gui/ImGuiUtils.h
        src/Gui/ImGuiUtils.cpp
        src/Gui/ModelLoaderGui.h
        src/Gui/ModelLoaderGui.cpp
        src/Gui/SceneGui.h
        src/Gui/SceneGui.cpp
        src/RendererComponents/DynamicUniformBuffer.h
        src/RendererComponents/DynamicUniformBuffer.cpp
        src/RendererComponents/EnvironmentHandler.h
        src/RendererComponents/EnvironmentHandler.cpp
        src/RendererComponents/ShadowmapHandler.h
        src/RendererComponents/ShadowmapHandler.cpp
        src/Renderers/HelloRenderer.h
        src/Renderers/HelloRenderer.cpp
        src/Renderers/Minimal3D.h
        src/Renderers/Minimal3D.cpp
        src/Renderers/MinimalPBR.h
        src/Renderers/MinimalPBR.cpp
        src/Renderers/Renderer.h
        src/Renderers/RendererFactory.h
        src/Renderers/RendererFactory.cpp
        src/Vulkan/Buffer.h
        src/Vulkan/Buffer.cpp
        src/Vulkan/BufferUtils.h
        src/Vulkan/BufferUtils.cpp
        src/Vulkan/Common.h
        src/Vulkan/Common.cpp
        src/Vulkan/Barrier.h
        src/Vulkan/Barrier.cpp
        src/Vulkan/DeletionQueue.h
        src/Vulkan/DeletionQueue.cpp
        src/Vulkan/Descriptor.h
        src/Vulkan/Descriptor.cpp
        src/Vulkan/Image.h
        src/Vulkan/Image.cpp
        src/Vulkan/ImageLoaders.h
        src/Vulkan/ImageLoaders.cpp
        src/Vulkan/ImageUtils.h
        src/Vulkan/ImageUtils.cpp
        src/Vulkan/Pipeline.h
        src/Vulkan/Pipeline.cpp
        src/Vulkan/Sampler.h
        src/Vulkan/Sampler.cpp
        src/Vulkan/Shader.h
        src/Vulkan/Shader.cpp
        src/Vulkan/Texture.h
        src/Vulkan/VkInit.h
        src/Vulkan/VkInit.cpp
        src/Vulkan/VkUtils.h
        src/Vulkan/VkUtils.cpp
)

#Setup include directories:
target_include_directories(${PROJECT_NAME}
    PRIVATE
        src
        src/Core
        src/Cpp
        src/Gui
        src/RendererComponents
        src/Renderers
        src/Vulkan
)

# This idiotic maneuver is needed because libktx cannot
# expose khr_df.h file it includes...
target_include_directories(${PROJECT_NAME}
    PRIVATE
        vendor/KTX-Software/external/dfdutils
)

#Add filters for IDEs like Visual studio:
source_group(src REGULAR_EXPRESSION "src/*")
source_group(src/Core REGULAR_EXPRESSION "src/Core/*")
source_group(src/Cpp REGULAR_EXPRESSION "src/Cpp/*")
source_group(src/Gui REGULAR_EXPRESSION "src/Gui/*")
source_group(src/RendererComponents REGULAR_EXPRESSION "src/RendererComponents/*")
source_group(src/Renderers REGULAR_EXPRESSION "src/Renderers/*")
source_group(src/Vulkan REGULAR_EXPRESSION "src/Vulkan/*")

#Setup precompiled header:
target_precompile_headers(${PROJECT_NAME} PUBLIC src/Pch.h)

#Link dependencies:
#target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan)
target_link_libraries(${PROJECT_NAME} PRIVATE GPUOpen::VulkanMemoryAllocator)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw)
target_link_libraries(${PROJECT_NAME} PRIVATE vk-bootstrap::vk-bootstrap)
target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm)
target_link_libraries(${PROJECT_NAME} PRIVATE imgui)
target_link_libraries(${PROJECT_NAME} PRIVATE ImGuizmo)
target_link_libraries(${PROJECT_NAME} PRIVATE stb_image)
target_link_libraries(${PROJECT_NAME} PRIVATE fastgltf::fastgltf)
target_link_libraries(${PROJECT_NAME} PRIVATE efsw)
target_link_libraries(${PROJECT_NAME} PRIVATE Tracy::TracyClient)
target_link_libraries(${PROJECT_NAME} PRIVATE tinyexr)
target_link_libraries(${PROJECT_NAME} PRIVATE ktx)
target_link_libraries(${PROJECT_NAME} PRIVATE mikktspace)
target_link_libraries(${PROJECT_NAME} PRIVATE cpptrace::cpptrace)
target_link_libraries(${PROJECT_NAME} PRIVATE volk)