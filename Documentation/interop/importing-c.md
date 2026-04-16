---
sidebar_position: 1
---

# Importing C Libraries

Iris can call any C library without writing wrapper code. The build system uses libclang to parse C headers and turn them into Iris modules that you import normally.

## Importing the C Standard Library

The `C_standard_library` artifact dependency gives you access to some standard C headers:

```json
{
    "dependencies": [
        { "name": "C_standard_library" }
    ]
}
```

Then in your Iris source:

```iris
module Use_printf;

import c.stdio as stdio;

export function run() -> ()
{
    var a = 1;
    stdio.printf("Value: %d, pointer: %p\n"c, a, &a);
}
```

The `"…"c` suffix converts a string literal to `*C_char`, which is what `printf` expects.

## Importing a Custom C Header

Place the C header in your project and add it as a dependency artifact of type `"import_c_header"`:

```json
{
    "name": "Artifact_name",
    "version": "0.1.0",
    "type": "library",
    "sources": [
        {
            "type": "import_c_header",
            "headers": [
                {
                    "name": "c_interface.module_name",
                    "header": "c_interface.h"
                }
            ],
            "public_prefixes": [],
            "remove_prefixes": []
        },
    ]
}
```

Then in any Iris file that depends on `my_c_lib`:

```iris
import c_interface.module_name as my_lib;

export function call_it() -> ()
{
    my_lib.some_c_function();
}
```

## C Type Mapping

When the imported C header uses C types, Iris provides matching built-in types:

| C type | Iris type |
|---|---|
| `char` | `C_char` |
| `short` | `C_short` |
| `int` | `C_int` |
| `long` | `C_long` |
| `long long` | `C_long_long` |
| `unsigned int` | `C_unsigned_int` |
| `bool` / `_Bool` | `C_bool` |
| `void *` | `*mutable Byte` |
