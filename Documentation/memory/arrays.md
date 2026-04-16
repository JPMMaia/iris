---
sidebar_position: 2
---

# Arrays

Iris has two array forms: **constant arrays** (compile-time fixed size), and **stack arrays** (mutable fixed-size).

## Constant Arrays — `Constant_array::<T, N>`

A fixed-size array whose length is a compile-time constant.

```iris
// Inference: type and size deduced from the literal
mutable a: Constant_array::<Int32, 4> = [0, 1, 2, 3];

// Element access
var b = a[3];

// Element assignment (requires mutable)
a[0] = 0;
a[1] = 1;
```

Constant arrays can also live inside structs:

```iris
struct My_struct
{
    a: Constant_array::<Int32, 4> = [0, 2, 4, 6];
}

var instance: My_struct = {};
var e = instance.a[0];   // 0
```

## Stack Array with Loop

A common pattern is allocating a fixed-size array on the stack and iterating over it:

```iris
mutable values: Constant_array::<Int32, 8> = [0, 0, 0, 0, 0, 0, 0, 0];

for index in 0 to 8
{
    values[index] = index * 2;
}
```

## Stack Arrays

:::caution TODO
Explain stack arrays
:::
