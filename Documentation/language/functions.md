---
sidebar_position: 4
---

# Functions

## Declaration Syntax

```iris
function add(lhs: Int32, rhs: Int32) -> (result: Int32)
{
    return lhs + rhs;
}
```

- Parameters are `name: Type` pairs separated by commas.
- Return type is specified after `->` using the same `name: Type` syntax. The name is part of the declaration but is just a label — you still use `return`.
- A function that returns nothing uses `-> ()`.

## Export

Functions are private by default. Add `export` to make them accessible from other modules:

```iris
export function my_interface(value: My_struct) -> (c: Int32)
{
    return value.a + value.b;
}
```

## Named Entry Point

Use `@unique_name` to bind an exported function to a specific linker symbol (required for the C runtime `main`):

```iris
@unique_name("main")
export function main() -> (result: Int32)
{
    return 0;
}
```

## Implicit Arguments (Method-Style Calls)

A function whose first parameter is a pointer can be called with `.` or `->` syntax on that pointer's pointee:

```iris
export function get_v0(instance: *My_struct) -> (result: Int32)
{
    return instance->v0;
}

function run() -> ()
{
    mutable instance: My_struct = {};

    // Method-style: compiler automatically passes &instance
    var a = instance.get_v0();

    // Pointer-style: explicit pointer, same result
    var ptr = &instance;
    var b = ptr->get_v0();
}
```

## Variadic Functions

Append `...` after the last named parameter to accept additional C-style variadic arguments:

```iris
function foo(first: Int32, ...) -> ()
{
}
```

Variadic functions are primarily used when declaring or wrapping C functions (e.g. `printf`).

## Function Pointers

The type syntax for a function pointer is `function<(params) -> (returns)>`:

```iris
struct My_struct
{
    a: function<(lhs: Int32, rhs: Int32) -> (result: Int32)> = null;
}

function run() -> ()
{
    // Assign a function to a variable
    var a = add;
    var r0 = a(1, 2);

    // Call through a struct field
    var b: My_struct = { a: add };
    var r1 = b.a(3, 4);
}
```

## Function Contracts

Preconditions and postconditions can be attached to any function. They are checked at runtime in debug builds:

```iris
export function run(x: Int32) -> (result: Int32)
    precondition "x >= 0" { x >= 0 }
    precondition "x <= 8" { x <= 8 }
    postcondition "result >= 0" { result >= 0 }
    postcondition "result <= 64" { result <= 64 }
{
    if x == 8
    {
        return 64;
    }

    return x * x;
}
```

The quoted string is the message emitted on violation; the block is the boolean expression to evaluate.
