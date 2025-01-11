## Contributing

The most important part about contributing to FastLED is how to test your changes.

FastLED has a powerful cli that can compile to any device. 

## FastLED compiler cli

[![clone and compile](https://github.com/FastLED/FastLED/actions/workflows/build_default.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_default.yml)

The FastLED compiler cli can be invoked when either [python](https://www.python.org/downloads/) or [uv](https://github.com/astral-sh/uv) are installed on the system.

```bash (MacOS/Linux, windows us git-bsh or compile.bat)
git clone https://github.com/fastled/fastled
cd fastled
./compile uno --examples Blink  # linux/macos
```

## Run the unit tests

```bash
./test
````

## Linting

```bash
./lint
```

## VSCode

We also support VSCode and IntelliSense auto-completion when the free [platformio](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide) extension is installed. The development sketch to test library changes can be found at [dev/dev.ino](dev/dev.ino).

 * Make sure you have [platformio](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide) installed.
 * Click the compile button.
 * Make sure to visit the FastLED sketch [dev/dev.ino](dev/dev.ino), the entry point for testing the library.

![image](https://github.com/user-attachments/assets/616cc35b-1736-4bb0-b53c-468580be66f4)


## Once you are done
  * run `./test`
  * run `./lint`
  * Then submit your code via a git pull request.

## Unit Tests

Shared code is unit-tested on the host machine. They can be found at `tests/` at the root of the repo. Unit testing only requires either `python` or `uv` to be installed. The C++ compiler toolchain will be installed automatically.
