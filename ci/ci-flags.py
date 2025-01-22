# mypy: ignore-errors
# flake8: noqa
# ruff: skip

Import("env")

env.Append(CXXFLAGS=["-Wno-register"])
