Import("env", "projenv")
import shutil
import os
import sys

for e in [ env, projenv ]:
    # If compiler uses `-m32`, propagate it to linker.
    # Add via script, because `-Wl,-m32` does not work.
    if "-m32" in e['CCFLAGS']:
        e.Append(LINKFLAGS = ["-m32"])

exec_name = "${BUILD_DIR}/${PROGNAME}${PROGSUFFIX}"

# Override unused "upload" to execute compiled binary
from SCons.Script import AlwaysBuild
AlwaysBuild(env.Alias("upload", exec_name, exec_name))

if sys.platform.startswith("win"):
    print("Copying SDL2.dll to build directory")
    shutil.copy(os.getcwd() + "/w64devkit/x86_64-w64-mingw32/bin/SDL2.dll", os.getcwd() + "/.pio/build/emulator_64bits")

# Add custom target to explorer
env.AddTarget(
    name = "execute",
    dependencies = exec_name,
    actions = '"{}"'.format(exec_name),
    title = "Execute",
    description = "Build and execute",
    group="General"
)

#print('=====================================')
#print(env.Dump())
