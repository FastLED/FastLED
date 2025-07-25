# Failure to compile using SHORTEST_HUES in fill_gradient in 3.9.17 and above.

Using FASTLed version 3.9.17 and above fails to compile using

fill_gradient(leds, 0, CHSV(0, 255, 255), NUM_LEDS - 1, CHSV(96, 255, 255), SHORTEST_HUES);

C:\Users\Test\AppData\Local\Temp.arduinoIDE-unsaved2025622-14384-1m5019q.178a\sketch_jul22a\sketch_jul22a.ino: In function 'void loop()':
C:\Users\Test\AppData\Local\Temp.arduinoIDE-unsaved2025622-14384-1m5019q.178a\sketch_jul22a\sketch_jul22a.ino:16:23: error: 'SHORTEST_HUES' was not declared in this scope
SHORTEST_HUES);
^~~~~~~~~~~~~
C:\Users\Test\AppData\Local\Temp.arduinoIDE-unsaved2025622-14384-1m5019q.178a\sketch_jul22a\sketch_jul22a.ino:16:23: note: suggested alternative:
In file included from d:\Users\Test\Documents\Arduino\libraries\FastLED\src/fl/colorutils.h:11:0,
from d:\Users\Test\Documents\Arduino\libraries\FastLED\src/colorutils.h:6,
from d:\Users\Test\Documents\Arduino\libraries\FastLED\src/FastLED.h:82,
from C:\Users\Test\AppData\Local\Temp.arduinoIDE-unsaved2025622-14384-1m5019q.178a\sketch_jul22a\sketch_jul22a.ino:1:
d:\Users\Test\Documents\Arduino\libraries\FastLED\src/fl/colorutils_misc.h:36:5: note: 'SHORTEST_HUES'
SHORTEST_HUES, ///< Hue goes whichever way is shortest
^~~~~~~~~~~~~
exit status 1

Compilation error: 'SHORTEST_HUES' was not declared in this scope