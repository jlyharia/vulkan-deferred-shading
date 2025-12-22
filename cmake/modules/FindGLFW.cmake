# Try to find GLFW library
# Once done this will define
#  GLFW_FOUND - if system found GLFW library
#  GLFW_INCLUDE_DIRS - The GLFW include directories
#  GLFW_LIBRARS - The libraries needed to use GLFW
#  GLFW_DEFINITIONS - Compiler switches required for using GLFW

if (WIN32)
    find_path(GLFW_INCLUDE_DIRS GLFW/glfw3.h
            $ENV{PROGRAMFILES}/usr/include
            ${PROJECT_SOURCE_DIR}/usr/include
            DOC "The directory where GLFW/glfw3.h resides")
    find_library(GLFW_LIBRARIES
            NAMES
            glfw
            glfw3
            PATHS
            $ENV{PROGRAMFILES}/usr/lib
            ${PROJECT_SOURCE_DIR}/usr/bin
            ${PROJECT_SOURCE_DIR}/usr/lib
            DOC "The GLFW library")

elseif (APPLE)
    find_path(GLFW_INCLUDE_DIRS
            NAMES GLFW/glfw3.h
            PATHS ${HOMEBREW_INCLUDE_PATH}
            DOC "The boost include directory")

    find_library(GLFW_LIBRARIES
            NAMES
            glfw
            PATHS ${HOMEBREW_LIB_PATH}
            DOC "The GLFW library")
else ()
    find_path(GLFW_INCLUDE_DIRS
            NAMES GLFW/glfw3.h
            PATHS ${HOMEBREW_INCLUDE_PATH}
            DOC "The boost include directory")

    # ubuntu use wayland but it is unstable that glfw + nVidia + linux wayland, so need to manually rebuild glfw + x11
    find_library(GLFW_LIBRARIES
            NAMES glfw3
            PATHS /usr/lib /usr/local/lib
            DOC "GLFW library"
    )
    message(STATUS "Building on linux platform")
endif (WIN32)

# Determine if GLFW was found
if (GLFW_INCLUDE_DIRS AND GLFW_LIBRARIES)
    set(GLFW_FOUND TRUE)
    set(GLFW_DEFINITIONS "")
    message(STATUS "Found GLFW:")
    message(STATUS "  Include dirs: ${GLFW_INCLUDE_DIRS}")
    message(STATUS "  Libraries: ${GLFW_LIBRARIES}")
else ()
    set(GLFW_FOUND FALSE)
    message(FATAL_ERROR "Could not find GLFW library or headers")
endif ()

# Keep your if(GLFW_FOUND) block
if (GLFW_FOUND)
    set(GLFW_LIBRARIES ${GLFW_LIBRARIES})
    set(GLFW_INCLUDE_DIRS ${GLFW_INCLUDE_DIRS})
    set(GLFW_DEFINITIONS)
endif ()

# Mark variables as advanced to hide in GUI
# mark_as_advanced(GLFW_INCLUDE_DIRS GLFW_LIBRARIES)
