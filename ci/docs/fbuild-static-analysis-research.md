# fbuild Static-Analysis Integration Research

Date: 2026-04-17
Scope: route `pio check` (cppcheck) and `clang-tool-chain-iwyu` through fbuild's compile database instead of a PlatformIO project tree.

---

## 1. clang-tool-chain-bin API surface

### Package identity

- The repo `github.com/zackees/clang-tool-chain-bin` returns 404 and no PyPI project of that name loads. The **actual** PyPI package is `clang-tool-chain` (repo: `github.com/zackees/clang-tool-chain`, homepage is set in PyPI metadata).
- Installed version in this venv: **1.2.9** (Apache-2.0, author Zachary Vorhies).
  - `uv run python -m pip show clang-tool-chain` confirms.
  - Already a transitive dep of FastLED's own `ci` package (`Required-by: ci, fastled`).
- Install / invoke: `uv pip install clang-tool-chain` and run via `uv run clang-tool-chain-*` CLI entry points, or import `clang_tool_chain.wrapper`.

### CLI wrappers that ship

Relevant wrappers present in 1.2.9 (verified by `clang-tool-chain-*-help`):

| Wrapper binary | Purpose | Accepts `-p <build-dir>`? |
|---|---|---|
| `clang-tool-chain-cpp` | clang++ compiler front-end | n/a (compiler) |
| `clang-tool-chain-c` / `-cpp-msvc` / `-c-msvc` | clang in GNU / MSVC ABI modes | n/a |
| `clang-tool-chain-iwyu` | bare `include-what-you-use` binary | **no**, single-file mode |
| `clang-tool-chain-iwyu-tool` | compilation-database driver | **yes — `-p <build-path>` is required** |
| `clang-tool-chain-fix-includes` | applies IWYU suggestions | n/a |
| `clang-tool-chain-tidy` | clang-tidy | yes (standard clang-tooling `-p`) |
| `clang-tool-chain-format` | clang-format | n/a |
| `clang-tool-chain-query` | clang-query | yes (standard clang-tooling `-p`) |
| `clang-tool-chain-llvm-{ar,nm,objdump,readelf,strip,…}` | binutils | n/a |
| `clang-tool-chain-lldb`, `-valgrind`, `-callgrind`, `-emcc`, `-empp`, `-cosmocc`, … | other tools | n/a |

**There is NO `clang-tool-chain-cppcheck` wrapper.** `clang_tool_chain` does not bundle cppcheck at all. For cppcheck we must either keep a system cppcheck or swap cppcheck for `clang-tidy` (which covers most of the same rules).

Example invocations against a compile DB:

```bash
# IWYU across all TUs in a compile_commands.json directory
uv run clang-tool-chain-iwyu-tool -p .fbuild/build/esp32c6/release -j 8 -- -Xiwyu --no_internal_mappings

# clang-tidy on a single file, using the DB for compile flags
uv run clang-tool-chain-tidy -p .fbuild/build/esp32c6/release src/FastLED.cpp

# clang-query — same -p convention
uv run clang-tool-chain-query -p .fbuild/build/esp32c6/release src/FastLED.cpp
```

### Python API (`clang_tool_chain.wrapper`)

The import `from clang_tool_chain.wrapper import iwyu_main` referenced at `ci/ci-iwyu.py:396` **works** — confirmed by live inspection.

All `*_main` functions are `() -> NoReturn` (they read `sys.argv`, run, `sys.exit`). Full list relevant to static analysis:

- `iwyu_main` — raw `include-what-you-use`.
- `iwyu_tool_main` — the compile-DB driver (equivalent to `clang-tool-chain-iwyu-tool`).
- `fix_includes_main` — apply IWYU fixits.
- `clang_tidy_main` — clang-tidy.
- `clang_query_main` — clang-query.
- `clang_format_main` — clang-format.
- Compiler fronts: `clang_main`, `clang_cpp_main`, `clang_msvc_main`, `clang_cpp_msvc_main`, `cosmocc_main`, `cosmocpp_main`, `emcc_main`, `empp_main`.
- Helpers: `execute_iwyu_tool(tool_name, args=None)`, `execute_tool`, `find_iwyu_tool`, `get_iwyu_binary_dir`, plus sanitizer-env and download plumbing from `clang_tool_chain` top-level (`prepare_sanitizer_environment`, `get_asan_runtime_dll`, etc.).

No cppcheck entry point exists in the Python API either.

---

## 2. fbuild compile-database state: **yes — it ships a `compiledb` build target**

### The key finding

`fbuild build --target compiledb` emits `compile_commands.json` without compiling. Verified live:

```
$ fbuild build --help
  -t, --target <TARGET>
      Build target: 'compiledb' generates compile_commands.json without compiling
      [possible values: compiledb]
```

