# TestConfiguration.cmake
# CTest integration and test discovery

# Function to configure CTest
function(configure_ctest)
    # Enable testing
    enable_testing()
    
    # Set test output options
    set(CMAKE_CTEST_ARGUMENTS "--output-on-failure" PARENT_SCOPE)
    
    # Configure test timeouts
    configure_test_timeouts()
    
    # Configure test parallelization
    enable_test_parallelization()
    
    # Configure test output formatting
    configure_test_output()
    
    message(STATUS "CTest configuration complete")
endfunction()

# Function to register a test executable with CTest
function(register_test_executable target)
    # Only register if it's an executable (not object library)
    if(NOT NO_LINK)
        # Add test to CTest
        add_test(NAME ${target} COMMAND ${target})
        
        # Set test properties
        set_tests_properties(${target} PROPERTIES
            TIMEOUT 300  # 5 minutes default timeout
            FAIL_REGULAR_EXPRESSION "FAILED|ERROR|SEGFAULT|SIGSEGV"
        )
        
        # Set working directory for test
        set_tests_properties(${target} PROPERTIES
            WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        )
        
        message(STATUS "Registered test: ${target}")
    endif()
endfunction()

# Function to configure test timeouts
function(configure_test_timeouts)
    # Default timeout for all tests (5 minutes)
    set(CTEST_TEST_TIMEOUT 300 CACHE INTERNAL "Default test timeout")
    
    # Set different timeouts for different test types
    set(QUICK_TEST_TIMEOUT 60 CACHE INTERNAL "Quick test timeout")     # 1 minute for quick tests
    set(LONG_TEST_TIMEOUT 600 CACHE INTERNAL "Long test timeout")     # 10 minutes for long tests
    
    message(STATUS "Test timeouts configured: Default 300s, Quick 60s, Long 600s")
endfunction()

# Function to enable test parallelization
function(enable_test_parallelization)
    # Use the same parallel job count as build
    if(CMAKE_BUILD_PARALLEL_LEVEL)
        set(CTEST_PARALLEL_LEVEL ${CMAKE_BUILD_PARALLEL_LEVEL} PARENT_SCOPE)
        message(STATUS "Test parallelization enabled: ${CMAKE_BUILD_PARALLEL_LEVEL} parallel jobs")
    else()
        # Fall back to detected cores
        include(ProcessorCount)
        ProcessorCount(CPU_COUNT)
        if(CPU_COUNT)
            set(CTEST_PARALLEL_LEVEL ${CPU_COUNT} PARENT_SCOPE)
            message(STATUS "Test parallelization enabled: ${CPU_COUNT} parallel jobs (CPU count)")
        else()
            set(CTEST_PARALLEL_LEVEL 4 PARENT_SCOPE)
            message(STATUS "Test parallelization enabled: 4 parallel jobs (default)")
        endif()
    endif()
endfunction()

# Function to configure test output formatting
function(configure_test_output)
    # Configure CTest to show output on failure
    set(CTEST_OUTPUT_ON_FAILURE ON PARENT_SCOPE)
    
    # Configure verbose output for debugging
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CTEST_VERBOSE ON PARENT_SCOPE)
    endif()
    
    # Set test result formatting
    set(CTEST_CUSTOM_TESTS_IGNORE "" PARENT_SCOPE)
    
    message(STATUS "Test output formatting configured")
endfunction()

# Function to register all test executables with specific patterns
function(register_all_test_executables)
    # Get all test source files
    file(GLOB TEST_SOURCES "test_*.cpp")
    
    set(registered_count 0)
    foreach(TEST_SOURCE ${TEST_SOURCES})
        get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
        
        # Register the test executable
        register_test_executable(${TEST_NAME})
        
        # Apply specific configurations based on test name patterns
        configure_test_by_pattern(${TEST_NAME})
        
        math(EXPR registered_count "${registered_count} + 1")
    endforeach()
    
    if(NOT NO_LINK)
        message(STATUS "Registered ${registered_count} tests with CTest")
    else()
        message(STATUS "Skipped test registration (NO_LINK mode)")
    endif()
endfunction()

