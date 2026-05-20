# Plan: Add `Optional::<T>` Builtin Type

## Design Decisions
- Syntax: `Optional::<T>` (consistent with `Array_slice::<T>`)
- `.has_value` and `.value` are field accesses (not method calls), reusing the Array_slice synthetic-struct pattern
- Single `Optional_type` variant; pointer optimization (`Optional::<*T>` → bare `T*`) handled internally in code gen and C export

## TDD Approach Per Phase
- **Grammar, Convertor, Validation, Compiler, C Export, C Import**: test first → confirm fail → implement → confirm pass
- **Core Module, Core Helpers**: build-verify only; tested indirectly by later phases

---

## Orchestration Model

Each phase is executed by a **subagent**. The **main agent** orchestrates the sequence, reviews each subagent's output, and validates integration before proceeding to the next phase.

### Main Agent Responsibilities
1. Launch subagents in dependency order (see phase dependency graph below)
2. After each subagent completes, **review all changed files** for correctness and consistency with the plan
3. Verify the subagent's test results match expectations (FAIL before impl, PASS after)
4. Run a **build check** after every phase to catch any integration regressions
5. Correct any issues directly before proceeding to the next phase
6. After all phases complete, do a final integration review: build the full project, run all test targets, and confirm no regressions

### Phase Dependency Graph
```
Phase 1 (Grammar) ──────────────────────────────────────┐
Phase 2 (Core Module) ──────────────────────────────────┤
                                                          ▼
                                        Phase 3 (Convertor + Formatter) ──┐
                                        Phase 4 (Core Helpers) ───────────┤
                                                                            ▼
                                                          Phase 5 (Validation) ──┐
                                                                                  ▼
                                                              Phase 6 (Code Generation) ──┐
                                                                                            ▼
                                                                      Phase 7 (C Export) ──┐
                                                                                            ▼
                                                                      Phase 8 (C Import) ──┐
                                                                                            ▼
                                                            Phase 9 (Language Server) ──┐
                                                                                         ▼
                                                                Phase 10 (Documentation)
```
- Phases 1 and 2 may run in parallel (independent)
- Phases 3 and 4 may run in parallel (both depend only on 1 and 2)
- Phases 5–10 are strictly sequential

---

## Phase 1 – Grammar (TDD)

> **Subagent task**: Implement the `Optional_type` grammar rule and its corpus test. Return: list of files changed, test output before and after implementation.

1. **[TEST FIRST]** Add corpus file `Source/Parser/tree-sitter-iris/test/corpus/optional_type.txt` with source using `Optional::<Int32>` and expected S-expression — mirror `array_slices.txt`
2. Run `npm run test` from `Source/Parser/tree-sitter-iris/` → confirm **FAILS**
3. Add grammar rule in `Source/Parser/tree-sitter-iris/grammar.js` (~line 54):
   ```js
   Optional_type: $ => seq("Optional", "::<", $.Type, ">")
   ```
   — mirror `Array_slice_type` rule
4. Regenerate parser; run `npm run test` → confirm **PASSES**

> **Main agent review**: Check that the new grammar node is named `Optional_type` and the S-expression matches the convertor's expected node name. Verify `grammar.js` diff is minimal and consistent with `Array_slice_type`. Build the project to confirm no regressions.

---

## Phase 2 – Core Module Representation (build-only)

> **Subagent task**: Add `Optional_type` to `Core.cppm` and insert it into the `Type_reference` variant. Return: exact lines changed in `Core.cppm`, build output confirming clean compile.

5. Add `Optional_type` struct to `Source/Core/Core.cppm` (before `Parameter_type`):
   ```cpp
   export struct Optional_type
   {
       std::pmr::vector<Type_reference> value_type;

       friend bool operator==(Optional_type const&, Optional_type const&) = default;
   };
   ```
6. Add `Optional_type` to `Type_reference::Data_type` variant between `Null_pointer_type` and `Parameter_type` (line ~232)
7. Build → `Binary_serializer/Generated.cppm` and `JSON_serializer/Generated.cppm` auto-regenerate; confirm clean compile

> **Main agent review**: Confirm `Optional_type` is positioned between `Null_pointer_type` and `Parameter_type` in the variant (maintaining consistent ordering across the codebase). Verify that auto-generated serializer files have been regenerated and that the build is clean with no warnings.

