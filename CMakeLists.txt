cmake_minimum_required(VERSION 3.24...3.31)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
project(defer_render)

set(TARGET_NAME defer_render)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ------------------------------------------------------------
# Packages
# ------------------------------------------------------------
find_package(volk REQUIRED)
find_package(VulkanHeaders REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(glslang CONFIG REQUIRED)
find_package(stb REQUIRED)
find_package(tinyobjloader REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(TinyGLTF REQUIRED)

# ------------------------------------------------------------
# ImGui Path Resolution (MUST be before add_executable)
# ------------------------------------------------------------
# Try to get the root package folder from Conan variables
# 1. Get the path (which currently contains $<$<CONFIG:Debug>:...>)
get_target_property(IMGUI_RAW_PATH imgui::imgui INTERFACE_INCLUDE_DIRECTORIES)

# 2. Use Regex to extract the actual absolute path from the Generator Expression
# This looks for the part starting with /home and ending before the closing '>'
string(REGEX MATCH "/home[^>]+" IMGUI_STRIPPED_PATH "${IMGUI_RAW_PATH}")

# 3. If regex fails, fallback to Conan's package folder variable
if(NOT IMGUI_STRIPPED_PATH)
    set(IMGUI_STRIPPED_PATH "${imgui_PACKAGE_FOLDER_RELEASE}")
endif()

# 4. Set our clean paths (removing /include from the end of the root path)
get_filename_component(IMGUI_ROOT "${IMGUI_STRIPPED_PATH}" DIRECTORY)
set(IMGUI_BACKEND_SRC_DIR "${IMGUI_ROOT}/res/bindings")

message(STATUS "FIXED: ImGui Root: ${IMGUI_ROOT}")
message(STATUS "FIXED: ImGui Backend Dir: ${IMGUI_BACKEND_SRC_DIR}")

# ------------------------------------------------------------
# Target
# ------------------------------------------------------------
add_executable(${TARGET_NAME}
        src/main.cpp
        src/app/App.cpp
        src/app/App.hpp
        src/vulkan/VulkanContext.cpp
        src/vulkan/VulkanContext.hpp
        src/vulkan/Validation.cpp
        src/vulkan/Validation.hpp
        src/vulkan/SwapChain.cpp
        src/vulkan/SwapChain.hpp
        src/vulkan/GraphicsPipeline.cpp
        src/vulkan/GraphicsPipeline.hpp
        src/renderer/Renderer.cpp
        src/renderer/Renderer.hpp
        src/common/Config.hpp
        src/common/Vertex.hpp
        src/external/vma_impl.cpp
        src/common/Uniform.hpp
        src/external/vendor_impl.cpp
        src/scene/Camera.hpp
        src/renderer/UserInterface.cpp
        src/renderer/UserInterface.hpp
        src/common/VulkanInclude.hpp
        # Now these variables are guaranteed to be populated
        "${IMGUI_BACKEND_SRC_DIR}/imgui_impl_glfw.cpp"
        "${IMGUI_BACKEND_SRC_DIR}/imgui_impl_vulkan.cpp"
        src/assets/GltfLoader.cpp
        src/assets/GltfLoader.hpp
        src/scene/Mesh.cpp
        src/scene/Mesh.hpp
        src/vulkan/VulkanUtils.hpp
        src/assets/AssetManager.cpp
        src/assets/AssetManager.hpp
        src/renderer/passes/GeometryPass.cpp
        src/renderer/passes/GeometryPass.hpp
        src/renderer/passes/LightingPass.cpp
        src/renderer/passes/LightingPass.hpp
        src/renderer/passes/OverlayPass.cpp
        src/renderer/passes/OverlayPass.hpp
        src/scene/MeshInstance.hpp
        src/scene/Transform.hpp
        src/scene/Texture.hpp
        src/scene/TextureManager.cpp
        src/scene/TextureManager.hpp
        src/vulkan/VulkanUtils.cpp
        src/common/Material.hpp
        src/common/PushConstantConstant.hpp
        src/common/InstanceData.hpp
        src/scene/PointLight.hpp
        src/scene/MeshUtils.cpp
        src/scene/MeshUtils.hpp
        src/vulkan/GBuffer.hpp
        src/vulkan/GBuffer.cpp
        src/renderer/passes/SsaoPass.cpp
        src/renderer/passes/SsaoPass.hpp
        src/renderer/passes/SsaoBlurPass.cpp
        src/renderer/passes/SsaoBlurPass.hpp
        src/renderer/passes/DirShadowPass.cpp
        src/renderer/passes/DirShadowPass.hpp
        src/scene/DirLightView.hpp
        src/vulkan/ShadowMap.cpp
        src/vulkan/ShadowMap.hpp
        src/renderer/passes/DirShadowPass.cpp
)

# ------------------------------------------------------------
# Includes & Definitions
# ------------------------------------------------------------
target_compile_definitions(${TARGET_NAME} PUBLIC
        VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
        VULKAN_HPP_STORAGE_SHARED=1
        VULKAN_HPP_STORAGE_SHARED_EXPORT=1
        GLM_ENABLE_EXPERIMENTAL
        IMGUI_IMPL_VULKAN_NO_PROTOTYPES
#        IMGUI_IMPL_VULKAN_HAS_DYNAMIC_RENDERING
)

target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
#        ${CMAKE_CURRENT_SOURCE_DIR}/src
        "${IMGUI_REAL_PATH}/include"
        "${IMGUI_BACKEND_SRC_DIR}"
)

# ------------------------------------------------------------
# Link libraries
# ------------------------------------------------------------
target_link_libraries(${TARGET_NAME}
        PRIVATE
        volk::volk
        vulkan-headers::vulkan-headers
        GPUOpen::VulkanMemoryAllocator
        glfw
        glm::glm
        glslang::glslang
        glslang::SPIRV
        stb::stb
        tinyobjloader::tinyobjloader
        imgui::imgui
        TinyGLTF::TinyGLTF
)

# ------------------------------------------------------------
# Assets & Shaders
# ------------------------------------------------------------
if (UNIX)
    execute_process(COMMAND ln -sfn "${CMAKE_SOURCE_DIR}/assets" "${CMAKE_BINARY_DIR}")
else ()
    file(COPY "${CMAKE_SOURCE_DIR}/assets" DESTINATION "${CMAKE_BINARY_DIR}")
endif ()

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/shaders)

