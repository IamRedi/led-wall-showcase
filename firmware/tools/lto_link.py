Import("env")

# PlatformIO's Arduino link step adds -fno-lto even when application objects
# are compiled with LTO. Keep the prebuilt framework objects unchanged.
env.Replace(LINKFLAGS=[flag for flag in env.get("LINKFLAGS", []) if flag != "-fno-lto"])
env.Append(LINKFLAGS=["-flto"])
