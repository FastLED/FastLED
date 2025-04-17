# Advanced Development with FastLED


![perpetualmaniac_neo_giving_two_pills _one_of_them_is_black_and__4b145870-9ead-4976-b031-f4df3f2bfbe1](https://github.com/user-attachments/assets/9bba6113-688f-469f-8b51-bcb4fea910e5)


## GDB On Unit Tests

Yes, we have step through debugging with FastLED.

  * VSCode
  * Install Plugin: GDB Debugger - Beyond
  * Navigate to one of the tests in `tests/` and open in
  * Hit `F5`

If the Python Debugger pops up, then manually switch the VSCode debugger using `Launch(gdb)`

![image](https://github.com/user-attachments/assets/c1246803-df4b-4583-8c11-8243c084afc5)



## Enabling 3-second compile times using our `web-compiler`

  * You must have `docker` installed for fastest compile times. It's free.
  * `cd <FASTLED FOLDER>`
  * `pip install fastled`
  * `fastled examples/Blink/Blink.ino`
    * Cpp changes to the fastled source can be compiled by the live fastled compiler.
           
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
