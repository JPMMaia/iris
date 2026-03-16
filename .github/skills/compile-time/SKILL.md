---
name: 'compile-time'
description: 'Describes Compile-Time and Reflection expressions and how these features should be implemented. Use this skill when you need to understand how Compile-Time and Reflections are implemented.'
---

# Compile Time and Reflection Expresssions

NOTE: This feature has not been fully implemented yet.

Compile time expressions are expressions that are processed at compile time at the Core Module level, before the Core Module is lowered to LLVM IR. These include compile time if, compile time for and also reflection expressions.

## Compile Time expressions

### Compile Time If

```
compile_time if <condition>
{
    ...
}
else
{
    ...
}
```

- Represented as a `h::If_expression` inside a `h::Compile_time_expression`.
- The Compile Time pass must evaluate the conditions and replace this compile time expression by the statements of the taken branch.

### Compile Time For Loop

```
compile_time for index in 0u64 to 4u64
{
    ...
}
```

- Represented as a `h::For_loop_expression` inside a `h::Compile_time_expression`.
- The Compile Time pass must evaluate the loop range and then unroll the loop. For each iteration, the index `h::Variable_expression` must be replaced by a `h::Constant_expression`.

## Reflection expressions
:
### `size_of`

`@size_of::<T>()`

- Computes the size of the type T
- Returns a Uint64 value
- `@size_of::<Int32>` is represented as `h::Reflection_expression` and must be replaced by a `h::Constant_expression` that contains 4 as Uint64

### `alignment_of`

`@alignment_of::<T>()`

- Computes the alignment of the type T
- Returns a Uint64 value

### `type_name`

`@type_name::<T>()`

- Returns a C string with the name of the type
- Uses the formatter
- The compile time pass should replace this by a `h::Constant_expression` with the value of the formatted type

### `member_count`

`@member_count::<T>()`

- The compile time pass should replace this by a `h::Constant_expression` with the member count value 
- Validation should check that T is a struct/union type

### `member_type`

`@member_type::<T>(index)`

- The compile time pass should replace this by a `h::Type_expression` that contains the member of T at `index`
- Validation should check that T is a struct/union type and that index is less than the member count

### `member_offset`

`@member_offset::<T>(index)`

 - The compile time pass should replace this by a `h::Constant_expression` with the value of the member offset.
- Validation should check that T is a struct/union type and that index is less than the member count


### `member_name`

`@member_name::<T>(index)`

- The compile time pass should replace this by a `h::Constant_expression` with the C-string value of the member name.
- Validation should check that T is a struct/union type and that index is less than the member count

### `get_type_kind`

`@get_type_kind::<T>()`

- The compile time pass should replace this by a `h::Constant_expression` with the an enum value that represents the type of the type T.
- This enum is named `Type_kind` and should be part of the Bultin module. Some enum values it should contain are `Int`, `Uint`, `Float`, `Struct`, `Union`, `Enum`, `Pointer`, `Array_slice`, `Constant_array`, etc.

## Tests

Build the H_compiler_tests CMake target. Then run the tests using the `[Compile_time_pass]` Catch2 tag.
The tests are located in [Compile_time_pass.tests.cpp](../../../Source/Compiler/passes/Compile_time_pass.tests.cpp).
