---
sidebar_position: 2
---

# Exporting as a C Header

Iris can generate a C-compatible header file from an Iris module. This lets C and C++ code call Iris functions and use Iris functions and types as if they were regular C declarations.

## Artifact Configuration

Add an `"export_c_header"` source entry to your `iris_artifact.json`, listing the Iris modules you want to export:

```json
{
    "name": "Artifact_name",
    "version": "0.1.0",
    "type": "library",
    "dependencies": [],
    "sources": [
        {
            "type": "iris",
            "include": [ "./**/*.iris" ]
        },
        {
            "type": "export_c_header",
            "include": [ "./module_a.iris" ]
        }
    ]
}
```

The build system reads the listed Iris modules and generates a `.h` file alongside the compiled library.

:::caution TODO
Explain linkage names
:::

:::caution TODO
Explain where headers are exported to
:::

## What Gets Exported

Declarations marked `export` in the listed modules (and all of their private dependencies) are included in the generated header.

**`module_a.iris`:**

```iris
module my_library.module_a;

import my_library.module_b as mb;

export function my_interface(value: mb.My_struct) -> (c: Int32)
{
    return value.a + value.b;
}
```

The generated `module_a.h` will contain a C function declaration for `my_interface`, with `mb.My_struct` translated to its C-compatible struct representation.

## Limitations

- Generics (function/type constructors) are not exported; only concrete instantiations would be.
- Iris-specific features (e.g. named return values, contracts) are dropped in the C header — only the ABI-compatible signature remains.
