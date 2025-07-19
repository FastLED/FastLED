# LinkerCompatibility.cmake
# Compatibility layer for translating GNU-style linker flags to MSVC-style flags
# when using lld-link (LLVM's linker) on Windows with Clang

# Function to translate GNU-style linker flags to MSVC-style for lld-link
function(translate_gnu_to_msvc_linker_flags input_flags output_var)
    set(translated_flags "")
    
    foreach(flag ${input_flags})
        # Dead code elimination flags
        if(flag STREQUAL "-Wl,--gc-sections")
            # GNU: Remove unused sections
            # MSVC: /OPT:REF removes unreferenced functions/data, /OPT:ICF merges identical sections
            list(APPEND translated_flags "-Xlinker" "/OPT:REF" "-Xlinker" "/OPT:ICF")
        elseif(flag MATCHES "^-Wl,--strip-")
            # GNU: Strip debug/symbol information
            # MSVC: /OPT:REF handles most stripping needs
            list(APPEND translated_flags "-Xlinker" "/OPT:REF")
        elseif(flag STREQUAL "-Wl,--as-needed")
            # GNU: Only link libraries that are actually used
            # MSVC: This is default behavior, no equivalent flag needed
            # Skip this flag
        elseif(flag MATCHES "^-Wl,-Map,")
            # GNU: Generate linker map file: -Wl,-Map,output.map
            # MSVC: /MAP:filename
            string(REGEX REPLACE "^-Wl,-Map," "" map_file "${flag}")
            list(APPEND translated_flags "-Xlinker" "/MAP:${map_file}")
        elseif(flag MATCHES "^-Wl,--version-script=")
            # GNU: Version script for symbol visibility
            # MSVC: /DEF:filename (module definition file)
            string(REGEX REPLACE "^-Wl,--version-script=" "" def_file "${flag}")
            list(APPEND translated_flags "-Xlinker" "/DEF:${def_file}")
        elseif(flag STREQUAL "-Wl,--no-undefined")
            # GNU: Report undefined symbols as errors
            # MSVC: This is default behavior
            # Skip this flag
        elseif(flag MATCHES "^-Wl,-rpath,")
            # GNU: Set runtime library search path
            # MSVC: No direct equivalent, Windows uses DLL search order
            # Skip this flag with warning
            message(STATUS "LinkerCompatibility: Skipping -rpath flag (not applicable on Windows)")
        elseif(flag MATCHES "^-Wl,--wrap,")
            # GNU: Wrap symbol calls
            # MSVC: /ALTERNATENAME can provide similar functionality
            string(REGEX REPLACE "^-Wl,--wrap," "" symbol "${flag}")
            list(APPEND translated_flags "-Xlinker" "/ALTERNATENAME:${symbol}=__wrap_${symbol}")
        # Optimization flags
        elseif(flag STREQUAL "-O1" OR flag STREQUAL "-O2" OR flag STREQUAL "-O3")
            # GNU: Optimization levels
            # MSVC: /O2 for release optimization
            list(APPEND translated_flags "-Xlinker" "/OPT:REF" "-Xlinker" "/OPT:ICF")
        elseif(flag STREQUAL "-Os")
            # GNU: Optimize for size
            # MSVC: /O1 optimizes for size
            list(APPEND translated_flags "-Xlinker" "/OPT:REF" "-Xlinker" "/OPT:ICF")
        # Debug flags
        elseif(flag STREQUAL "-g" OR flag MATCHES "^-g[0-9]")
            # GNU: Generate debug information
            # MSVC: /DEBUG:FULL
            list(APPEND translated_flags "-Xlinker" "/DEBUG:FULL")
        elseif(flag STREQUAL "-gline-tables-only")
            # GNU: Generate minimal debug info for stack traces
            # MSVC: /DEBUG with minimal info
            list(APPEND translated_flags "-Xlinker" "/DEBUG")
        # Already MSVC-style flags (pass through)
        elseif(flag MATCHES "^/")
            # Already MSVC-style flag, pass through with -Xlinker
            list(APPEND translated_flags "-Xlinker" "${flag}")
        elseif(flag MATCHES "^-Xlinker")
            # Already using -Xlinker, pass through as-is
            list(APPEND translated_flags "${flag}")
        # Unsupported or unrecognized flags
        else()
            message(STATUS "LinkerCompatibility: Unknown flag '${flag}', passing through unchanged")
            list(APPEND translated_flags "${flag}")
        endif()
    endforeach()
    
    # Return the translated flags
    set(${output_var} ${translated_flags} PARENT_SCOPE)
