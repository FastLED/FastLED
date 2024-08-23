Import("env")

def before_build(source, target, env):
    print("Hello")

env.AddPreAction("buildprog", before_build)
