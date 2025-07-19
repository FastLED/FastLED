# ParallelBuild.cmake
# Parallel compilation and build caching optimization

# Function to detect available CPU cores
function(detect_available_cores)
    include(ProcessorCount)
    ProcessorCount(CPU_COUNT)
    
    if(CPU_COUNT)
        set(DETECTED_CORES ${CPU_COUNT} PARENT_SCOPE)
        message(STATUS "Detected ${CPU_COUNT} CPU cores")
    else()
        set(DETECTED_CORES 4 PARENT_SCOPE)
        message(STATUS "Could not detect CPU count, defaulting to 4 cores")
    endif()
endfunction()

# Function to configure parallel build
function(configure_parallel_build)
    detect_available_cores()
    
    # Allow override via environment variable
    if(DEFINED ENV{FASTLED_PARALLEL_JOBS})
        set(PARALLEL_JOBS $ENV{FASTLED_PARALLEL_JOBS})
        message(STATUS "Using custom parallel jobs from environment: ${PARALLEL_JOBS}")
    else()
        # Use more jobs than CPU cores for better I/O utilization
        # This is especially effective with fast storage and when some jobs are I/O bound
        math(EXPR PARALLEL_JOBS "${DETECTED_CORES} * 2")
        message(STATUS "Setting parallel build level to ${PARALLEL_JOBS} (${DETECTED_CORES} CPU cores * 2)")
    endif()
    
    # Set the parallel build level
    set(CMAKE_BUILD_PARALLEL_LEVEL ${PARALLEL_JOBS} PARENT_SCOPE)
    
    # Configure parallel compilation within individual files for supported compilers
    configure_parallel_compilation()
endfunction()

# Function to configure parallel compilation within files
function(configure_parallel_compilation)
    is_compiler_capability_available("PARALLEL_COMPILATION" parallel_available)
    
    if(parallel_available)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            # Enable pipe for faster communication between compilation stages
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pipe" PARENT_SCOPE)
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pipe" PARENT_SCOPE)
            
            # Enable parallel compilation within files (for template-heavy code)
            if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
                set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftree-parallelize-loops=auto" PARENT_SCOPE)
                set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ftree-parallelize-loops=auto" PARENT_SCOPE)
                message(STATUS "Enabled parallel compilation within files (GCC)")
            endif()
            
            message(STATUS "Enabled parallel compilation optimizations")
        endif()
    endif()
endfunction()

# Function to configure build caching
function(configure_build_caching)
    # This delegates to DependencyManagement.cmake
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/DependencyManagement.cmake)
    configure_compiler_cache()
endfunction()

# Function to configure linker for parallel builds
function(configure_parallel_linker)
    # Check if mold linker is available (fastest parallel linker)
    find_program(MOLD_EXECUTABLE mold)
    
    if(MOLD_EXECUTABLE)
        # Set mold as the default linker
        message(STATUS "Using mold linker for parallel linking: ${MOLD_EXECUTABLE}")
        
        # Add mold linker flags
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fuse-ld=mold" PARENT_SCOPE)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fuse-ld=mold" PARENT_SCOPE)
        
        # Set linker flags globally
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=mold" PARENT_SCOPE)
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=mold" PARENT_SCOPE)
        set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -fuse-ld=mold" PARENT_SCOPE)
        
        set(FASTLED_LINKER_TYPE "Mold" PARENT_SCOPE)
        
    else()
        find_program(LLDLINK_EXECUTABLE lld-link)
        if(LLDLINK_EXECUTABLE)
            message(STATUS "Using lld-link linker: ${LLDLINK_EXECUTABLE}")
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld" PARENT_SCOPE)
            set(FASTLED_LINKER_TYPE "LLD" PARENT_SCOPE)
        else()
            message(STATUS "Using system default linker")
            if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
                set(FASTLED_LINKER_TYPE "GNU" PARENT_SCOPE)
            elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
                set(FASTLED_LINKER_TYPE "MSVC" PARENT_SCOPE)
            else()
                set(FASTLED_LINKER_TYPE "Unknown" PARENT_SCOPE)
            endif()
        endif()
    endif()
endfunction()

# Function to optimize build performance
function(optimize_build_performance)
    message(STATUS "=== Build Performance Optimization ===")
    
    # Configure parallel compilation
    configure_parallel_build()
    
    # Configure build caching
    configure_build_caching()
    
    # Configure parallel linking
    configure_parallel_linker()
    
    # Additional performance optimizations
    configure_build_performance_flags()
    
    message(STATUS "=== Build Performance Optimization Complete ===")
endfunction()

# Function to configure build output directories
function(configure_build_output_directories)
    # Set standard output directories for organized builds
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/.build/lib PARENT_SCOPE)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/.build/lib PARENT_SCOPE)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/.build/bin PARENT_SCOPE)
    
    # Set binary directory for backwards compatibility
    set(CMAKE_BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/.build/bin PARENT_SCOPE)
    
    message(STATUS "Build output directories configured:")
    message(STATUS "  Libraries: ${CMAKE_CURRENT_SOURCE_DIR}/.build/lib")
    message(STATUS "  Executables: ${CMAKE_CURRENT_SOURCE_DIR}/.build/bin")
endfunction()

# Function to configure additional build performance flags
function(configure_build_performance_flags)
    # Use faster memory allocator if available (for build tools)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        # Check for jemalloc or tcmalloc
        find_library(JEMALLOC_LIB jemalloc)
        find_library(TCMALLOC_LIB tcmalloc)
        
        if(JEMALLOC_LIB)
            message(STATUS "Using jemalloc for faster memory allocation")
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -ljemalloc" PARENT_SCOPE)
        elseif(TCMALLOC_LIB)
            message(STATUS "Using tcmalloc for faster memory allocation")
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -ltcmalloc" PARENT_SCOPE)
        endif()
    endif()
    
    # Enable fast debug info for faster compilation
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            # Use faster debug info generation
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -gline-tables-only" PARENT_SCOPE)
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -gline-tables-only" PARENT_SCOPE)
            message(STATUS "Using fast debug info generation for faster builds")
        endif()
    endif()
endfunction()

# Function to report build performance status
function(report_build_performance)
    message(STATUS "=== Build Performance Summary ===")
    message(STATUS "Parallel jobs: ${CMAKE_BUILD_PARALLEL_LEVEL}")
    message(STATUS "Linker: ${FASTLED_LINKER_TYPE}")
    
    if(DEFINED CACHE_TOOL_NAME)
        message(STATUS "Compiler cache: ${CACHE_TOOL_NAME}")
    else()
        message(STATUS "Compiler cache: None")
    endif()
    
    if(CMAKE_CXX_COMPILER_LAUNCHER)
        message(STATUS "Compiler launcher: ${CMAKE_CXX_COMPILER_LAUNCHER}")
    endif()
    
    message(STATUS "=== Build Performance Summary Complete ===")
endfunction() 
