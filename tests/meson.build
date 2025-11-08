# ============================================================================
# SHARED PRECOMPILED HEADER (Manual PCH Management)
# ============================================================================
# Create a single PCH file shared by all tests to avoid per-target duplication.
# Meson creates one PCH per executable by default, which would create 100+ PCH
# files for identical compilation settings. This manual approach builds test.h
# once and shares it across all test executables.

# Find uv for running Zig compiler
uv = find_program('uv')

# Build precompiled header from test_pch.h (contains FastLED.h, doctest + common headers)
# Uses Zig's bundled Clang to match the compile flags (Meson detected GCC but we need Clang)
# Wrapper script fixes the depfile to reference PCH output instead of temp .obj file
test_pch = custom_target('test_pch',
  input: 'test_pch.h',
  output: 'test_pch.h.pch',      # Only PCH is an output; .d file is depfile only
  depfile: 'test_pch.h.d',
  command: [
    uv, 'run', 'python',
    meson.project_source_root() / 'ci/compile_pch.py',
    'uv', 'run', 'python', '-m', 'ziglang', 'c++',
    '-x', 'c++-header',           # Treat input as C++ header
    '@INPUT@',                     # test_pch.h
    '-o', '@OUTPUT@',              # test_pch.h.pch
    '-MD', '-MF', '@DEPFILE@',     # Generate dependency file (will be fixed by wrapper)
    unit_test_compile_args,        # Same flags as all tests
    '-I' + meson.project_source_root() / 'src',
    '-I' + meson.project_source_root() / 'tests',
    '-I' + meson.project_source_root() / 'src/platforms/stub',
    # STRICT PCH VALIDATION FLAGS (to catch invalidation issues):
    '-Werror=invalid-pch',                      # Treat invalid PCH as hard error
    '-verify-pch',                              # Load and verify PCH is not stale
    '-fpch-validate-input-files-content',       # Validate based on content, not just mtime
    '-fpch-instantiate-templates',              # Instantiate templates during PCH build
  ],
  install: false
)

# Doctest main object
doctest_main = static_library('doctest_main',
  'doctest_main.cpp',
  include_directories: [src_dir, tests_dir, stub_dir],
  cpp_args: unit_test_compile_args,
  override_options: ['unity=off'],  # Don't apply unity builds to this single-file library
  install: false
)

# Discover and organize tests using external Python script
# Output format: TEST:<name>:<file_path>:<category>
test_metadata_output = run_command(
  'python', meson.current_source_dir() / 'organize_tests.py',
  meson.current_source_dir(),
  check: true
).stdout().strip()

# Parse test metadata
test_names = []
test_file_paths = {}
test_categories = {}

foreach line : test_metadata_output.split('\n')
  if line != ''
    # Parse line format: TEST:<name>:<file_path>:<category>
    parts = line.split(':')
    test_name = parts[1]
    test_file = parts[2]
    test_category = parts[3]

    test_names += test_name
    test_file_paths += {test_name: test_file}
    test_categories += {test_name: test_category}
  endif
endforeach

# Check if unity builds are enabled
unity_enabled = get_option('unity') == 'on'

if unity_enabled
  # UNITY MODE: Combine tests into categorized test executables
  # Categories are pre-computed by organize_tests.py

  # Dynamically organize test files by category
  # Build dictionary on-the-fly based on discovered categories
  category_files = {}

  foreach test_name : test_names
    test_file = test_file_paths.get(test_name, test_name + '.cpp')
    test_category = test_categories.get(test_name)

    # Initialize category list if not exists, then append
    if not category_files.has_key(test_category)
      category_files += {test_category: files(test_file)}
    else
      # Get existing list, append, and update
      existing_files = category_files.get(test_category)
      updated_files = existing_files + files(test_file)
      category_files += {test_category: updated_files}
    endif
  endforeach

  # Track all test executables for alias
  all_test_executables = []

  # Build test executables for each category
  foreach category_name, test_sources : category_files
    if test_sources.length() > 0
      # Add PCH to sources
      category_sources = [test_pch] + test_sources

      # Build test executable
      # Unity builds enabled: Meson will combine multiple .cpp files into unity
      # compilation units, significantly speeding up compilation
      test_exe = executable(category_name,
        category_sources,
        include_directories: [src_dir, tests_dir, stub_dir],
        cpp_args: unit_test_compile_args + [
          '-include-pch', meson.current_build_dir() / 'test_pch.h.pch',
          '-Werror=invalid-pch',                      # Treat invalid PCH as hard error
          '-fpch-validate-input-files-content',       # Validate based on content, not mtime
        ],
        link_args: unit_test_link_args,
        link_with: [fastled_lib, shared_lib, doctest_main],
        install: false
      )

      # Register test target
      test(category_name, test_exe, workdir: meson.project_source_root())

      # Track for all_tests alias
      all_test_executables += test_exe

      message('Unity test category: ' + category_name + ' (' + test_sources.length().to_string() + ' tests)')
    endif
  endforeach

  # Create alias target that builds all test executables
  alias_target('all_tests', all_test_executables)

  message('Unity test build enabled: tests organized into categories')

else
  # NORMAL MODE: Build individual test executables (current behavior)

  # Build each test executable with shared PCH
  foreach test_name : test_names
    # Get test file path from dynamic map (for subdirectories) or default to test_name.cpp
    test_file = test_file_paths.get(test_name, test_name + '.cpp')

    # Some tests need additional source files for ESP32 parallel SPI host simulation
    test_sources = [test_file, test_pch]
    test_cpp_args = unit_test_compile_args + [
      '-include-pch', meson.current_build_dir() / 'test_pch.h.pch',
      '-Werror=invalid-pch',                      # Treat invalid PCH as hard error
      '-fpch-validate-input-files-content',       # Validate based on content, not mtime
    ]

    # host_sim.cpp is now compiled as part of libfastled.a when STUB_PLATFORM is defined
    # No need to add it explicitly to individual tests anymore

    test_exe = executable(test_name,
      test_sources,
      include_directories: [src_dir, tests_dir, stub_dir],
      cpp_args: test_cpp_args,
      link_args: unit_test_link_args,
      link_with: [fastled_lib, shared_lib, doctest_main],
      install: false
    )

    # Register test with project root as working directory
    test(test_name, test_exe, workdir: meson.project_source_root())
  endforeach

endif  # unity_enabled
