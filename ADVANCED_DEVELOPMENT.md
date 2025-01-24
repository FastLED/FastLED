# Advanced Development with FastLED

![perpetualmaniac_neo_giving_two_pills _one_of_them_is_black_and__4b145870-9ead-4976-b031-f4df3f2bfbe1](https://github.com/user-attachments/assets/9bba6113-688f-469f-8b51-bcb4fea910e5)

*Warning - this is not for most people*

Within the FastLED repo contains a custom dev environment designed specifically for the FastLED library. This library is symlinked in to the `dev/dev.ino` development sketch.

Instead of building your project and naming FastLED as a dependency, you actually fork/clone our repo and build your app right into the `dev/` folder of FastLED.

## Why would you need this?

Well, most of the users of FastLED, don't.

But some of you integrate so closely with FastLED that you need:

  * Access to the FastLED code without barriers.
  * You have a crazy number of targets you are compiling to and you need our state-of-the-art testing infrastructure built right into our cli.
  * You are using our new, semi un-announced `web-compiler`, and it's so fresh that of course it doesn't support what you envision but you have the competence to put it in and test it.
  * You are modifying shared code (anything outside of src/platforms/) and you are a freak, obsessed with instant compile times and lightning fast deployments to our fastest platform: Your Browser

## Enabling

  * VSCode + PlatformIO extension
  * `git clone https://github.com/fastled/fastled && cd fastled`
    * Or better yet, fork and clone our repo.
  * Open this up in VSCode
  * Hit the compile button to make sure everything works.
  * Now go into `dev/` folder
  * Put your source code into here.
  * Now compile + upload it using PlatformIO.

## Enabling 3-second compile times using our `web-compiler`

  * You must have `docker` installed. It's free. The fallback web compiler meant for common sketch compiling will NOT pick up C++ src code changes.
  * `pip install fastled`
  * `fastled dev/`
    * If you are making changes to our web-compiler located in `src/platforms/wasm/compiler/`, then use `fastled --build dev/`
    * All other changes outside of the `src/platforms/wasm/compiler/` folder will be rsync'd in automatically - you can omit the `--build` flag in this case.
    * Leave `fastled` running
      * If you change your sketch, the `fastled` program will automatically start compiling and then redeploy 1 second after you finish typing
      * If you change C++ code, then `fastled` will notify you that code has changed, but will wait until you hit the space bar.
        * C++ changes take a lot longer than sketch changes, so we aren't going to automatically compile in this case.
    * If you need to debug your C++ code, flip on debug mode so you can inspect problematic C++ code in your browser
      * `fastled dev/ --debug`
    * If `docker` is behaving weird then purge all the fastled docker images and containers
      * `fastled --purge`
           
## Testing your changes

Most of this is in the basic CONTRIBUTING.md guide. But as a reminder
  * Unit Testing: `./test`
  * Linting: `./lint`
  * Compiling on platforms `./compile uno,teensy41,esp32s3 --examples Blink,Apa102HD`
           
## Enabling AI coding

`aider.chat` is available for advanced and high velocity coding with FastLED. To use it, have your open-ai or Anthropic api key ready. It's recommended to use Anthropic as it's performance is much better than Open-AI for coding.

At the root of the project type:

`./ai` and follow the prompts. Once the key is installed you will get a prompt that looks like this:

```bash
architect>  
```

There are two modes to use this AI, a watch mode which watches your files for changes and launches automatically, and a slow way which you add target files then instruct it to make changes:

  * watch mode (best for implimenting a function or two)
    * Edit any file in the repo. Add a comment with `AI!` at the end. The ai will see this and start implementing what you just typed.
    * Example: Edit `src/fl/vector.h` and put in a comment `// Add more comments AI!`, then say yes to the changes in the prompt.
  * Slow mode (much better for bigger changes across the file or several)
    * While you are in the `architect>  ` prompt you will add a file to the chat
      * `/add src/fl/vector.h`
      * Now tell the AI what you want it to do, and it will do it.
  * Making the AI fix it's own problems it introduced.
    * At the AI command prompt, have it run the following
      * Linux/Mac: `/run ./test`
      * On Windows: `/run uv run test.py`
    * After the test concludes, the AI will ask you if you want to add the output back into the chat, agree to it then let it try to correct it's mistakes.
     
Every single time you do a change, make sure and thoroughly check it. I recommend VSCodes built in git diff tool.

Although the AI is pretty amazing, it will inject entropy into your code and this is the source of a lot of problems. So watch all changes it makes very thoroughly. Under almost all circumstances you will have to revert unnecessary changes (like comments) line by line.