# Function to configure tests based on naming patterns
function(configure_test_by_pattern test_name)
    if(NOT NO_LINK)
        # Quick tests (should run fast)
        if(test_name MATCHES ".*quick.*|.*fast.*|.*simple.*")
            set_tests_properties(${test_name} PROPERTIES TIMEOUT ${QUICK_TEST_TIMEOUT})
            set_tests_properties(${test_name} PROPERTIES LABELS "quick")
            message(STATUS "Configured ${test_name} as quick test (${QUICK_TEST_TIMEOUT}s timeout)")
            
        # Long tests (may take more time)
        elseif(test_name MATCHES ".*long.*|.*slow.*|.*stress.*|.*performance.*")
            set_tests_properties(${test_name} PROPERTIES TIMEOUT ${LONG_TEST_TIMEOUT})
            set_tests_properties(${test_name} PROPERTIES LABELS "long")
            message(STATUS "Configured ${test_name} as long test (${LONG_TEST_TIMEOUT}s timeout)")
            
        # Integration tests
        elseif(test_name MATCHES ".*integration.*|.*system.*")
            set_tests_properties(${test_name} PROPERTIES TIMEOUT ${LONG_TEST_TIMEOUT})
            set_tests_properties(${test_name} PROPERTIES LABELS "integration")
            message(STATUS "Configured ${test_name} as integration test")
            
        # Unit tests (default)
        else()
            set_tests_properties(${test_name} PROPERTIES LABELS "unit")
            message(STATUS "Configured ${test_name} as unit test (default timeout)")
        endif()
    endif()
endfunction()

# Function to create test suites and labels
function(create_test_suites)
    if(NOT NO_LINK)
        # Create test suites based on labels
        # Note: set_tests_properties for all tests is handled per-test above
        
        # Quick test suite
        add_custom_target(test_quick
            COMMAND ${CMAKE_CTEST_COMMAND} -L quick --output-on-failure
            COMMENT "Running quick tests"
        )
        
        # Unit test suite
        add_custom_target(test_unit
            COMMAND ${CMAKE_CTEST_COMMAND} -L unit --output-on-failure
            COMMENT "Running unit tests"
        )
        
        # Integration test suite
        add_custom_target(test_integration
            COMMAND ${CMAKE_CTEST_COMMAND} -L integration --output-on-failure
            COMMENT "Running integration tests"
        )
        
        # Long test suite
        add_custom_target(test_long
            COMMAND ${CMAKE_CTEST_COMMAND} -L long --output-on-failure
            COMMENT "Running long tests"
        )
        
        message(STATUS "Created test suites: test_quick, test_unit, test_integration, test_long")
    endif()
endfunction()

# Function to configure test environment
function(configure_test_environment)
    # Set environment variables for tests
    set(ENV{FASTLED_TESTING} "1")
    set(ENV{FASTLED_FORCE_NAMESPACE} "1")
    
    # Set test output directory
    if(CMAKE_RUNTIME_OUTPUT_DIRECTORY)
        set(ENV{FASTLED_TEST_OUTPUT_DIR} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    endif()
    
    # Configure crash handling for tests
    if(USE_LIBUNWIND)
        set(ENV{FASTLED_ENABLE_STACK_TRACES} "1")
    endif()
    
    message(STATUS "Test environment configured")
endfunction()

# Function to set up complete test configuration
function(setup_test_configuration)
    message(STATUS "=== Test Configuration Setup ===")
    
    # Configure CTest
    configure_ctest()
    
    # Configure test environment
    configure_test_environment()
    
    # Register all test executables
    register_all_test_executables()
    
    # Create test suites
    create_test_suites()
    
    message(STATUS "=== Test Configuration Setup Complete ===")
endfunction()

# Function to display build configuration summary
function(display_build_configuration_summary)
    message(STATUS "")
    message(STATUS "🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨")
    message(STATUS "🚨                                  BUILD FLAGS SUMMARY                                 🚨")
    message(STATUS "🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨")
    message(STATUS "")

    # Display Build Configuration
    message(STATUS "📚 BUILD CONFIGURATION (MODULAR SYSTEM):")
    message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
    message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
    message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
    message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}")
    message(STATUS "")

    # Show current CMAKE compiler flags (applied globally via modules)
    message(STATUS "  📌 Global CMAKE_CXX_FLAGS (applied to all C++ files):")
    message(STATUS "      ${CMAKE_CXX_FLAGS}")
    message(STATUS "")
    message(STATUS "  📌 Global CMAKE_C_FLAGS (applied to all C files):")
    message(STATUS "      ${CMAKE_C_FLAGS}")
    message(STATUS "")

    # Show compile definitions
    get_directory_property(ALL_DEFINITIONS COMPILE_DEFINITIONS)
    string(REPLACE ";" " " ALL_DEFINITIONS_STR "${ALL_DEFINITIONS}")
    message(STATUS "  📌 Preprocessor Definitions:")
    message(STATUS "      ${ALL_DEFINITIONS_STR}")
    message(STATUS "")

    # Show linker flags
    message(STATUS "🔗 LINKER FLAGS:")
    message(STATUS "  📌 CMAKE_EXE_LINKER_FLAGS (executables):")
    message(STATUS "      ${CMAKE_EXE_LINKER_FLAGS}")
    message(STATUS "")

    # Additional debug information (module-managed configurations)
    message(STATUS "🔧 MODULAR CONFIGURATION:")
    message(STATUS "  📌 All compiler flags and dependencies managed by modules")
    message(STATUS "  📌 Libunwind: Handled by DependencyManagement module")  
    message(STATUS "  📌 Dead Code Elimination: Handled by OptimizationSettings module")
    message(STATUS "")

    message(STATUS "🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨🚨")
    message(STATUS "")
