# TargetCreation.cmake
# Streamlined test target creation with automatic configuration

# Global target settings
set(FASTLED_TARGET_SETTINGS_APPLIED FALSE)

# Function to apply test settings to a target
function(apply_test_settings target)
    # Set C++17 standard
    target_compile_features(${target} PRIVATE cxx_std_17)
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )
    
    # Include current source directory for tests
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    
    # Apply common compile definitions
    target_compile_definitions(${target} PRIVATE 
        DEBUG
        FASTLED_FORCE_NAMESPACE=1
        FASTLED_USE_JSON_UI=1
        FASTLED_NO_AUTO_NAMESPACE
        FASTLED_TESTING
        ENABLE_CRASH_HANDLER
        FASTLED_STUB_IMPL
        FASTLED_NO_PINMAP
        HAS_HARDWARE_PIN_SUPPORT
        _GLIBCXX_DEBUG
        _GLIBCXX_DEBUG_PEDANTIC
        FASTLED_FIVE_BIT_HD_GAMMA_FUNCTION_2_8
        PROGMEM=
    )
    
    # Fix for Microsoft STL version check with older Clang versions on Windows
    if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_definitions(${target} PRIVATE _ALLOW_COMPILER_AND_STL_VERSION_MISMATCH)
    endif()
    
    # Set FASTLED_ALL_SRC=1 for clang builds or when explicitly requested
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR DEFINED ENV{FASTLED_ALL_SRC})
        target_compile_definitions(${target} PRIVATE FASTLED_ALL_SRC=1)
    endif()
    
    # Add libunwind support if available
    if(USE_LIBUNWIND)
        target_compile_definitions(${target} PRIVATE USE_LIBUNWIND)
        if(LIBUNWIND_LIBRARIES)
            target_link_libraries(${target} ${LIBUNWIND_LIBRARIES})
        endif()
        if(LIBUNWIND_INCLUDE_DIRS)
            target_include_directories(${target} PRIVATE ${LIBUNWIND_INCLUDE_DIRS})
        endif()
    endif()
    
    message(STATUS "Applied test settings to target: ${target}")
endfunction()

# Function to apply unit test compiler flags to a target
function(apply_unit_test_flags target)
    # Base unit test flags (more permissive than library flags)
    set(UNIT_TEST_FLAGS
        -Wall
        -funwind-tables
        -fno-rtti                    # 🚨 CRITICAL: Ensure RTTI is disabled at target level
        -Werror=return-type
        -Werror=missing-declarations
        -Werror=init-self
        -Werror=date-time
        -Werror=uninitialized
        -Werror=array-bounds
        -Werror=null-dereference
        -Werror=deprecated-declarations
        -Wno-comment
    )
    
    # 🚨 CRITICAL: Verify -fno-rtti is in the flags
    list(FIND UNIT_TEST_FLAGS "-fno-rtti" rtti_flag_index)
    if(rtti_flag_index EQUAL -1)
        message(FATAL_ERROR "🚨 CRITICAL: -fno-rtti missing from UNIT_TEST_FLAGS for target ${target}!")
    endif()
    
    # 🚨 CRITICAL: Universal RTTI enforcement at target level (all platforms)
    message(STATUS "🚨 Universal target-level RTTI enforcement for: ${target}")
    # Add -fno-rtti multiple times and with different approaches to ensure it sticks
    list(APPEND UNIT_TEST_FLAGS "-fno-rtti")  # Add again to be absolutely sure
    target_compile_options(${target} PRIVATE "-fno-rtti")  # Apply directly to target
    target_compile_definitions(${target} PRIVATE "FASTLED_CHECK_NO_RTTI=1")  # Enable compile-time check
    message(STATUS "✅ Universal RTTI disabled for target: ${target}")
    
    # Add GCC-specific unit test warning flags
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        list(APPEND UNIT_TEST_FLAGS -Werror=maybe-uninitialized)
    endif()
    
    # C++-specific unit test flags
    set(UNIT_TEST_CXX_FLAGS
        -Werror=suggest-override
        -Werror=non-virtual-dtor
        -Werror=switch-enum
        -Werror=delete-non-virtual-dtor
    )
    
    # Apply flags to target
    target_compile_options(${target} PRIVATE ${UNIT_TEST_FLAGS})
    target_compile_options(${target} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${UNIT_TEST_CXX_FLAGS}>)
    
    message(STATUS "Applied unit test flags to target: ${target} (✅ RTTI disabled)")
endfunction()

# Function to create a test executable with standard settings
function(create_test_executable name sources)
    # Build phase control - check if we should create executable or object library
    if(NO_LINK)
        # Create object library instead of executable
        add_library(${name} OBJECT ${sources})
        message(STATUS "Created object library: ${name}")
    else()
        # Create executable
        add_executable(${name} ${sources})
        
        # Link against FastLED library
        target_link_libraries(${name} fastled)
        
        # Link against test infrastructure
        target_link_libraries(${name} test_shared_static)
        
        # Configure Windows executable settings if needed
        if(WIN32)
            configure_windows_executable(${name})
        endif()
        
        # Apply platform-specific static runtime linking
        apply_static_runtime_linking(${name})
        
        message(STATUS "Created test executable: ${name}")
    endif()
    
    # Apply common test settings and flags
    apply_test_settings(${name})
    apply_unit_test_flags(${name})
    
    # Set the target as created
    set(${name}_CREATED TRUE PARENT_SCOPE)
