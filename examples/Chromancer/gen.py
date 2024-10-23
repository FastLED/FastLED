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

    def __repr__(self) -> str:
        x_rounded = round(self.x, 2)
        y_rounded = round(self.y, 2)
        return f"({x_rounded}, {y_rounded})"


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
    if (not input) or (not leds_per_strip):
        return points
    # Start FSM. Start pointer get's put into the accumulator.
    curr_point: Point = Point(startPos.x, startPos.y)
    points.append(curr_point)
    last_angle = input[0]
    for i,angle in enumerate(input):
        values = list(range(leds_per_strip - 1))
        last_angle = angle
        for v in values:
            last_angle = angle
            curr_point = next_point(curr_point, angle, SPACE_PER_LED)
            points.append(curr_point)
        if i == len(input) - 1:
            break
        curr_point = next_point(curr_point, last_angle, SPACE_PER_LED)
        points.append(curr_point)
    return points



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

    print(points)



def simple_test() -> None:
    startPos = Point(0, 0)
    hexagon_angles = [
        HexagonAngle.UP,
    ]
    points = gen_points(hexagon_angles, LED_PER_STRIP, startPos)
    print(points)
    # assert len(points) == LED_PER_STRIP + 1

def two_angle_test() -> None:
    startPos = Point(0, 0)
    hexagon_angles = [
        HexagonAngle.UP,
        HexagonAngle.UP,
    ]
    points = gen_points(hexagon_angles, LED_PER_STRIP, startPos)
    print(points)
    # assert len(points) == LED_PER_STRIP * 2, f"Expected {LED_PER_STRIP * 2} points, got {len(points)} points"



def two_angle_test2() -> None:
    print("two_angle_test2")
    startPos = Point(0, 0)
    hexagon_angles = [
        HexagonAngle.UP,
        HexagonAngle.DOWN,
    ]
    points = gen_points(hexagon_angles, LED_PER_STRIP, startPos)
    print(points)
    # assert len(points) == LED_PER_STRIP * 2, f"Expected {LED_PER_STRIP * 2} points, got {len(points)} points"

# Red is defined by this instruction tutorial: https://voidstar.dozuki.com/Guide/Chromance+Assembly+Instructions/6
def find_red_anchor_point() -> list[Point]:
    hexagon_angles = [
        HexagonAngle.LEFT_UP,
        HexagonAngle.LEFT_UP,
        HexagonAngle.UP,
        HexagonAngle.RIGHT_UP,
    ]
    points = gen_points(hexagon_angles, LED_PER_STRIP, Point(0, 0))
    return points

def find_green_anchore_point() -> list[Point]:
    hexagon_angles = [
        HexagonAngle.RIGHT_UP,
        HexagonAngle.RIGHT_UP,
        HexagonAngle.UP,
    ]
    points = gen_points(hexagon_angles, LED_PER_STRIP, Point(0, 0))
    return points

def find_blue_anchor_point() -> list[Point]:
    hexagon_angles = [
        HexagonAngle.RIGHT_UP,
        HexagonAngle.RIGHT_UP,
        HexagonAngle.UP,
        HexagonAngle.LEFT_UP,
    ]
    points = gen_points(hexagon_angles, LED_PER_STRIP, Point(0, 0))
    return points

FIRST_ANCHOR_POINT = find_red_anchor_point()[-1]
SECOND_ANCHOR_POINT = Point(0,0)  # Black
THIRD_ANCHOR_POINT = find_green_anchore_point()[-1]
BLUE_ANCHOR_POINT = Point(0, 0)


def generate_red_points() -> list[Point]:
    starting_point = FIRST_ANCHOR_POINT.copy()
    hexagon_angles = [
        HexagonAngle.UP,
        HexagonAngle.LEFT_UP,
        HexagonAngle.LEFT_DOWN,
        HexagonAngle.DOWN,
        HexagonAngle.UP,
        HexagonAngle.LEFT_UP
    ]




def unit_test() -> None:
    #simple_test()
    #two_angle_test()
    find_red_anchor_point()

if __name__ == "__main__":
    unit_test()
