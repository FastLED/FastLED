# About `dev` folder

This can be used to develop FastLED quickly.

You can directly jump to the local build copy of the
FastLED source code and look at the repo code copy and make changes.

Once your code changes to the local build copy have been verified as working,
then copy your code changes to the FastLED/src folder and issue a pull request.

If you want to force the repo FastLED/src to propagate into your
build directly then invoke `./clean` or `clean.bat` (Windows) and then
rebuild your project. If there is an error the first time then invoke build
again, and it should work.