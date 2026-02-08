# OBJECTIVE: Optimize Animartrix2 (Chasing_Spirals) to use Q31 signed integer math instead of floating point operations in the inner double for loop

  * Animartrix2 Chasing_Spirals
  * evaluate
  * convert operations to Q31
  * all floating ops to int ops
  * create a new unit test in the existing test, #if 0 out everything else
    * test it's bit by bit equal in the generated from buffer
    * test that setting uin32 time value to a high number results in approximate < 3% difference in output.
  * After this continue to find all additional optimizations for this one visualizer
    * For example SIMD
  * Stop when you have run out of optimizations you can apply and you are getting diminishing returns.
    * Only change operations in the inner loop. Do not change const data like the perlin data. If you must change this function / data then generate a new one in integer form and do that.

Change the tests so they validate and also compare timing. After every test, output to the user what the timing improvement is.

DO NOT TOUCH ANIMARTRIX!!! IT MUST BE CONSTNANT. ONLY TOUCH ANIMARTRIX2!! NO EXCEPTIONS.

# Background

Animartrix is slow, animartrix2 is the intended fast rewrite.

Animartrix2_FX will be tested via the tests/**/animartrix.cpp, which will take Animartrix and Animartrix2 and compute the same frame with the same parameters and test that the two are bit by bit equal in the frame buffer.

# REFACTOR OUTLINE

## STATUS QUO: GOD OBJECT

Animartrix detail is a god object, it has a bunch of void member functions that are part fo the same same class.

The outer wrapper takes this detail and make it confirm to the Fx2d class so it an be used.

Each void member function (visualizer) is part of the same class so it can access it's data.

## ANIMARTRIX2: Composed of free fl::functors operating on passed in data.

  * Take the shared object state and move it into Context struct, this will be passed into the functor. It will have inputs like oscillators, current time and any input state necessary and outputs like the CRGB pixels which will be drawn
  * a selected functor will operate on this data and write to the output.
  * The agent may decide to do an interface class for the functor instead, so things like name and other attributes can be attached.
  * All functors will go into a detail/ folder relative to animartrix2

## TESTING

we have a animartrix test that compares two Fx2d instances and provides the same inputs and compares the outputs for equality.

We must comprehensively test each mode, with deterministic randomzied timing parameters, at 32x32 grid size.

If there is a output mismatch then the test shall output which animartrix test is failing and which led index it is. We should have each sub visualizer as it's own TEST() block, although the framework to setup, render and test can be shared.

## ON GETTING STUCK

If the agent get's stuck for 3 iteraitons, then back up and research and evaluate what went wrong.

## MISC

On each success iteration, do a git commit. If there is a regression, do a git reset to the last commit. Put lots of detail in the commit of what was done.

## SUCCESS CONDITION: All possible optimizations were applied and we are getting bit by bit equality, or <1% error for time values close to 0, time values > 0xffffff has a <5% error. Try to minimize it as much as possible.