# DebugSettings.cmake
# Centralized debug configuration with automatic platform adaptation

# Debug levels
set(DEBUG_LEVELS NONE MINIMAL STANDARD FULL)

# Current debug level (defaults to FULL for development)
set(FASTLED_DEBUG_LEVEL "FULL" CACHE STRING "Debug information level")
set_property(CACHE FASTLED_DEBUG_LEVEL PROPERTY STRINGS ${DEBUG_LEVELS})

# Function to configure debug build
function(configure_debug_build)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
    message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
    
    # Set debug-specific optimizations and flags
    configure_debug_symbols(${FASTLED_DEBUG_LEVEL})
    disable_optimizations()
    enable_stack_traces()
    
    # Add debug-specific compile definitions
    add_compile_definitions(
        DEBUG
        FASTLED_TESTING
        ENABLE_CRASH_HANDLER
        _GLIBCXX_DEBUG
        _GLIBCXX_DEBUG_PEDANTIC
    )
    
    # Fix for Microsoft STL version check with older Clang versions on Windows
    if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_definitions(_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH)
    endif()
    
    message(STATUS "Debug build configured with level: ${FASTLED_DEBUG_LEVEL}")
endfunction()

# Function to configure release build
function(configure_release_build)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
    message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
    
    # Set release-specific optimizations
    configure_debug_symbols("MINIMAL")  # Keep minimal symbols for stack traces
    
    # Add release-specific compile definitions
    add_compile_definitions(
        NDEBUG
        FASTLED_TESTING
    )
    
    message(STATUS "Release build configured")
endfunction()

