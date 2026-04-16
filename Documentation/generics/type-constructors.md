---
sidebar_position: 2
---

# Type Constructors

A **type constructor** is a compile-time function that generates a new struct, enum, or union type.

## Declaration

```iris
export type_constructor Dynamic_array(element_type: Type)
{
    return struct
    {
        data: *mutable element_type = null;
        length: Uint64 = 0u64;
        capacity: Uint64 = 0u64;
    };
}
```

- The parameter `element_type: Type` is resolved at compile time.
- The body returns a `struct` (or `enum` / `union`) expression.

## Instantiation — Explicit Type Arguments

```iris
var instance_1: Dynamic_array::<Int32> = {};
var instance_2: Dynamic_array::<Float32> = {};
```

## `using` Alias for a Concrete Instantiation

Assign a concrete instantiation to an alias so you don't have to repeat the type arguments:

```iris
using My_dynamic_array = Dynamic_array::<Float32>;

var arr: My_dynamic_array = {};
```

## Key Properties

| Feature | Behaviour |
|---|---|
| Instantiation | At compile time — each unique type argument combination creates a new, distinct type |
| Memory layout | Identical to a hand-written struct with the concrete types substituted |
| `using` alias | Can alias any instantiation to a shorter name |
