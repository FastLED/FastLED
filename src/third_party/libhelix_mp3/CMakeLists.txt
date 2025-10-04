# This CMakeLists.txt is provided for standalone builds of libhelix-mp3.
# When building as part of FastLED, the parent CMakeLists.txt will
# automatically include all .c files in this directory.

cmake_minimum_required(VERSION 3.9)

project(helixmp3)

# Only build as a standalone library if this is the root project
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    if (MSVC)
            add_compile_options(/W4)
    else()
            add_compile_options(-Wall -Werror)
    endif()

    FILE(GLOB HelixMP3_real_src
            real/*.c)

    add_library(${PROJECT_NAME}
            ${HelixMP3_real_src}
            mp3dec.c
            mp3tabs.c)

    target_include_directories(${PROJECT_NAME} PUBLIC
            pub)
endif()
