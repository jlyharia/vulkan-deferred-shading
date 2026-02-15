cmake_minimum_required(VERSION 3.24...3.31) # 4.1 not required unless you need it
# Add this BEFORE project() to help older dependencies
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
project(defer_render)

set(TARGET_NAME defer_render)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ------------------------------------------------------------
# Packages
# ------------------------------------------------------------
# Use VULKAN_SDK environment variable if set

#find_package(Vulkan REQUIRED)
find_package(VulkanHeaders REQUIRED)
find_package(VulkanLoader REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(glslang CONFIG REQUIRED)
find_package(stb REQUIRED)
find_package(tinyobjloader REQUIRED)


# ------------------------------------------------------------
# Target
# ------------------------------------------------------------
add_executable(defer_render
        src/main.cpp
        src/app/app.cpp
        src/app/app.hpp
        src/vulkan/VulkanContext.cpp
        src/vulkan/VulkanContext.hpp
        src/vulkan/Validation.cpp
        src/vulkan/Validation.hpp
        src/vulkan/swap_chain.cpp
        src/vulkan/swap_chain.hpp
        src/vulkan/graphics_pipeline.cpp
        src/vulkan/graphics_pipeline.hpp
        src/vulkan/render_pass.cpp
        src/vulkan/render_pass.hpp
        src/renderer/renderer.cpp
        src/renderer/renderer.hpp
        src/common/config.hpp
        src/renderer/Vertex.hpp
        src/external/vma_impl.cpp
        src/renderer/Uniform.hpp
        src/external/vendor_impl.cpp
        src/system/ModelSystem.cpp
        src/system/ModelSystem.hpp
        src/renderer/Camera.cpp
        src/renderer/Camera.hpp
)

# ------------------------------------------------------------
# Includes
# ------------------------------------------------------------

# Use target_compile_definitions instead of add_definitions for better scope control

target_compile_definitions(defer_render PRIVATE
        VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
        VULKAN_HPP_STORAGE_SHARED=1
        VULKAN_HPP_STORAGE_SHARED_EXPORT=1
        GLM_ENABLE_EXPERIMENTAL                # <--- fix GTX
)
# Fix the include priority: Put Vulkan SDK FIRST
target_include_directories(defer_render
        PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

# ------------------------------------------------------------
# Link libraries
# ------------------------------------------------------------

target_link_libraries(${TARGET_NAME}
        PRIVATE
        Vulkan::Loader
        vulkan-headers::vulkan-headers
        GPUOpen::VulkanMemoryAllocator
        glfw
        glm::glm
        glslang::glslang
        glslang::SPIRV
        stb::stb
        tinyobjloader::tinyobjloader
)

# copy assets, models, texture ...etc put this after add_executable(..)
# This creates a virtual link from your build folder to your actual assets
if (UNIX)
    execute_process(COMMAND ln -sfn "${CMAKE_SOURCE_DIR}/assets" "${CMAKE_BINARY_DIR}")
else ()
    file(COPY "${CMAKE_SOURCE_DIR}/assets" DESTINATION "${CMAKE_BINARY_DIR}")
endif ()

# Ensure build/shaders folder exists
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/shaders)

# Find glslangValidator

find_program(GLSLANG_VALIDATOR
        NAMES glslangValidator glslc
        HINTS
        "$ENV{VULKAN_SDK}/bin"
        "$ENV{VULKAN_SDK}/x86_64/bin" # Standard Vulkan SDK layout
)
if (NOT GLSLANG_VALIDATOR)
    message(FATAL_ERROR "glslangValidator not found! Make sure Vulkan SDK is installed.")
endif ()

# GLSL → SPV
#
#add_dependencies(defer_render Shaders)
# ------------------------------------------------------------
# Automatic Shader Compilation (GLSL → SPV)
# ------------------------------------------------------------

set(SHADER_SOURCE_DIR "${CMAKE_SOURCE_DIR}/shaders")
set(SHADER_BINARY_DIR "${CMAKE_BINARY_DIR}/shaders")

# Collect all vertex and fragment shaders
file(GLOB_RECURSE SHADER_SOURCES
        "${SHADER_SOURCE_DIR}/*.vert"
        "${SHADER_SOURCE_DIR}/*.frag"
        "${SHADER_SOURCE_DIR}/*.glsl" # Added .glsl for your blinn-phong files
)

set(SPIRV_BINARY_FILES "")

foreach (SHADER_PATH ${SHADER_SOURCES})
    # Get the relative path so we can mirror the folder structure in build/
    file(RELATIVE_PATH REL_PATH "${SHADER_SOURCE_DIR}" "${SHADER_PATH}")
    set(OUTPUT_SPV "${SHADER_BINARY_DIR}/${REL_PATH}.spv")

    # Ensure the subfolder exists in the build directory
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

# Create the target that triggers the compilation
add_custom_target(Shaders ALL DEPENDS ${SPIRV_BINARY_FILES})

# Ensure the executable waits for shaders to be compiled
add_dependencies(${TARGET_NAME} Shaders)