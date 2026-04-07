---
name: 'core-module-representation'
description: 'Understand the Core Module representation. Use this skill when accessing [Core.cppm](Source/Core/Core.cppm) or if you need to know what a Core Module is.'
---

We are making a programming language. The Core Module representation is where we store the data about a Module.
A Module contains data about declarations (and possibly definitions). Modules can import other Modules too.

Declarations include Functions, Structs, Unions, Enums, Alias, Global Variables, Forward Declarations, Function Constructors, and Type Constructors.

Declarations and Definitions use Statements. A Statement is composed of one or more Expressions.

To represent data types we use Type References.

All these data structures are defined in [Core.cppm](../../../Source/Core/Core.cppm).

## Editing

After editing [Core.cppm](../../../Source/Core/Core.cppm), the [Code Generator tool](../../../Tools/code_generator) will generate the [JSON_serializer Generated.cppm](../../../Source/JSON_serializer/Generated.cppm) and [Binary_serializer Generated.cppm](../../../Source/Binary_serializer/Generated.cppm). We cache some files for testing that require the exact version of the serializers, so we need to remove some cached files in the build folder: [C_standard_library](../../../build/C_standard_library) and [Temp](../../../build/Temp).