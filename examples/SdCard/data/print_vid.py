# imports for file manip
import os
from pathlib import Path

HERE = Path(__file__).resolve().parent
VIDEO_DAT = HERE / "video.dat"

# open video file
with open(VIDEO_DAT, "rb") as f:
    # print out the first 1000 bytes of the file as a tuple of RGB values
    size = os.path.getsize(VIDEO_DAT)
    # size is 1000 or min of 1000 and file size
    size = min(size, 5000)
    for i in range(size):
        r = f.read(1)
        g = f.read(1)
        b = f.read(1)
        print(f"({r[0]}, {g[0]}, {b[0]})")