from dataclasses import dataclass
from enum import Enum


from math import pi, cos, sin

LED_PER_STRIP = 14
SPACE_PER_LED = 1.0  # Increased for better visibility

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
    return Point(x, y)


def gen_points(
    input: list[HexagonAngle], leds_per_strip: int, startPos: Point
) -> list[Point]:
    points: list[Point] = []
    curr_point: Point = Point(startPos.x, startPos.y)
    points.append(curr_point)
    for angle in input:
        for _ in range(leds_per_strip - 1):
            curr_point = next_point(curr_point, angle, SPACE_PER_LED)
            points.append(curr_point)
    return points

def remove_duplicates(points: list[Point]) -> list[Point]:
    unique_points = []
    seen = set()
    for point in points:
        rounded_point = (round(point.x, 2), round(point.y, 2))
        if rounded_point not in seen:
            seen.add(rounded_point)
            unique_points.append(Point(round(point.x, 2), round(point.y, 2)))
    return unique_points


def main() -> None:
    startPos = Point(0, 0)
    hexagon_angles = [
        HexagonAngle.UP,
        HexagonAngle.RIGHT_UP,
        HexagonAngle.RIGHT_DOWN,
        HexagonAngle.DOWN,
        HexagonAngle.LEFT_DOWN,
        HexagonAngle.LEFT_UP,
    ]
    points = gen_points(hexagon_angles, LED_PER_STRIP, startPos)
    unique_points = remove_duplicates(points)
    print(unique_points)


if __name__ == "__main__":
    main()
