from dataclasses import dataclass
from enum import Enum


from math import pi, cos, sin

LED_PER_STRIP = 14
SPACE_PER_LED = 0.5

SMALLEST_ANGLE = 360 / 6


class HexagonAngle(Enum):
    UP = 90
    DOWN = 270
    RIGHT_UP = 30
    RIGHT_DOWN = 360 - 30
    LEFT_UP = 150  # (RIGHT_DOWN + 180) % 360
    LEFT_DOWN = 210  # (RIGHT_UP + 180) % 360


def toRads(angle: float) -> float:
    return angle * (pi / 180)


@dataclass
class Point:
    x: float
    y: float


def next_point(pos: Point, angle: HexagonAngle, space: float) -> Point:
    degrees = angle.value
    angle_rad = toRads(degrees)
    x = pos.x + space * cos(angle_rad)
    y = pos.y + space * sin(angle_rad)
    return Point(round(x), round(y))


def gen_points(
    input: list[HexagonAngle], leds_per_strip: int, startPos: Point
) -> list[Point]:
    points: list[Point] = []
    curr_point: Point = Point(startPos.x, startPos.y)
    points.append(Point(startPos.x, startPos.y))
    for i, angle in enumerate(input):
        for j in range(leds_per_strip - 1):
            if j == leds_per_strip:
                break
            curr_point = next_point(curr_point, angle, SPACE_PER_LED)
            points.append(curr_point)
    return points


def main() -> None:
    startPos = Point(0, 0)
    points = gen_points([HexagonAngle.UP], LED_PER_STRIP, startPos)
    print(points)


if __name__ == "__main__":
    main()
