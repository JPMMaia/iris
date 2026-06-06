---
sidebar_position: 4
---

# Structure of Arrays (SoA)

When you have an array of structs, the default **Array of Structures (AoS)** layout interleaves all fields of every element:

```
AoS: [ x0 y0 | x1 y1 | x2 y2 | x3 y3 ]
```

For workloads that only touch one or two fields of each element (e.g. updating all X positions), this layout wastes cache bandwidth loading unused fields.

**Structure of Arrays (SoA)** keeps each field in its own contiguous region:

```
SoA: x: [ x0 x1 x2 x3 ]
     y: [ y0 y1 y2 y3 ]
```

Iterating over all X values now reads a single cache line with no wasted bytes.

## `Soa_array::<T, N>`

An owning, fixed-size SoA container. `T` must be a struct; `N` is the number of elements (known at compile time).

```iris
struct Particle
{
    x: Float32 = 0.0f32;
    y: Float32 = 0.0f32;
}

mutable particles: Soa_array::<Particle, 4> = {};
```

### Structure Access

Read or write one complete element at index `i`:

```iris
var p1 = particles[1];        // returns a Particle{x, y} value

particles[1] = {
    x: 3.0f32,
    y: 4.0f32
};
```

Reading element `i` is equivalent to constructing:

```iris
Particle { x: particles->x[i], y: particles->y[i] }
```

### Member Access

Access the contiguous storage of a single field across all elements:

```iris
var x2 = particles->x[2];          // read x of element 2
particles->x[2] = 1.0f32;          // write x of element 2

var y3 = particles->y[3];
particles->y[3] = 2.0f32;
```

`->member[index]` is the idiomatic way to iterate or SIMD-process a single field efficiently.

### Length and Raw Data

```iris
var length = particles.length;   // compile-time N (Uint64)
var data   = particles.data;     // raw pointer to the SoA memory block
```

## `Soa_array_view::<T>`

A non-owning view (like `Array_slice` for SoA):

```iris
var view: Soa_array_view::<Particle> = particles.view();
```

## Initialisation with Values

Default-initialise all elements to their field defaults:

```iris
var default_array: Soa_array::<Particle, 2> = [];
```

Explicitly initialise all elements:

```iris
var init_array: Soa_array::<Particle, 2> = [
    { x: 0.0f32, y: 0.0f32 },
    { x: 1.0f32, y: 2.0f32 },
];
```

## Performance Pattern — Update All X Values

```iris
for index in 0u64 to particles.length
{
    particles->x[index] += 1.0f32;
}
```

This code accesses only the contiguous `x` array — maximally cache-friendly.
