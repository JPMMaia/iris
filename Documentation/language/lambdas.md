---
sidebar_position: 9
---

# Lambdas

A lambda is a value you can call. It is a pair of a function pointer and a `user_data` pointer, which
makes it two words wide and directly compatible with the C convention of passing a callback next to a
context pointer. There is no runtime, no allocation, and no hidden indirection.

## Lambda Types

A callable type is declared at module scope with `lambda`. The syntax is the same as a function
declaration without a body:

```iris
lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

lambda Mapper(value: Int32) -> (result: Int32);

lambda Action() -> ();
```

Like every other declaration, a lambda type is private to its module unless it is exported:

```iris
export lambda Comparator(a: Int32, b: Int32) -> (result: Int32);
```

The name is what makes the type usable across module and language boundaries — as a struct member, a
parameter, a return type, or a C header declaration.

## Lambda Literals

A value of a lambda type is written with the `lambda` keyword, a parameter list, and `=>` followed by
the body. The keyword is always required.

```iris
export function main() -> ()
{
    var cmp: Comparator = lambda(a, b) => a - b;
}
```

The parameter and return types are taken from the type the literal is being used as, so they need not
be repeated. Writing them is allowed:

```iris
var mapper = lambda(x: Int32) -> Int32 => x * 2;
```

The return type accepts both the bare form above and the named form used by functions:

```iris
var mapper = lambda(x: Int32) -> (result: Int32) => x * 2;
```

A lambda needs *some* source of type information. With neither an expected type nor written parameter
types there is nothing to resolve the signature from, and the compiler reports
`Cannot infer lambda type — no expected type available.`:

```iris
var cmp = lambda(a, b) => a - b;    // error: nothing says what a and b are
```

## Body Forms

The body is either a single expression or a block. An expression body is the value the lambda
returns; a block body uses `return` like any other function:

```iris
var cmp: Comparator = lambda(a, b) => a - b;

var cmp2: Comparator = lambda(a, b) => {
    var difference = a - b;
    return difference;
};
```

A `return` inside a block body returns from the lambda, not from the enclosing function.

## Calling

A lambda-typed value is called like a function:

```iris
lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

export function apply(cmp: Comparator, x: Int32, y: Int32) -> (result: Int32)
{
    return cmp(x, y);
}

export function main() -> ()
{
    var cmp: Comparator = lambda(a, b) => a - b;
    var result = apply(cmp, 10, 3);
}
```

## Captures

A lambda body may read variables from the function it was written in. They are captured **by value**,
at the point the lambda is created:

```iris
export function main() -> ()
{
    var offset: Int32 = 10;
    var cmp: Comparator = lambda(a, b) => a - b + offset;
}
```

Captures are collected automatically — there is no capture list to write. Only names that resolve to
variables of the enclosing scope are captured; parameters of the lambda itself, variables declared
inside its body, and module-level declarations are not.

The captured values are stored in an environment allocated in the frame of the enclosing function,
and the lambda's `user_data` points at it. That has one consequence worth knowing:

**A lambda that captures cannot be returned from the function it was written in.** Its environment
would not outlive the frame, so the compiler rejects it:

```iris
export function make_adder() -> (result: Mapper)
{
    var offset: Int32 = 10;
    return lambda(x) => x + offset;    // error: Lambda captures 'offset' from the
}                                      // enclosing function and cannot be returned from it.
```

A lambda that captures nothing carries a null environment and is free to travel anywhere:

```iris
export function create_mapper() -> (mapper: Mapper)
{
    return lambda(x) => x * 2;
}
```

## Nesting

A lambda may be written inside another lambda's body, and the inner one may capture the outer one's
parameters and locals:

```iris
lambda Inner(x: Int32) -> (result: Int32);
lambda Outer(a: Int32, b: Int32) -> (result: Int32);

export function main() -> ()
{
    var outer: Outer = lambda(a, b) => {
        var inner: Inner = lambda(x) => x + a;
        return inner(b);
    };
}
```

## Representation and C Interoperability

A lambda value is exactly:

```c
struct
{
    <return> (*function_pointer)(<parameters>, void* user_data);
    void* user_data;
};
```

Exporting a module that declares a lambda type produces that struct in the generated C header, with
the Iris signature recorded in the accompanying metadata comment:

```c
/** IRIS_META v=1 module=my.namespace name=Comparator kind=lambda data=(a: Int32, b: Int32) -> (result: Int32) */
struct my_namespace_Comparator
{
    int32_t (*function_pointer)(int32_t a, int32_t b, void* user_data);
    void* user_data;
};
```

Importing a C header turns a struct carrying `kind=lambda` back into a lambda declaration. The
metadata is the only criterion: a struct that merely has the same shape stays a struct.

C code calls a lambda by passing the environment back as the last argument:

```c
struct my_namespace_Comparator cmp = /* obtained from Iris */;
int32_t result = cmp.function_pointer(10, 3, cmp.user_data);
```

See [Exporting as a C Header](../interop/exporting-as-c-header.md) and
[Importing C](../interop/importing-c.md).

## How It Is Compiled

A lambda literal is lowered before code generation into ordinary declarations of the module it
appears in:

- an internal function named `<enclosing>__lambda<N>`, whose parameters are the lambda's parameters
  plus a trailing `user_data`, and whose body is the lambda's body prefixed by one variable per
  capture, read out of the environment;
- a `<enclosing>__lambda<N>__environment` struct of the captured variables, when there are any.

Nothing else is generated: a lambda is the same two-word value whether it captures or not, so calling
one is an indirect call with one extra argument, and nothing is allocated on the heap.
