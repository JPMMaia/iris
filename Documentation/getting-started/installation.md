---
sidebar_position: 1
---

# Installation

This page explains how to build Iris from source. Pre-built binaries are not yet distributed; you need the full build toolchain.

## Prerequisites

| Tool | Notes |
|---|---|
| Visual Studio 2022 (Windows only) | With the **Desktop development with C++** workload |
| CMake ≥ 3.25 | Available from [cmake.org](https://cmake.org/download/) |
| Ninja | Bundled with Visual Studio or available via `winget install Ninja-build.Ninja` |
| Python ≥ 3.10 | Required for install scripts |
| Git | To clone the repository |

## Clone the Repository

```powershell
git clone https://github.com/iris-lang/iris
cd iris
```

## Configure & Build

### Windows

Open a **PowerShell** terminal, then configure and build:

```powershell
. ./Scripts/Enter-VsDevEnv.ps1
Enter-VsDevEnv
cmake --preset windows-debug
cmake --build build
```

`Enter-VsDevEnv` initialises the Visual Studio developer environment in the current shell, which makes the MSVC toolchain and Windows SDK available to CMake and Ninja.

:::tip[Release build]
To produce an optimised release build, replace the configure step:
```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```
:::

### Linux

:::caution TODO
Add instructions for building on Linux once it is supported
:::

## Install

After a successful build, run the install script to copy binaries and standard library files to a directory of your choice:

```powershell
python Scripts/build_utilities.py install_iris ../iris_install
```

Add the resulting `iris_install/bin` directory to your `PATH` so the `iris` command is available globally.

## Verify

```powershell
iris --version
```

You should see the version string printed to stdout.

## Next Step

[Create your first project →](first-project.md)
