---
sidebar_position: 3
---

# Array Slices

An `Array_slice::<T>` is a **non-owning view** into a contiguous region of memory. It carries a `data` pointer and a `length` count. Slices do not own the underlying memory.

## Type Syntax

| Type | Meaning |
|---|---|
| `Array_slice::<T>` | Read-only view of `T` elements |
| `Array_slice::<mutable T>` | Read-write view of `T` elements |

## Accessing a Slice

```iris
export function take(integers: Array_slice::<Int32>) -> ()
{
    var data:   *Int32  = integers.data;
    var length: Uint64  = integers.length;

    var v0 = integers[0];
    var v1 = integers[1];
    var v2 = integers[index];
}
```

## Creating a Slice

### From a Constant Array

A `Constant_array` can be passed directly where a slice is expected — the compiler creates a temporary slice automatically:

```iris
var a: Constant_array::<Int32, 4> = [0, 1, 2, 3];
take(a);       // implicit slice of the whole array
take([]);      // empty slice
take([4]);     // one-element slice
```

### From a Pointer

Use `create_array_slice_from_pointer` to wrap a raw pointer and a length:

```iris
var b = 0;
var c: *Int32 = &b;
var d = create_array_slice_from_pointer(c, 1u64);
take(d);
```

For a mutable view:

```iris
mutable f = 0;
var g: *mutable Int32 = &f;
var h: Array_slice::<mutable Int32> = create_array_slice_from_pointer(g, 1u64);
var i: *mutable Int32 = h.data;
var j: *mutable Int32 = &h.data[0];
```

### Manual Initialisation

A slice can also be initialised by setting its fields directly:

```iris
var value = 0;
var s0: Array_slice::<Int32> = {
    data:   &value,
    length: 1u64
};
```

## Slices in Structs

```iris
struct My_struct
{
    slice: Array_slice::<Int32> = {};
}

var v0: My_struct = {};   // slice.data = null, slice.length = 0
```

## Key Properties

- Slices do not manage lifetime — you are responsible for keeping the backing data alive.
- The `data` pointer inside a `*mutable` slice allows in-place modification.
- Slice length is always `Uint64`.
