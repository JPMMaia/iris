---
name: 'validation'
description: 'Describe the Compiler Validation which validates the Core Module representation and creates Diagnostics.'
---

Validation tests are located in `Source/Compiler/Validation.tests.cpp`. Each test should have a `[Validation]` tag and another tag related to the validation check. Tests should be sorted alphabetically by this last tag.

Validation is performed in `Source/Compiler/Validation.cpp`. Diagnostics created by the Validation are location in `Source/Compiler/Diagnostic.cppm`

Validation often only reports what `Source/Compiler/Analysis.cpp` computed. `Analysis.cpp` derives each expression's `Type_info` (its type and whether it `is_mutable`), so a false positive frequently comes from `Type_info` being wrong rather than from the check in `Validation.cpp`.

## Editing validation

1. Edit `Source/Compiler/Validation.tests.cpp` to add/edit/remove a test
2. You might want to build and run the validation tests now. For example, if you added a new test you might want to check that it fails.
3. Edit `Source/Compiler/Validation.cpp` as needed, or `Source/Compiler/Analysis.cpp` if the underlying `Type_info` is what is wrong.
4. Build and run the validation tests again. If they are green, the task is complete. Otherwise, go to step 3.

## Building and running Validation tests

Run `cmake --build build --target Iris_validation_tests` to build the tests.
Run `build/bin/Debug/Iris_validation_tests.exe [Validation]` to run all Validation tests.

Pass a test name instead of `[Validation]` to run a single test. Note that a filter matching no test reports "No tests ran" rather than failing, so check that the test you expect actually ran.

Changing `Analysis.cpp` also affects the other compiler suites, so run `Iris_compiler_tests`, `Iris_passes_tests` and `Iris_builder_tests` from `build/bin/Debug/` as well.
