---
sidebar_position: 1
---

# Reflection

:::warning[Experimental]
Compile-time constants and reflection expressions are partially implemented. Some features may not yet work in all contexts.

:::

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

## `@member_offset` — Field Byte Offset

Returns the byte offset of the field at `index` within the struct:

```iris
var offset = @member_offset::<My_struct>(0u64);
```

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
