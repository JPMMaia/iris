---
slug: /
sidebar_position: 0
---

# Iris Programming Language

Iris is a compiled, statically-typed systems programming language designed to interoperate seamlessly with C and C++. It compiles via LLVM to native code and targets performance-sensitive applications.

## Quick Example

```iris
module hello_world;

import c.stdio as stdio;

@unique_name("main")
export function main() -> (result: Int32)
{
    stdio.puts("Hello world!"c);
    return 0;
}
```

## Features

- Interoperability with C
- Low level memory management
- Function contracts (preconditions and postconditions)
- Decimal type
- Structure of Arrays type
- Templates/generics
- Defer statements
- Some compile time and reflection features
- Native test framework

## Caveats

Iris is still in early development and as such there are many bugs, caveats, and features that are not supported yet:
- Currently, it only supports **x64 windows ABI**. Linux x64 or arm64 ABIs are not supported yet.
- Iris uses a custom build system (`iris_artifact.json`/`iris_repository.json`). Integration with other build systems (e.g. CMake) is not supported yet.
- External library dependencies can be managed with `iris_project.json` (see [Dependencies](tooling/dependencies.md)).
- The syntax and features are changing all the time.
- Almost non-existing Iris standard library.
- Some interactions with the C standard library don't work well yet (e.g. using the `stdout` macro).
- No memory safety features yet.

## Where to Start

| I want to… | Go to… |
|---|---|
| Install Iris and build my first program | [Getting Started → Installation](getting-started/installation.md) |
| Understand the language fundamentals | [Language → Modules](language/modules.md) |
| Call a C library | [Interoperability → Importing C](interop/importing-c.md) |
| Understand the build system | [Tooling → Build System](tooling/build-system.md) |
| Manage external library dependencies | [Tooling → Dependencies](tooling/dependencies.md) |
| Write generic/reusable code | [Generic Programming → Function Constructors](generics/function-constructors.md) |
| Optimise memory layout | [Memory → Structure of Arrays](memory/structure-of-arrays.md) |
