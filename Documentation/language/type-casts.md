---
sidebar_position: 7
---

# Type Casts

Iris requires explicit casts between types using the `as` keyword. There are no implicit numeric conversions.

## `as` — Explicit Cast

```iris
using My_uint = Uint32;

export function run(first: My_uint, second: Int32, third: C_int) -> ()
{
    var a = 1u32;
    var b = a == (first as Uint32);    // cast alias to its base type
    var c = second as My_uint;          // Int32 → Uint32
    var d = a as C_int;                 // Uint32 → C_int
    var e = third as Int32;             // C_int → Int32
}
```

Common uses:

| From | To | Example |
|---|---|---|
| `Int32` | `Uint32` | `value as Uint32` |
| `Uint32` | `C_int` | `value as C_int` |
| `C_int` | `Int32` | `cval as Int32` |
| Type alias | Underlying type | `my_uint_alias as Uint32` |
| Any numeric | Any numeric | `f32 as Int64` |

## Numeric Casts

Standard numeric casts follow C-like truncation/extension rules:

```iris
var i: Int32 = 300;
var as_i8: Int8 = i as Int8;      // truncated to 44 (300 mod 256)
var as_f32: Float32 = i as Float32;
```

Narrowing casts are silent (no runtime check).

### Decimal Casts

Decimals participate in explicit numeric casts with `as`.

```iris
var i32_to_d4: Decimal4 = i32_value as Decimal4;
var d4_to_i32: Int32 = d4_value as Int32;
var f32_to_d4: Decimal4 = f32_value as Decimal4;
var d4_to_f32: Float32 = d4_value as Float32;
var d4_to_d7: Decimal7 = d4_value as Decimal7;
```

When casting decimal or float values to integers, Iris rounds half away from zero:

- `0.5d2 as Int32` -> `1`
- `-0.5d2 as Int32` -> `-1`

Arithmetic between different decimal scales still requires an explicit cast to a common scale before the operation.

## `reinterpret_as` — Bitcast

`reinterpret_as` reinterprets the raw bits of a value as a different type of the **same size**. This is equivalent to `memcpy`-based type-punning:

```iris
var bits: Uint32 = 0x3F800000u32;
var as_float: Float32 = reinterpret_as::<Float32>(bits);
```

This is an unsafe operation — you are responsible for ensuring the resulting bit pattern is a valid value of the target type.
