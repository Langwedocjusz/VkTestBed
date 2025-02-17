cmake_minimum_required(VERSION 3.30.2)

project(VkTestBed)

include(cmake/utils.cmake)

if(MSVC)
    msvc_enable_multiprocess_global()
endif()

add_executable(${PROJECT_NAME})

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)

enable_warnings(${PROJECT_NAME})

if(MSVC)
    msvc_startup_project(${PROJECT_NAME})
    msvc_working_directory(${PROJECT_NAME} ${CMAKE_SOURCE_DIR})
endif()

#To enable usage of clang-tidy:
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")

#Sources
target_sources(${PROJECT_NAME}
    PRIVATE
        #Also listing headers to make them visible in IDEs
        src/Core/Application.h
        src/Core/Application.cpp
        src/Core/Camera.h
        src/Core/Camera.cpp
        src/Core/Event.h
        src/Core/Frame.h
        src/Core/Frame.h
        src/Core/GeometryData.h
        src/Core/ImageData.h
        src/Core/ImageData.cpp
        src/Core/Keycodes.h
        src/Core/Material.h
        src/Core/Material.cpp
        src/Core/ModelLoader.h
        src/Core/ModelLoader.cpp
        src/Core/Scene.h
        src/Core/Scene.cpp
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
        src/Cpp/Bitflags.h
        src/Cpp/CppUtils.h
        src/Cpp/OpaqueBuffer.h
        src/Cpp/SyncQueue.h
        src/Cpp/ThreadPool.h
        src/EntryPoint.cpp
        src/Gui/FilesystemBrowser.h
        src/Gui/FilesystemBrowser.cpp
        src/Gui/ImGuiInit.h
        src/Gui/ImGuiInit.cpp
        src/Gui/ImGuiUtils.h
        src/Gui/ImGuiUtils.cpp
        src/Gui/ModelLoaderGui.h
        src/Gui/ModelLoaderGui.cpp
        src/Gui/SceneEditor.h
        src/Gui/SceneEditor.cpp
        src/RendererComponents/EnvironmentHandler.h
        src/RendererComponents/EnvironmentHandler.cpp
        src/Renderers/Renderer.h
        src/Renderers/HelloRenderer.h
        src/Renderers/HelloRenderer.cpp
        src/Renderers/Minimal3D.h
        src/Renderers/Minimal3D.cpp
        src/Renderers/MinimalPBR.h
        src/Renderers/MinimalPBR.cpp
        src/Vulkan/Buffer.h
        src/Vulkan/Buffer.cpp
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
        src/Vulkan/MeshBuffers.h
        src/Vulkan/Pipeline.h
        src/Vulkan/Pipeline.cpp
        src/Vulkan/Sampler.h
        src/Vulkan/Sampler.cpp
        src/Vulkan/Shader.h
        src/Vulkan/Shader.cpp
        src/Vulkan/VkInit.h
        src/Vulkan/VkInit.cpp
        src/Vulkan/VkUtils.h
        src/Vulkan/VkUtils.cpp
)

#Implementation files for imgui
file(GLOB imgui_impl vendor/imgui/backends/imgui_impl_vulkan.cpp vendor/imgui/backends/imgui_impl_glfw.cpp)
target_sources(${PROJECT_NAME} PRIVATE ${imgui_impl})

#Include directories
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

#For filters in IDEs like Visual studio:
source_group(src REGULAR_EXPRESSION "src/*")
source_group(src/Core REGULAR_EXPRESSION "src/Core/*")
source_group(src/Cpp REGULAR_EXPRESSION "src/Cpp/*")
source_group(src/Gui REGULAR_EXPRESSION "src/Gui/*")
source_group(src/RendererComponents REGULAR_EXPRESSION "src/RendererComponents/*")
source_group(src/Renderers REGULAR_EXPRESSION "src/Renderers/*")
source_group(src/Vulkan REGULAR_EXPRESSION "src/Vulkan/*")
source_group(imgui_impl FILES ${imgui_impl})

#Compile definitions:

#Subdirectory options:
option(TRACY_ENABLE "" ON )

#Get dependencies:
find_package(Vulkan REQUIRED)

add_subdirectory(vendor/glfw)
add_subdirectory(vendor/vk-bootstrap)
add_subdirectory(vendor/VulkanMemoryAllocator)
add_subdirectory(vendor/glm)
add_subdirectory(vendor/imgui)
add_subdirectory(vendor/stb_image)
add_subdirectory(vendor/fastgltf)
add_subdirectory(vendor/efsw)
add_subdirectory(vendor/tracy)
add_subdirectory(vendor/tinyexr)
add_subdirectory(vendor/mikktspace)

#Treat some external headers as system, since they trigger warnings...
get_target_property(_inc GPUOpen::VulkanMemoryAllocator INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(VulkanMemoryAllocator SYSTEM INTERFACE ${_inc})

get_target_property(_inc fastgltf::fastgltf INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(fastgltf SYSTEM INTERFACE ${_inc})

#Link dependencies:
target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan)
target_link_libraries(${PROJECT_NAME} PRIVATE GPUOpen::VulkanMemoryAllocator)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw)
target_link_libraries(${PROJECT_NAME} PRIVATE vk-bootstrap::vk-bootstrap)
target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm)
target_link_libraries(${PROJECT_NAME} PRIVATE imgui)
target_link_libraries(${PROJECT_NAME} PRIVATE stb_image)
target_link_libraries(${PROJECT_NAME} PRIVATE fastgltf::fastgltf)
target_link_libraries(${PROJECT_NAME} PRIVATE efsw)
target_link_libraries(${PROJECT_NAME} PRIVATE Tracy::TracyClient)
target_link_libraries(${PROJECT_NAME} PRIVATE tinyexr)
target_link_libraries(${PROJECT_NAME} PRIVATE mikktspace)
