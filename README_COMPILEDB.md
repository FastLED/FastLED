# Compilation Database (compile_commands.json) Setup

This repository now includes [compiledb](https://github.com/nickdiego/compiledb) integration to generate `compile_commands.json` files for better IDE support with clangd and other language servers.

## What is compile_commands.json?

A compilation database is a JSON file that contains the compilation commands for each source file in a project. It enables:
- Better IntelliSense/autocomplete in IDEs
- Accurate code navigation and symbol resolution
- Integration with clang-based tools like clangd, clang-tidy, etc.

## Installation

The compiledb tool is automatically installed when you run:

```bash
bash install
```

This will:
1. Install compiledb in the virtual environment
2. Build the C++ tests using the new Python build system
3. Generate `compile_commands.json` in the project root
4. Enable clangd IntelliSense support for VSCode/Cursor

## Manual Generation

If you need to regenerate the compilation database manually, you can use:

```bash
./generate_compile_commands.sh
```

This script will:
- Build the test files to ensure compilation database generation
- Copy the generated `compile_commands.json` from `tests/.build/` to the project root
- Verify the file was created successfully

## How It Works

The current implementation uses the existing CMake-based build system in `tests/.build/` to generate the compilation database, since the new Python build system doesn't directly output compilation commands in the standard format that compiledb expects.

The process is:
1. Run the C++ test compilation with the Python build system
2. The Python build system generates compilation database via CMake in `tests/.build/`
3. Copy the generated `compile_commands.json` to the project root for IDE consumption

## Integration Status

- ✅ **compiledb installed**: Available in virtual environment
- ✅ **install script fixed**: Corrected path to `ci/compiler/cpp_test_run.py`
- ✅ **compile_commands.json generated**: Successfully created in project root
- ✅ **IDE support enabled**: clangd should work automatically in VSCode/Cursor

## Usage

Once the compilation database is in place:

1. **VSCode/Cursor**: clangd extension should automatically detect and use the `compile_commands.json`
2. **Other editors**: Configure your editor to use clangd with the compilation database
3. **Command line tools**: Use clang-tidy, clang-format, and other tools with the `-p .` flag to use the compilation database

## Troubleshooting

If IntelliSense isn't working:
1. Ensure `compile_commands.json` exists in the project root
2. Check that clangd extension is installed in your editor
3. Restart your editor after generating the compilation database
4. Run `./generate_compile_commands.sh` to regenerate if needed

## Future Improvements

The current approach relies on the CMake build system to generate the compilation database. Future improvements could include:
- Direct compilation database generation from the Python build system
- Integration with the new unified build system when it's fully migrated
- Support for different build configurations (debug, release, etc.)