cmake_minimum_required(VERSION 4.1)
project(defer_render)

set(CMAKE_CXX_STANDARD 23)

include(${PROJECT_SOURCE_DIR}/cmake/macro.cmake)

add_executable(defer_render main.cpp)
