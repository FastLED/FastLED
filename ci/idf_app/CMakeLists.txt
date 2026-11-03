cmake_minimum_required(VERSION 3.16)

# IDF 4.4 bundles mDNS and its component manager predates manifest rules.
# This harness needs only SDK components on 4.x; keep registry resolution
# enabled on 5.x for mDNS and the P4 codec dependency.
include($ENV{IDF_PATH}/tools/cmake/version.cmake)
if(IDF_VERSION_MAJOR LESS 5)
    set(ENV{IDF_COMPONENT_MANAGER} "0")
endif()

# Register this checkout directly, regardless of its directory name.
get_filename_component(FASTLED_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(EXTRA_COMPONENT_DIRS "${FASTLED_ROOT}")
add_compile_definitions(FL_NO_ARDUINO=1)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(fastled_bare_idf)

idf_build_get_property(components BUILD_COMPONENTS)
foreach(component IN LISTS components)
    if(component MATCHES "[Aa][Rr][Dd][Uu][Ii][Nn][Oo]")
        message(FATAL_ERROR "Bare-IDF harness resolved Arduino component: ${component}")
    endif()
endforeach()
message(STATUS "Bare-IDF component inventory contains no Arduino components")
