---
name: 'add-language-feature'
description: 'Describes the general workflow for adding new features. Use this skill when planning how to edit or add a new language feature.'
---

# Build

Use the [windows-build](../windows-build/SKILL.md) skill to know how to build and use CMake in a PowerShell started with Enter-VsDevEnv.

# Steps

1. Grammar changes

If needed, use the [edit-grammar-rules](../edit-grammar-rules/SKILL.md) skill to add a new grammar rule.
Validate by running tree-sitter tests.
Use the #askQuestions tool to validate the changes with the user.

2. Core Module Representation changes

Use the [core-module-representation](../core-module-representation/SKILL.md) skill to edit the Core Module representation.
Emsire that it builds successfully.
Use the #askQuestions tool to validate the changes with the user.

3. Edit Conversion from tree-sitter CST to Module

Use the [edit-conversion-cst-to-module](../edit-conversion-cst-to-module/SKILL.md) skill to take in the changes from step 1 and store the data in the structures edited in step 2.
Ensure that everything builds correctly and tests are green before proceeding.
Use the #askQuestions tool to validate the changes with the user.

4. Validation changes

Use the [validation](../validation/SKILL.md) skill to edit the validation.
Ensure that it builds successfully and tests pass before proceeding.
Use the #askQuestions tool to validate the changes with the user.

5. Core helpers

Check if [iris.core.types](../../../Source/Core/Types.cppm), [iris.core.declarations](../../../Source/Core/Declarations.cppm), and [iris.core.hash](../../../Source/Core/Hash.cpp) need to be updated.
If so, and after edits, ensure that it builds correctly.
Use the #askQuestions tool to validate the changes with the user.

6. Compiler changes

Use the [compiler](../compiler/SKILL.md) to make changes if needed.
Check if [iris.compiler.clang_code_generation](../../../Source/Compiler/Clang_code_generation.cpp) needs to be updated.
Check if we need to update [iris.compiler.expressions](../../../Source/Compiler/Expressions.cpp).
Check if we need to update any of [passes](../../../Source/Compiler/passes/).
Check if we need to update `get_expression_type_info` in [iris.compiler.analysis](../../../Source/Compiler/Analysis.cpp).
After all this, build and test.
Use the #askQuestions tool to validate the changes with the user.

7. Debug Info

If debug info changes are needed check [iris.compiler.types](../../../Source/Compiler/Types.cpp) and the [natvis visualizers](../../../share/iris/visualizers).
If natvis is added, add the natvis linker flag in [Linker_coff.cpp](../../../Source/Compiler/Linker_coff.cpp).
After all this, build and test.
Use the #askQuestions tool to validate the changes with the user.

8. Interoperabily

Check if the feature requires changes to import or export C headers.
Use the [interoperability](../interoperability/SKILL.md) skill if needed.
Build and run the tests before proceeding.
Use the #askQuestions tool to validate the changes with the user.

9. Language Server support

Use the [language-server](../language-server/SKILL.md) skill if the feature requires changes to the language server.
