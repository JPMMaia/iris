---
name: 'edit-conversion-cst-to-module'
agent: 'agent'
description: 'Use this skill when you need edit the conversion from tree-sitter Concrete Syntax Tree (CST) to the Core Module representation'
tools: ['search/codebase', 'edit', 'vscode/askQuestions']
---
We are making a programming language. We probably changed the parser grammar and now your goal is to edit the parser conversion from the CST to the Core Module representation.

## Step-by-step

You need to:
1. Before starting, build the Parser Conversion tests and check that they are green. If they are not green use #tool:vscode/askQuestions to ask how to proceeed or abort.
2. Get context about the grammar we changed, and how we want to map that grammar change to the Core Module representation. Use #tool:vscode/askQuestions to ask any questions or if you don't remember what grammar rule was changed.
3. Add a new test file to `Examples/txt` (or modify an existing one) if needed. Use #tool:vscode/askQuestions if you are unsure.
4. Add a new test case to `Source/Parser/Convertor.tests.cpp` to test the file in case 2.
5. Build the Parser Conversion tests and check that the new test is red
6. Change `Source/Parser/Convertor.cpp` as needed. This is where we convert from CST to the Core Module representation.
7. Change `Source/Core/Formatter.cpp` as needed. This is where we convert from Core Module representation back to text.
8. Build the Parser tests and check that all tests are green. If not, then go back to step 6 and repeat.

## Core Module representation

The Core Module representation is the data structure where we store all the information about the program/module we need to compile. It's located in `Source/Core/Core.cppm`.

## Parser Convertor tests

Build Parser Convertor tests:

```
cmake --build build --target H_parser_tests
```

Run Parser Convertor tests:

```
./build/Source/Parser/H_parser_tests
```
