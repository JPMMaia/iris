---
name: 'validation'
description: 'Describe the Compiler Validation which validates the Core Module representation and creates Diagnostics.'
---

Validation tests are located in `Source/Compiler/Validation.tests.cpp`. Each test should have a `[Validation]` tag and another tag related to the validation check. Tests should be sorted alphabetically by this last tag.

Validation is performed in `Source/Compiler/Validation.cpp`. Diagnostics created by the Validation are location in `Source/Compiler/Diagnostic.cppm`

## Editing validation

1. Edit `Source/Compiler/Validation.tests.cpp` to add/edit/remove a test
2. You might want to build and run the validation tests now. For example, if you added a new test you might want to check that it fails.
3. Edit `Source/Compiler/Validation.cpp` as needed.
4. Build and run the validation tests again. If they are green, the task is complete. Otherwise, go to step 3.

## Building and running Validation tests

Run `cmake --build build --target H_compiler_tests` to build the tests.
Run `build/Source/Compiler/H_compiler_tests.exe [Validation]` to run all Validation tests.
