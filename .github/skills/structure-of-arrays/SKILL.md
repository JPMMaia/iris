---
name: 'structure-of-arrays'
description: 'Describe the Structure of Arrays (SoA) feature in the Iris programming language.'
---

# Overview

In the Iris programming language `Soa_array` implements a **Structure of Arrays (SoA)** memory layout for arrays of structures.

Given a structure type `T`, a `Soa_array::<T, N>` stores **each member of `T` in its own contiguous memory region**, instead of storing complete instances sequentially.

This layout improves **cache locality and SIMD/vectorization opportunities** when operating on individual members across many elements.

The type provides two primary access patterns:

1. **Structure access** — retrieve a full instance of `T`
2. **Member access** — access a single member across all elements

---

# Type Definitions

## `Soa_array::<T, N>`

Generic owning container.

```
Soa_array::<T, N>
```

Parameters:

* `T` — a `struct` type
* `N` — `Uint64`, the number of elements stored

Constraints:

* `T` must be a struct
* all members of `T` must have known compile-time layout

The container **owns the memory** for all elements.

## Memory Layout

For a struct:

```
struct Particle {
    x: Float32 = 0.0f32;
    y: Float32 = 0.0f32;
    velocity: Float32 = 0.0f32;
}
```

A `Soa_array::<Particle, 4>` is laid out in memory as:

```
x:        [x0, x1, x2, x3]
y:        [y0, y1, y2, y3]
velocity: [v0, v1, v2, v3]
```

Instead of:

```
[x0 y0 v0] [x1 y1 v1] [x2 y2 v2] [x3 y3 v3]
```

---

# Length

Given `array` of type `Soa_array::<T, N>`, `array.length` returns `N`.

# Data pointer

Given `array` of type `Soa_array::<T, N>`, `array.data` returns `data` which is a pointer to the raw array data.

---

# Initialization

To create an array with 2 elements of `Particles` that are default initialized:

```
var default_array: Soa_array::<Particles, 2> = [];
---

To create an array with 2 elements explicitly initialized:

```
var default_array: Soa_array::<Particles, 2> = [
    {
        x: 0.0f32,
        y: 0.0f32,
        velocity: 1.0f32
    },
    {
        x: 1.0f32,
        y: 2.0f32,
        velocity: 3.0f32
    },
];
```

---

# Element Access

## Structure access

```
var v = array[index];
```

Behavior:

* Reads the member values for `index`
* Constructs and returns a **temporary instance of `T`**

Example:

```
var p = particles[3];
```

Equivalent to:

```
var p: Particle = {
    x: particles.x[3],
    y: particles.y[3],
    velocity: particles.velocity[3]
};
```

## Member access

Members of `T` can be accessed as arrays.

```
array->member[index]
```

Example:

```
particles->x[3]
particles->velocity[10]
```

Semantics:

* Accesses the contiguous storage of that member
* Equivalent to operating on a normal array

Example iteration:

```
for index in 0u64 to particles.length {
    particles->velocity[index] += 1.0f32;
}
```

---

# `Soa_array_view::<T>`

Non-owning view type.

```
Soa_array_view::<T>
```

Provides the same **access semantics** as `Soa_array` but **does not own memory**. `Soa_array_view` needs to store a length at runtime, whereas `Soa_array` doesn't as the length is known at compile time. This view is read-only.

For a read-write view, mutable must be added before `T`:

```
Soa_array_view::<mutable T>
```

## Structure

Conceptually equivalent to:

```
struct Soa_array_view::<T> {
    start_index: Uint64 = 0u64;
    end_index: Uint64 = 0u64:
    length: Uint64 = 0u64;
    data: pointer_to_member_arrays = null;
}
```

`data` is **a single pointer to the start of a contiguous allocation**.
The range of elements in the view is defined by `start_index` and `end_index`.
`length` is the same value as the `Soa_array` and is needed to calculate the proper offsets.

```

---

## Access Semantics

Identical to `Soa_array` but uses runtime `start_index`, `end_index` and `length`.

---

# Conversion

A `Soa_array` can produce a view for the whole range (0 to `particles.length`):

```
var view = particles.view();
```

This creates a `Soa_array_view::<Particle>` with the following data:

```
{
    start_index: 0,
    end_index: particles.length,
    length: particles.length,
    data: particles.data
}
```

Optional arguments can be provided too to specify the `start_index` and `end_index`

```
var subview = particles.view(2, 4);
```

This creates a `Soa_array_view::<Particle>` with the following data:

```
{
    start_index: 2,
    end_index: 4,
    length: particles.length,
    data: particles.data
}
```

---

# Runtime Representation

The runtime representation uses **a single pointer to a contiguous allocation**.

Conceptually:

```
struct Soa_array::<T, N> {
    ptr: *byte
}
```

The total allocated memory is:

```
Σ (N * sizeof(member))
```

for all members of `T`.

The compiler computes **static offsets** for each member block.

Example:

```
x_offset = 0
y_offset = N * sizeof(Float32)
velocity_offset = 2 * N * sizeof(Float32)
```

General formula:

```
offset(member_i) =
    Σ (N * sizeof(member_j))  where j < i
```

---

# Member Access

## Syntax

```
array->member[index]
```

Example:

```
particles->velocity[i]
```

Semantics:

* compute the base address of the member block
* index into the block

Conceptually:

```
member_ptr = ptr + offset(member)
element_ptr = member_ptr + index * sizeof(member_type)
```

The resulting pointer is treated as `*member_type`.

---

# LLVM Lowering

The compiler lowers `Soa_array` using **a single byte pointer with computed offsets**.

Example internal representation:

```
%Soa_array = type { i8* }
```

---

## Member Access Lowering

Example source:

```
particles->velocity[i]
```

Lowering steps:

1. load base pointer
2. add member offset
3. cast to typed pointer
4. index into element

Example LLVM IR pattern:

```
%base = load i8*, i8** %particles_ptr

%member_base = getelementptr i8, i8* %base, i64 velocity_offset

%typed = bitcast i8* %member_base to float*

%elem_ptr = getelementptr float, float* %typed, i64 %i

%val = load float, float* %elem_ptr
```

Stores use the same pointer.

---

# Advantages

* Improved cache locality for column-wise operations
* Better SIMD/vectorization opportunities
* Efficient bulk processing of single struct members
* Zero-copy views via `Soa_array_view`
