# BuildOptions.cmake
# Build option declarations and validation logic

# Function to declare all build options
function(declare_build_options)
    # Option to disable thin optimizations (archives + LTO) for debugging or compatibility
    option(NO_THIN_LTO "Disable thin optimizations (archives and LTO)" OFF)
    
    # Option to disable dead code elimination for debugging or compatibility
    option(NO_DEAD_CODE_ELIMINATION "Disable dead code elimination (function/data sections + gc-sections)" OFF)
    
    # Build phase control options
    option(NO_LINK "Compile object files but skip linking executables" OFF)
    option(NO_BUILD "Skip compilation but perform linking (requires existing object files)" OFF)
    
    message(STATUS "BuildOptions: Build options declared")
endfunction()

# Function to validate build options for conflicts
function(validate_build_options)
    # Validate build phase options for conflicts
    if(NO_LINK AND NO_BUILD)
        message(FATAL_ERROR "BuildOptions: NO_LINK and NO_BUILD cannot both be enabled simultaneously")
    endif()
    
    message(STATUS "BuildOptions: Build options validated - no conflicts detected")
endfunction()

# Function to report current build mode based on options
function(report_build_mode)
    if(NO_LINK)
        message(STATUS "BuildOptions: Build mode: Compile-only (NO_LINK enabled)")
        message(STATUS "BuildOptions:   Object files will be created but executables will not be linked")
    elseif(NO_BUILD)
        message(STATUS "BuildOptions: Build mode: Link-only (NO_BUILD enabled)")
        message(STATUS "BuildOptions:   Existing object files will be linked but no compilation will occur")
    else()
        message(STATUS "BuildOptions: Build mode: Full build (compile + link)")
    endif()
endfunction()

# Function to mark manually-specified variables as used (prevents CMake warnings)
function(mark_variables_as_used)
    # Explicitly reference variables that might be set via -D flags to prevent
    # "Manually-specified variables were not used" warnings
    if(DEFINED NO_LINK)
        # Variable is used in create_test_executable() function
        message(STATUS "BuildOptions: NO_LINK variable acknowledged (set to: ${NO_LINK})")
    endif()
    
    if(DEFINED NO_BUILD)
        # Variable is declared but implementation may be added later
        message(STATUS "BuildOptions: NO_BUILD variable acknowledged (set to: ${NO_BUILD})")
    endif()
    
    if(DEFINED NO_THIN_LTO)
        # Variable is used in optimization configuration
        message(STATUS "BuildOptions: NO_THIN_LTO variable acknowledged (set to: ${NO_THIN_LTO})")
    endif()
    
    if(DEFINED NO_DEAD_CODE_ELIMINATION)
        # Variable is used in linker configuration
        message(STATUS "BuildOptions: NO_DEAD_CODE_ELIMINATION variable acknowledged (set to: ${NO_DEAD_CODE_ELIMINATION})")
    endif()
endfunction()

# Function to configure all build options (main entry point)
function(configure_build_options)
    declare_build_options()
    validate_build_options()
    report_build_mode()
    mark_variables_as_used()
endfunction() 
