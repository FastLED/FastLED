## Contributing

The most important part about contributing to FastLED is knowing how to test your changes.

FastLED has a powerful cli that can compile to any device. It will run if you have either [python](https://www.python.org/downloads/) or [uv](https://github.com/astral-sh/uv) installed on the system.

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
./test
````

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

## Unit Tests

Shared code is unit-tested on the host machine. They can be found at `tests/` at the root of the repo. Unit testing only requires either `python` or `uv` to be installed. The C++ compiler toolchain will be installed automatically.

Alternatively, tests can be built and run for your development machine with CMake:

```
cmake -S tests -S tests/.build
ctest --test-dir tests/.build --rerun-failed --output-on-failure
```
