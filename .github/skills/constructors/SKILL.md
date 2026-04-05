---
name: 'constructors'
description: 'Describes Function and Type Constructors and how they should be implemented. Use this skill when you need to know about Function Constructors and Type Constructors and how they are implemented.'
---

# Constructors

In the H programming language we have `function_constructor` and `type_constructor`. These are functions that are called at compile time to create new functions / types.

## Function Constructor

It's a compile-time function that returns a function expression.

```
function_constructor function_constructor_name(arg_type: Type)
{
    return function(a: arg_type, b: arg_type) -> ()
    {
    };
}
```

## Type Constructor

It's a compile-time function that returns an enum, struct or union expression.

```
type_constructor type_construtor_Name(arg_type: Type)
{
    return struct
    {
        a: arg_type = {},
        b: Int64 = 0u64
    };
}
```

## Instantiation

To call a function or type condtructor we use `constructor_name::<...>` where `...` are the constructor arguments (usually types) separated by comma.

### Deducing function constructor arguments

In some cases, we can simply call a function constructor without constructor arguments. In this  case, the constructor arguments need to be deduced from the normal function call arguments.

## Implementation

We should have compilation pass that runs on a Core Module. It should:
1. Gather all the instantiations or uses of function/type constructors.
2. Deduce function constructor arguments of needed (e.g. replace `h::Call_expression` by a `h::Instance_call_expression` with all deduced constructor arguments)
3. Execute constructors to create new functions or types. The functions must have private linkage so that they doesn't clash with other duplicate instantiations from other modules.
5. Run all the passes that apply (e.g. compile-time pass, instantation pass) on the generated function/type.
6. Replace `h::Instance_call_expression` with a corresponding `h::Call_expression`
7. Instantiated functions/types need to be validated

