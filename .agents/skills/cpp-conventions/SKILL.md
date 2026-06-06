---
name: 'cpp-conventions'
description: 'C++ coding conventions for the Iris compiler project: naming, C++20 modules, style rules'
applyTo: '**/*.[cppm|cpp|h|hpp]'
license: MIT
---

# C++ Coding Conventions

## Naming Conventions
- Use `Snake_case` for type names
- Use `scake_case` for variables and function names
- Use `g_` prefix for global variables
- Avoid using abbreviations, prefer descriptive names

## Features
- Avoid using `auto` unless it's an iterator
- Use `nullptr` instead of `NULL` or `0` for pointers
- Prefer using aggregate initialization for structs with designated initializers (e.g. explicitly initialized elements)
  - Place each element on a new line
- Use const for function variables whenever possible
- Use right-side const (e.g. `int const* const pointer`)

## C++ 20 modules

We use C++ 20 modules throughout the codebase. `.cppm` is for module interface files and `.cpp` is for implementation files.