This is built into the Rust binary itself; no FastLED code currently invokes it. The `fbuild` Python package (`fbuild 2.1.11` at `C:\Users\niteris\dev\fastled9\.venv\Lib\site-packages\fbuild\__init__.py`) exposes only `Daemon`, `DaemonConnection`, `connect_daemon` — no direct compile-DB Python API. `DaemonConnection` exposes `build`, `deploy`, `monitor` only, so the compile DB must be triggered via the CLI `fbuild build --target compiledb -e <env>`.

fbuild also has three built-in static-analysis subcommands that likely consume the same DB internally (though each is thin and takes no tool-specific args yet):

```
fbuild clang-tidy [--environment <ENV>] [PROJECT_DIR]
fbuild iwyu       [--environment <ENV>] [PROJECT_DIR]
fbuild clang-query [--environment <ENV>] [PROJECT_DIR]
```

See `fbuild --help` and per-subcommand `--help`. None accept rule filters, mapping files, or `-p`; for any serious configuration we will want to drive `clang-tool-chain-iwyu-tool` / `clang-tool-chain-tidy` ourselves from the generated DB.

### Where the DB lives today

Live inspection of the working tree:

```
C:\Users\niteris\dev\fastled9\.fbuild\build\<env>\release\
  obj\
```

Example from current tree: `.fbuild/build/esp32c6/release/obj/` and `.fbuild/build/esp32s3/release/obj/` exist; `compile_commands.json` is **not** present yet (`--target compiledb` has never been run here). Running `fbuild build -e esp32c6 --target compiledb` will materialize it, per the CLI doc. Expected location (consistent with fbuild convention): `.fbuild/build/<env>/release/compile_commands.json` next to `obj/`.

### What the existing code already assumes

- `ci/compiler/pio.py:1095` artifacts path: `self.build_dir / ".fbuild" / "build" / env_name / "release"` — matches the compiledb root.
- `ci/compiler/pio.py` never passes `--target compiledb`; it only calls `run_fbuild_compile` in `ci/util/fbuild_runner.py`, which builds `[fbuild, <dir>, build, -e <env>]` (lines 104-114) with no target flag.
- `ci/util/fbuild_adapter.py` uses the Python `connect_daemon(...).build(...)` path and also never requests compiledb.
- `ci/ci-cppcheck.py:102-145` already shorts-out with `was_compiled_with_fbuild()` returning `.build/.fbuild/build/<env>/release` existence → prints a SKIP and returns 0. Tickets already filed in that message: `FastLED#2301` (cppcheck/fbuild), `#2302` (c2 regression), `#2303` (clang-tool-chain-bin integration).
- `ci/util/generate_compile_commands.sh` is a **Meson-only** path today (writes `.build/meson-quick/compile_commands.json`); it does not understand fbuild.
- `ci/iwyu_wrapper.py` is the handwritten per-file IWYU driver (no `-p` support; takes `iwyu-args -- compiler args source.cpp`) and is invoked by `ci/ci-iwyu.py` in PIO mode. It won't be needed once we move to compile-DB mode with `iwyu-tool`.

---

## 3. Recommended integration path

Add a small helper `ci/util/fbuild_compiledb.py` that shells out `fbuild <project_dir> build -e <env> --target compiledb` and returns `.fbuild/build/<env>/release/compile_commands.json` (creating it if missing). Then teach both entry points to prefer fbuild's DB on fbuild-compiled boards:

- **`ci/ci-iwyu.py`**: when `was_compiled_with_fbuild(...)` is true (lift that helper out of `ci-cppcheck.py`), skip the PIO project-dir branch and invoke `clang-tool-chain-iwyu-tool -p <db-dir> -j $(nproc) -- -Xiwyu --mapping_file=ci/iwyu/fastled.imp -Xiwyu --mapping_file=ci/iwyu/stdlib.imp -Xiwyu --no_internal_mappings` (either via subprocess or `from clang_tool_chain.wrapper import iwyu_tool_main` after temporarily rewriting `sys.argv`). This deletes the custom `ci/iwyu_wrapper.py` per-file logic for fbuild boards.
- **`ci/ci-cppcheck.py`**: replace the SKIP block with a call to `clang-tool-chain-tidy -p <db-dir>` using a `.clang-tidy` config that enables the cppcheck-equivalent checks (`clang-analyzer-*`, `bugprone-*`, `cert-*`, `misc-*`). This is the smallest change that unblocks esp32c2 and friends while removing the cppcheck dependency entirely. If cppcheck parity is strictly required, keep a system-cppcheck fallback driven from the same compile DB (`cppcheck --project=<db>/compile_commands.json`), since cppcheck itself understands compile_commands.json natively and does not need `clang-tool-chain-bin`.

Smallest unblocking diff: (1) add the compiledb runner helper, (2) in `ci-iwyu.py` swap the PIO project dir branch for `-p <fbuild-db-dir>` when `was_compiled_with_fbuild`, (3) in `ci-cppcheck.py` do the same with `clang-tool-chain-tidy` (or system cppcheck via `--project=`). No changes needed in `fbuild_runner.py` / `fbuild_adapter.py`; no new Python API in `fbuild` is required.