---

## Phase 3 – Convertor + Formatter (TDD) *(depends on 1–2)*

> **Subagent task**: Add the convertor case for `Optional_type` nodes and the formatter case for round-trip output. Return: test file created, convertor and formatter diffs, test output before and after.

8. **[TEST FIRST]** Create `Examples/txt/optional_type.iris` with a minimal function using `Optional::<Int32>`; add `TEST_CASE` in `Source/Parser/Convertor.tests.cpp` calling `test_convertor("optional_type.iris")` — mirrors existing convertor tests
9. Build & run `Iris_parser_tests` → confirm **FAILS**
10. In `Source/Parser/Convertor.cpp` `node_to_type_reference()` (~line 799): add `"Optional_type"` node case reading child[2] as element type — mirror `"Array_slice_type"` case
11. In `Source/Core/Formatter.cpp` `add_format_type_name()` (~line 1669): add `Optional_type` case:
    ```cpp
    else if (std::holds_alternative<Optional_type>(type.data))
    {
        Optional_type const& value = std::get<Optional_type>(type.data);
        add_text(buffer, "Optional::<");
        add_format_type_name(buffer, value.value_type, options);
        add_text(buffer, ">");
    }
    ```
    — mirror `Array_slice_type` / `Constant_array_type` cases; required for round-trip convertor tests
12. Run `Iris_parser_tests` → confirm **PASSES**

> **Main agent review**: Confirm that child index used in the convertor (child[2]) correctly picks up the element type based on the grammar rule `seq("Optional", "::<", $.Type, ">")`. Verify the formatter output exactly matches the input source syntax for the round-trip test. Check the `.iris` example file is a minimal, valid snippet. Build project and run `Iris_parser_tests` to confirm.

---

## Phase 4 – Core Helpers (build-only, *parallel with Phase 3*)

> **Subagent task**: Add `Optional_type` support to `Types.cppm`/`Types.cpp` and `Hash.cpp`. Return: list of functions added/modified with their signatures, build output confirming clean compile.

13. Add to `Source/Core/Types.cppm` / `Source/Core/Types.cpp`:
    - `is_optional_type_reference(Type_reference const&) -> bool`
    - `create_optional_type_reference(Type_reference value_type) -> Type_reference`
    - `create_optional_type_struct_declaration(Type_reference value_type) -> Struct_declaration`
      → returns synthetic `{ T value; bool has_value; }` — mirrors `create_array_slice_type_struct_declaration` (line 642)
14. In `visit_type_references_recursively` in `Source/Core/Types.cppm` (~line 115): add `Optional_type` case iterating over `value_type` — place after `Null_pointer_type`, before `Parameter_type`; mirror `Pointer_type` case:
    ```cpp
    else if (std::holds_alternative<Optional_type>(type_reference.data))
    {
        Optional_type const& data = std::get<Optional_type>(type_reference.data);
        for (Type_reference const& nested : data.value_type)
        {
            if (visit_type_references_recursively(nested, predicate))
                return true;
        }
        return false;
    }
    ```
15. In `replace_parameter_types_by_instance_arguments_impl` in `Source/Core/Types.cpp` (~line 740): add `Optional_type` case iterating over `value_type` — mirror `Array_slice_type` case:
    ```cpp
    else if (std::holds_alternative<Optional_type>(type_reference.data))
    {
        Optional_type& optional_type = std::get<Optional_type>(type_reference.data);
        for (Type_reference& element_type : optional_type.value_type)
        {
            if (!replace_parameter_types_by_instance_arguments_impl(element_type, constructor_parameters, instance_arguments))
                return false;
        }
    }
    ```
16. Update `Source/Core/Hash.cpp` for `Optional_type` hashing if needed (check other variants for precedent)
17. Build → confirm clean compile

> **Main agent review**: Verify that `create_optional_type_struct_declaration` produces member names and types consistent with what Phase 5 (validation) and Phase 6 (code gen) will use (`.has_value: Bool`, `.value: T`). Confirm `visit_type_references_recursively` and `replace_parameter_types_by_instance_arguments_impl` are inserted in the same relative position as other variants. Check `Hash.cpp` for consistency. Build the project.

---

## Phase 5 – Validation (TDD) *(depends on 3–4)*

