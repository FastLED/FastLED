# One native consumer links the public API against the already-built core.
compile_tests_exe = executable('compile-tests', 'compile_tests.cpp',
  cpp_args: example_compile_args + [path_norm_I_src, path_norm_I_stub],
  link_args: runner_link_args,
  link_with: fastled_lib,
  implicit_include_directories: false,
  build_by_default: false,
  install: false
)

test('compile-tests', compile_tests_exe, suite: 'compile-tests')
