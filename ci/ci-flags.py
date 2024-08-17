Import("env")

env.Append(CXXFLAGS=["-Wno-register", "-Wno-volatile"])
