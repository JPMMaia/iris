---
sidebar_position: 5
---

# Structs, Enums & Unions

## Structs

Structs group named fields with mandatory default values:

```iris
export struct My_struct
{
    a: Int32 = 1;
    b: Int32 = 2;
}
```

### Instantiation

Use `{}` to create an instance. Omitted fields receive their defaults; supply only the fields you want to override:

```iris
var instance_0: My_struct = {};           // a=1, b=2
var instance_1: My_struct = { b: 3 };    // a=1, b=3
var instance_2: My_struct = explicit { a: 10, b: 11 };  // all fields explicit
```

`explicit {}` requires every field to be named — useful when you want the compiler to catch missing fields.

### Mutation

Struct fields can be mutated on `mutable` instances:

```iris
mutable instance: My_struct = {};
instance.a = 0;
instance = explicit { a: 10, b: 11 };
```

### Nested Structs

```iris
export struct My_struct_2
{
    a: My_struct = {};
    b: My_struct = { a: 2 };
    c: My_struct = { a: 3, b: 4 };
}
```

---

## Enums

Enums declare a set of named integer constants:

```iris
export enum My_enum
{
    Value_0 = 0,
    Value_1,               // implicitly Value_0 + 1 = 1
    Value_2 = Value_1 + 3, // 4
    Value_3 = 1 << 3,      // 8
    Value_10 = 10,
    Value_11,              // 11
}
```

Access values with `My_enum.Value_0`. Use `switch` to branch on enum values:

```iris
switch enum_argument
{
case My_enum.Value_0:
case My_enum.Value_1:
    return 0;
case My_enum.Value_10:
    return 1;
}
```

---

## Unions

A union stores one of several possible fields in the same memory:

```iris
export union My_union
{
    a: Int32;
    b: Float32;
}
```

Initialise by naming the active member:

```iris
var instance_0: My_union = { a: 2 };
var instance_1: My_union = { b: 3.0f32 };
```

Access the active member by its field name. Iris does not automatically track which member is active — use a companion enum tag if you need discriminated unions:

```iris
export enum My_union_tag
{
    a = 0,
    b = 1,
}

// Check the tag before accessing the union field
if my_union_tag == My_union_tag.a
{
    var a = my_union.a;
}
```

---

## Type Aliases

`using` creates a compile-time alias for any existing type:

```iris
using My_int = Int64;

export using My_alias_to_enum = My_enum;
```

