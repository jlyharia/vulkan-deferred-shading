cmake_minimum_required(VERSION 3.21) # 4.1 not required unless you need it
project(defer_render)

set(TARGET_NAME defer_render)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ------------------------------------------------------------
# Packages
# ------------------------------------------------------------
# Use VULKAN_SDK environment variable if set

find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

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
)

# ------------------------------------------------------------
# Includes
# ------------------------------------------------------------
target_include_directories(defer_render
        PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/external
)

# ------------------------------------------------------------
# Link libraries
# ------------------------------------------------------------
target_link_libraries(defer_render
        PRIVATE
        Vulkan::Vulkan
        glfw
        glm::glm
)

# copy assets, models, texture ...etc put this after add_executable(..)
add_custom_command(
        TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:defer_render>/assets
        COMMENT "Copying textures folder to build directory"
)

# Ensure build/shaders folder exists
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/shaders)

# Find glslangValidator
find_program(GLSLANG_VALIDATOR
        NAMES glslangValidator
        HINTS /usr/bin /home/johnny/VulkanSDK/1.4.335.0/x86_64/bin/)
if (NOT GLSLANG_VALIDATOR)
    message(FATAL_ERROR "glslangValidator not found! Make sure Vulkan SDK is installed.")
endif ()

# GLSL → SPV
add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/shaders/vert.spv
        COMMAND ${GLSLANG_VALIDATOR} -V ${CMAKE_SOURCE_DIR}/shaders/09_shader_base.vert -o ${CMAKE_BINARY_DIR}/shaders/vert.spv
        DEPENDS ${CMAKE_SOURCE_DIR}/shaders/09_shader_base.vert
)

add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/shaders/frag.spv
        COMMAND ${GLSLANG_VALIDATOR} -V ${CMAKE_SOURCE_DIR}/shaders/09_shader_base.frag -o ${CMAKE_BINARY_DIR}/shaders/frag.spv
        DEPENDS ${CMAKE_SOURCE_DIR}/shaders/09_shader_base.frag
)

#add_custom_command(
#        OUTPUT ${CMAKE_BINARY_DIR}/shaders/comp.spv
#        COMMAND ${GLSLANG_VALIDATOR} -V ${CMAKE_SOURCE_DIR}/shaders/31_shader_base.comp -o ${CMAKE_BINARY_DIR}/shaders/comp.spv
#        DEPENDS ${CMAKE_SOURCE_DIR}/shaders/31_shader_base.comp
#)

add_custom_target(Shaders ALL DEPENDS
        ${CMAKE_BINARY_DIR}/shaders/vert.spv
        ${CMAKE_BINARY_DIR}/shaders/frag.spv
        #        ${CMAKE_BINARY_DIR}/shaders/comp.spv
)

add_dependencies(defer_render Shaders)
