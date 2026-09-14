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
- `check(condition)` asserts that `condition` is `true`. A failing `check` reports the location and marks the test as failed; the rest of the test body still runs.

## Running Tests

Build the tests using:

```powershell
iris build-tests [artifact_name] --build-directory build [--repository path/to/repository]
```

Run the built tests using:

```powershell
iris test [artifact_name] --build-directory build [--repository path/to/repository]
```

If `artifact_name` is omitted, all discovered artifacts are processed. Test executables are generated in `build/bin/*.iris.test*`.

`artifact_name` is an **artifact** name, not a module name. To narrow a run down to a single test, use `--test-name` instead:

```powershell
iris test my_library --test-name=my_library.test_addition
iris test my_library --list-tests
```

`--test-name` may be repeated. A name that matches no test is an error, not an empty successful run.

## Selecting and Filtering

| Flag | Effect |
|---|---|
| `--test-name=<module>.<test>` | Run only this test. Repeatable. |
| `--list-tests` | List the tests each executable contains instead of running them. |
| `--stop-on-crash` | Stop a test executable's run at the first crash instead of continuing with the next test. |

These are forwarded to the test executables, which also accept them directly:

```powershell
build/bin/my_library.iris.test.exe --list-tests
```

## Reading the Output

Every run ends with a summary line, whether it passed or failed:

```
12 tests selected, 12 run, 12 passed, 0 failed (0 crashed), 0 not run
```

The `selected` and `run` counts are worth watching: if they differ, the run is incomplete and the tests that did not execute are listed by name. A run with **no summary line at all** means the process died before it could print one.

A test that faults — a null dereference, a bad pointer — is reported and the suite continues:

```
[ RUN      ] "my_library.test_broken"
[    CRASH ] "my_library.test_broken" (access violation, code 0xC0000005)
```

A crash counts as a failure. Because a process that has taken a memory fault is not fully trustworthy afterwards, `--stop-on-crash` is available when you would rather end the run there; the tests that were skipped are then named.

## Exit Codes

| Code | Meaning |
|---|---|
| `0` | Every selected test ran and passed. |
| `1` | The run completed; some tests failed or crashed. |
| `2` | The run is incomplete: tests were not executed, or `--test-name` matched nothing. |

`iris test` distinguishes these when reporting, so an executable that did not run to completion is reported as incomplete results rather than as failing tests.
