# OptimizationSettings.cmake
# Cross-platform optimization control with performance/size trade-offs

# Global optimization settings
set(FASTLED_OPTIMIZATION_LEVEL "O0" CACHE STRING "Optimization level")
set_property(CACHE FASTLED_OPTIMIZATION_LEVEL PROPERTY STRINGS "O0;O1;O2;O3;Os;Ofast")

# Function to set optimization level
function(set_optimization_level level)
    set(opt_flags "")
    
    # Clear any existing optimization flags from CMAKE_CXX_FLAGS to prevent conflicts
    string(REGEX REPLACE "-O[0-9s]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REGEX REPLACE "-Ofast" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REGEX REPLACE "/O[12dx]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REGEX REPLACE "/Ox" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    
    # Also clear from C flags
    string(REGEX REPLACE "-O[0-9s]" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    string(REGEX REPLACE "-Ofast" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    string(REGEX REPLACE "/O[12dx]" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    string(REGEX REPLACE "/Ox" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    
    # Set cleaned flags back to parent scope before adding new optimization
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" PARENT_SCOPE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}" PARENT_SCOPE)
    
    if(level STREQUAL "O0")
        # No optimization for debugging
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            set(opt_flags "-O0")
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
            set(opt_flags "/Od")
        endif()
        message(STATUS "Optimization: Level 0 (no optimization)")
        
    elseif(level STREQUAL "O1")
        # Basic optimization
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            set(opt_flags "-O1")
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
            set(opt_flags "/O1")
        endif()
        message(STATUS "Optimization: Level 1 (basic)")
        
    elseif(level STREQUAL "O2")
        # Standard optimization
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            set(opt_flags "-O2")
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
            set(opt_flags "/O2")
        endif()
        message(STATUS "Optimization: Level 2 (standard)")
        
    elseif(level STREQUAL "O3")
        # Aggressive optimization
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            set(opt_flags "-O3")
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
            set(opt_flags "/Ox")
        endif()
        message(STATUS "Optimization: Level 3 (aggressive)")
        
    elseif(level STREQUAL "Os")
        # Size optimization
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            set(opt_flags "-Os")
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
            set(opt_flags "/O1" "/Os")
        endif()
        message(STATUS "Optimization: Size optimized")
        
    elseif(level STREQUAL "Ofast")
        # Fast math optimization
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            set(opt_flags "-Ofast")
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
            set(opt_flags "/Ox" "/fp:fast")
        endif()
        message(STATUS "Optimization: Fast math (aggressive)")
        
    else()
        message(WARNING "Unknown optimization level: ${level}, using O2")
        set_optimization_level("O2")
        return()
    endif()
    
    # Apply cleaned flags and new optimization flags globally
    string(REPLACE ";" " " opt_flags_str "${opt_flags}")
    # Set the cleaned flags (without old optimization) plus new optimization
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${opt_flags_str}" PARENT_SCOPE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${opt_flags_str}" PARENT_SCOPE)
    
    set(FASTLED_OPTIMIZATION_LEVEL ${level} PARENT_SCOPE)
endfunction()

# Function to enable LTO (Link-Time Optimization)
function(enable_lto)
    is_compiler_capability_available("LTO" lto_available)
    
    if(lto_available)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            # Use thin LTO for faster linking - manual flags only, no CMAKE_INTERPROCEDURAL_OPTIMIZATION
            message(STATUS "Enabling thin LTO for Clang compiler")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto=thin" PARENT_SCOPE)
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -flto=thin" PARENT_SCOPE)
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto=thin" PARENT_SCOPE)
            
            # Enable thin archives for faster linking
            set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> rcT <TARGET> <OBJECTS>" PARENT_SCOPE)
            set(CMAKE_CXX_ARCHIVE_APPEND "<CMAKE_AR> rT <TARGET> <OBJECTS>" PARENT_SCOPE)
            set(CMAKE_CXX_ARCHIVE_FINISH "<CMAKE_RANLIB> <TARGET>" PARENT_SCOPE)
            
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
            # Manual LTO flags for GCC - no CMAKE_INTERPROCEDURAL_OPTIMIZATION
            message(STATUS "Enabling LTO for GCC compiler")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto=auto -fno-fat-lto-objects" PARENT_SCOPE)
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -flto=auto -fno-fat-lto-objects" PARENT_SCOPE)
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto=auto" PARENT_SCOPE)
            
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
            message(STATUS "Enabling Link-Time Code Generation for MSVC")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GL" PARENT_SCOPE)
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /GL" PARENT_SCOPE)
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LTCG" PARENT_SCOPE)
        endif()
        
        message(STATUS "Link-Time Optimization enabled (manual flags)")
    else()
        message(STATUS "LTO disabled: ${DISABLE_THIN_REASON}")
    endif()
endfunction()

# Function to disable LTO
function(disable_lto)
    # Don't use CMAKE_INTERPROCEDURAL_OPTIMIZATION - we use manual flags
    
    # Remove any existing LTO flags from compiler options
    string(REPLACE "-flto=thin" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REPLACE "-flto=auto" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REPLACE "-flto" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REPLACE "-fno-fat-lto-objects" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} PARENT_SCOPE)
    
    string(REPLACE "-flto=thin" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    string(REPLACE "-flto=auto" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    string(REPLACE "-flto" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    string(REPLACE "-fno-fat-lto-objects" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} PARENT_SCOPE)
    
    # Remove LTO flags from linker options
    string(REPLACE "-flto=thin" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    string(REPLACE "-flto=auto" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    string(REPLACE "-flto" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS} PARENT_SCOPE)
    
    message(STATUS "Link-Time Optimization disabled")
endfunction()

# Function to configure fast math optimizations
function(configure_fast_math)
    is_compiler_capability_available("FAST_MATH" fast_math_available)
    
    if(fast_math_available)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            set(math_flags "-ffast-math" "-fno-math-errno" "-funsafe-math-optimizations")
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
            set(math_flags "/fp:fast")
        endif()
        
        string(REPLACE ";" " " math_flags_str "${math_flags}")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${math_flags_str}" PARENT_SCOPE)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${math_flags_str}" PARENT_SCOPE)
        
        message(STATUS "Fast math optimizations enabled")
    else()
        message(STATUS "Fast math optimizations not available for this compiler")
    endif()
endfunction()

# Function to configure size optimization
function(configure_size_optimization)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(size_flags "-Os" "-ffunction-sections" "-fdata-sections")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        set(size_flags "/O1" "/Os" "/Gy")
    endif()
    
    string(REPLACE ";" " " size_flags_str "${size_flags}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${size_flags_str}" PARENT_SCOPE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${size_flags_str}" PARENT_SCOPE)
    
    # Apply dead code elimination at link time
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/LinkerCompatibility.cmake)
    get_dead_code_elimination_flags(dce_flags)
    if(dce_flags)
        string(REPLACE ";" " " dce_flags_str "${dce_flags}")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${dce_flags_str}" PARENT_SCOPE)
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${dce_flags_str}" PARENT_SCOPE)
        set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${dce_flags_str}" PARENT_SCOPE)
    endif()
    
    message(STATUS "Size optimization configured")
endfunction()

# Function to configure dead code elimination
function(configure_dead_code_elimination)
    is_compiler_capability_available("DEAD_CODE_ELIMINATION" dce_available)
    
    if(dce_available)
        # Enable function and data sections for dead code elimination
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            set(dce_flags "-ffunction-sections" "-fdata-sections")
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
            set(dce_flags "/Gy")  # Function-level linking
        endif()
        
        string(REPLACE ";" " " dce_flags_str "${dce_flags}")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${dce_flags_str}" PARENT_SCOPE)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${dce_flags_str}" PARENT_SCOPE)
        
        # Get platform-appropriate linker flags
        include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/LinkerCompatibility.cmake)
        get_dead_code_elimination_flags(dce_linker_flags)
        
        if(dce_linker_flags)
            string(REPLACE ";" " " dce_linker_str "${dce_linker_flags}")
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${dce_linker_str}" PARENT_SCOPE)
            set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${dce_linker_str}" PARENT_SCOPE)
            set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${dce_linker_str}" PARENT_SCOPE)
            message(STATUS "Dead code elimination enabled: ${dce_linker_flags}")
        else()
            message(STATUS "Dead code elimination not available for this compiler/linker")
        endif()
    else()
        message(STATUS "Dead code elimination disabled via NO_DEAD_CODE_ELIMINATION option")
    endif()
endfunction()

# Optimization profiles for common use cases
function(apply_development_profile)
    message(STATUS "Applying development optimization profile")
    set_optimization_level("O0")         # No optimization
    enable_stack_traces()               # Keep debugging capability
    disable_lto()                       # Faster compilation
    configure_dead_code_elimination()   # Still remove unused code
endfunction()

function(apply_release_profile)
    message(STATUS "Applying release optimization profile")
    set_optimization_level("O2")         # Standard optimization
    enable_lto()                         # Link-time optimization
    configure_dead_code_elimination()    # Remove unused code
endfunction()

function(apply_size_profile)
    message(STATUS "Applying size optimization profile")
    set_optimization_level("Os")         # Size optimization
    enable_lto()                         # Link-time optimization
    configure_size_optimization()        # Additional size flags
    configure_dead_code_elimination()    # Remove unused code
endfunction()

function(apply_performance_profile)
    message(STATUS "Applying performance optimization profile")
    set_optimization_level("O3")         # Aggressive optimization
    enable_lto()                         # Link-time optimization
    configure_fast_math()               # Aggressive math optimizations
    configure_dead_code_elimination()    # Remove unused code
endfunction()

# Function to configure build settings based on build type
function(configure_build_type_settings)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        # Release builds: optimized for performance
        message(STATUS "OptimizationSettings: Configuring Release build")
        configure_release_build()
        set_optimization_level("O2")
        if(NOT NO_THIN_LTO)
            enable_lto()
        else()
            message(STATUS "OptimizationSettings: LTO disabled by NO_THIN_LTO option")
        endif()
        configure_dead_code_elimination()
    else()
        # Debug, Quick, and other builds: optimized for compilation speed and debugging
        message(STATUS "OptimizationSettings: Configuring Debug/non-Release build (${CMAKE_BUILD_TYPE})")
        configure_debug_build()
        set_optimization_level("O0")
        configure_dead_code_elimination()
    endif()
endfunction() 