> **Subagent task**: Add validation tests for all `Optional_type` scenarios and implement the validation logic. Return: list of test cases added, `Validation.cpp` diff, test output before and after.

18. **[TEST FIRST]** Add tests in `Source/Compiler/Validation.tests.cpp` using `test_validate_module()`:
    - Valid: `.has_value` on `Optional::<Int32>` → no errors, result type `Bool`
    - Valid: `.value` on `Optional::<Int32>` → no errors, result type `Int32`
    - Valid: `{}` zero-init of `Optional::<Int32>` → no errors
    - Valid: `create_optional<Int32>()` → `Optional::<Int32>`
    - Valid: `create_optional(1u32)` (type inferred from argument) → `Optional::<Uint32>`
    - Valid: `.has_value` / `.value` on `Optional::<*Int32>`
    - Error: access to unknown member on `Optional`
19. Build & run `Iris_validation_tests` → confirm new tests **FAIL**
20. Implement in `Source/Compiler/Validation.cpp`:
    - Member access `.has_value` / `.value` using `create_optional_type_struct_declaration` synthetic struct (~line 1789, mirror Array_slice block)
    - `{}` zero-init for `Optional_type`
    - Register `"create_optional"` in `is_builtin_function_name()` in `Source/Core/Core.cpp` (~line 668)
    - Dynamic `create_optional` signature dispatch (~line 2405): explicit type-param overload + type-inferred-from-arg overload
21. Run `Iris_validation_tests` → confirm **PASSES**

> **Main agent review**: Check that both `create_optional` overloads (explicit type parameter and type-inferred) are tested and produce the correct return type. Verify the error case for unknown member access produces a clear diagnostic consistent with the Array_slice error messages. Confirm `is_builtin_function_name` and signature dispatch are placed in the same position as other builtins. Run `Iris_validation_tests`.

---

## Phase 6 – Code Generation (TDD) *(depends on 5)*

> **Subagent task**: Implement LLVM IR type mapping and expression code gen for `Optional_type`. Return: LLVM IR test file created, diffs for `Clang_code_generation.cpp` and `Expressions.cpp`, test output before and after.

22. **[TEST FIRST]** Add test in `Source/Compiler/Compiler.tests.cpp` using `test_create_llvm_module("optional_type.iris", ...)` verifying expected LLVM IR for:
    - Non-pointer `Optional::<Int32>` → `{ i32, i1 }` struct
    - Pointer `Optional::<*Int32>` → `ptr`
    - `.has_value` and `.value` access
    - `create_optional()` (empty) and `create_optional(value)`
23. Build & run `Iris_compiler_tests` → confirm **FAILS**
24. `Source/Compiler/Clang_code_generation.cpp` (~line 2714): map `Optional_type`:
    - Non-pointer element: anonymous `{ T; bool }` struct
    - Pointer element: just `T*`
25. `Source/Compiler/Expressions.cpp` (~line 830): handle member access:
    - Non-pointer: use `create_optional_type_struct_declaration` + `create_access_struct_member`
    - Pointer: `.has_value` → `ptr != null`; `.value` → return ptr directly
    - `create_optional()` / `create_optional(value)` code gen
    - `{}` zero-init → `has_value = false` / null ptr
26. Run `Iris_compiler_tests` → confirm **PASSES**

> **Main agent review**: Verify the IR for `Optional::<*T>` is just a `ptr` (no wrapping struct). Confirm `create_optional()` with no arguments emits a zero-initialized value (`{ 0, false }` for non-pointer, `null` for pointer). Confirm `.value` on a pointer optional does not emit a GEP — it returns the pointer directly. Run `Iris_compiler_tests` and the full build.

---

## Phase 7 – C Header Export (TDD) *(depends on 6)*

> **Subagent task**: Implement C header export for `Optional_type` including the IRIS_META `raw_pointer_members=` annotation. Return: test cases added, exporter diff, test output before and after.