find_program(GLSLANG_VALIDATOR
        NAMES glslangValidator glslc
        HINTS
        "$ENV{VULKAN_SDK}/bin"
        "$ENV{VULKAN_SDK}/x86_64/bin"
)

if (NOT GLSLANG_VALIDATOR)
    message(FATAL_ERROR "glslangValidator not found!")
endif ()

set(SHADER_SOURCE_DIR "${CMAKE_SOURCE_DIR}/shaders")
set(SHADER_BINARY_DIR "${CMAKE_BINARY_DIR}/shaders")

file(GLOB_RECURSE SHADER_SOURCES
        "${SHADER_SOURCE_DIR}/*.vert"
        "${SHADER_SOURCE_DIR}/*.frag"
        "${SHADER_SOURCE_DIR}/*.glsl"
)

set(SPIRV_BINARY_FILES "")
foreach (SHADER_PATH ${SHADER_SOURCES})
    file(RELATIVE_PATH REL_PATH "${SHADER_SOURCE_DIR}" "${SHADER_PATH}")
    set(OUTPUT_SPV "${SHADER_BINARY_DIR}/${REL_PATH}.spv")
    get_filename_component(OUT_DIR ${OUTPUT_SPV} DIRECTORY)
    file(MAKE_DIRECTORY ${OUT_DIR})

    add_custom_command(
            OUTPUT ${OUTPUT_SPV}
            COMMAND ${GLSLANG_VALIDATOR} -V ${SHADER_PATH} -o ${OUTPUT_SPV}
            DEPENDS ${SHADER_PATH}
            COMMENT "Compiling ${REL_PATH} to SPIR-V"
    )
    list(APPEND SPIRV_BINARY_FILES ${OUTPUT_SPV})
endforeach ()

add_custom_target(Shaders ALL DEPENDS ${SPIRV_BINARY_FILES})
add_dependencies(${TARGET_NAME} Shaders)