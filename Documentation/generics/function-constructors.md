---
sidebar_position: 1
---

# Function Constructors

A **function constructor** is a compile-time function that generates a new function. It is Iris's mechanism for generic functions — what other languages call templates or generics.

## Declaration

```iris
export function_constructor add(value_type: Type)
{
    return function (first: value_type, second: value_type) -> (result: value_type)
    {
        return first + second;
    };
}
```

- The parameter `value_type: Type` is resolved at compile time.
- The body returns a `function` expression that becomes the instantiated function.

## Instantiation — Explicit Type Arguments

Call the constructor with explicit type arguments using `::<…>`:

```iris
var a = add::<Int32>(1, 2);        // instantiates add for Int32
var b = add::<Float32>(3.0f32, 4.0f32);
```

## Instantiation — Type Deduction

When the type can be deduced from the call-site arguments, the `::<…>` is optional:

```iris
var c = add(1u32, 2u32);   // deduced: value_type = Uint32
```

## Multiple Type Parameters

A constructor can take any number of `Type` parameters:

```iris
export function_constructor map(from_type: Type, to_type: Type)
{
    return function (value: from_type) -> (result: to_type)
    {
        return value as to_type;
    };
}

var x = map::<Int32, Float32>(42);
```

## Key Properties

| Feature | Behaviour |
|---|---|
| Instantiation | At compile time — each unique set of type arguments produces its own function |
| Linkage | Instantiated functions have private linkage; no symbol clashes across modules |