27. **[TEST FIRST]** Add `TEST_CASE("Export Optional type")` in `Source/Interoperability/C_header_exporter.tests.cpp`:
    - Non-pointer: struct with `Optional::<Int32>` member → inline named C struct `struct Optional_int32_t { int32_t value; bool has_value; }`
    - Pointer optional: `Optional::<*Entity>` member → emits `Entity*` with **no metadata** (default; importer will reconstruct as `Optional::<*T>`)
    - Raw pointer: `*Entity` member → emits `Entity*` **plus** adds `raw_pointer_members=member_name` to the struct's IRIS_META comment to signal a non-optional pointer, e.g.:
      ```c
      /** IRIS_META v=1 module=my.module name=Node kind=struct raw_pointer_members=next */
      struct my_module_Node {
          struct my_module_Node* parent;   /* Optional::<*Node> — no annotation */
          struct my_module_Node* next;     /* *Node — annotated as raw */
      };
      ```
28. Run `Iris_C_header_exporter_tests` → confirm **FAILS**
29. In `Source/Interoperability/C_header_exporter.cpp`:
    - `add_type_reference_declaration`: recurse into `Optional_type.value_type` (mirror `Constant_array_type`)
    - `write_c_type_name`: non-pointer optional → emit inline named C struct; pointer optional → emit `T*` (no annotation)
    - IRIS_META comment emission: collect member names whose type is a plain `Pointer_type` (raw `*T`) and append `raw_pointer_members=name1 name2` (space-separated) to the comment
30. Run `Iris_C_header_exporter_tests` → confirm **PASSES**

> **Main agent review**: Verify the IRIS_META format is consistent with the existing comment format parsed in Phase 8. Confirm pointer-optional members emit no annotation and raw pointer members are correctly identified and listed. Check `add_type_reference_declaration` recursion doesn't duplicate forward declarations. Run `Iris_C_header_exporter_tests`.

---

## Phase 8 – C Header Import Reconstruction (TDD) *(depends on 7)*

> **Subagent task**: Extend the C header importer to reconstruct `Optional_type` from pointer members, using `raw_pointer_members` IRIS_META annotation to opt out. Return: test cases added, importer diff, test output before and after.

31. **[TEST FIRST]** Add test cases in `Source/Interoperability/C_header_importer.tests.cpp`:
    - Default pointer import: import a C header struct with a `T*` member and **no** IRIS_META annotation; verify the member is reconstructed as `Optional::<*T>` (not plain `Pointer_type`)
    - Raw pointer annotation: import a C header struct with `raw_pointer_members=next` in its IRIS_META comment; verify the annotated member is reconstructed as plain `*T` while unannotated pointer members remain `Optional::<*T>`
32. Run `Iris_C_header_importer_tests` → confirm **FAILS**
33. In `Source/Interoperability/C_header_importer.cpp`:
    - Extend `Iris_meta_comment` struct (~line 120) with `std::pmr::vector<std::pmr::string> raw_pointer_members`
    - Extend `parse_iris_meta_comment` (~line 153) to parse `raw_pointer_members=` field using `find_iris_meta_field`; split by spaces into the vector
    - After building a struct declaration from its Clang AST (~line 1456): for every pointer-typed member, wrap it with `Optional_type` using `create_optional_type_reference` **unless** its name appears in `raw_pointer_members`
34. Run `Iris_C_header_importer_tests` → confirm **PASSES**

> **Main agent review**: Verify the export/import round-trip: run both `Iris_C_header_exporter_tests` and `Iris_C_header_importer_tests`. Confirm that a C header produced by Phase 7 can be re-imported and produce the original `Optional::<*T>` and `*T` types correctly. Check that `raw_pointer_members` parsing handles edge cases (empty list, multiple names, extra whitespace). Run the full build.

---

## Phase 9 – Language Server (manual)

> **Subagent task**: Audit hover type display and completion for `Optional_type` in the Language Server. If gaps are found, implement fixes. Return: list of Language Server files inspected, any changes made, and manual test steps used to verify.

35. Check hover type display and completion for `Optional_type`; add type-display handling in Language Server if needed

> **Main agent review**: Manually verify hover shows `Optional::<Int32>` (not a raw struct name) and that `.has_value` / `.value` appear in completion suggestions. Confirm no regressions in existing hover/completion tests.

---

## Phase 10 – Update Documentation (manual)

> **Subagent task**: Write documentation for `Optional::<T>` in the appropriate `Documentation/` files. Return: list of files created or modified, summary of content added.

