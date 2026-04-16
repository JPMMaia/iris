---
sidebar_position: 1
---

# Pointers

Iris uses explicit pointers with C-like semantics. Pointer types are not hidden behind references — you always know when you are operating through a pointer.

## Pointer Types

| Type | Meaning |
|---|---|
| `*T` | Immutable pointer to `T` |
| `*mutable T` | Mutable pointer to `T` (allows writing through the pointer) |
| `*C_char` | Pointer to char — used for C strings |

## Address-Of (`&`)

Take the address of a variable, array element, or struct field with `&`:

```iris
var a = 1;
var pointer_a = &a;            // type: *Int32

var integers: *Int32 = ...;
var p0 = &integers[1];         // address of second element

var instance: My_struct = {};
var p1 = &instance.v1;         // address of struct field
```

## Dereference (`*`)

Read the value a pointer points to:

```iris
var dereferenced_a = *pointer_a;
```

## Member Access Through Pointer (`->`)

Access a struct field through a pointer using `->`:

```iris
export function get_v0(instance: *My_struct) -> (result: Int32)
{
    return instance->v0;
}
```

This is equivalent to `(*instance).v0`.

## Writing Through a Pointer

Write to the location a mutable pointer points to:

```iris
export function pointers(external_pointer: *mutable Int32) -> ()
{
    mutable p0 = offset_pointer(external_pointer, 2i64);
    *p0 = 0;   // write 0 at external_pointer + 2
}
```

## Pointer Arithmetic

`offset_pointer(ptr, offset)` moves a pointer by `offset` elements (not bytes):

```iris
mutable p = offset_pointer(base_ptr, 2i64);
```

## Null Pointers

The `null` literal is assignable to any pointer type. Check for null explicitly:

```iris
export function pointers(parameter: *Int32) -> (result: Int32)
{
    if parameter == null
    {
        return -1;
    }

    if parameter != null
    {
        return 1;
    }

    return 0;
}
```

## Void Pointers

`*Byte` (or `*mutable Byte`) serves as a generic untyped pointer:

```iris
var raw: *mutable Byte = allocate(64u64, 8u64);
var typed: *mutable Int32 = reinterpret_as::<*mutable Int32>(raw);
```

