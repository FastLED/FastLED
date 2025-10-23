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
  output: ['test_pch.h.pch', 'test_pch.h.d'],
  depfile: 'test_pch.h.d',
  command: [
    uv, 'run', 'python', '-m', 'ziglang', 'c++',
    '-x', 'c++-header',           # Treat input as C++ header
    '@INPUT@',                     # test_pch.h
    '-o', '@OUTPUT0@',             # test_pch.h.pch
    '-MD', '-MF', '@OUTPUT1@',     # Generate dependency file
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
  'test_allocator_realloc',
  'test_allocator_slab',
  'test_async',
  'test_atomic',
  'test_await',
  'test_bitset',
  'test_bytestream',
  'test_circular_buffer',
  'test_clockless_block_generic',
  'test_codec',
  'test_corkscrew',
  'test_dbg',
  'test_delay_ns',
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
  'test_isr_api',
  'test_istream',
  'test_jpeg',
  'test_lcd50',
  'test_limits',
  'test_line_simplification',
  'test_list',
  'test_lut',
  'test_malloc_hooks',
  'test_map',
  'test_map_range',
  'test_mp3_decoder',
  'test_mpeg1',
  'test_mutex',
  'test_noise_range',
  'test_noise_ring',
  'test_ostream',
  'test_pixelview',
  'test_point',
  'test_priority_queue',
  'test_printf',
  'test_promise',
  'test_quad_spi',
  # Note: test_parallel_spi_isr_quad built separately below
  # Note: test_spi_isr_16 built separately below with manual tick mode
  'test_queue',
  'test_raster',
  'test_rbtree',
  'test_rectangular_buffer',
  'test_screenmap',
  'test_screenmap_serialization',
  'test_set_inlined',
  'test_shared_ptr',
  'test_single_spi',
  'test_spi_block_8',
  'test_spi_block_16',
  'test_spi_block_32',
  'test_sketch_runner',
  'test_slab_multi_alloc',
  'test_slice',
  'test_span_conversions',
  'test_spi_bus_manager',
  'test_splat',
  'test_str',
  'test_strip_id_map',
  'test_sstring',
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
  'test_esp32_riscv_interrupts',
]

# Build each test executable with shared PCH
foreach test_name : test_names
  # Some tests need additional source files for ESP32 parallel SPI host simulation
  test_sources = [test_name + '.cpp', test_pch[0]]
  test_cpp_args = unit_test_compile_args + [
    '-include-pch', meson.current_build_dir() / 'test_pch.h.pch',
    '-Winvalid-pch',
  ]

  # host_sim.cpp is now compiled as part of libfastled.a when STUB_PLATFORM is defined
  # No need to add it explicitly to individual tests anymore

  test_exe = executable(test_name,
    test_sources,
    include_directories: [src_dir, tests_dir, stub_dir],
    cpp_args: test_cpp_args,
    link_args: unit_test_link_args,
    link_with: [fastled_lib, doctest_main],
    install: false
  )

  # Register test with project root as working directory
  test(test_name, test_exe, workdir: meson.project_source_root())
endforeach

# ============================================================================
# Parallel SPI ISR Tests (Consolidated)
# ============================================================================
# SPI ISR tests use thread-based automatic ISR execution for real-time testing.

spi_isr_objs = static_library('spi_isr',
  files(
    meson.project_source_root() / 'src/platforms/shared/spi_bitbang/spi_isr_engine.cpp',
    meson.project_source_root() / 'src/platforms/shared/spi_bitbang/host_timer.cpp',
  ),
  include_directories: [src_dir, tests_dir, stub_dir],
  cpp_args: unit_test_compile_args,
  install: false
)

test_parallel_spi_isr_exe = executable('test_parallel_spi_isr',
  ['test_parallel_spi_isr.cpp', test_pch[0]],
  include_directories: [src_dir, tests_dir, stub_dir],
  cpp_args: unit_test_compile_args + [
    '-include-pch', meson.current_build_dir() / 'test_pch.h.pch',
    '-Winvalid-pch',
  ],
  link_args: unit_test_link_args,
  # Link SPI ISR library BEFORE libfastled to override symbols
  link_with: [spi_isr_objs, fastled_lib, doctest_main],
  install: false
)

test('test_parallel_spi_isr', test_parallel_spi_isr_exe, workdir: meson.project_source_root())

# ============================================================================
# SPI ISR 16 Tests
# ============================================================================
# SPI ISR 16 tests use thread-based automatic ISR execution.

test_spi_isr_16_exe = executable('test_spi_isr_16',
  ['test_spi_isr_16.cpp', test_pch[0]],
  include_directories: [src_dir, tests_dir, stub_dir],
  cpp_args: unit_test_compile_args + [
    '-include-pch', meson.current_build_dir() / 'test_pch.h.pch',
    '-Winvalid-pch',
  ],
  link_args: unit_test_link_args,
  # Link SPI ISR library BEFORE libfastled to override symbols
  link_with: [spi_isr_objs, fastled_lib, doctest_main],
  install: false
)

test('test_spi_isr_16', test_spi_isr_16_exe, workdir: meson.project_source_root())

# ============================================================================
# SPI ISR 32 Tests
# ============================================================================
# SPI ISR 32 tests use thread-based automatic ISR execution.

test_spi_isr_32_exe = executable('test_spi_isr_32',
  ['test_spi_isr_32.cpp', test_pch[0]],
  include_directories: [src_dir, tests_dir, stub_dir],
  cpp_args: unit_test_compile_args + [
    '-include-pch', meson.current_build_dir() / 'test_pch.h.pch',
    '-Winvalid-pch',
  ],
  link_args: unit_test_link_args,
  # Link SPI ISR library BEFORE libfastled to override symbols
  link_with: [spi_isr_objs, fastled_lib, doctest_main],
  install: false
)

test('test_spi_isr_32', test_spi_isr_32_exe, workdir: meson.project_source_root())
