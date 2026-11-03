cmake_minimum_required(VERSION 3.16)

# The local fastled component reuses the root CMake registration without
# exposing the PlatformIO component-manager manifest to bare IDF.
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
