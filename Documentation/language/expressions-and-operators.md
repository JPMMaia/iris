---
sidebar_position: 6
---

# Expressions & Operators

## Arithmetic

```iris
var add      = a + b;
var subtract = a - b;
var multiply = a * b;
var divide   = a / b;   // signed or unsigned depending on operand type
var modulus  = a % b;
```

## Comparison

```iris
var eq  = a == b;
var neq = a != b;
var lt  = a < b;
var lte = a <= b;
var gt  = a > b;
var gte = a >= b;
```

## Boolean Logic

```iris
var and_result = first && second;   // logical AND
var or_result  = first || second;   // logical OR
```

## Bitwise

```iris
var band  = a & b;   // AND
var bor   = a | b;   // OR
var bxor  = a ^ b;   // XOR
var shl   = a << b;  // shift left
var shr   = a >> b;  // arithmetic right (signed), logical right (unsigned)
```

## Unary Operators

```iris
var neg   = -a;   // arithmetic negation
var lnot  = !a;   // logical NOT (Bool)
var bnot  = ~a;   // bitwise complement
```

## Compound Assignment

```iris
a += b;
a -= b;
a *= b;
a /= b;
a %= b;
a &= b;
a |= b;
a ^= b;
a <<= b;
a >>= b;
```

## Ternary / Conditional Expression

```iris
var result = condition ? value_if_true : value_if_false;
```

## Operator Precedence

Operators follow standard C-like precedence (highest to lowest):

| Level | Operators |
|---|---|
| Unary | `-` `!` `~` `++` |
| Multiplicative | `*` `/` `%` |
| Additive | `+` `-` |
| Shift | `<<` `>>` |
| Relational | `<` `<=` `>` `>=` |
| Equality | `==` `!=` |
| Bitwise AND | `&` |
| Bitwise XOR | `^` |
| Bitwise OR | `\|` |
| Logical AND | `&&` |
| Logical OR | `\|\|` |
| Ternary | `?:` |
| Assignment | `=` `+=` `-=` … |

When in doubt, use parentheses to make precedence explicit.

## Assertions

`assert` evaluates a boolean expression and aborts (with a message) if it is false:

```iris
assert "Allocation did not fail" { allocation != null };
```

The quoted string is the diagnostic message; the block is the condition. Assertions are checked in debug builds.
