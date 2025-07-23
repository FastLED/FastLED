# StaticAnalysis.cmake
# Static analysis tools configuration for FastLED

# Include What You Use (IWYU) Configuration
function(configure_iwyu)
    option(FASTLED_ENABLE_IWYU "Enable include-what-you-use analysis" OFF)
    
    if(FASTLED_ENABLE_IWYU)
        find_program(IWYU_EXE NAMES include-what-you-use)
        
        if(IWYU_EXE)
            message(STATUS "Found include-what-you-use: ${IWYU_EXE}")
            
            # IWYU command line options
            set(IWYU_OPTS 
                --verbose=1
                --max_line_length=100
                --no_comments
                --quoted_includes_first
            )
            
            # Add mapping files if they exist
            if(EXISTS "${CMAKE_SOURCE_DIR}/ci/iwyu/fastled.imp")
                list(APPEND IWYU_OPTS "--mapping_file=${CMAKE_SOURCE_DIR}/ci/iwyu/fastled.imp")
            endif()
            
            if(EXISTS "${CMAKE_SOURCE_DIR}/ci/iwyu/stdlib.imp")
                list(APPEND IWYU_OPTS "--mapping_file=${CMAKE_SOURCE_DIR}/ci/iwyu/stdlib.imp")
            endif()
            
            # Set the IWYU command for C++
            set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE ${IWYU_EXE} ${IWYU_OPTS} PARENT_SCOPE)
            
            message(STATUS "IWYU enabled for C++ targets with options: ${IWYU_OPTS}")
        else()
            message(WARNING "🚫 include-what-you-use requested but NOT FOUND")
            message(WARNING "   No IWYU analysis will be performed")
            message(WARNING "   Install it with:")
            message(WARNING "     Ubuntu/Debian: sudo apt install iwyu")
            message(WARNING "     macOS: brew install include-what-you-use")
            message(WARNING "     Windows: See STATIC_ANALYSIS.md for installation guide")
            message(WARNING "     Or build from source: https://include-what-you-use.org/")
        endif()
    endif()
endfunction()

# Function to enable IWYU for specific targets
function(enable_iwyu_for_target target_name)
    if(FASTLED_ENABLE_IWYU AND IWYU_EXE)
        # Get the current IWYU command
        get_property(iwyu_cmd GLOBAL PROPERTY CMAKE_CXX_INCLUDE_WHAT_YOU_USE)
        if(iwyu_cmd)
            set_target_properties(${target_name} PROPERTIES
                CXX_INCLUDE_WHAT_YOU_USE "${iwyu_cmd}"
            )
            message(STATUS "IWYU enabled for target: ${target_name}")
        endif()
    endif()
endfunction()

# Function to disable IWYU for specific files (useful for third-party code)
function(disable_iwyu_for_files)
    if(FASTLED_ENABLE_IWYU AND IWYU_EXE)
        foreach(file ${ARGN})
            set_source_files_properties(${file} PROPERTIES
                SKIP_LINTING TRUE
            )
        endforeach()
    endif()
endfunction()

# Function to apply IWYU to all test targets
function(apply_iwyu_to_test_targets)
    if(FASTLED_ENABLE_IWYU AND IWYU_EXE)
        # Get all targets in the current directory
        get_property(all_targets DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY BUILDSYSTEM_TARGETS)
        
        foreach(target ${all_targets})
            # Only apply to executable targets (tests)
            get_target_property(target_type ${target} TYPE)
            if(target_type STREQUAL "EXECUTABLE")
                enable_iwyu_for_target(${target})
            endif()
        endforeach()
        
        message(STATUS "IWYU applied to all test targets in ${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
endfunction()

# Clang-Tidy Configuration (enhance existing if needed)
function(configure_clang_tidy)
    option(FASTLED_ENABLE_CLANG_TIDY "Enable clang-tidy analysis" OFF)
    
    if(FASTLED_ENABLE_CLANG_TIDY)
        find_program(CLANG_TIDY_EXE NAMES clang-tidy)
        
        if(CLANG_TIDY_EXE)
            message(STATUS "Found clang-tidy: ${CLANG_TIDY_EXE}")
            
            # Clang-tidy options
            set(CLANG_TIDY_OPTS
                -checks=clang-analyzer-core.*,clang-analyzer-deadcode.*,clang-analyzer-cplusplus.*,readability-*,-readability-magic-numbers
                -header-filter=.*
                -format-style=file
            )
            
            # Set the clang-tidy command for C++
            set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_EXE} ${CLANG_TIDY_OPTS} PARENT_SCOPE)
            
            message(STATUS "Clang-tidy enabled for C++ targets")
        else()
            message(WARNING "🚫 clang-tidy requested but NOT FOUND")
            message(WARNING "   No clang-tidy analysis will be performed")
            message(WARNING "   Install it with your system's LLVM/Clang package")
        endif()
    endif()
endfunction()

# Main function to configure all static analysis tools
function(configure_static_analysis)
    message(STATUS "=== Configuring Static Analysis Tools ===")
    
    configure_iwyu()
    configure_clang_tidy()
    
    # Display available options
    message(STATUS "Static Analysis Options:")
    message(STATUS "  -DFASTLED_ENABLE_IWYU=ON      : Enable include-what-you-use")
    message(STATUS "  -DFASTLED_ENABLE_CLANG_TIDY=ON: Enable clang-tidy")
    
    # Summary of what's actually enabled
    if(FASTLED_ENABLE_IWYU OR FASTLED_ENABLE_CLANG_TIDY)
        message(STATUS "")
        message(STATUS "📋 STATIC ANALYSIS SUMMARY:")
        if(FASTLED_ENABLE_IWYU)
            if(IWYU_EXE)
                message(STATUS "  ✅ IWYU: ENABLED and FOUND")
            else()
                message(STATUS "  ❌ IWYU: REQUESTED but NOT FOUND")
            endif()
        endif()
        if(FASTLED_ENABLE_CLANG_TIDY)
            if(CLANG_TIDY_EXE)
                message(STATUS "  ✅ Clang-Tidy: ENABLED and FOUND")
            else()
                message(STATUS "  ❌ Clang-Tidy: REQUESTED but NOT FOUND")
            endif()
        endif()
    endif()
    message(STATUS "==========================================")
endfunction()