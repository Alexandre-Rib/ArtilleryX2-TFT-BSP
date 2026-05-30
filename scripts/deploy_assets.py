import os
import shutil
import atexit
Import("env")

def deploy_release():
    env_name     = env.get("PIOENV")
    firmware_bin = os.path.join(".pio", "build", env_name, "firmware.bin")
    release_dir  = os.path.join("out", env_name, "release")

    # Guard: binary must exist (build succeeded)
    if not os.path.exists(firmware_bin):
        return

    # Create output tree if missing (works on fresh clone)
    os.makedirs(release_dir, exist_ok=True)

    # 1 — binary
    dest_bin = os.path.join(release_dir, "MKSTFT28.bin")
    shutil.copyfile(firmware_bin, dest_bin)
    print(f"--> Firmware : {dest_bin}")

    # 2 — res/ tree (full copy, structure preserved)
    src_res  = "res"
    dest_res = os.path.join(release_dir, "res")
    if os.path.exists(src_res):
        if os.path.exists(dest_res):
            shutil.rmtree(dest_res)
        shutil.copytree(src_res, dest_res)
        print(f"--> Resources : {dest_res}")
    else:
        print("--> Note: 'res/' absent, ressources ignorees")

    print("--> Deploy OK")

# atexit fires at the end of every `pio run`, compiled or not.
atexit.register(deploy_release)
