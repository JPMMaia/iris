---
name: 'compiler'
description: 'Describes the Compiler which takes care of making Core Module transformations and lowering a Core Module to LLVM IR.'
---

The main compiler logic is located in `Source/Compiler/Compiler.cpp`. It lowers `h::Module` to `llvm::Module` and `h::Function_declaration` and `h::Function_definition` to `llvm::Function`s.

`Source/Compiler/Expressions.cpp` lowers `h::Statement` and `h::Expression` to LLVM instructions (`llvm::Value`).
`Source/Compiler/Instructions.cpp` contains utility functions to create llvm instructions (e.g. alloca, load, memcpy).

## Core Module representation transformations

Before we lower `h::Module` to `llvm::Module` we want to do some transformations on the `h::Module` such as reflection, instantiate new functions and types (from function constructors and type constructors) and also replace compile-time expressions. These passes should be called from `Source/Compiler/Compiler.cpp` and they are located in `Source/Compiler/passes`.

## C Platform ABI

To guarantee we can call C functions from our programming language, we need to ask clang how the lowering to LLVM IR should be done. This is done in `Source/Compiler/Clang_code_generation.cpp`.

## Tests

The compiler tests are located in `Source/Compiler/Compiler.tests.cpp`. Each test input is some source code (usually coming from a file in `Examples/txt`). The source code is then compiled and we check that it matches the expected LLVM-IR.

## Building and running tests

Run `cmake --build build --target H_compiler_tests` to build the tests.
Run `build/Source/Compiler/H_compiler_tests.exe [LLVM_IR]` to run all LLVM-IR tests.

## Editing the compiler or expressions

1. Add/edit/remove a test file in `Examples/txt`.
2. Edit `Source/Compiler/Compiler.tests.cpp` to add/edit/remove a test using that test file
3. Build and run the LLVM-IR tests and check that the output is what is expected (e.g. if we haven't implemented the feature yet, then it will likely fail).
4. Make changes to the compiler (e.g. `Source/Compiler/Compiler.cpp`, `Source/Compiler/Expressions.cpp`, etc) as needed.
5. Build and run the LLVM-IR tests again. Check the actual output and verify that it matches what we would expect as the LLVM-IR. If so, then update the expected value in the test, and then build and run the tests. Otherwise, go to step 4.
