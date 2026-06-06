---
sidebar_position: 1
---

# Modules

Every Iris source file belongs to exactly one module. Modules are the unit of compilation, encapsulation, and code re-use.

## Module Declaration

The first statement in every `.iris` file must be a module declaration:

```iris
module my_module;
```

Module names can contain dots to form a hierarchy:

```iris
module my_library.utils;
```

## Exporting Symbols

By default all declarations are **private** — visible only within the current module. Add `export` to make a declaration part of the public API:

```iris
module MC;

// Public — accessible from outside
export struct Struct_c
{
    value: Private_struct_c = {};
}

// Private — not visible outside this module
struct Private_struct_c
{
    value: Int32 = 0;
}
```

## Importing Modules

Use `import` to bring another module into scope under a local alias:

```iris
module MB;

import MC as mc;

export struct Struct_b
{
    c: mc.Struct_c = {};
}
```

Access imported functions/types with `alias.name` syntax.

## Re-exporting with `using`

A module can re-export a type alias from a dependency, making it directly available to importers of the re-exporting module:

```iris
module MB;

import MC as mc;

export using Alias_b = mc.Alias_to_struct_c;
```

Callers that import `MB` can now use `mb.Alias_b` without needing to import `MC` themselves.

## Multi-Module Example

The following three modules form a chain — `MA` → `MB` → `MC`:

**MC** (base layer):
```iris
module MC;

export using Alias_to_struct_c = Struct_c;

export struct Struct_c
{
    value: Private_struct_c = {};
}

struct Private_struct_c
{
    value: Int32 = 0;
}
```

**MB** (middle layer):
```iris
module MB;

import MC as mc;

export using Alias_b = mc.Alias_to_struct_c;

export struct Struct_b
{
    c: mc.Struct_c = {};
}
```

**MA** (top layer):
```iris
module MA;

import MB as mb;

export using Alias_a = mb.Alias_b;

export struct Struct_a
{
    b: mb.Struct_b = {};
}
```

`MA` can use `mb.Struct_b` and all re-exported aliases without importing `MC` directly.

## Visibility Summary

| Declaration | Visible outside module? |
|---|---|
| `export struct Foo` | Yes |
| `export function bar()` | Yes |
| `export using MyType = …` | Yes |
| `struct Foo` (no `export`) | No |
| `function bar() -> ()` (no `export`) | No |
