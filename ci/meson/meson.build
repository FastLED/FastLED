# FastLED Meson Build System
# For native builds only (Windows/Linux/macOS) - unit tests, native compilation, development builds

project('fastled', 'cpp',
  version : '6.0.0',
  default_options : [
    'cpp_std=c++17',
    'warning_level=3',
    'optimization=2',
    'debug=false',          # -g0: No debug symbols for fast builds
    'buildtype=plain'       # Fast builds, no debug info
  ]
)

# Get the project root (2 levels up from ci/meson/)
proj_root = meson.project_source_root() / '..' / '..'

# Add subdirectories
subdir('src')
subdir('tests')
