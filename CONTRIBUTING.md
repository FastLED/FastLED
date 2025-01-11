## Contributing

The most important part about contributing to FastLED is how to test your changes.

FastLED has a power command line interface. We also support VSCode and IntelliSense auto-completion when the free [platformio](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide) extension is installed. The development sketch to test library changes can be found at [dev/dev.ino](dev/dev.ino).

## FastLED compiler cli

[![clone and compile](https://github.com/FastLED/FastLED/actions/workflows/build_default.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_default.yml)

FastLED features a powerful compiler that can run on nearly every chipset, thanks to platformio. The FastLED compiler cli can be invoked when either [python](https://www.python.org/downloads/) or [uv](https://github.com/astral-sh/uv) are installed on the system.

```bash
git clone https://github.com/fastled/fastled
cd fastled
./compile uno --examples Blink  # linux/macos
# compile.bat uno --examples Blink # on windows.
# if you don't have any arguments then the compiler will ask you what you want.
```

## Run the unit tests

```
./test
````

## Linting

```
./lint
```

Then follow the prompts.

## VSCode

 * Make sure you have [platformio](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide) installed.
 * Click the compile button.
 * Make sure to visit the FastLED sketch [dev/dev.ino](dev/dev.ino), the entry point for testing the library.

<img width="1220" alt="image" src="https://github.com/user-attachments/assets/66f1832d-3cfb-4633-8af8-e66148bcad1b">

## Once you are done
  * run `./test`
  * run `./lint`
  * Then submit your code via a git pull request.

## Unit Tests

Shared code is unit tested on the host machine. They can be found at `tests/` at the root of the repo.
