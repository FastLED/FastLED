

from dataclasses import dataclass

LED_PER_STRIP = 14

SMALLEST_ANGLE = 360 / 6

# Hexegon angles
UP = 90
DOWN = 270
RIGHT_UP = 30
RIGHT_DOWN = (RIGHT_UP + 60) % 360
LEFT_UP = (RIGHT_DOWN + 180 % 360
LEFT_DOWN = RIGHT_DOWN + 180 % 360

