cmake_minimum_required(VERSION 3.21) # 4.1 not required unless you need it
project(defer_render)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ------------------------------------------------------------
# Packages
# ------------------------------------------------------------
find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

# ------------------------------------------------------------
# Target
# ------------------------------------------------------------
add_executable(defer_render
        src/main.cpp
        src/app/App.cpp
        src/app/App.hpp
        src/vulkan/VulkanContext.cpp
        src/vulkan/VulkanContext.hpp
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
