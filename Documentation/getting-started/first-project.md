---
sidebar_position: 2
---

# First Project

This walkthrough creates a "Hello, World!" executable — the smallest possible Iris project.

## Project Layout

```
hello_world/
├── iris_artifact.json
└── hello_world.iris
```

Every Iris project needs an **artifact file** that describes what to build and a set of Iris source files.

## iris_artifact.json

```json
{
    "name": "Hello_world",
    "version": "0.1.0",
    "type": "executable",
    "dependencies": [
        {
            "name": "C_standard_library"
        }
    ],
    "sources": [
        {
            "type": "iris",
            "include": [
                "./**/*.iris"
            ]
        }
    ],
    "executable": {
        "source": "hello_world.iris"
    }
}
```

Key fields:

| Field | Meaning |
|---|---|
| `type` | `"executable"` — build a runnable binary |
| `dependencies` | Other artifacts required; `C_standard_library` gives access to `c.stdio`, `c.stdlib`, etc. |
| `sources[].include` | Glob patterns selecting which `.iris` files to compile |
| `executable.source` | The entry-point module (must export a function annotated with `@unique_name("main")`) |

## hello_world.iris

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

A few things to notice:

- Every file begins with a **module declaration** — `module hello_world;`
- `import c.stdio as stdio;` imports the C standard I/O library under the local alias `stdio`.
- The `"Hello world!"c` suffix converts the string literal to a `*C_char` — required by C functions like `puts`.
- The return type is declared with `->` and uses **named returns** (`result: Int32`).
- `@unique_name("main")` tells the linker to use the platform entry-point name `main`.

## Build & Run

```powershell
iris hello_world/iris_artifact.json
.\hello_world.exe
```

Expected output:

```
Hello world!
```

## Next Step

[Understand the full project structure →](project-structure.md)
