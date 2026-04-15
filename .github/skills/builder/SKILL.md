---
name: 'builder'
description: 'Describes the Iris Build System used for building Iris projects. Use this when dealing with the Iris Build System or with Iris Artifacts (`iris_artifact.json`) or Iris Repositories (`iris_repository.json`).'
---

The main Iris Build System logic is located in `Source/Compiler/Builder.cpp`. There are two important components of the build system: Artifacts and Repositories.

Repository files tell the location of Artifact files. The repository data structure is located in `Compiler/Project/Repository.cpp`. Iris projects define these in `iris_repository.json` files.

Artifacts tell the build system what do build. Artifacts are used to build executables or libraries, but also to import already built external libraries like C libraries. The artifact data structure is located in `Source/Compiler/Project/Artifact.cppm`. Iris projects define these in `iris_artifact.json` files.

The Iris Build System tests are located in `Builder.tests.cpp`. These use the projects in `Examples` for tests.

The C++ source files compilation is handled in `Source/Compiler/Clang_compiler.cpp`.

The linker is handled in `Source/Compiler/Linker_coff.cpp` and `Source/Compiler/Linker_elf.cpp`.

## Command line interface

To use the Iris Build System, we have an application defined in `Source/Builder`. In `Source/Builder/main.cpp` we parse the command line arguments and then call the builder functions as needed that are exposed in `Source/Compiler/Builder.cppm`.
