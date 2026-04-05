#!/usr/bin/env python3

import platform
import sys
import subprocess
import re
from pathlib import Path

if len(sys.argv) != 2:
    print("Usage: build_file.py <source_file>")
    sys.exit(1)

source_file = sys.argv[1]
filename = Path(source_file).name
obj_name = filename + ".obj"

build_file = "build/build.ninja"

target = None

with open(build_file, "r", encoding="utf-8") as f:
    for line in f:
        if obj_name in line and line.startswith("build "):
            m = re.match(r"^build\s+([^:]+):", line)
            if m:
                value = m.group(1)
                if value.endswith(".obj"):
                    target = m.group(1)
                    break

if not target:
    print(f"Could not find target for {obj_name}")
    sys.exit(1)

print(f"Building target: {target}")


if platform.system() == 'Windows':

    process = subprocess.Popen(
        ["powershell.exe"],
        stdin=subprocess.PIPE,
        text=True
    )

    process.stdin.write("Enter-VsDevEnv\n")
    process.stdin.write(f"ninja -C build {target}\n")

    process.stdin.close()

else:
    ninja_command = ["ninja", "-C", "build", target]
    print("Executing command: " + " ".join(ninja_command))
    subprocess.run(ninja_command, shell=True, check=True)
