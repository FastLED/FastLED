"""
Generates the hexegon using math.
"""


from dataclasses import dataclass
from enum import Enum
import json


from math import pi, cos, sin

LED_PER_STRIP = 14
SPACE_PER_LED = 30.0  # Increased for better visibility
LED_DIAMETER = SPACE_PER_LED / 4
MIRROR_X = True  # Diagramed from the reverse side. Reverse the x-axis

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

    @staticmethod
    def toJson(points: list["Point"]) -> list[dict]:
        x_values = [p.x for p in points]
        y_values = [p.y for p in points]
        # round 
        x_values = [round(x, 4) for x in x_values]
        y_values = [round(y, 4) for y in y_values]
        if MIRROR_X:
            x_values = [-x for x in x_values]

        return {"x": x_values, "y": y_values, "diameter": LED_DIAMETER}

    def copy(self) -> "Point":
        return Point(self.x, self.y)

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
    input: list[HexagonAngle], leds_per_strip: int, startPos: Point,
    exclude: list[int] | None = None,
    add_last: bool = False
) -> list[Point]:
    points: list[Point] = []
    if (not input) or (not leds_per_strip):
        return points
    exclude = exclude or []
    # Start FSM. Start pointer get's put into the accumulator.
    curr_point: Point = Point(startPos.x, startPos.y)
    # points.append(curr_point)
    last_angle = input[0]
    for i,angle in enumerate(input):
        excluded = i in exclude
        values = list(range(leds_per_strip))
        last_angle = angle
        for v in values:
            last_angle = angle
            curr_point = next_point(curr_point, angle, SPACE_PER_LED)
            if not excluded:
                points.append(curr_point)
        #if i == len(input) - 1:
        #    break
        # Next starting point
        curr_point = next_point(curr_point, last_angle, SPACE_PER_LED)
        #if not excluded:
        #    points.append(curr_point)
    if add_last:
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
    points = gen_points(hexagon_angles, LED_PER_STRIP, Point(0, 0), add_last=True)
    return points

def find_green_anchore_point() -> list[Point]:
    hexagon_angles = [
        HexagonAngle.RIGHT_UP,
        HexagonAngle.RIGHT_UP,
        HexagonAngle.UP,
    ]
    points = gen_points(hexagon_angles, LED_PER_STRIP, Point(0, 0), add_last=True)
    return points


RED_ANCHOR_POINT = find_red_anchor_point()[-1]
BLACK_ANCHOR_POINT = Point(0,0)  # Black
GREEN_ANCHOR_POINT = find_green_anchore_point()[-1]
BLUE_ANCHOR_POINT = Point(0, 0)


def generate_red_points() -> list[Point]:
    starting_point = RED_ANCHOR_POINT.copy()
    hexagon_angles = [
        HexagonAngle.UP,
        HexagonAngle.LEFT_UP,
        HexagonAngle.LEFT_DOWN,
        HexagonAngle.DOWN,
        HexagonAngle.RIGHT_DOWN,
        HexagonAngle.UP,
        HexagonAngle.LEFT_UP
    ]
    points = gen_points(hexagon_angles, LED_PER_STRIP, starting_point, exclude=[5])
    return points


def generate_black_points() -> list[Point]:
    starting_point = BLACK_ANCHOR_POINT.copy()
    hexagon_angles = [
        HexagonAngle.LEFT_UP,
        HexagonAngle.LEFT_UP,
        HexagonAngle.UP,
        HexagonAngle.RIGHT_UP,
        HexagonAngle.RIGHT_DOWN,
        HexagonAngle.DOWN,
        HexagonAngle.LEFT_DOWN,
        HexagonAngle.UP,
        HexagonAngle.LEFT_UP,
        HexagonAngle.UP,
        HexagonAngle.RIGHT_UP,
    ]
    points = gen_points(hexagon_angles, LED_PER_STRIP, starting_point)
    return points


def generate_green_points() -> list[Point]:
    starting_point = GREEN_ANCHOR_POINT.copy()
    hexagon_angles = [
        HexagonAngle.RIGHT_UP,
        HexagonAngle.UP,
        HexagonAngle.LEFT_UP,
        HexagonAngle.LEFT_DOWN,
        HexagonAngle.DOWN,
        HexagonAngle.RIGHT_DOWN, # skip
        HexagonAngle.LEFT_DOWN,  # skip
        HexagonAngle.LEFT_UP,
        HexagonAngle.UP,
        HexagonAngle.RIGHT_UP,
        HexagonAngle.LEFT_UP,
        HexagonAngle.LEFT_DOWN,
        HexagonAngle.RIGHT_DOWN,
        HexagonAngle.RIGHT_UP,   # skip
        HexagonAngle.RIGHT_DOWN,
    ]
    points = gen_points(hexagon_angles, LED_PER_STRIP, starting_point, exclude=[5,6,13])
    return points

def generate_blue_points() -> list[Point]:
    starting_point = BLUE_ANCHOR_POINT.copy()
    hexagon_angles = [
        HexagonAngle.RIGHT_UP,
        HexagonAngle.RIGHT_UP,
        HexagonAngle.UP,
        HexagonAngle.LEFT_UP,
        HexagonAngle.LEFT_DOWN,
        HexagonAngle.LEFT_DOWN,
        HexagonAngle.RIGHT_DOWN, # skip
        HexagonAngle.RIGHT_DOWN,
        HexagonAngle.UP,
        HexagonAngle.RIGHT_UP,
        HexagonAngle.UP,
        HexagonAngle.RIGHT_UP,
    ]
    points = gen_points(hexagon_angles, LED_PER_STRIP, starting_point, exclude=[6])
    return points

def unit_test() -> None:
    #simple_test()
    #two_angle_test()
    out = {}
    map = out.setdefault("map", {})
    map.update({
        "red_segment": Point.toJson(generate_red_points()),
        "back_segment": Point.toJson(generate_black_points()),
        "green_segment": Point.toJson(generate_green_points()),
        "blue_segment": Point.toJson(generate_blue_points()),
    })
    print(json.dumps(out))
    # write it out to a file
    with open("output.json", "w") as f:
        f.write(json.dumps(out))


if __name__ == "__main__":
    unit_test()