endfunction()

# Function to create a test library
function(create_test_library name sources)
    add_library(${name} STATIC ${sources})
    
    # Apply common test settings
    apply_test_settings(${name})
    apply_unit_test_flags(${name})
    
    message(STATUS "Created test library: ${name}")
endfunction()

# Function to configure Windows-specific executable settings
function(configure_windows_executable target)
    if(WIN32)
        # Add Windows debugging libraries for crash handler
        target_link_libraries(${target} dbghelp psapi)
        
        # Add Windows socket libraries for networking support
        target_link_libraries(${target} ws2_32 wsock32)
        
        # Windows subsystem settings
        if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
            set_target_properties(${target} PROPERTIES
                LINK_FLAGS "/SUBSYSTEM:CONSOLE"
            )
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            # Use compatibility layer for Windows + Clang + lld-link
            get_windows_debug_build_flags(win_compiler_flags win_linker_flags)
            get_subsystem_flags(subsystem_flags "console")
            
            # Apply the platform-appropriate flags
            if(win_compiler_flags)
                target_compile_options(${target} PRIVATE ${win_compiler_flags})
            endif()
            
            # Combine subsystem and debug linker flags
            set(combined_linker_flags ${subsystem_flags} ${win_linker_flags})
            if(combined_linker_flags)
                string(REPLACE ";" " " combined_linker_flags_str "${combined_linker_flags}")
                set_target_properties(${target} PROPERTIES
                    WIN32_EXECUTABLE FALSE
                    LINK_FLAGS "${combined_linker_flags_str}")
            else()
                set_target_properties(${target} PROPERTIES
                    WIN32_EXECUTABLE FALSE)
            endif()
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
            # GCC/MinGW on Windows - ensure console subsystem
            set_target_properties(${target} PROPERTIES
                WIN32_EXECUTABLE FALSE
            )
            # MinGW automatically uses console subsystem for non-WIN32 executables
        endif()
        
        # Ensure debug information is preserved for all compilers
        target_compile_definitions(${target} PRIVATE _DEBUG)
        
        message(STATUS "Applied Windows executable settings to: ${target}")
    endif()
endfunction()

# Function to create the test infrastructure library
function(create_test_infrastructure)
    # Create static library for test infrastructure
    add_library(test_shared_static STATIC doctest_main.cpp)
    
    # Apply test settings
    apply_test_settings(test_shared_static)
    apply_unit_test_flags(test_shared_static)
    
    message(STATUS "Created test infrastructure library: test_shared_static")
endfunction()

# Function to create all test targets from source files
function(create_all_test_targets)
    # Get all test source files
    file(GLOB TEST_SOURCES "test_*.cpp")
    
    set(target_count 0)
    foreach(TEST_SOURCE ${TEST_SOURCES})
        get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
        
        # Create test executable
        create_test_executable(${TEST_NAME} ${TEST_SOURCE})
        
        math(EXPR target_count "${target_count} + 1")
    endforeach()
    
    # Build summary
    if(NO_LINK)
        message(STATUS "Build summary: Created ${target_count} object library targets (NO_LINK mode)")
        message(STATUS "  Object files will be in: ${CMAKE_BINARY_DIR}/CMakeFiles/test_*/")
        message(STATUS "  To link executables later, run: cmake -DNO_BUILD=ON . && make")
    elseif(NO_BUILD)
        message(STATUS "Build summary: Created ${target_count} executable targets from existing objects (NO_BUILD mode)")
        message(STATUS "  Executables will be in: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/")
    else()
        message(STATUS "Build summary: Created ${target_count} executable targets (full build mode)")
    endif()
endfunction()

# Function to clean orphaned binaries
function(clean_orphaned_binaries)
    option(CLEAN_ORPHANED_BINARIES "Remove orphaned test binaries" ON)
    
    if(CLEAN_ORPHANED_BINARIES)
        # Find existing test binaries
        file(GLOB_RECURSE TEST_BINARIES 
            "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_*"
            "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_*.exe"
        )
        
        foreach(ORPHANED_BINARY ${TEST_BINARIES})
            get_filename_component(BINARY_NAME ${ORPHANED_BINARY} NAME_WE)
            get_filename_component(BINARY_DIR ${ORPHANED_BINARY} DIRECTORY)
            get_filename_component(PARENT_DIR ${BINARY_DIR} DIRECTORY)
            get_filename_component(GRANDPARENT_DIR ${PARENT_DIR} DIRECTORY)
            set(CORRESPONDING_SOURCE "${GRANDPARENT_DIR}/${BINARY_NAME}.cpp")
            
            if(NOT EXISTS "${CORRESPONDING_SOURCE}")
                message(STATUS "Found orphaned binary without source: ${ORPHANED_BINARY}")
                file(REMOVE "${ORPHANED_BINARY}")
                message(STATUS "Deleted orphaned binary: ${ORPHANED_BINARY}")
            endif()
        endforeach()
    endif()
endfunction() 
