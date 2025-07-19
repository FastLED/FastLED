# TestSourceDiscovery.cmake
# Test source file discovery and processing logic

# Function to discover and process test source files
function(discover_and_process_test_sources)
    # Determine which test source files to process
    if(DEFINED SPECIFIC_TEST)
        # Only process the specific test file
        set(SPECIFIC_TEST_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/test_${SPECIFIC_TEST}.cpp")
        message(STATUS "🔍 Looking for specific test file: ${SPECIFIC_TEST_SOURCE}")
        if(EXISTS ${SPECIFIC_TEST_SOURCE})
            set(TEST_SOURCES ${SPECIFIC_TEST_SOURCE})
            message(STATUS "✅ Building ONLY specific test: test_${SPECIFIC_TEST}")
        else()
            message(FATAL_ERROR "❌ Specific test file not found: ${SPECIFIC_TEST_SOURCE}")
        endif()
    else()
        # Find all test source files for full build
        file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp")
        list(LENGTH TEST_SOURCES TEST_COUNT)
        message(STATUS "🌐 Building ALL tests - found ${TEST_COUNT} test files")
    endif()
    
    # Set the TEST_SOURCES variable in parent scope
    set(TEST_SOURCES ${TEST_SOURCES} PARENT_SCOPE)
endfunction()

# Function to process test targets with modular creation
function(process_test_targets)
    # Discover test sources first
    discover_and_process_test_sources()
    
    # Create test executables using modular approach
    foreach(TEST_SOURCE ${TEST_SOURCES})
        get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
        
        # For specific test builds, we know this is the right test
        if(DEFINED SPECIFIC_TEST)
            string(REPLACE "test_" "" CURRENT_TEST_NAME ${TEST_NAME})
            message(STATUS "Processing specific test: ${TEST_NAME}")
        endif()
        
        # Display verbose test creation logging (handled by TestConfiguration module)
        display_verbose_test_creation_logging(${TEST_NAME} ${TEST_SOURCE})
        
        # Create test executable using modular system
        create_test_executable(${TEST_NAME} ${TEST_SOURCE})
        
        # Register with CTest
        register_test_executable(${TEST_NAME})
        
        # Display detailed linker flags for specific test builds (debugging)
        if(DEFINED SPECIFIC_TEST AND CURRENT_TEST_NAME STREQUAL SPECIFIC_TEST)
            display_target_linker_flags(${TEST_NAME})
        endif()
    endforeach()
endfunction()

# Function to setup FastLED source directory
function(setup_fastled_source_directory)
    # Set path to FastLED source directory
    set(FASTLED_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
    
    # Include FastLED source directory
    include_directories(${FASTLED_SOURCE_DIR}/src)
    
    # Delegate source file computation to src/CMakeLists.txt
    add_subdirectory(${FASTLED_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/fastled)
    
    message(STATUS "FastLED source directory configured: ${FASTLED_SOURCE_DIR}")
endfunction() 
