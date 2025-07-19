# CompilerFlags.cmake
# Unified C/C++ compiler flag management with automatic platform translation

# GNU-style flags (work across GCC and Clang) - comprehensive warning set for tests
set(GNU_WARNING_FLAGS
    -Wall                            # Enable all standard warnings
    -Wextra                          # Enable extra warnings beyond -Wall
    -Werror=return-type              # Missing return statements
    -Werror=missing-declarations     # Missing function declarations
    -Werror=redundant-decls          # Duplicate declarations
    -Werror=init-self                # Self-initialization
    -Werror=missing-field-initializers  # Missing field initializers
    -Werror=pointer-arith            # Pointer arithmetic warnings
    -Werror=write-strings            # String literal conversions
    -Werror=format=2                 # Format string checking (level 2)
    -Werror=implicit-fallthrough     # Switch fallthrough
    -Werror=missing-include-dirs     # Missing include directories
    -Werror=date-time                # __DATE__ and __TIME__ usage
    -Werror=unused-parameter         # Unused function parameters
    -Werror=unused-variable          # Unused local variables
    -Werror=unused-value             # Unused computed values
    -Werror=uninitialized            # Use of uninitialized variables
    -Werror=array-bounds             # Array boundary violations
    -Werror=tautological-constant-out-of-range-compare  # Tautological constant comparisons
    -Wcast-align                     # Alignment-breaking casts
    -Wmissleading-indentation        # Confusing indentation
    -Wdeprecated-declarations        # Use of deprecated functions
)

set(GNU_CXX_FLAGS
    -Werror=suggest-override         # Missing override specifiers
    -Werror=non-virtual-dtor         # Non-virtual destructors
    -Werror=reorder                  # Member initialization order
    -Werror=sign-compare             # Signed/unsigned comparisons
    -Werror=delete-non-virtual-dtor  # Deleting through base pointer
    # -Wold-style-cast               # Disabled: C-style casts used extensively in FastLED
    -Werror=redundant-move           # Redundant std::move usage (GCC only)
)

# Test-specific flags (more permissive than main FastLED library)
set(GNU_TEST_SPECIFIC_FLAGS
    -ffunction-sections              # Enable dead code elimination
    -fdata-sections                  # Enable dead data elimination
    -funwind-tables                  # Enable stack unwinding for debugging
)

# Windows-specific flags for calling convention consistency
set(WINDOWS_CALLING_CONVENTION_FLAGS
    # These flags ensure consistent calling conventions on Windows
)

# FastLED library flags (strict for main library)
set(GNU_FASTLED_LIBRARY_FLAGS
    # FastLED embedded design requirement: disable exceptions and RTTI
    -fno-exceptions                  # Disable C++ exceptions (library only)
    -fno-rtti                        # Disable runtime type info (library only)
    -ffunction-sections              # Enable dead code elimination
    -fdata-sections                  # Enable dead data elimination
)

# Function to filter GNU flags for specific compilers
function(filter_gnu_flags_for_compiler input_flags output_var)
    set(filtered_flags "")
    
    foreach(flag ${input_flags})
        set(keep_flag TRUE)
        
        # Clang-specific filtering
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            # Clang doesn't support some GCC-specific warnings
            if(flag STREQUAL "-Werror=logical-op" OR 
               flag STREQUAL "-Werror=class-memaccess" OR
               flag STREQUAL "-Werror=maybe-uninitialized" OR
               flag STREQUAL "-Werror=redundant-move")
                set(keep_flag FALSE)
            endif()
        endif()
        
        # GCC-specific filtering  
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            # GCC doesn't support some Clang-specific warnings
            if(flag STREQUAL "-Werror=self-assign" OR
               flag STREQUAL "-Werror=infinite-recursion" OR
               flag STREQUAL "-Werror=extra-tokens" OR
               flag STREQUAL "-Werror=unused-private-field" OR
               flag STREQUAL "-Werror=tautological-constant-out-of-range-compare" OR
               flag STREQUAL "-Wglobal-constructors" OR
               flag STREQUAL "-Werror=global-constructors")
                set(keep_flag FALSE)
            endif()
        endif()
        
        if(keep_flag)
            list(APPEND filtered_flags ${flag})
        endif()
    endforeach()
    
    set(${output_var} ${filtered_flags} PARENT_SCOPE)
