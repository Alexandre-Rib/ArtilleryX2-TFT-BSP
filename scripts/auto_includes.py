"""
1. Scans lib/, src/, include/ → adds -I flags for every dir containing a .h file.
2. Force-compiles ALL source files in lib/ via BuildSources.
   LDF is disabled (lib_ldf_mode=off); this script is its replacement.
"""
import os

Import("env")

# --- 1. Include paths ---
include_dirs = set()
for base_dir in ["lib", "src", "include"]:
    if not os.path.isdir(base_dir):
        continue
    for root, dirs, files in os.walk(base_dir):
        if any(f.endswith(".h") for f in files):
            include_dirs.add(root.replace("\\", "/"))

sorted_dirs = sorted(include_dirs)
include_flags = ["-I" + d for d in sorted_dirs]
env.Append(
    CCFLAGS=include_flags,
    CXXFLAGS=include_flags,
    ASFLAGS=include_flags,
)
print(f"[auto_includes] {len(sorted_dirs)} include path(s) added")

# --- 2. Force-compile all lib/ sources ---
lib_dir = os.path.join(env.subst("$PROJECT_DIR"), "lib")
if os.path.isdir(lib_dir):
    env.BuildSources(
        os.path.join(env.subst("$BUILD_DIR"), "lib_all"),
        lib_dir,
    )
    print("[auto_includes] lib/ sources added to build via BuildSources")