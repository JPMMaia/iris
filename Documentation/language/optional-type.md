---
sidebar_position: 6
---

# Optional

`Optional::<T>` holds either a value of type `T` or nothing. It is the type to reach for when a
function might not produce a result, in place of returning a zeroed struct, a `null` pointer, or a
separate `-> (success: Bool)` output.

```iris
export function find_index(values: Array_slice::<Int32>, target: Int32) -> (result: Optional::<Uint64>)
{
    for index in 0u64 to values.length
    {
        if values[index] == target
        {
            return create_optional(index);
        }
    }

    return create_optional::<Uint64>();
}
```

## Members

| Member | Type | Meaning |
|---|---|---|
| `.has_value` | `Bool` | whether a value is present |
| `.value` | `T` | the value — valid only when `.has_value` is `true` |

```iris
var found = find_index(values, 42);

if found.has_value
{
    print_integer(found.value);
}
```

Reading `.value` when `.has_value` is `false` is a programming error. In builds with contracts
enabled the compiler emits a check that reports the module and function and aborts; with contracts
disabled the check is not emitted and the read returns unspecified data. `.value` is never
implicitly checked — always test `.has_value` first.

## Construction

```iris
var empty_0: Optional::<Int32> = {};              // empty
var empty_1 = create_optional::<Int32>();          // empty, type given explicitly
var present = create_optional(1);                  // present, type deduced from the argument
```

`{}` is the same empty value a struct member gets by default:

```iris
export struct Config
{
    timeout: Optional::<Int32> = {};
}
```

## Representation

`Optional::<T>` is laid out through the same C ABI machinery as any other struct, so it can cross
the C boundary:

| `T` | Representation |
|---|---|
| a pointer (`*U` or `*mutable U`) | the bare pointer — `.has_value` is a null test, and there is no wrapper |
| anything else | `struct { T value; Bool has_value; }` |

The pointer case costs nothing over a raw pointer: `Optional::<*U>` and `*U` are the same bytes.
That is what lets an existing C API be read as optional without changing its ABI.

## Generic code

`Optional::<T>` works inside a `function_constructor`, where `T` may be a type parameter:

```iris
export function_constructor first(value_type: Type)
{
    return function (values: Array_slice::<value_type>) -> (result: Optional::<value_type>)
    {
        if values.length == 0u64
        {
            return create_optional::<value_type>();
        }

        return create_optional(values[0]);
    };
}
```

## C interoperability

See [Exporting as a C Header](../interop/exporting-as-c-header.md) and
[Importing C Libraries](../interop/importing-c.md) for how `Optional::<T>` is spelled in a C header
and how C pointers are read back as optionals.