endfunction()

# Function to translate GNU flags to MSVC equivalents
function(translate_gnu_to_msvc input_flags output_var)
    set(msvc_flags "")
    
    foreach(flag ${input_flags})
        # Basic warning levels
        if(flag STREQUAL "-Wall" OR flag STREQUAL "-Wextra")
            list(APPEND msvc_flags "/W4")
        # Error flags with MSVC equivalents
        elseif(flag STREQUAL "-Werror=return-type")
            list(APPEND msvc_flags "/we4716")
        elseif(flag STREQUAL "-Werror=uninitialized")
            list(APPEND msvc_flags "/we4700")
        elseif(flag STREQUAL "-Werror=array-bounds")
            list(APPEND msvc_flags "/we4789")
        elseif(flag STREQUAL "-Werror=unused-parameter")
            list(APPEND msvc_flags "/we4100")
        elseif(flag STREQUAL "-Werror=unused-variable")
            list(APPEND msvc_flags "/we4101")
        elseif(flag STREQUAL "-Werror=unused-value")
            list(APPEND msvc_flags "/we4552")
        elseif(flag STREQUAL "-Werror=deprecated-declarations")
            list(APPEND msvc_flags "/we4996")
        elseif(flag STREQUAL "-Werror=non-virtual-dtor")
            list(APPEND msvc_flags "/we4265")
        elseif(flag STREQUAL "-Werror=reorder")
            list(APPEND msvc_flags "/we4513")
        elseif(flag STREQUAL "-Werror=sign-compare")
            list(APPEND msvc_flags "/we4389")
        elseif(flag STREQUAL "-Werror=implicit-fallthrough")
            list(APPEND msvc_flags "/we26819")
        # Optimization/code generation flags
        elseif(flag STREQUAL "-fno-exceptions")
            list(APPEND msvc_flags "/EHs-c-")
        elseif(flag STREQUAL "-fno-rtti")
            list(APPEND msvc_flags "/GR-")
        elseif(flag STREQUAL "-ffunction-sections")
            list(APPEND msvc_flags "/Gy")
        # Flags without MSVC equivalents are silently ignored
        # This includes: -fdata-sections, -funwind-tables, many GNU-specific warnings
        endif()
    endforeach()
    
    # Remove duplicates
    list(REMOVE_DUPLICATES msvc_flags)
    set(${output_var} ${msvc_flags} PARENT_SCOPE)
endfunction()

# Function to get platform-appropriate flags
function(get_platform_flags input_flags output_var)
    if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        translate_gnu_to_msvc("${input_flags}" platform_flags)
    else()
        # For GCC/Clang, filter out unsupported flags
        filter_gnu_flags_for_compiler("${input_flags}" platform_flags)
    endif()
    
    set(${output_var} ${platform_flags} PARENT_SCOPE)
endfunction()

# Core interface functions
function(set_gnu_cpp_flags flag_list)
    get_platform_flags("${flag_list}" platform_flags)
    foreach(flag ${platform_flags})
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}" PARENT_SCOPE)
    endforeach()
endfunction()

function(set_gnu_c_flags flag_list)
    get_platform_flags("${flag_list}" platform_flags)
    foreach(flag ${platform_flags})
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flag}" PARENT_SCOPE)
    endforeach()
endfunction()

function(add_gnu_flag flag)
    get_platform_flags("${flag}" platform_flags)
    foreach(flag ${platform_flags})
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}" PARENT_SCOPE)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flag}" PARENT_SCOPE)
    endforeach()
endfunction()

