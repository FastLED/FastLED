# DependencyManagement.cmake
# External library detection and configuration

# Function to find and configure libunwind for stack traces
function(find_and_configure_libunwind)
    # Try to find libunwind, but make it optional
    # libunwind doesn't always provide CMake config files, so we'll search manually

    # First try to find using pkg-config
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
        
        # Set up variables for use by targets
        set(LIBUNWIND_LIBRARIES ${LIBUNWIND_LIBRARIES} PARENT_SCOPE)
        set(LIBUNWIND_INCLUDE_DIRS ${LIBUNWIND_INCLUDE_DIRS} PARENT_SCOPE)
    else()
        set(USE_LIBUNWIND FALSE PARENT_SCOPE)
        message(STATUS "LibUnwind not found. Falling back to basic stack traces with execinfo.")
    endif()
endfunction()

# Function to find and configure sccache for compilation caching
function(find_and_configure_sccache)
    set(CACHE_TOOL_FOUND FALSE PARENT_SCOPE)
    set(CACHE_TOOL_NAME "" PARENT_SCOPE)
    set(CACHE_TOOL_PATH "" PARENT_SCOPE)
    
    # Try to find sccache via Python/uv installation first
    find_program(SCCACHE_PYTHON_FOUND sccache HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../.venv/bin ${CMAKE_CURRENT_SOURCE_DIR}/../.venv/Scripts)
    
    if(SCCACHE_PYTHON_FOUND)
        set(CACHE_TOOL_FOUND TRUE PARENT_SCOPE)
        set(CACHE_TOOL_NAME "sccache (Python)" PARENT_SCOPE)
        set(CACHE_TOOL_PATH ${SCCACHE_PYTHON_FOUND} PARENT_SCOPE)
        
        # Configure sccache settings
        message(STATUS "Using sccache (Python): ${SCCACHE_PYTHON_FOUND}")
        set(CMAKE_CXX_COMPILER_LAUNCHER ${SCCACHE_PYTHON_FOUND} PARENT_SCOPE)
        set(CMAKE_C_COMPILER_LAUNCHER ${SCCACHE_PYTHON_FOUND} PARENT_SCOPE)
        
        # Configure sccache for optimal performance
        message(STATUS "Configuring sccache for optimal performance")
        set(ENV{SCCACHE_CACHE_SIZE} "2G")
        set(ENV{SCCACHE_DIR} "${CMAKE_BINARY_DIR}/sccache")
        # Enable local disk cache (fastest option)
        set(ENV{SCCACHE_CACHE} "local")
        # Simple activity indicator
        set(ENV{SCCACHE_LOG} "warn")
        message(STATUS "sccache: Compiler caching active - builds will show cache hits/misses")
        
        # Cross-platform optimizations
        if(WIN32)
            set(ENV{SCCACHE_CACHE_MULTIPART_UPLOAD} "true")
        endif()
        
    else()
        # Try system-installed sccache
        find_program(SCCACHE_SYSTEM_FOUND sccache)
        if(SCCACHE_SYSTEM_FOUND)
            set(CACHE_TOOL_FOUND TRUE PARENT_SCOPE)
            set(CACHE_TOOL_NAME "sccache (system)" PARENT_SCOPE)
            set(CACHE_TOOL_PATH ${SCCACHE_SYSTEM_FOUND} PARENT_SCOPE)
            
            # Configure similar to above
            message(STATUS "Using sccache (system): ${SCCACHE_SYSTEM_FOUND}")
            set(CMAKE_CXX_COMPILER_LAUNCHER ${SCCACHE_SYSTEM_FOUND} PARENT_SCOPE)
            set(CMAKE_C_COMPILER_LAUNCHER ${SCCACHE_SYSTEM_FOUND} PARENT_SCOPE)
        endif()
    endif()
endfunction()

# Function to find and configure ccache as fallback
function(find_and_configure_ccache)
    find_program(CCACHE_FOUND ccache)
    if(CCACHE_FOUND)
        set(CACHE_TOOL_FOUND TRUE PARENT_SCOPE)
        set(CACHE_TOOL_NAME "ccache" PARENT_SCOPE)
        set(CACHE_TOOL_PATH ${CCACHE_FOUND} PARENT_SCOPE)
        
        message(STATUS "Using ccache: ${CCACHE_FOUND}")
        set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_FOUND} PARENT_SCOPE)
        set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_FOUND} PARENT_SCOPE)
        
        # ccache configuration
        message(STATUS "Configuring ccache for optimal performance")
        set(ENV{CCACHE_SLOPPINESS} "time_macros")
        set(ENV{CCACHE_COMPRESS} "true")
        set(ENV{CCACHE_MAXSIZE} "2G")
        set(ENV{CCACHE_DIR} "${CMAKE_BINARY_DIR}/ccache")
        # Enable more aggressive caching
        set(ENV{CCACHE_HASHDIR} "false")
        set(ENV{CCACHE_NOHASHDIR} "true")
    else()
        set(CACHE_TOOL_FOUND FALSE PARENT_SCOPE)
    endif()
endfunction()

# Function to configure compiler cache (sccache or ccache)
function(configure_compiler_cache)
    option(PREFER_SCCACHE "Prefer sccache over ccache for compilation caching" ON)
    
    set(CACHE_TOOL_FOUND FALSE)
    
    if(PREFER_SCCACHE)
        find_and_configure_sccache()
    endif()
    
    # Fall back to ccache if sccache not found or not preferred
    if(NOT CACHE_TOOL_FOUND)
        find_and_configure_ccache()
    endif()
    
    if(NOT CACHE_TOOL_FOUND)
        message(STATUS "No compiler cache found. Install sccache (uv add sccache) or ccache for faster builds")
        if(PREFER_SCCACHE)
            message(STATUS "  Recommended: Run 'uv add sccache' for cross-platform caching")
        endif()
    endif()
endfunction()

# Function to configure Windows debug libraries
function(configure_windows_debug_libs)
    if(WIN32)
        # Windows-specific debug libraries
        message(STATUS "Configuring Windows debug libraries (dbghelp, psapi)")
        
        # These are typically available on Windows by default
        set(WINDOWS_DEBUG_LIBS dbghelp psapi PARENT_SCOPE)
        message(STATUS "Windows debug libraries configured")
    endif()
endfunction()

# Function to find and configure all dependencies
function(find_and_configure_dependencies)
    message(STATUS "=== Dependency Detection ===")
    
    # Configure crash handler and stack traces
    find_and_configure_libunwind()
    
    # Configure compilation caching
    configure_compiler_cache()
    
    # Configure platform-specific libraries
    configure_windows_debug_libs()
    
    message(STATUS "=== Dependency Detection Complete ===")
endfunction() 
