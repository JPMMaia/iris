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

## `compile_time var`

Declares a variable whose value is evaluated at **compile time** and propagated directly into every use site. No runtime local is generated — the declaration itself is completely erased from the compiled output.

```iris
export function run() -> (result: Int32)
{
    compile_time var kind = @get_type_kind::<Int32>();

    compile_time if kind == Type_kind.Int
    {
        return 1;
    }
    else
    {
        return 0;
    }
}
```

After the compile-time pass the example above compiles as if it had been written:

```iris
export function run() -> (result: Int32)
{
    {
        return 1;
    }
}
```

### Rules

- The right-hand side must be computable at compile time (a compile-time constant, a reflection expression, or another `compile_time var`).
- `compile_time var` is useful together with `compile_time if` and `compile_time for` to avoid repeating the same reflection call in every branch condition.
- A typed form is also supported: `compile_time var kind: Type_kind = @get_type_kind::<T>();`
