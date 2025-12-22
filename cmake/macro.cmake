# Find modules
macro(include_module name)
    find_package(${name} REQUIRED)
    if (NOT ${name}_FOUND)
        message(FATAL_ERROR "Package ${name} not found")
    endif (NOT ${name}_FOUND)

    # -D
    add_definitions(${${name}_DEFINITIONS})
    include_directories(${${name}_INCLUDE_DIRS})
    set(LIBRARIES ${LIBRARIES} ${${name}_LIBRARIES})
endmacro()