36. Add `Optional::<T>` entry to `Documentation/language/` (e.g. a new `optional-type.md` or extend the types reference page):
    - Syntax: `Optional::<T>`
    - Fields: `.has_value: Bool`, `.value: T`
    - Pointer optimization: `Optional::<*T>` is represented as a bare pointer at the ABI level
    - Builtin constructors: `create_optional<T>()` (empty) and `create_optional(value)` (with value)
37. Update `Documentation/interop/` to document the default behaviour (C pointers import as `Optional::<*T>`) and the IRIS_META `raw_pointer_members=` annotation for opting individual members back into plain `*T`
38. Update `Documentation/index.md` (or the sidebar config) to link the new optional-type page if a new file was added

> **Main agent review**: Confirm documentation is consistent with the final implementation (member names, constructor names, ABI behaviour). Verify `raw_pointer_members` annotation format in the interop docs matches the actual IRIS_META format produced by Phase 7. Check that the sidebar or index links are correct.

---

## Final Integration Check (Main Agent)

After all phases pass their individual reviews:

1. Build the full project (`Build Debug`)
2. Run all test targets in order: `npm run test` (grammar), `Iris_parser_tests`, `Iris_validation_tests`, `Iris_compiler_tests`, `Iris_C_header_exporter_tests`, `Iris_C_header_importer_tests`
3. Confirm zero new failures across all targets
4. Review `Examples/txt/optional_type.iris` to ensure it is a good end-to-end demonstration of the feature (non-pointer optional, pointer optional, both constructors, both member accesses)

---

## Key Reference Files

| File | Purpose |
|---|---|
| `Source/Core/Core.cppm` | Add `Optional_type` struct + insert in variant (line ~232) |
| `Source/Parser/tree-sitter-iris/grammar.js` | Grammar rule (~line 54) |
| `Source/Parser/tree-sitter-iris/test/corpus/` | Grammar corpus tests |
| `Source/Parser/Convertor.cpp` | `node_to_type_reference` (~line 799) |
| `Source/Parser/Convertor.tests.cpp` | Convertor round-trip tests |
| `Source/Core/Formatter.cpp` | `add_format_type_name` (~line 1669) |
| `Source/Core/Types.cppm` | `visit_type_references_recursively` (~line 115); helper declarations |
| `Source/Core/Types.cpp` | `create_array_slice_type_struct_declaration` (line 642); `replace_parameter_types_by_instance_arguments_impl` (~line 740) |
| `Source/Core/Core.cpp` | `is_builtin_function_name` (~line 668) |
| `Source/Compiler/Validation.cpp` | Member access (~line 1789); builtin signatures (~line 2405) |
| `Source/Compiler/Validation.tests.cpp` | Validation tests |
| `Source/Compiler/Clang_code_generation.cpp` | Type mapping (~line 2714) |
| `Source/Compiler/Expressions.cpp` | Access + call code gen (~line 830) |
| `Source/Compiler/Compiler.tests.cpp` | LLVM IR tests |
| `Source/Interoperability/C_header_exporter.cpp` | C export; IRIS_META comment with `raw_pointer_members=` for plain `*T` members |
| `Source/Interoperability/C_header_exporter.tests.cpp` | Export tests |
| `Source/Interoperability/C_header_importer.cpp` | `Iris_meta_comment` (~line 120); `parse_iris_meta_comment` (~line 153); struct reconstruction (~line 1456); default-wrap pointers as `Optional::<*T>` |
| `Source/Interoperability/C_header_importer.tests.cpp` | Import tests |

## CMake Targets Per Phase

| Phase | Test Target |
|---|---|
| Grammar | `npm run test` in `Source/Parser/tree-sitter-iris/` |
| Convertor | `Iris_parser_tests` |
| Validation | `Iris_validation_tests` |
| Compiler | `Iris_compiler_tests` |
| C Export | `Iris_C_header_exporter_tests` |
| C Import | `Iris_C_header_importer_tests` |

## Notes

- **Serialization**: `Binary_serializer/Generated.cppm` and `JSON_serializer/Generated.cppm` are auto-regenerated by CMake when `Core.cppm` changes — no manual serialization code needed for `Optional_type`
- **`optional_pointer_members` format**: space-separated member names in the IRIS_META comment; parsed using the existing `find_iris_meta_field` pattern
- **Non-pointer Optional** exports as a named inline C struct; pointer Optional exports as `T*` + metadata — lossless round-trip through C headers
