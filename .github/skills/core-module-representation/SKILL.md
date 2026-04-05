---
name: 'core-module-representation'
description: 'Understand the Core Module representation. Use this skill when accessing [Core.cppm](Source/Core/Core.cppm) or if you need to know what a Core Module is.'
---

We are making a programming language. The Core Module representation is where we store the data about a Module.
A Module contains data about declarations (and possibly definitions). Modules can import other Modules too.

Declarations include Functions, Structs, Unions, Enums, Alias, Global Variables, Forward Declarations, Function Constructors, and Type Constructors.

Declarations and Definitions use Statements. A Statement is composed of one or more Expressions.

To represent data types we use Type References.

All these data structures are defined in `Source/Core/Core.cppm`.
