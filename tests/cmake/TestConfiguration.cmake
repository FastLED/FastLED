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
