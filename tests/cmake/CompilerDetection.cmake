# CompilerDetection.cmake
# Centralized compiler and platform detection with capability reporting

# Function to detect compiler capabilities and set global flags
function(detect_compiler_capabilities)
    # Initialize all capabilities to FALSE
    set(HAS_LTO_SUPPORT FALSE PARENT_SCOPE)
    set(HAS_STACK_TRACE_SUPPORT FALSE PARENT_SCOPE)  
    set(HAS_DEAD_CODE_ELIMINATION FALSE PARENT_SCOPE)
    set(HAS_FAST_MATH FALSE PARENT_SCOPE)
    set(HAS_PARALLEL_COMPILATION FALSE PARENT_SCOPE)
    set(HAS_DEBUG_SYMBOLS FALSE PARENT_SCOPE)
    
    # Detect compiler type
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        set(FASTLED_COMPILER_TYPE "GCC" PARENT_SCOPE)
        set(HAS_LTO_SUPPORT TRUE PARENT_SCOPE)
        set(HAS_DEAD_CODE_ELIMINATION TRUE PARENT_SCOPE)
        set(HAS_FAST_MATH TRUE PARENT_SCOPE)
        set(HAS_PARALLEL_COMPILATION TRUE PARENT_SCOPE)
        set(HAS_DEBUG_SYMBOLS TRUE PARENT_SCOPE)
        set(HAS_STACK_TRACE_SUPPORT TRUE PARENT_SCOPE)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(FASTLED_COMPILER_TYPE "Clang" PARENT_SCOPE)
        set(HAS_LTO_SUPPORT TRUE PARENT_SCOPE)
        set(HAS_DEAD_CODE_ELIMINATION TRUE PARENT_SCOPE)
        set(HAS_FAST_MATH TRUE PARENT_SCOPE)
        set(HAS_PARALLEL_COMPILATION TRUE PARENT_SCOPE)
        set(HAS_DEBUG_SYMBOLS TRUE PARENT_SCOPE)
        set(HAS_STACK_TRACE_SUPPORT TRUE PARENT_SCOPE)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        set(FASTLED_COMPILER_TYPE "MSVC" PARENT_SCOPE)
        set(HAS_LTO_SUPPORT TRUE PARENT_SCOPE)
        set(HAS_DEAD_CODE_ELIMINATION TRUE PARENT_SCOPE)
        set(HAS_FAST_MATH TRUE PARENT_SCOPE)
        set(HAS_PARALLEL_COMPILATION TRUE PARENT_SCOPE)
        set(HAS_DEBUG_SYMBOLS TRUE PARENT_SCOPE)
        # MSVC has limited stack trace support compared to GCC/Clang
        set(HAS_STACK_TRACE_SUPPORT FALSE PARENT_SCOPE)
    else()
        set(FASTLED_COMPILER_TYPE "Unknown" PARENT_SCOPE)
        message(WARNING "Unknown compiler: ${CMAKE_CXX_COMPILER_ID}. Some features may not work correctly.")
    endif()
    
    # Detect platform type
    if(WIN32)
        set(FASTLED_PLATFORM_TYPE "Windows" PARENT_SCOPE)
    elseif(APPLE)
        set(FASTLED_PLATFORM_TYPE "macOS" PARENT_SCOPE)
        # Disable LTO on Apple due to known issues
        set(HAS_LTO_SUPPORT FALSE PARENT_SCOPE)
    elseif(UNIX)
        set(FASTLED_PLATFORM_TYPE "Linux" PARENT_SCOPE)
    else()
        set(FASTLED_PLATFORM_TYPE "Unknown" PARENT_SCOPE)
    endif()
    
    # Detect linker type
    find_program(MOLD_EXECUTABLE mold)
    find_program(LLDLINK_EXECUTABLE lld-link)
    
    if(MOLD_EXECUTABLE)
        set(FASTLED_LINKER_TYPE "Mold" PARENT_SCOPE)
    elseif(LLDLINK_EXECUTABLE)
        set(FASTLED_LINKER_TYPE "LLD" PARENT_SCOPE)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        set(FASTLED_LINKER_TYPE "MSVC" PARENT_SCOPE)
    else()
        set(FASTLED_LINKER_TYPE "GNU" PARENT_SCOPE)
    endif()
    
    # Platform-specific capability adjustments
    if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # Windows + Clang has known LTO linking issues
        set(HAS_LTO_SUPPORT FALSE PARENT_SCOPE)
    endif()
    
    # Report detected capabilities
    message(STATUS "Compiler Detection Results:")
    message(STATUS "  Compiler: ${FASTLED_COMPILER_TYPE}")
    message(STATUS "  Platform: ${FASTLED_PLATFORM_TYPE}")
    message(STATUS "  Linker: ${FASTLED_LINKER_TYPE}")
    message(STATUS "  LTO Support: ${HAS_LTO_SUPPORT}")
    message(STATUS "  Dead Code Elimination: ${HAS_DEAD_CODE_ELIMINATION}")
    message(STATUS "  Stack Trace Support: ${HAS_STACK_TRACE_SUPPORT}")
    message(STATUS "  Fast Math: ${HAS_FAST_MATH}")
    message(STATUS "  Parallel Compilation: ${HAS_PARALLEL_COMPILATION}")
    message(STATUS "  Debug Symbols: ${HAS_DEBUG_SYMBOLS}")
endfunction()

# Function to get compiler information
function(get_compiler_info output_var)
    set(${output_var} "${FASTLED_COMPILER_TYPE} ${CMAKE_CXX_COMPILER_VERSION} on ${FASTLED_PLATFORM_TYPE}" PARENT_SCOPE)
endfunction()

# Function to check if a capability is available
function(is_compiler_capability_available capability result_var)
    if(capability STREQUAL "LTO")
        set(${result_var} ${HAS_LTO_SUPPORT} PARENT_SCOPE)
    elseif(capability STREQUAL "STACK_TRACE")
        set(${result_var} ${HAS_STACK_TRACE_SUPPORT} PARENT_SCOPE)
    elseif(capability STREQUAL "DEAD_CODE_ELIMINATION")
        set(${result_var} ${HAS_DEAD_CODE_ELIMINATION} PARENT_SCOPE)
    elseif(capability STREQUAL "FAST_MATH")
        set(${result_var} ${HAS_FAST_MATH} PARENT_SCOPE)
    elseif(capability STREQUAL "PARALLEL_COMPILATION")
        set(${result_var} ${HAS_PARALLEL_COMPILATION} PARENT_SCOPE)
    elseif(capability STREQUAL "DEBUG_SYMBOLS")
        set(${result_var} ${HAS_DEBUG_SYMBOLS} PARENT_SCOPE)
    else()
        message(WARNING "Unknown capability: ${capability}")
        set(${result_var} FALSE PARENT_SCOPE)
    endif()
endfunction() 
