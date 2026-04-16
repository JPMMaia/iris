---
sidebar_position: 2
---

# Compile-Time expressions

## `compile_time if`

Evaluates a condition at **compile time** and includes only the matching branch in the compiled output. The other branch is completely discarded.

This is equivalent to `#ifdef` / `if constexpr` in C/C++.

```iris
var g_debug = true;

export function run_0() -> (result: Int32)
{
    compile_time if g_debug
    {
        return 0;   // only this branch is compiled when g_debug == true
    }
    else
    {
        return 1;
    }
}

export function run_1() -> (result: Int32)
{
    compile_time if !g_debug
    {
        return 2;
    }
    else
    {
        return 3;   // only this branch is compiled
    }
}
```

## `compile_time for`

Unrolls a loop at compile time. Each iteration produces a separate copy of the body with the index substituted as a compile-time constant. This is equivalent to template-based loop unrolling in C++.

```iris
function foo(index: Uint64) -> ()
{
}

function run() -> ()
{
    compile_time for index in 0u64 to 3u64
    {
        foo(index);   // expands to: foo(0u64); foo(1u64); foo(2u64);
    }
}
```

The range bounds must be compile-time constants. The index variable is a `Uint64`.
