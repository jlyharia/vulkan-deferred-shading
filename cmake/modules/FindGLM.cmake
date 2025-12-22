# Try to find GLM library
# Once done this will define
#  GLM_FOUND - if system found GLM library
#  GLM_INCLUDE_DIRS - The GLM include directories
#  GLM_DEFINITIONS - Compiler switches required for using GLM


find_path(GLM_INCLUDE_DIRS
        NAMES glm/glm.hpp
        PATHS ${HOMEBREW_INCLUDE_PATH}
        DOC "The GLM include directory")


include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LOGGING_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(GLM DEFAULT_MSG GLM_INCLUDE_DIRS)

if (GLM_FOUND)
    set(GLM_INCLUDE_DIRS ${GLM_INCLUDE_DIRS})
    set(GLM_DEFINITIONS)
endif ()

# Tell cmake GUIs to ignore the "local" variables.
# mark_as_advanced(GLM_INCLUDE_DIRS GLM_LIBRARIES)