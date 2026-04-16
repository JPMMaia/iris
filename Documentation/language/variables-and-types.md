---
sidebar_position: 2
---

# Variables & Types

## Variables

Declare a variable with `var`. Variables are **immutable by default**:

```iris
var my_constant_variable = 1;
```

To allow reassignment, use `mutable`:

```iris
mutable my_mutable_variable = 2;
my_mutable_variable = 3;   // OK
```

Types are inferred from the initialiser in most cases. You can also annotate the type explicitly:

```iris
var x: Int32 = 42;
mutable y: Float32 = 3.14f32;
```

## Built-in Integer Types

| Type | Size | Signed | Literal suffix |
|---|---|---|---|
| `Int8` | 8-bit | ✓ | `1i8` |
| `Int16` | 16-bit | ✓ | `1i16` |
| `Int32` | 32-bit | ✓ | `1` (default) |
| `Int64` | 64-bit | ✓ | `1i64` |
| `Uint8` | 8-bit | — | `1u8` |
| `Uint16` | 16-bit | — | `1u16` |
| `Uint32` | 32-bit | — | `1u32` |
| `Uint64` | 64-bit | — | `1u64` |

## Built-in Float Types

| Type | Size | Literal suffix |
|---|---|---|
| `Float16` | 16-bit | `1f16` |
| `Float32` | 32-bit | `1f32` |
| `Float64` | 64-bit | `1f64` |

## Built-in Decimal Types

`DecimalN` is a signed fixed-point type where `N` is the decimal scale.

Valid types are `Decimal1` through `Decimal18`.

| Type range | Backing integer | Literal suffix |
|---|---|---|
| `Decimal1` - `Decimal6` | `Int32` | `dN` (for example `1.25d2`) |
| `Decimal7` - `Decimal18` | `Int64` | `dN` (for example `1.25d7`) |

For arithmetic constraints and cross-scale behavior, see [Decimal Types](./decimal-types.md).

## Boolean

```iris
var my_true_boolean = true;
var my_false_boolean = false;
```

`Bool` is the type; literals are `true` and `false`.

## C-Compatible Types

Iris provides a set of types that map exactly to C equivalents, useful when interfacing with C libraries:

| Iris type | C equivalent | Literal suffix |
|---|---|---|
| `C_char` | `char` | `1cc` |
| `C_short` | `short` | `1cs` |
| `C_int` | `int` | `1ci` |
| `C_long` | `long` | `1cl` |
| `C_long_long` | `long long` | `1cll` |
| `C_unsigned_char` | `unsigned char` | `1cuc` |
| `C_unsigned_short` | `unsigned short` | `1cus` |
| `C_unsigned_int` | `unsigned int` | `1cui` |
| `C_unsigned_long` | `unsigned long` | `1cul` |
| `C_unsigned_long_long` | `unsigned long long` | `1cull` |
| `C_bool` | `_Bool` / `bool` | `1cb` |

## String Literals

Append `c` to a string literal to produce a `*C_char` (a null-terminated C string):

```iris
var greeting = "Hello"c;   // type: *C_char
```

## Global Variables

Variables declared outside of any function are module-level globals:

```iris
var g_global_0 = 0;
mutable g_global_1 = 0;
```

Global `var` values are constant after initialisation. Global `mutable` values can be written by any function.
