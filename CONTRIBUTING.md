## Contributing

The most important part about contributing to FastLED is knowing how to test your changes.

The FastLED library includes a powerful cli that can compile to any device. It will run if you have either [python](https://www.python.org/downloads/) or [uv](https://github.com/astral-sh/uv) installed on the system.

## FastLED compiler cli

[![clone and compile](https://github.com/FastLED/FastLED/actions/workflows/build_clone_and_compile.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_clone_and_compile.yml)

The FastLED compiler cli can be invoked at the project root.

```bash (MacOS/Linux, windows us git-bsh or compile.bat)
git clone https://github.com/fastled/fastled
cd fastled
./compile uno --examples Blink  # linux/macos/git-bash
# compile.bat  # Windows.
```

## Linting and Unit Testing

```bash
./lint
./test  # runs unit tests
# Note that you do NOT need to install the C++ compiler toolchain
# for compiling + running unit tests via ./test. If `gcc` is not
# found in your system `PATH` then the `ziglang` clang compiler
# will be swapped in automatically.
````


### Testing a bunch of platforms at once.

```
./compile teensy41,teensy40 --examples Blink
./compile esp32dev,esp32s3,esp32c3,esp32c6,esp32s2 --examples Blink,Apa102HD
./compiles uno,digix,attiny85 --examples Blink,Apa102HD 
```

## Unit Tests

Shared code is unit-tested on the host machine. They can be found at `tests/` at the root of the repo. Unit testing only requires either `python` or `uv` to be installed. The C++ compiler toolchain will be installed automatically.

The easiest way to run the tests is just use `./test`

Alternatively, tests can be built and run for your development machine with CMake:

```bash
cmake -S tests -B tests/.build
ctest --test-dir tests/.build --output-on-failure
# Not that this will fail if you do not have gcc installed. When in doubt
# use ./test to compile the unit tests, as a compiler is guaranteed to be
# available via this tool.
```

## VSCode

We also support VSCode and IntelliSense auto-completion when the free [platformio](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide) extension is installed. The development sketch to test library changes can be found at [dev/dev.ino](dev/dev.ino).

 * Make sure you have [platformio](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide) installed.
 * Click the compile button.

![image](https://github.com/user-attachments/assets/616cc35b-1736-4bb0-b53c-468580be66f4)
*Changes in non platform specific code can be tested quickly in our webcompiler by invoking the script `./wasm` at the project root*


## Once you are done
  * run `./test`
  * run `./lint`
  * Then submit your code via a git pull request.


## Going deeper

[ADVANCED_DEVELOPMENT.md](https://github.com/FastLED/FastLED/blob/master/ADVANCED_DEVELOPMENT.md)
