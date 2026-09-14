---
sidebar_position: 1
---

# Reflection

:::warning[Experimental]
Compile-time constants and reflection expressions are partially implemented. Some features may not yet work in all contexts.

:::

Reflection expressions are evaluated by a source-to-source pass that runs before code generation,
so each one is replaced by a constant, a type, or a member access. Nothing here costs anything at
runtime.

A worked example that uses most of this page together is
`share/iris/libraries/Iris_standard_library/json.iris`, which writes any value as JSON: it walks
struct fields with `@member_count` / `@member_name` / `@member_access`, recurses with
`@member_type`, names enum values with `@enum_count` / `@enum_name` / `@enum_value`, and dispatches
on `@get_type_kind`.

## `@size_of` — Type Size

Returns the size of a type in bytes as a `Uint64` compile-time constant:

```iris
var sz = @size_of::<Int32>();          // 4u64
var sz2 = @size_of::<My_struct>();
```

## `@alignment_of` — Type Alignment

Returns the required alignment of a type in bytes:

```iris
var align = @alignment_of::<Float64>();   // 8u64
```

## `@type_of` — Type of an Expression

Returns the type of a single expression as a type expression.

```iris
function external_function(value: *Int32) -> ()
{
}

function run(pointer: *Int32) -> ()
{
    var typed_pointer = reinterpret_as::<@type_of(external_function)>(pointer);
}
```

Rules:

- `@type_of` takes exactly one expression parameter.
- `@type_of` does not take type arguments.
- `@type_of` is intended for type positions (for example, typed declarations and constructor/type arguments).

## `@member_count` — Struct/Union Member Count

Returns the number of fields in a struct or union:

```iris
var count = @member_count::<My_struct>();
```

The type argument must be a struct or union; the compiler rejects other types.

## `@member_type` — Field Type at Index

Returns the type of the field at `index` (zero-based) in a struct or union:

```iris
// Use the result as a type in generic code
type_constructor nth_field(T: Type, index: Uint64)
{
    return @member_type::<T>(index);
}
```

## `@member_name` — Field Name as C String

Returns the field name at `index` as a `*C_char` compile-time string:

```iris
var name = @member_name::<My_struct>(0u64);
```

## `@member_offset` — Field Bit Offset

Returns the offset **in bits** of the field at `index` within the struct, as a `Uint64`. Divide by
8 for a byte offset:

```iris
struct Pair
{
    first: Int32 = 0;
    second: Int32 = 0;
}

var offset_in_bits = @member_offset::<Pair>(1u64);        // 32u64
var offset_in_bytes = @member_offset::<Pair>(1u64) / 8u64; // 4u64
```

The offset comes from the real target layout, so it accounts for padding and alignment.

## `@member_access` — Field Value at Index

Returns the field at `index` of a *value*, which is what makes it possible to read and write a
struct generically. The first parameter is the value, the second the field index:

```iris
struct Pair
{
    first: Int32 = 0;
    second: Int32 = 0;
}

function read(pair: Pair) -> (result: Int32)
{
    return @member_access::<Pair>(pair, 1u64);    // pair.second
}
```

It expands to an ordinary member access, so it also works as an assignment target:

```iris
function write(pair: *mutable Pair) -> ()
{
    @member_access::<Pair>(pair, 0u64) = 3;       // pair.first = 3
}
```

Combined with `@member_count` and `compile_time for`, this walks every field of a struct:

```iris
compile_time for index in 0u64 to @member_count::<Pair>()
{
    consume(@member_access::<Pair>(pair, index));
}
```

## `@enum_count` — Enum Value Count

Returns the number of values declared by an enum, as a `Uint64`:

```iris
enum Stance
{
    Aggressive = 6,
    Defensive,
    Passive = 20,
}

var count = @enum_count::<Stance>();   // 3u64
```

## `@enum_name` — Enum Value Name as C String

Returns the name of the value at `index` (declaration order, zero-based) as a `*C_char`:

```iris
var name = @enum_name::<Stance>(1u64);   // "Defensive"
```

## `@enum_value` — Enum Value as an Integer

Returns the numeric value at `index` as an `Int32`. Declaration order is not the value: an
implicit value continues from the nearest preceding explicit one, exactly as codegen computes it.

```iris
var a = @enum_value::<Stance>(0u64);   // 6
var b = @enum_value::<Stance>(1u64);   // 7, implicit
var c = @enum_value::<Stance>(2u64);   // 20
```

Together these three describe an enum well enough to map a stored integer back to a name:

```iris
compile_time for index in 0u64 to @enum_count::<Stance>()
{
    if raw_value == @enum_value::<Stance>(index)
    {
        write(@enum_name::<Stance>(index));
    }
}
```

## `@decimal_scale` — Decimal Scale

Returns the scale of a decimal type as a `Uint32` — the number of fractional digits its backing
integer carries:

```iris
var scale = @decimal_scale::<Decimal7>();   // 7u32
```

The type argument must be a decimal type.

## `@type_name` — Type Name as C String

Returns a `*C_char` with the human-readable name of the type:

```iris
var name = @type_name::<Int32>();   // "Int32"
```

## `@get_type_kind` — Type Classification

Returns a `Type_kind` enum value classifying the type:

```iris
var kind = @get_type_kind::<Float32>();   // Type_kind.Float
```

Expected enum values include: `Int`, `Uint`, `Float`, `Struct`, `Union`, `Enum`, `Pointer`, `Array_slice`, `Constant_array`, and others.