# Function to apply test flags (inherits library flags but filters out overly strict ones)
function(apply_test_compiler_flags)
    # Start with the SAME flags as the FastLED library for ABI compatibility
    set(ALL_GNU_FLAGS ${GNU_WARNING_FLAGS} ${GNU_FASTLED_LIBRARY_FLAGS})
    
    # Add compiler-specific flags (same as library)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        list(APPEND ALL_GNU_FLAGS 
            -Werror=maybe-uninitialized
            -Werror=logical-op
            -Werror=class-memaccess
            -Wno-comment
        )
    endif()
    
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        list(APPEND ALL_GNU_FLAGS 
            -Werror=self-assign 
            -Werror=infinite-recursion 
            -Werror=extra-tokens 
            -Werror=unused-private-field 
            -Wglobal-constructors 
            -Werror=global-constructors
            -Wno-comment
        )
    endif()
    
    # 🚨 CRITICAL: Universal RTTI enforcement - ensure -fno-rtti is always included
    message(STATUS "🚨 UNIVERSAL RTTI ENFORCEMENT - ensuring RTTI is disabled on ALL platforms")
    # Force -fno-rtti to be included regardless of compiler or platform detection
    list(FIND ALL_GNU_FLAGS "-fno-rtti" rtti_flag_idx)
    if(rtti_flag_idx EQUAL -1)
        message(STATUS "🚨 CRITICAL: Adding missing -fno-rtti to build flags")
        list(APPEND ALL_GNU_FLAGS "-fno-rtti")
    endif()
    # Always add compile-time RTTI check
    list(APPEND ALL_GNU_FLAGS "-DFASTLED_CHECK_NO_RTTI=1")
    
    # Get platform-appropriate flags
    get_platform_flags("${ALL_GNU_FLAGS}" platform_flags)
    
    # Filter out overly strict flags for tests (but keep ABI-affecting flags)
    set(filtered_flags "")
    foreach(flag ${platform_flags})
        set(keep_flag TRUE)
        
        # Remove flags that are too strict for tests but don't affect ABI
        if(flag STREQUAL "-fno-exceptions")
            set(keep_flag FALSE)  # Tests can use exceptions
        endif()
        # Keep -fno-rtti for ABI compatibility with FastLED library - NEVER FILTER THIS OUT
        
        if(keep_flag)
            list(APPEND filtered_flags ${flag})
        endif()
    endforeach()
    
    # 🚨 CRITICAL: Double-check RTTI enforcement after filtering
    list(FIND filtered_flags "-fno-rtti" rtti_flag_index)
    if(rtti_flag_index EQUAL -1)
        message(FATAL_ERROR "🚨 CRITICAL: -fno-rtti flag was filtered out! This should never happen. RTTI must be disabled.")
    endif()
    
    # 🚨 CRITICAL: Triple-check by adding -fno-rtti again to be absolutely sure
    list(APPEND filtered_flags "-fno-rtti")  # Add it again in case it got lost
    
    # Add RTTI detection at compile time - this will cause build failure if RTTI is enabled
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        # Add a compile check that fails if RTTI is enabled
        list(APPEND filtered_flags "-Werror" "-DFASTLED_CHECK_NO_RTTI=1")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        # MSVC equivalent  
        list(APPEND filtered_flags "/we4996" "/DFASTLED_CHECK_NO_RTTI=1")
    endif()
    
    message(STATUS "✅ RTTI enforcement complete: -fno-rtti confirmed in filtered flags")
    
    # Apply the filtered flags to maintain ABI compatibility with library
    foreach(flag ${filtered_flags})
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flag}" PARENT_SCOPE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}" PARENT_SCOPE)
    endforeach()
    
    # Add test-specific flags that don't affect ABI
    foreach(flag ${GNU_TEST_SPECIFIC_FLAGS})
        get_platform_flags("${flag}" platform_flag)
        foreach(pf ${platform_flag})
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${pf}" PARENT_SCOPE)
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${pf}" PARENT_SCOPE)
        endforeach()
    endforeach()
    
    # Apply Windows calling convention fixes if needed (same as library)
    apply_windows_calling_convention_fixes()
    
    # Disable LTO for tests - test frameworks need predictable symbol resolution
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF PARENT_SCOPE)
    # Remove any existing LTO flags from compiler options
    string(REPLACE "-flto=thin" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REPLACE "-flto=auto" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REPLACE "-flto" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REPLACE "-fno-fat-lto-objects" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" PARENT_SCOPE)
    
    string(REPLACE "-flto=thin" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    string(REPLACE "-flto=auto" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    string(REPLACE "-flto" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    string(REPLACE "-fno-fat-lto-objects" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}" PARENT_SCOPE)
    
    string(REPLACE "-flto=thin" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    string(REPLACE "-flto=auto" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    string(REPLACE "-flto" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}" PARENT_SCOPE)
    
    message(STATUS "Applied test compiler flags (inherited from library, LTO disabled) for ${CMAKE_CXX_COMPILER_ID}")
endfunction()

# Function to apply test-specific compile definitions
function(apply_test_compile_definitions)
    # Standard test definitions for all targets
    set(TEST_DEFINITIONS
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
    )
    
    # Platform-specific definitions
    if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        list(APPEND TEST_DEFINITIONS _ALLOW_COMPILER_AND_STL_VERSION_MISMATCH)
    endif()
    
    # Unified compilation for Clang builds or when explicitly requested
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR DEFINED ENV{FASTLED_ALL_SRC})
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            message(STATUS "Clang compiler detected: enabling FASTLED_ALL_SRC for unified compilation testing")
        else()
            message(STATUS "FASTLED_ALL_SRC environment variable set: enabling unified compilation testing")
        endif()
        set(FASTLED_ALL_SRC 1 PARENT_SCOPE)
        list(APPEND TEST_DEFINITIONS FASTLED_ALL_SRC=1)
    endif()
    
    # Apply definitions globally
    add_compile_definitions(${TEST_DEFINITIONS})
    
    message(STATUS "Applied test compile definitions: ${CMAKE_CXX_COMPILER_ID}")
endfunction()

# Function to apply standard FastLED library flags (strict for main library)
function(apply_fastled_library_flags)
    # Combine GNU-style warning and library flags
    set(ALL_GNU_FLAGS ${GNU_WARNING_FLAGS} ${GNU_FASTLED_LIBRARY_FLAGS})
    
    # Add compiler-specific flags
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        list(APPEND ALL_GNU_FLAGS 
            -Werror=maybe-uninitialized
            -Werror=logical-op
            -Werror=class-memaccess
            -Wno-comment
        )
    endif()
    
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        list(APPEND ALL_GNU_FLAGS 
            -Werror=self-assign 
            -Werror=infinite-recursion 
            -Werror=extra-tokens 
            -Werror=unused-private-field 
            -Wglobal-constructors 
            -Werror=global-constructors
            -Wno-comment
        )
    endif()
    
    # Get platform-appropriate flags and apply them
    get_platform_flags("${ALL_GNU_FLAGS}" platform_flags)
    foreach(flag ${platform_flags})
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flag}" PARENT_SCOPE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}" PARENT_SCOPE)
    endforeach()
    
    message(STATUS "Applied FastLED library compiler flags for ${CMAKE_CXX_COMPILER_ID}")
endfunction()

# Backward compatibility function - delegates to library flags
function(apply_fastled_compiler_flags)
    apply_fastled_library_flags()
endfunction()

# Function to apply Windows calling convention fixes for Clang
function(apply_windows_calling_convention_fixes)
    if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # On Windows with Clang, ensure consistent calling conventions
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Xclang -fms-compatibility" PARENT_SCOPE)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Xclang -fms-compatibility" PARENT_SCOPE)
        
        # Ensure standard calling conventions are used consistently
        add_definitions(-D_WIN32_WINNT=0x0601)
        
        message(STATUS "Applied Windows calling convention fixes for Clang")
    endif()
endfunction()

# Function to apply C++-specific flags
function(apply_fastled_cxx_flags)
    # Get platform-appropriate C++ flags
    get_platform_flags("${GNU_CXX_FLAGS}" platform_cxx_flags)
    
    # Remove the duplicate redundant-move flag addition since it's already in the main flag list
    # and handled by the filtering logic
    
    # Apply C++-specific flags only to C++ compilation
    foreach(flag ${platform_cxx_flags})
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}" PARENT_SCOPE)
    endforeach()
    
    # Apply Windows calling convention fixes if needed
    apply_windows_calling_convention_fixes()
    
    message(STATUS "Applied C++-specific FastLED flags for ${CMAKE_CXX_COMPILER_ID}")
endfunction() 
