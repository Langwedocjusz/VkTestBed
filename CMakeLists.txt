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
        src/Core/GeometryProvider.h
        src/Core/ImGuiUtils.h
        src/Core/ImGuiUtils.cpp
        src/Core/Scene.h
        src/Core/Primitives.h
        src/Core/Primitives.cpp
        src/Core/RenderContext.h
        src/Core/RenderContext.cpp
        src/Core/SystemWindow.h
        src/Core/SystemWindow.cpp
        src/Core/Vertex.h
        src/Core/VmaImpl.cpp
        src/Core/VulkanContext.h
        src/Core/VulkanContext.cpp
        src/EntryPoint.cpp
        src/Renderers/Renderer.h
        src/Renderers/HelloRenderer.h
        src/Renderers/HelloRenderer.cpp
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
        src/Vulkan/Pipeline.h
        src/Vulkan/Pipeline.cpp
        src/Vulkan/Shader.h
        src/Vulkan/Shader.cpp
        src/Vulkan/Utils.h
        src/Vulkan/Utils.cpp
        src/Vulkan/VkInit.h
        src/Vulkan/VkInit.cpp
)

#Implementation files for imgui
file(GLOB imgui_impl vendor/imgui/backends/imgui_impl_vulkan.cpp vendor/imgui/backends/imgui_impl_glfw.cpp)
target_sources(${PROJECT_NAME} PRIVATE ${imgui_impl})

#Include directories
target_include_directories(${PROJECT_NAME}
    PRIVATE
        src
        src/Core
        src/Renderers
        src/Vulkan
)

#For filters in IDEs like Visual studio:
source_group(src REGULAR_EXPRESSION "src/*")
source_group(src/Core REGULAR_EXPRESSION "src/Core/*")
source_group(src/Renderers REGULAR_EXPRESSION "src/Renderers/*")
source_group(src/Vulkan REGULAR_EXPRESSION "src/Vulkan/*")
source_group(imgui_impl FILES ${imgui_impl})

#Get dependencies:
find_package(Vulkan REQUIRED)

add_subdirectory(vendor/glfw)
add_subdirectory(vendor/vk-bootstrap)
add_subdirectory(vendor/VulkanMemoryAllocator)
add_subdirectory(vendor/glm)
add_subdirectory(vendor/imgui)

#Treat some external headers as system, since they trigger warnings...
get_target_property(_inc GPUOpen::VulkanMemoryAllocator INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(VulkanMemoryAllocator SYSTEM INTERFACE ${_inc})

#Link dependencies:
target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan)
target_link_libraries(${PROJECT_NAME} PRIVATE GPUOpen::VulkanMemoryAllocator)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw)
target_link_libraries(${PROJECT_NAME} PRIVATE vk-bootstrap::vk-bootstrap)
target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm)
target_link_libraries(${PROJECT_NAME} PRIVATE imgui)