endfunction()

# Function to apply linker compatibility for current platform and compiler
function(apply_linker_compatibility)
    # Only apply compatibility layer for Windows + Clang + lld-link
    if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        find_program(LLDLINK_EXECUTABLE lld-link)
        if(LLDLINK_EXECUTABLE)
            message(STATUS "LinkerCompatibility: Applying GNU→MSVC flag translation for lld-link")
            
            # Example usage: translate common GNU flags to MSVC equivalents
            set(gnu_style_flags 
                "-Wl,--gc-sections"
                "-g3"
            )
            
            translate_gnu_to_msvc_linker_flags("${gnu_style_flags}" msvc_flags)
            
            # Apply the translated flags globally
            string(REPLACE ";" " " msvc_flags_str "${msvc_flags}")
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${msvc_flags_str}" PARENT_SCOPE)
            set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${msvc_flags_str}" PARENT_SCOPE)
            set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${msvc_flags_str}" PARENT_SCOPE)
            
            message(STATUS "LinkerCompatibility: Applied flags: ${msvc_flags_str}")
        endif()
    endif()
endfunction()

# Function to get MSVC-compatible dead code elimination flags
function(get_dead_code_elimination_flags output_var)
    if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        find_program(LLDLINK_EXECUTABLE lld-link)
        if(LLDLINK_EXECUTABLE)
            # Use MSVC-style optimization flags for lld-link
            set(flags "-Xlinker" "/OPT:REF" "-Xlinker" "/OPT:ICF")
            # Note: lld-link debug section warnings are informational and harmless
            # They occur because DWARF debug sections have names longer than 8 characters
            message(STATUS "LinkerCompatibility: Using MSVC-style dead code elimination for lld-link")
        else()
            # Use GNU-style flags for other linkers
            set(flags "-Wl,--gc-sections")
            message(STATUS "LinkerCompatibility: Using GNU-style dead code elimination")
        endif()
    else()
        # Use GNU-style flags for non-Windows or non-Clang
        set(flags "-Wl,--gc-sections")
        message(STATUS "LinkerCompatibility: Using GNU-style dead code elimination")
    endif()
    
    set(${output_var} ${flags} PARENT_SCOPE)
endfunction()

# Function to get platform-appropriate debug flags
function(get_debug_flags output_var debug_level)
    if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        find_program(LLDLINK_EXECUTABLE lld-link)
        if(LLDLINK_EXECUTABLE)
            # Use MSVC-style debug flags for lld-link
            if(debug_level STREQUAL "full")
                set(flags "-Xlinker" "/DEBUG:FULL")
            elseif(debug_level STREQUAL "minimal")
                set(flags "-Xlinker" "/DEBUG")
            else()
                set(flags "-Xlinker" "/DEBUG")
            endif()
            message(STATUS "LinkerCompatibility: Using MSVC-style debug flags for lld-link")
        else()
            # Use GNU-style debug flags
            if(debug_level STREQUAL "full")
                set(flags "-g3")
            elseif(debug_level STREQUAL "minimal")
                set(flags "-gline-tables-only")
            else()
                set(flags "-g")
            endif()
            message(STATUS "LinkerCompatibility: Using GNU-style debug flags")
        endif()
    else()
        # Use GNU-style debug flags for non-Windows or non-Clang
        if(debug_level STREQUAL "full")
            set(flags "-g3")
        elseif(debug_level STREQUAL "minimal")
            set(flags "-gline-tables-only")
        else()
            set(flags "-g")
        endif()
        message(STATUS "LinkerCompatibility: Using GNU-style debug flags")
    endif()
    
    set(${output_var} ${flags} PARENT_SCOPE)
endfunction() 
