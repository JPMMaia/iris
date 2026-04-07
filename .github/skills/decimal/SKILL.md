---
name: 'decimal'
description: 'Describes Decimals and how they should be implemented.'
---

NOTE: This feature has not been implemented yet.

A DecimalN is a signed fixed point numeric type where N is the exponent of 10 in the divisor:

integer_value / 10^N

For 1<= N <= 6, integer_value is represented using Int32. For N >= 7 it is represented using Int64. The valid range for N is 1 to 18.

Examples:
- Decimal2: Int32 value / 10^2
- Decimal6: Int32 value / 10^6
- Decimal7: Int64 value / 10^7.


## Syntax

The syntax for decimal is simply `DecimalN` where N ranges between 1 and 18.

Examples:
- `var x: Decimal4 = 4.56d4`

### Literals and encoding

Decimal constants can be created using the dN suffix. The fixed point value needs to be encoded as a integer. This is done by multiplying the fixed point value by 10^N.

Examples:
- `0.15d2` represents 15/10^2. Since the backing storage for N=2 is Int32, it should be encoded as a Int32 value 15.
- `1.57d4` represents 1570/10^4 and should be encoded as a Int32 value 1570.
- `123.4567d7` represents 1234567000/10^7 and should be encoded as Int64 value 1234566000.

## Operations

### Arithmetric Operations

Addition and subtraction are the same as integers.
Multiplication is performed by a*b/10^N.
Division is performed by 10^N*a/b

Int64 must be used for multiplication and division intermediate results if the backing storage is Int32. Int128 must be used instead if the backing storage is Int64.

### Casting operations

Decimals can be casted from and to different numeric types (like Float32 or Int64).
Decimals can also be casted between different decimal types (e.g. Decimal3 and Decimal6).
Arithmetric operations between different scales (Decimal3 with Decimal6) is disallowed. Explicit cast is required.
Arithmetric operations between different numeric types (e.g. Int/Float) are disallowed.
No implicit casts, explicit cast is always required.

#### Rounding

When casting Decimal to Integer, round half away from zero: 0.5 rounds to 1, -0.5 to -1.