# Function to configure debug symbols with platform-specific translation
function(configure_debug_symbols level)
    set(debug_flags "")
    
    if(level STREQUAL "NONE")
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            set(debug_flags "-g0")
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
            set(debug_flags "/Zi-")
        endif()
        message(STATUS "Debug symbols: NONE")
        
    elseif(level STREQUAL "MINIMAL")
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            set(debug_flags "-gline-tables-only")
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
            set(debug_flags "/Zi")
        endif()
        message(STATUS "Debug symbols: MINIMAL (line tables only)")
        
    elseif(level STREQUAL "STANDARD")
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            set(debug_flags "-g")
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
            set(debug_flags "/Zi")
        endif()
        message(STATUS "Debug symbols: STANDARD")
        
    elseif(level STREQUAL "FULL")
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            set(debug_flags "-g3")
            # Platform-specific debug format handling
            if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
                # On Windows with Clang: generate both DWARF (for GDB) and CodeView (for stack traces)
                list(APPEND debug_flags "-gdwarf-4" "-gcodeview")
                message(STATUS "Debug symbols: FULL (DWARF + CodeView for Windows)")
            else()
                # Non-Windows or non-Clang: standard DWARF debug info
                message(STATUS "Debug symbols: FULL (DWARF format)")
            endif()
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
            set(debug_flags "/Zi" "/Od")
        endif()
        
    else()
        message(WARNING "Unknown debug level: ${level}, using STANDARD")
        configure_debug_symbols("STANDARD")
        return()
    endif()
    
    # Apply the debug flags globally
    string(REPLACE ";" " " debug_flags_str "${debug_flags}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${debug_flags_str}" PARENT_SCOPE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${debug_flags_str}" PARENT_SCOPE)
    
    set(FASTLED_DEBUG_LEVEL ${level} PARENT_SCOPE)
endfunction()

# Function to enable stack traces
function(enable_stack_traces)
    # Add frame pointer preservation for better stack traces
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(frame_flags "-fno-omit-frame-pointer")
        set(unwind_flags "-funwind-tables")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        set(frame_flags "/Oy-")  # Disable frame pointer omission
        set(unwind_flags "")
    endif()
    
    # Apply globally
    string(REPLACE ";" " " stack_flags_str "${frame_flags} ${unwind_flags}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${stack_flags_str}" PARENT_SCOPE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${stack_flags_str}" PARENT_SCOPE)
    
    message(STATUS "Stack traces enabled")
endfunction()

# Function to disable optimizations
function(disable_optimizations)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(opt_flags "-O0")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        set(opt_flags "/Od")
    endif()
    
    # Apply globally
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${opt_flags}" PARENT_SCOPE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${opt_flags}" PARENT_SCOPE)
    
    message(STATUS "Optimizations disabled for debugging")
endfunction()

# Function to configure crash handler
function(configure_crash_handler)
    # Try to find libunwind for enhanced stack traces
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(LIBUNWIND QUIET libunwind)
    endif()

    # If pkg-config didn't work, try manual search
    if(NOT LIBUNWIND_FOUND)
        find_path(LIBUNWIND_INCLUDE_DIR 
            NAMES libunwind.h
            PATHS /usr/include /usr/local/include /opt/homebrew/include
        )

        find_library(LIBUNWIND_LIBRARY 
            NAMES unwind
            PATHS /usr/lib /usr/local/lib /opt/homebrew/lib /usr/lib/x86_64-linux-gnu
        )

        find_library(LIBUNWIND_X86_64_LIBRARY 
            NAMES unwind-x86_64
            PATHS /usr/lib /usr/local/lib /opt/homebrew/lib /usr/lib/x86_64-linux-gnu
        )

        # Check if both header and library are found
        if(LIBUNWIND_INCLUDE_DIR AND LIBUNWIND_LIBRARY)
            set(LIBUNWIND_FOUND TRUE)
            if(LIBUNWIND_X86_64_LIBRARY)
                set(LIBUNWIND_LIBRARIES ${LIBUNWIND_LIBRARY} ${LIBUNWIND_X86_64_LIBRARY})
            else()
                set(LIBUNWIND_LIBRARIES ${LIBUNWIND_LIBRARY})
            endif()
            set(LIBUNWIND_INCLUDE_DIRS ${LIBUNWIND_INCLUDE_DIR})
        endif()
    endif()

    # Set the final flag based on what we found
    if(LIBUNWIND_FOUND)
        set(USE_LIBUNWIND TRUE PARENT_SCOPE)
        # For x86_64 systems, we need both libunwind and libunwind-x86_64
        if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
            set(LIBUNWIND_LIBRARIES unwind unwind-x86_64 lzma PARENT_SCOPE)
            message(STATUS "LibUnwind found for x86_64: unwind unwind-x86_64 lzma")
        else()
            # Use pkg-config results if available, otherwise use manual search
            if(PKG_CONFIG_FOUND AND LIBUNWIND_STATIC_LIBRARIES)
                set(LIBUNWIND_LIBRARIES ${LIBUNWIND_STATIC_LIBRARIES} PARENT_SCOPE)
                set(LIBUNWIND_INCLUDE_DIRS ${LIBUNWIND_STATIC_INCLUDE_DIRS} PARENT_SCOPE)
                message(STATUS "LibUnwind found via pkg-config: ${LIBUNWIND_STATIC_LIBRARIES}")
            elseif(LIBUNWIND_LIBRARIES AND LIBUNWIND_INCLUDE_DIRS)
                # Add x86_64 library if available
                if(LIBUNWIND_X86_64_LIBRARY)
                    list(APPEND LIBUNWIND_LIBRARIES ${LIBUNWIND_X86_64_LIBRARY})
                endif()
                set(LIBUNWIND_LIBRARIES ${LIBUNWIND_LIBRARIES} PARENT_SCOPE)
                set(LIBUNWIND_INCLUDE_DIRS ${LIBUNWIND_INCLUDE_DIRS} PARENT_SCOPE)
                message(STATUS "LibUnwind found manually: ${LIBUNWIND_LIBRARIES}")
            endif()
        endif()
        message(STATUS "LibUnwind headers: ${LIBUNWIND_INCLUDE_DIRS}")
        
        # Add compile definition for libunwind usage
        add_compile_definitions(USE_LIBUNWIND)
    else()
        set(USE_LIBUNWIND FALSE PARENT_SCOPE)
        message(STATUS "LibUnwind not found. Falling back to basic stack traces with execinfo.")
    endif()
endfunction() 
