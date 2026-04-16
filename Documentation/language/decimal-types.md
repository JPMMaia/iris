---
sidebar_position: 8
---

# Decimal Types

`DecimalN` is a signed fixed-point numeric type where `N` is the decimal exponent in the divisor:

```
value = integer_value / 10^N
```

| Range of `N` | Backing integer | Range example |
|---|---|---|
| 1 - 6 | `Int32` | `Decimal4` stores values like `1570 / 10^4 = 0.1570` |
| 7 - 18 | `Int64` | `Decimal7` stores values like `1234567 / 10^7 = 0.1234567` |

Valid types: `Decimal1` through `Decimal18`.

## Literals

Use the `dN` suffix to create a decimal literal. The compiler multiplies the numeric value by `10^N` internally:

```iris
var a: Decimal2 = 0.15d2;      // stored as Int32(15)
var b: Decimal4 = 1.57d4;      // stored as Int32(1570)
var c: Decimal7 = 123.4567d7;  // stored as Int64(1234567000)
var d: Decimal2 = 100d2;       // stored as Int32(10000)
```

## Arithmetic

Addition and subtraction work the same as integer arithmetic on the backing type:

```iris
var sum: Decimal4      = x + y;
var diff: Decimal4     = x - y;
var product: Decimal4  = x * y;    // computed as x * y / 10^4
var quotient: Decimal4 = x / y;    // computed as 10^4 * x / y
```

Intermediate multiplication and division use a wider integer to reduce overflow risk:

- `Int32`-backed (N is 6 or less): intermediate uses `Int64`.
- `Int64`-backed (N is 7 or greater): intermediate uses `Int128`.

Arithmetic between different scales is not allowed; cast to a common scale first:

```iris
var d4: Decimal4 = 1.0000d4;
var d7: Decimal7 = 1.0000000d7;

// Error: cannot add Decimal4 and Decimal7 directly
// var bad = d4 + d7;

var good = (d4 as Decimal7) + d7;
```

## Casts

Decimals can be cast to and from all numeric types using `as`:

```iris
// Integer to Decimal
var i32_to_d4: Decimal4 = i32_value as Decimal4;

// Decimal to Integer (rounds half away from zero)
var d4_to_i32: Int32 = d4_value as Int32;

// Float to Decimal
var f32_to_d4: Decimal4 = f32_value as Decimal4;

// Decimal to Float
var d4_to_f32: Float32 = d4_value as Float32;

// Between decimal scales
var d4_to_d7: Decimal7 = d4_value as Decimal7;
var d7_to_d4: Decimal4 = d7_value as Decimal4;   // possible precision loss
```

### Rounding

When casting a decimal or float to an integer, Iris rounds half away from zero:

- `0.5d2 as Int32` -> `1`
- `-0.5d2 as Int32` -> `-1`

No implicit conversions are ever performed.
