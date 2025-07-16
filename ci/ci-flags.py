# flake8: noqa
# ruff: skip

Import("env")  # type: ignore

env.Append(CXXFLAGS=["-Wno-register"])  # type: ignore
