---
name: 'windows-build'
description: 'Describes how to configure and build all CMake targets on Windows. Use this when you need to use CMake or build any project.'
---

## Building with CMake

We need to run the commands in a PowerShell terminal. Then, we need to setup the environment by running:

To configure CMake:

`Enter-VsDevEnv ; cmake --preset windows-debug`

To build all CMake targets:

`Enter-VsDevEnv ; cmake --build build`

To build a specific CMake `target`:

`Enter-VsDevEnv ; cmake --build build --target target`

Do not use the CMake tools extension.

### Troubleshooting

If during compilation you get a message similar to:

```
error C3474: could not open output file 'path/to/source/file.ifc'
```

then stop and aks the user to solve the issue manually.

## Building a single file

Building a CMake target with Ninja might trigger errors from other files. To keep the session more efficient and focused it might preferable to compile a single file.

To compile a single file named `path/to/file_name.cpp` run:

`python3 ./Scripts/build_file.py path/to/file_name.cpp`

## Running tests

To run tests, prefer using the tests executable directly (we use Catch2). Don't use ctest.
