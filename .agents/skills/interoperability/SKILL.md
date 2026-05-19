---
name: 'interoperability'
description: 'Describes how we import C headers as Core Modules and how we export Core Modules as C headers.'
---

## Importing C Headers as Core Modules

We use libclang to parse C headers and convert the declarations to our Core Module representation. This is done in `Source/Interoperability/C_header_importer.cpp`. The tests are in `Source/Interoperability/C_header_importer.tests.cpp`.
The tests CMake target is `Iris_C_header_importer_tests`.

## Exporting Core Modules as C Headers

In `Source/Interoperability/C_header_exporter.cpp`, we read a Core Module and create a C Header. The tests are in `Source/Interoperability/C_header_exporter.tests.cpp`.
The tests CMake target is `Iris_C_header_exporter_tests`.
