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
test_pch = custom_target('test_pch',
  input: 'test_pch.h',
  output: 'test_pch.h.pch',
  command: [
    uv, 'run', 'python', '-m', 'ziglang', 'c++',
    '-x', 'c++-header',           # Treat input as C++ header
    '@INPUT@',                     # test_pch.h
    '-o', '@OUTPUT@',              # test_pch.h.pch
    unit_test_compile_args,        # Same flags as all tests
    '-I' + meson.project_source_root() / 'src',
    '-I' + meson.project_source_root() / 'tests',
    '-I' + meson.project_source_root() / 'src/platforms/stub',
  ],
  install: false
)

# Doctest main object
doctest_main = static_library('doctest_main',
  'doctest_main.cpp',
  include_directories: [src_dir, tests_dir, stub_dir],
  cpp_args: unit_test_compile_args,
  install: false
)

# List of all test files (excluding test_example_compilation which has its own main)
test_names = [
  'test_json',
  'test_json_roundtrip',
  'test_active_strip_data_json',
  'test_algorithm',
  'test_allocator',
  'test_allocator_inlined',
  'test_allocator_inlined_slab',
  'test_allocator_slab',
  'test_async',
  'test_atomic',
  'test_await',
  'test_bitset',
  'test_bytestream',
  'test_circular_buffer',
  'test_codec',
  'test_corkscrew',
  'test_dbg',
  'test_deque',
  'test_downscale',
  'test_dual_spi',
  'test_easing',
  'test_fastled',
  'test_fft',
  'test_filesystem',
  'test_fixed_set',
  'test_fl_string_comprehensive',
  'test_frame',
  'test_frame_tracker',
  'test_function',
  'test_fx',
  'test_fx2d_blend',
  'test_fx_engine',
  'test_fx_time',
  'test_gif',
  'test_grid',
  'test_hashmap',
  'test_hashmap_lru',
  'test_hsv16',
  'test_intmap',
  'test_invoke',
  'test_istream',
  'test_jpeg',
  'test_lcd50',
  'test_line_simplification',
  'test_lut',
  'test_malloc_hooks',
  'test_map',
  'test_map_range',
  'test_mp3_decoder',
  'test_mpeg1',
  'test_mutex',
  'test_noise_range',
  'test_ostream',
  'test_pixelview',
  'test_point',
  'test_priority_queue',
  'test_printf',
  'test_promise',
  'test_quad_spi',
  'test_queue',
  'test_raster',
  'test_rbtree',
  'test_rectangular_buffer',
  'test_screenmap',
  'test_screenmap_serialization',
  'test_set_inlined',
  'test_shared_ptr',
  'test_single_spi',
  'test_sketch_runner',
  'test_slab_multi_alloc',
  'test_slice',
  'test_span_conversions',
  'test_spi_bus_manager',
  'test_splat',
  'test_str',
  'test_strip_id_map',
  'test_strstream',
  'test_task',
  'test_thread_local',
  'test_tile2x2',
  'test_time',
  'test_transform',
  'test_transition_ramp',
  'test_traverse_grid',
  'test_tuple',
  'test_type_traits',
  'test_ui',
  'test_ui_help',
  'test_ui_title_bug',
  'test_unordered_set',
  'test_variant',
  'test_vector',
  'test_video',
  'test_videofx_wrapper',
  'test_weak_ptr',
  'test_xymap',
  'test_xypath',
]

# Build each test executable with shared PCH
foreach test_name : test_names
  test_exe = executable(test_name,
    [test_name + '.cpp', test_pch],  # Include PCH as dependency
    include_directories: [src_dir, tests_dir, stub_dir],
    cpp_args: unit_test_compile_args + [
      '-include-pch', meson.current_build_dir() / 'test_pch.h.pch',
      '-Winvalid-pch',
    ],
    link_args: unit_test_link_args,
    link_with: [fastled_lib, doctest_main],
    install: false
  )

  # Register test with project root as working directory
  test(test_name, test_exe, workdir: meson.project_source_root())
endforeach

# Build test_example_compilation separately (has its own main, doesn't need PCH or doctest)
test_example_compilation = executable('test_example_compilation',
  'test_example_compilation.cpp',
  include_directories: [src_dir, tests_dir, stub_dir],
  cpp_args: unit_test_compile_args,
  link_args: unit_test_link_args,
  link_with: [fastled_lib],
  install: false
)
test('test_example_compilation', test_example_compilation, workdir: meson.project_source_root())
