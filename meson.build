project('fastled', 'cpp',
  version: '6.0.0',
  meson_version: '>=1.4.0',
  default_options: [
    'cpp_std=c++11',  # Use c++11 to match AVR builds
    'warning_level=2',
    'werror=false'
  ]
)

# Source directory
src_dir = include_directories('src')
tests_dir = include_directories('tests')

# Platform-specific stub include (provides Arduino.h and other stub headers)
stub_dir = include_directories('src/platforms/stub')

# Recursively discover all .cpp source files in src/ directory with caching
# Note: Meson doesn't have native rglob, so we use Python's pathlib
# All files should always be included, no exceptions.
# If there is a build issue then the cpp file does not have
# the right #if-def guard in it to prevent compilation
uv_prog = find_program('uv', required: true)

# Setup cache paths
src_cache_script = meson.project_source_root() / 'ci' / 'meson' / 'src_metadata_cache.py'
rglob_script = meson.project_source_root() / 'ci' / 'meson' / 'rglob.py'
src_dir_path = meson.project_source_root() / 'src'
build_dir_path = meson.current_build_dir()

# Check if cache is valid
cache_check = run_command(
  'python', src_cache_script, '--check', src_dir_path, build_dir_path, '--pattern', '*.cpp',
  check: false
)

if cache_check.returncode() == 0
  # Cache hit - use cached metadata
  all_sources_output = cache_check.stdout().strip()
  message('Using cached source file metadata (no changes detected)')
else
  # Cache miss - run full discovery
  message('Running source file discovery (cache invalid or missing)')

  discovered_sources = run_command(
    uv_prog, 'run', 'python', rglob_script, 'src', '*.cpp',
    check: true,
    capture: true
  )
  all_sources_output = discovered_sources.stdout().strip()

  # Update cache
  run_command(
    'python', src_cache_script, '--update',
    src_dir_path, build_dir_path, all_sources_output, '--pattern', '*.cpp',
    check: true
  )
endif

all_sources = all_sources_output.split('\n')

# Separate platforms/shared sources (to avoid weak/strong symbol conflicts)
platforms_shared_sources = []
fastled_sources_list = []

foreach src : all_sources
  # Check for both forward and backslash paths (Windows uses backslash)
  if src.contains('platforms/shared/') or src.contains('platforms\\shared\\')
    platforms_shared_sources += src
  else
    fastled_sources_list += src
  endif
endforeach

fastled_sources = files(fastled_sources_list)
platforms_shared_files = files(platforms_shared_sources)

# ============================================================================
# Build mode (shared by both native and WASM builds)
# ============================================================================
build_mode = get_option('build_mode')
cpp_compiler = meson.get_compiler('cpp')
is_wasm_build = host_machine.system() == 'emscripten'

if not is_wasm_build
  # Native build: compilation flags, link flags, libraries, tests, and examples
  subdir('ci/meson/native')

  # Build tests subdirectory
  subdir('tests')

  # Build examples subdirectory (provides meson targets for example compilation)
  subdir('examples')
else
  # WASM build: emscripten cross-compilation with different flags and targets
  subdir('ci/meson/wasm')
endif  # is_wasm_build
