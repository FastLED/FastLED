## Contributing

The most important part about working with the FastLED is testing your changes.

FastLED features a powerful compiler that can run on any supported chipset. This compiler can be invoked when either [python](https://www.python.org/downloads/) or [uv](https://github.com/astral-sh/uv) are installed. We also support VSCode and IntelliSense auto-completion through [platformio](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide)

## FastLED compiler cli

[![clone and compile](https://github.com/FastLED/FastLED/actions/workflows/build_default.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_default.yml)

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
 * Sketch [dev/dev.ino](dev/dev.ino).

<img width="1220" alt="image" src="https://github.com/user-attachments/assets/66f1832d-3cfb-4633-8af8-e66148bcad1b">

When changes are made then push to your fork to your repo and git will give you a url to trigger a pull request into the master repo.