endfunction()

# Function to display target-specific linker flags for debugging
function(display_target_linker_flags target_name)
    message(STATUS "")
    message(STATUS "🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗")
    message(STATUS "🔗                        FINAL LINKER FLAGS: ${target_name}                         🔗")
    message(STATUS "🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗")
    message(STATUS "")
    
    # Get the actual target properties
    get_target_property(TEST_LINK_FLAGS ${target_name} LINK_FLAGS)
    if(TEST_LINK_FLAGS)
        message(STATUS "  📌 Target-specific Link Flags:")
        message(STATUS "     ${TEST_LINK_FLAGS}")
    else()
        message(STATUS "  📌 Target-specific Link Flags: (none)")
    endif()
    message(STATUS "")
    
    message(STATUS "  📌 Global Executable Linker Flags:")
    message(STATUS "     CMAKE_EXE_LINKER_FLAGS: ${CMAKE_EXE_LINKER_FLAGS}")
    message(STATUS "")
    
    if(WIN32)
        message(STATUS "  📌 Windows-specific Configuration:")
        get_target_property(TEST_WIN32_EXECUTABLE ${target_name} WIN32_EXECUTABLE)
        message(STATUS "     WIN32_EXECUTABLE: ${TEST_WIN32_EXECUTABLE}")
        message(STATUS "     - Console subsystem enforced")
        message(STATUS "     - Debug symbols included")
    endif()
    
    if(NOT APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        message(STATUS "  📌 Static Runtime Linking:")
        message(STATUS "     -static-libgcc -static-libstdc++")
    endif()
    
    message(STATUS "")
    message(STATUS "🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗")
    message(STATUS "🔗                      END FINAL LINKER FLAGS: ${target_name}                       🔗")
    message(STATUS "🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗🔗")
    message(STATUS "")
endfunction()

# Function to display build summary
function(display_build_summary)
    # Build summary
    get_property(ALL_TARGETS DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY BUILDSYSTEM_TARGETS)
    list(FILTER ALL_TARGETS INCLUDE REGEX "^test_")
    list(LENGTH ALL_TARGETS TARGET_COUNT)
    if(DEFINED SPECIFIC_TEST)
        message(STATUS "Build summary: Created ${TARGET_COUNT} executable targets (specific test mode)")
    else()
        message(STATUS "Build summary: Created ${TARGET_COUNT} executable targets (full build mode)")
    endif()
    
    # Configure verbose test output
    set(CMAKE_CTEST_ARGUMENTS "--output-on-failure" PARENT_SCOPE)
endfunction()

# Function to display verbose test creation logging for specific tests
function(display_verbose_test_creation_logging test_name test_source)
    # Only display verbose logging for specific test builds
    if(NOT DEFINED SPECIFIC_TEST)
        return()
    endif()
    
    # Extract test name without "test_" prefix for comparison
    string(REPLACE "test_" "" current_test_name ${test_name})
    if(NOT current_test_name STREQUAL SPECIFIC_TEST)
        return()
    endif()
    
    message(STATUS "")
    message(STATUS "🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪")
    message(STATUS "🧪                        MODULAR TEST CREATION: ${test_name}                       🧪")
    message(STATUS "🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪")
    message(STATUS "")
    message(STATUS "  📁 Source File: ${test_source}")
    message(STATUS "  🎯 Target Name: ${test_name}")
    message(STATUS "")
    
    message(STATUS "  🔧 Modular Target Creation:")
    message(STATUS "     - Using create_test_executable() from TargetCreation module")
    message(STATUS "     - Automatic Windows configuration via configure_windows_executable()")
    message(STATUS "     - Standard test settings via apply_test_settings()")
    message(STATUS "     - Unit test flags via apply_unit_test_flags()")
    message(STATUS "     - CTest registration via register_test_executable()")
    message(STATUS "")
    
    message(STATUS "  📌 Modular system will automatically provide:")
    message(STATUS "     - FastLED library linking")
    message(STATUS "     - Test infrastructure (test_shared_static) linking")
    message(STATUS "     - C++17 standard compliance")
    message(STATUS "     - Platform-specific debugging libraries (Windows: dbghelp, psapi)")
    message(STATUS "     - Stack unwinding support (if available)")
    message(STATUS "     - Static runtime linking (GNU on non-Apple platforms)")
    message(STATUS "     - Consistent compile definitions and flags")
    message(STATUS "")
    
    message(STATUS "🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪")
    message(STATUS "🧪                       END MODULAR TEST CREATION: ${test_name}                    🧪")
    message(STATUS "🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪🧪")
    message(STATUS "")
endfunction() 
