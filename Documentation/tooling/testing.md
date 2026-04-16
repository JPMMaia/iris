---
sidebar_position: 2
---

# Testing

Iris has a lightweight built-in test framework. Test functions are annotated with `@test` and use `check()` assertions.

## Writing Tests

```iris
module Test_framework;

function add(a: Int32, b: Int32) -> (result: Int32)
{
    return a + b;
}

@test
function test_addition() -> ()
{
    check(add(1, 2) == 3);
    check(add(2, 3) == 5);
}
```

- `@test` marks a function as a test case. Test functions take no parameters and return `()`.
- `check(condition)` asserts that `condition` is `true`. A failing `check` reports the location and stops the test.

## Running Tests

Build the tests using:

```powershell
iris build-tests --build-directory build [--repository path/to/repository]
```

This will create an test executable for each artifact that has tests in `build/bin/*.tests*`.
