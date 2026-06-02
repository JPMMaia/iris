# Lambda Feature Implementation Plan

## Overview

Lambdas in Iris are represented as a **struct with two fields**: a function pointer and user data (for captures). This makes them fully C ABI compatible, supports capturing, and enables easy C header generation.

```cpp
struct Lambda_T {
    function_pointer: function<(Args...) -> (Return_type)> = null;
    user_data: *mutable Void = null;
}
```

---

## Step 1: Grammar Changes (tree-sitter)

**Files to edit:**
- `Source/Parser/tree-sitter-iris/grammar.js`
- `Source/Parser/tree-sitter-iris/test/corpus/` (new test file)

### 1a. Lambda Type Declaration (Named Lambda Interface)

Add a new `Lambda` declaration rule alongside `Function`, `Struct`, etc.:

```javascript
Lambda: $ => seq("lambda", $.Lambda_name, $.Lambda_input_parameters, "->", $.Lambda_output_parameters),
Lambda_name: $ => $.Identifier,
Lambda_input_parameters: $ => seq("(", optional(seq($.Function_parameter, repeat(seq(",", $.Function_parameter)))), ")"),
Lambda_output_parameters: $ => seq("(", optional(seq($.Function_parameter, repeat(seq(",", $.Function_parameter)))), ")"),
```

### 1b. Lambda Literal Expression

Add a new `Expression_lambda` rule to `Generic_expression` in `Statement` and in the expression choice:

```javascript
Expression_lambda: $ => prec(100, seq(
    "lambda",
    "(", optional(seq($.Lambda_literal_parameter, repeat(seq(",", $.Lambda_literal_parameter)))), ")",
    optional(seq("->", $.Type)),  // optional explicit return type
    "=>",
    choice($.Expression_block, $.Generic_expression)
)),
Lambda_literal_parameter: $ => seq($.Identifier, optional(seq(":", $.Type))),
```

**Note:** The `lambda` keyword prefix eliminates all grammar conflicts — no `conflicts` declarations are needed. The `prec(100)` is only needed to resolve the residual ambiguity with ternary expressions inside lambda bodies (`lambda() => x ? y : z`), ensuring the ternary is consumed as part of the lambda body.

### 1c. Test file

Create `Source/Parser/tree-sitter-iris/test/corpus/lambdas.txt` with test cases for:
- Named lambda type declaration
- Lambda literal with inline body
- Lambda literal with block body
- Lambda literal with explicit return type
- Lambda with captures

**Validation:** Run `npm run test_tree_sitter_update` in `Source/Parser/tree-sitter-iris/`

---

## Step 2: Core Module Representation Changes

**Files to edit:**
- `Source/Core/Core.cppm`

### 2a. New Type Reference Variant — `Lambda_type`

Add a new variant to `Type_reference::Data_type`. This represents an **anonymous** lambda type (signature only, no name):

```cpp
export struct Lambda_type
{
    std::pmr::vector<Type_reference> input_parameter_types;
    std::pmr::vector<Type_reference> output_parameter_types;

    friend bool operator==(Lambda_type const&, Lambda_type const&) = default;
};
```

Add `Lambda_type` to the `Type_reference::Data_type` variant.

**Note:** `Lambda_declaration` (step 2b) handles **named** lambda types. Named types are referenced via `Custom_type_reference` pointing to the declaration. Anonymous lambda literals use `Lambda_type` with just the signature.

### 2b. New Declaration — `Lambda_declaration`

Add to `Module_declarations`:

```cpp
export struct Lambda_declaration
{
    std::pmr::string name;
    std::optional<std::pmr::string> unique_name;
    std::pmr::vector<Type_reference> input_parameter_types;
    std::pmr::vector<Type_reference> output_parameter_types;
    std::pmr::vector<std::pmr::string> input_parameter_names;
    std::pmr::vector<std::pmr::string> output_parameter_names;
    std::optional<std::pmr::string> comment;
    std::optional<Source_range_location> source_location;

    friend bool operator==(Lambda_declaration const&, Lambda_declaration const&) = default;
};
```

Add `std::pmr::vector<Lambda_declaration> lambda_declarations;` to `Module_declarations`.

### 2c. New Expression — `Lambda_expression`

Add to `Expression::Data_type`:

```cpp
export struct Lambda_expression
{
    std::pmr::vector<std::pmr::string> parameter_names;
    std::pmr::vector<std::optional<Type_reference>> parameter_types;  // explicit types; nullopt = inferred
    std::optional<Type_reference> return_type;  // explicit return type; nullopt = inferred
    Statement body;  // either an inline expression or a block
    std::optional<Source_range> source_range;

    friend bool operator==(Lambda_expression const&, Lambda_expression const&) = default;
};
```

### 2d. Helper function

Add `find_lambda_declaration()` to `Core.cppm` / `Core.cpp`, similar to existing `find_*_declaration` helpers.

**Build step:** Run `cmake --build build` to regenerate serializers. Remove cached files in `build/C_standard_library` and `build/Temp`.

---

## Step 3: Parser Conversion (CST → Core Module)

**Files to edit:**
- `Source/Parser/Convertor.cpp`
- `Source/Core/Formatter.cpp`

### 3a. Convert Lambda declaration

Implement `node_to_lambda_declaration()` — parses the tree-sitter `Lambda` node into an `iris::Lambda_declaration`.

### 3b. Convert Lambda literal expression

Implement `node_to_expression_lambda()` — parses the tree-sitter `Expression_lambda` node into an `iris::Lambda_expression`. This must:
- Collect parameter names and optional explicit types (`std::nullopt` if not written)
- Collect optional explicit return type (`std::nullopt` if not written)
- Store the body (inline expression or block)
- **Capture analysis**: Walk the body AST to identify which outer-scope variables are referenced, and record them

### 3c. Add to module declarations

In the declaration conversion logic, add a case for `Lambda` nodes and append to `internal_declarations.lambda_declarations`.

### 3d. Formatter updates

Update `Source/Core/Formatter.cpp` to format `Lambda_type`, `Lambda_declaration`, and `Lambda_expression` (including explicit return type when present).

**Build & test:** Build `Iris_parser_tests`, add test cases to `Convertor.tests.cpp`, verify green.

---

## Step 4: Type Analysis

**Files to edit:**
- `Source/Compiler/Analysis.cpp` (`get_expression_type_info`)

### 4a. Type analysis for Lambda_expression

Update `get_expression_type_info` to recognize `Lambda_expression` and return the appropriate lambda type:
- **Explicit parameter types**: If `parameter_types[i]` has a value, use that as the parameter type
- **Infer remaining parameter types**: For entries with `nullopt`, extract the parameter types from the expected `Lambda_type` (e.g., the function parameter type)
- **Explicit return type**: If `return_type` has a value, use that as the return type
- **Infer return type**: If `return_type` is `nullopt`, analyze the body expression to determine the return type
- Return a `Lambda_type` variant with the resolved signatures
- **Error case**: If no expected type is available (e.g., assigned to an untyped `var`) and no explicit types are provided for any parameter, emit a diagnostic error: "Cannot infer lambda type — no expected type available"

### 4b. Type analysis for Lambda_declaration

When a `Lambda_declaration` is encountered, ensure its type can be resolved to a proper `Lambda_type` that can be used as a parameter type.

### 4c. Capture analysis

Walk lambda literal bodies to identify which outer-scope variables are referenced. This information is needed for both validation and code generation.

### 4d. Test file — `Analysis.tests.cpp`

Create `Source/Compiler/Analysis.tests.cpp` with tests similar to `Validation.tests.cpp`, but verifying **expected expression types** instead of diagnostics.

**Pattern:**
1. Define an input Iris source string
2. Parse and run type analysis via `process_module()`
3. Assert that specific expressions resolve to the expected `Type_reference` (e.g., `Lambda_type` with resolved input/output types)

**Test cases to include:**
- Lambda literal with explicit parameter types and explicit return type — verify resolved `Lambda_type` matches
- Lambda literal with explicit return type but inferred parameter types — verify parameter types are resolved from expected type
- Lambda literal with no explicit types, assigned to a named lambda type — verify all types inferred from expected type
- Lambda literal with no explicit types, no expected type (assigned to untyped `var`) — verify diagnostic error: "Cannot infer lambda type — no expected type available"
- Lambda literal with partial explicit types (some parameters explicit, some inferred) — verify correct resolution
- Lambda literal with inferred return type from body expression — verify return type matches body expression type
- Lambda literal with explicit return type that mismatches body expression — verify diagnostic error
- Lambda literal with captures — verify captured variables are recorded and the lambda type is correct
- Named lambda type declaration — verify it resolves to the correct `Lambda_type` signature

---

## Step 5: Validation

**Files to edit:**
- `Source/Compiler/Validation.cpp`
- `Source/Compiler/Validation.tests.cpp`
- `Source/Compiler/Diagnostic.cppm` (if new diagnostics needed)

### 5a. Validate Lambda declarations

- Ensure lambda parameter types are valid
- Ensure output types are valid
- Ensure no duplicate lambda names in the same scope

### 5b. Validate Lambda expressions

- Ensure explicit parameter types are valid types
- Ensure explicit parameter types match the expected lambda type (when available)
- Ensure explicit return type is a valid type
- Ensure explicit return type matches the expected lambda type's return type (when available)
- Ensure captured variables actually exist in enclosing scope
- Ensure the body's inferred return type is consistent with the expected (or explicit) return type
- **Error**: Emit diagnostic if lambda literal cannot infer any types (no expected type available and no explicit parameter types)

### 5c. Validation tests

Add tests to `Validation.tests.cpp` with `[Validation]` tag covering:
- Valid lambda declarations
- Valid lambda literals
- Invalid: capturing non-existent variable
- Invalid: type mismatch between lambda literal and named lambda type

**Build & test:** `cmake --build build --target Iris_compiler_tests`, run `[Validation]` tests.

---

## Step 6: Core Helpers

**Files to check/edit:**
- `Source/Core/Types.cpp` — add helper to resolve `Lambda_type` to its underlying struct representation
- `Source/Core/Declarations.cpp` — add lookup helpers for lambda declarations
- `Source/Core/Hash.cpp` — add hash support for `Lambda_type` and `Lambda_declaration`

---

## Step 7: Compiler Changes (Lowering to LLVM IR)

**Files to edit:**
- `Source/Compiler/Compiler.cpp` — struct type generation, function declaration registration
- `Source/Compiler/Expressions.cpp` — lambda literal lowering, lambda call lowering
- `Source/Compiler/Types.cpp` — `Lambda_type` → LLVM struct type conversion
- `Source/Compiler/Clang_code_generation.cpp` — `convert_type` for lambda struct types
- `Source/Compiler/Debug_info.cpp` — debug info for lambda structs and functions

### 7a. Lambda type → Struct generation (Compiler.cpp + Types.cpp + Clang_code_generation.cpp)

When a `Lambda_declaration` is encountered during module declaration processing, generate an **internal LLVM struct type** with two fields:

```llvm
%struct.LambdaName = type { <function_ptr_type>*, ptr }
```

Where `<function_ptr_type>` is the LLVM function type matching the lambda's signature (input parameter types → output parameter types).

**Implementation details:**

1. **In `Compiler.cpp` — `add_module_declarations()`**: Add a new case for `Lambda_declaration` in the declaration processing loop (similar to how `Function_declaration` and `Global_variable_declaration` are handled). For each `Lambda_declaration`:
   - Create the lambda struct type via `convert_lambda_type_to_llvm_struct()`
   - Register it in the `type_database.name_to_llvm_type` map under `core_module.name` with the lambda's name
   - Create an `llvm::StructType` declaration in the LLVM module (forward declaration, no definition yet — definition comes from the struct type)

2. **In `Types.cpp` — `type_reference_to_llvm_type()`**: Add a case for `Lambda_type`:
   - Look up the lambda's struct type in the `type_database` (via `Custom_type_reference` → `Lambda_declaration`)
   - If the lambda is anonymous (no named declaration), create an anonymous struct type on the fly: `llvm::StructType::create(llvm_context, {function_ptr_type, ptr_type}, "__lambda_anonymous")`
   - The `function_ptr_type` is `llvm::PointerType::get(llvm_function_type, 0)` where `llvm_function_type` is built from the lambda's input/output parameter types

3. **In `Clang_code_generation.cpp` — `convert_type()`**: Add handling for lambda struct types:
   - Given a `Lambda_declaration`, build the LLVM function type from its `input_parameter_types` and `output_parameter_types`
   - Create the struct type: `llvm::StructType::get(llvm_context, {function_ptr_type, ptr_type})`
   - Set the struct name to `mangle_struct_name(core_module, lambda_declaration.name)` for debug info consistency

**Naming convention:**
- Lambda struct type: `%struct.LambdaName` (mangled via `mangle_struct_name()`)
- The struct is registered in `type_database.name_to_llvm_type[core_module.name]["LambdaName"]`

**Example LLVM IR output for `lambda Comparator(a: Int32, b: Int32) -> (result: Int32)`:**

```llvm
%struct.Comparator = type { ptr, ptr }
```

Where the first `ptr` points to the function type `ptr(i32, i32, ptr)` and the second `ptr` is the user_data (void*).

### 7b. Lambda function generation (Compiler.cpp + Expressions.cpp)

Each lambda literal gets its own **internal static function** that implements the lambda body.

**Implementation details in `Expressions.cpp` — `create_lambda_expression_value()`:**

When a `Lambda_expression` is encountered during expression lowering:

1. **Determine capture set**: Walk the lambda body's AST to identify which outer-scope variables are referenced. For each captured variable, record:
   - Variable name
   - Variable type
   - Whether it's captured by reference or by value (determined from the variable's mutability and the lambda's capture semantics)

2. **Generate capture environment struct** (only if captures exist):
   ```llvm
   %struct.__lambda_env0 = type { <type0>, <type1>, ... }
   ```
   - Create the struct type and register it in the LLVM module
   - Field order matches the order of captured variables (deterministic, e.g., sorted by declaration order)
   - Register in `type_database` under the current module's name

3. **Generate the lambda function**:
   - **Name**: `@LambdaName_lambda` or `@__lambdaN_module` (use a unique counter per module, e.g., `__lambda0_Comparator`, `__lambda1_Comparator`)
   - **Signature**:
     - **With captures**: `<return_type> @__lambdaN(<param_types...>, ptr %user_data)`
       - Inside: cast `%user_data` to `*%struct.__lambda_envN`, load captured variables from `env->field`
     - **Without captures**: `<return_type> @__lambdaN(<param_types...>)`
       - No user_data parameter, captured variables are accessed directly from enclosing scope (or are constants)
   - **Body**: Emit the lambda body's LLVM IR using the existing `create_statement_values()` mechanism, but with a new `Expression_parameters` context where:
     - `local_variables` includes the captured variables (loaded from the environment struct for captures-by-reference, or stored directly for captures-by-value)
     - `function_declaration` is set to a synthetic function declaration for debug info
   - **Linkage**: `internal` (static) linkage — the function is only visible within the module
   - Register the function in the LLVM module's function list

4. **Create the lambda value** (the struct containing function pointer + user data):
   - **With captures**:
     ```llvm
     ; Allocate environment on stack
     %env = alloca %struct.__lambda_env0
     ; Store captured values into environment
     store <captured_value_0>, ptr %env
     store <captured_value_1>, ptr getelementptr(%struct.__lambda_env0, ptr %env, i32 0, i32 1)
     ; Create lambda struct
     %lambda_val = insertvalue { ptr, ptr } undef, ptr @__lambda0, i32 0
     %lambda_val = insertvalue { ptr, ptr } %lambda_val, ptr %env, i32 1
     ```
   - **Without captures**:
     ```llvm
     %lambda_val = insertvalue { ptr, ptr } undef, ptr @__lambda0, i32 0
     %lambda_val = insertvalue { ptr, ptr } %lambda_val, ptr null, i32 1
     ```
   - Return `{ .value = %lambda_val, .type = Lambda_type resolved type }`

**Lambda function naming pattern:**
```
__lambda{counter}_{module_name}
```
For example: `__lambda0_Test`, `__lambda1_Test`
The counter is a module-level static counter in `Expressions.cpp` to ensure uniqueness.

### 7c. Lambda call lowering (Expressions.cpp)

When a lambda-typed variable is called (e.g., `cmp(a, b)` where `cmp` is a lambda), lower it as a direct call through the function pointer stored in the lambda struct.

**Implementation in `create_call_expression_value()` or a new `create_lambda_call_expression_value()`:**

When the expression being called has a `Lambda_type` (or a `Custom_type_reference` pointing to a `Lambda_declaration`):

1. **Load the lambda struct** (if the lambda value is on the stack):
   ```llvm
   %lambda_struct = load { ptr, ptr }, ptr %lambda_var
   ```

2. **Extract the function pointer** (first field):
   ```llvm
   %fn_ptr = extractvalue { ptr, ptr } %lambda_struct, 0
   ```

3. **Extract the user data** (second field):
   ```llvm
   %user_data = extractvalue { ptr, ptr } %lambda_struct, 1
   ```

4. **Prepare arguments**: For each argument expression:
   - Evaluate the argument expression with the expected type set to the corresponding input parameter type
   - Store in `llvm_arguments[i]`

5. **Emit the call**:
   ```llvm
   ; With user_data (all lambdas have user_data parameter in their function signature)
   %result = call <return_type> %fn_ptr(<args...>, %user_data)
   ```

6. **Handle return type**: If the lambda has output parameters (tuple return), the result is already a struct; otherwise it's a single value.

**Key consideration**: The function pointer type in the lambda struct must match the actual function signature (including the user_data parameter). When creating the lambda function in 7b, the function type must be:
```
<return_type> (<param_types...>, ptr user_data)
```
So the function pointer stored in the struct is compatible.

**Example LLVM IR for calling a lambda:**

```llvm
; Assume cmp is stored in %cmp_var as { ptr, ptr }
%cmp_struct = load { ptr, ptr }, ptr %cmp_var
%fn = extractvalue { ptr, ptr } %cmp_struct, 0
%ud = extractvalue { ptr, ptr } %cmp_struct, 1
%result = call i32 %fn(i32 %a, i32 %b, ptr %ud)
```

### 7d. Anonymous lambda type support (Types.cpp)

When a lambda literal is used without a named `Lambda_declaration` (anonymous lambda), the type system must still produce a valid LLVM type.

**In `type_reference_to_llvm_type()`**: Add a case for `Lambda_type`:
- If the `Lambda_type` has a corresponding named declaration in the `type_database`, use that struct type
- Otherwise, create an anonymous struct type: `llvm::StructType::get(llvm_context, {function_ptr_type, ptr_type})`
- The `function_ptr_type` is built from the lambda's resolved input/output parameter types

**In `type_database` registration**: Lambda struct types are registered during `add_module_declarations()` for named lambdas. Anonymous lambdas get their struct type created on-demand during expression lowering.

### 7e. Debug info for lambdas (Debug_info.cpp)

1. **Lambda struct debug type**: Create a `DICompositeType` for each lambda struct with:
   - Name: `"lambda <name>"` or `"__lambda_envN"` for capture environments
   - Members: `function_pointer` (function pointer type) and `user_data` (void*)

2. **Lambda function debug type**: For each generated lambda function:
   - Create a `DISubprogram` with the lambda's source location
   - Parameter types include input parameters + user_data
   - Link to the parent function's debug scope (for proper nesting in debuggers)

3. **Capture environment debug type**: For each `__lambda_envN` struct:
   - Create a `DICompositeType` with member names matching the captured variable names
   - This enables debuggers to show captured variable values when inspecting lambda environments

### 7f. Compiler tests

Add LLVM-IR tests in `Examples/txt/` and corresponding `TEST_CASE` entries in `Compiler.tests.cpp`.

**Test file pattern:**
Create `.iris` files in `Examples/txt/` and reference them in `Compiler.tests.cpp`:

```cpp
TEST_CASE("Compile Lambda No Captures", "[LLVM_IR][Lambda]")
{
    char const* const input_file = "lambda_no_captures.iris";
    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map{};
    char const* const expected_llvm_ir = R"(
%struct.Comparator = type { ptr, ptr }

; Function Attrs: convergent
define internal i32 @__lambda0_Test(i32 noundef %0, i32 noundef %1, ptr noundef %2) #0 {
entry:
  %a = alloca i32, align 4
  %b = alloca i32, align 4
  store i32 %0, ptr %a, align 4
  store i32 %1, ptr %b, align 4
  %0 = load i32, ptr %a, align 4
  %1 = load i32, ptr %b, align 4
  %sub = sub nsw i32 %0, %1
  ret i32 %sub
}

; Function Attrs: convergent
define void @Test_main() #0 {
entry:
  %cmp = alloca { ptr, ptr }, align 8
  %0 = insertvalue { ptr, ptr } undef, ptr @__lambda0_Test, i32 0
  %1 = insertvalue { ptr, ptr } %0, ptr null, i32 1
  store { ptr, ptr } %1, ptr %cmp, align 8
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
}
```

**Test cases to add:**

| Test file | Description | What to verify |
|-----------|-------------|----------------|
| `lambda_no_captures.iris` | Lambda literal with no captures, passed to a function | Lambda function is `internal`, struct has `{ ptr, null }`, call extracts function pointer |
| `lambda_with_captures.iris` | Lambda literal capturing outer variables | Capture environment struct is created, captured values are stored into env, lambda function accesses env |
| `lambda_call.iris` | Calling a lambda-typed function parameter | Function pointer is extracted from struct, call is emitted with user_data |
| `lambda_named_type.iris` | Named `Lambda_declaration` used as a type | Struct type is registered, variable of lambda type is allocated, lambda value is constructed |
| `lambda_mixed.iris` | Combination: named lambda type, lambda literal with captures, and lambda call | All features work together in one module |
| `lambda_return_lambda.iris` | Function returns a lambda literal | Lambda struct is created and returned from function |
| `lambda_multiple.iris` | Multiple lambda literals in the same function | Each gets a unique name (`__lambda0`, `__lambda1`, etc.) |
| `lambda_nested.iris` | Nested lambda literals (lambda inside lambda) | Outer and inner lambdas each have their own env struct and function |

**Iris source for `lambda_no_captures.iris`:**
```iris
module Lambda_no_captures;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

export function main() -> ()
{
    var cmp: Comparator = (a, b) => a - b;
}
```

**Iris source for `lambda_with_captures.iris`:**
```iris
module Lambda_with_captures;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

export function main() -> ()
{
    var offset: Int32 = 10;
    var cmp: Comparator = (a, b) => a - b + offset;
}
```

**Iris source for `lambda_call.iris`:**
```iris
module Lambda_call;

lambda Comparator(a: Int32, b: Int32) -> (result: Int32);

export function apply(cmp: Comparator, x: Int32, y: Int32) -> (result: Int32)
{
    return cmp(x, y);
}

export function main() -> ()
{
    var cmp: Comparator = lambda(a, b) => a - b;
    var result = apply(cmp, 10, 3);
}
```

**Build & test:** `cmake --build build --target Iris_compiler_tests`, run `[LLVM_IR][Lambda]` tests.

---

## Step 8: Debug Info (Natvis)

**Files to check/edit:**
- `Source/Compiler/Types.cpp` — ensure lambda struct types get proper debug info
- `share/iris/visualizers/` — optionally add natvis visualizer for lambda structs
- `Source/Compiler/Linker_coff.cpp` — if new natvis file is added, update the linker flag

---

## Step 9: Interoperability (C Header Generation)

**Files to check/edit:**
- C header generation code (likely in `Source/Compiler/` or a dedicated header generator)
- C header import code (likely in `Source/Compiler/` or `Source/Core/`)

### 9a. Export — Lambda to C Header

When generating C headers, for each `Lambda_declaration`:

```c
struct Comparator {
    int32_t (*function_pointer)(int32_t a, int32_t b, void* user_data);
    void* user_data;
};
```

Ensure that functions accepting lambda types generate proper C function signatures:

```c
void filter(int32_t* values, uint64_t count, Comparator predicate);
```

**Test:** Add a compiler test with a module containing a `Lambda_declaration` and a function that uses it. Verify the generated C header contains the correct struct definition with the `IRIS_META` metadata comment.

### 9b. Export — Metadata Annotation

Add metadata to the generated C struct so the import path can recognize it as a lambda, using the same `IRIS_META` format as other declarations:

```c
/** IRIS_META v=1 module=my.namespace name=Comparator kind=lambda data=(a: Int32, b: Int32) -> (result: Int32) */
struct Comparator {
    int32_t (*function_pointer)(int32_t a, int32_t b, void* user_data);
    void* user_data;
};
```

**Test:** Verify the generated C header contains the `IRIS_META` comment with the correct module name, lambda name, `kind=lambda`, and the `data=` field containing the signature (input params, arrow, output params). Test with multiple lambda declarations having different signatures.

### 9c. Import — C Header to Lambda

When importing a C header, scan for structs that match the lambda pattern:

1. **Detect lambda struct**: A struct with exactly two fields:
   - Field 1: a function pointer type (pointer to function with `user_data` as the last parameter)
   - Field 2: a `void*` field (the user data)
2. **Parse metadata**: Extract the lambda signature from the `IRIS_META` comment (`kind=lambda`)
3. **Create `Lambda_declaration`**: Generate an `iris::Lambda_declaration` with:
   - Name from the metadata (`name=` field)
   - Input/output parameter types from the `data=` field (parse the signature)
   - Parameter names from the function pointer types
4. **Add to internal declarations**: Register the recovered lambda in `internal_declarations.lambda_declarations`

**Test:** Create a C header file with a lambda struct (with `IRIS_META` comment containing `kind=lambda`) and import it. Verify the recovered `Lambda_declaration` has the correct name, parameter types, parameter names, and return type.

### 9d. Import — Strict Metadata Requirement

If no `IRIS_META` comment with `kind=lambda` is found, the struct is treated as a **regular struct** — no lambda declaration is created. The metadata is the sole criterion for recognizing a lambda.

**Test:** Create a C header file with a struct that has the lambda pattern but **without** the `IRIS_META` lambda comment:
```c
struct Comparator {
    int32_t (*function_pointer)(int32_t a, int32_t b, void* user_data);
    void* user_data;
};
```
Import the header and verify:
- No `Lambda_declaration` is created
- The struct is imported as a regular struct

---

## Step 10: Language Server Support

**Files to check/edit:**
- `Source/Language_server/Inlay_hints.cpp` — main inlay hints logic
- `Source/Language_server/Go_to_location.cpp` — go-to-definition
- `Source/Language_server/Completion.cpp` — completion
- `Source/Language_server/Server.cpp` — inlay hint provider registration
- `Source/Language_server/Signature_help.cpp` — signature help
- `Source/Language_server/CMakeLists.txt` — add new test files

**Test files:**
- `Source/Language_server/Inlay_hints.tests.cpp` — new file for inlay hint tests
- `Source/Language_server/Go_to_location.tests.cpp` — new file for go-to-definition tests
- `Source/Language_server/Completion.tests.cpp` — new file for completion tests
- `Source/Language_server/Signature_help.tests.cpp` — existing file, add lambda tests
- `Source/Language_server/Hover.tests.cpp` — new file for hover tests

**Test setup pattern** (following `Signature_help.tests.cpp`):
1. Use the `$CURSOR_POSITION` marker in source strings to indicate cursor location
2. Use the `Parse_session` helper struct to parse source and build modules
3. Use `extract_cursor_position()` to convert marker to `lsp::Position`
4. Use `create_declaration_database()` to build the declaration database
5. Call the function under test directly (e.g., `compute_signature_help`)
6. Use Catch2 macros (`TEST_CASE`, `REQUIRE`, `CHECK`) for assertions
7. Add `[Language_server][Lambda]` tag to all lambda tests

### 10z. Test Infrastructure — New Test Files

Create the following new test files following the `Signature_help.tests.cpp` pattern:

- `Source/Language_server/Inlay_hints.tests.cpp` — tests for `create_lambda_inlay_hints()`
- `Source/Language_server/Go_to_location.tests.cpp` — tests for `compute_go_to_definition()` with lambdas
- `Source/Language_server/Completion.tests.cpp` — tests for lambda-related completions
- `Source/Language_server/Signature_help.tests.cpp` — add lambda literal signature help tests
- `Source/Language_server/Hover.tests.cpp` — tests for lambda hover information

Add the new test files to `Source/Language_server/CMakeLists.txt`.

### 10a. Inlay Hints — Parameter Types

Add a new function `create_lambda_inlay_hints()` in `Inlay_hints.cpp` that:

1. **Walks the function body** using `visit_statements_using_scope` (same pattern as `create_function_inlay_hints`)
2. **Detects `Lambda_expression` nodes** by checking `std::holds_alternative<iris::Lambda_expression>(expression.data)`
3. **For each lambda literal**, computes the resolved types (using the type analysis from Step 4) and emits inlay hints:
   - After each parameter name, insert `: <type>` (e.g., `lambda(a, b) => ...` → show `lambda(a: Int32, b: Int32) => ...`)
   - Only show hints for parameters where `parameter_types[i]` is `nullopt` (skip explicit ones)
   - Position: right after the parameter identifier, before `)` or `,`

**Test:** In `Inlay_hints.tests.cpp`, add a test case with a named lambda type and a lambda literal passed to a function expecting it. Verify that `create_lambda_inlay_hints()` returns inlay hints with the correct positions and types for each parameter.

### 10b. Inlay Hints — Return Type

In the same `create_lambda_inlay_hints()` function:

1. Before the `=>` token of each lambda literal, insert a return type hint `-> <type>`
2. Only show if `return_type` is `nullopt` (skip explicit ones)
3. Position: Right after the input parameters `)` and right before `=>`

**Test:** Add two test cases: (1) lambda literal without explicit return type — verify `-> Int32` hint appears after `)`, (2) lambda literal with explicit return type — verify no hint is shown.

### 10c. Go-to-Definition for Lambda Declarations

In `Go_to_location.cpp`:

1. Add a case in `visit_type_references` (or the function body visitor) to handle `Custom_type_reference` pointing to a `Lambda_declaration`
2. When the user clicks on a lambda type name (e.g., `Comparator` in `cmp: Comparator`), navigate to the `Lambda` declaration
3. Reuse the existing `create_result_from_declaration` pattern

**Test:** In `Go_to_location.tests.cpp`, add a test case with a named lambda type used as a function parameter. Verify `compute_go_to_definition()` navigates to the `Lambda` declaration location when the cursor is on the type name.

### 10d. Go-to-Definition for Lambda Literals

When the cursor is on a lambda literal expression:

1. Navigate to the corresponding `Lambda_declaration` if the lambda type is a named type
2. If the lambda literal has no matching named type (anonymous), navigate to the generated lambda function declaration (e.g., `__lambda1`)

**Test:** Add test case with a lambda literal used as a function argument. Verify `compute_go_to_definition()` navigates to the matching `Lambda_declaration` (or generated function for anonymous lambdas).

### 10e. Signature Help

When typing a lambda literal and the user presses `(`:

1. Show the signature of the expected lambda type (from the function parameter type)
2. Highlight the current parameter being typed
3. Show return type in the signature help tooltip

**Test:** In `Signature_help.tests.cpp`, add a test case with a lambda literal being typed. Verify `compute_signature_help()` shows the expected lambda type signature with the active parameter highlighted.

### 10f. Hover Information

When hovering over a lambda literal or lambda type reference:

1. **On lambda type reference** (e.g., `Comparator`): show the full lambda signature `lambda Comparator(a: Int32, b: Int32) -> (result: Int32)`
2. **On lambda literal** (e.g., `lambda(a, b) => a - b`): show the resolved signature with inferred types `lambda (a: Int32, b: Int32) -> (result: Int32)`
3. **On captured variables**: show which variables are captured and their types

**Test:** In `Hover.tests.cpp`, add test cases for: (1) hovering over a lambda type reference — verify full signature is shown, (2) hovering over a lambda literal — verify resolved signature with inferred types, (3) hovering over captured variables — verify capture info is shown.

### 10i. Integration with Server

In `Server.cpp`'s `compute_document_inlay_hints()`:

```cpp
// After the existing function inlay hints loop:
auto const process_lambda_declaration = [&](iris::Lambda_declaration const& lambda_declaration) -> void {
    // Find all lambda literals in the module that use this lambda type
    // and add inlay hints for them
};

for (iris::Lambda_declaration const& lambda_declaration : core_module.internal_declarations.lambda_declarations)
    process_lambda_declaration(lambda_declaration);
```

**Test:** Build `Iris_language_server_tests` and run `[Language_server][Lambda]` tests to verify all lambda-related inlay hints, go-to-definition, completion, signature help, and hover work correctly across multiple functions and modules.

---

## Step 11: Documentation

**Files to update:**
- `Documentation/` — add a new section or file for Lambdas
- Update any relevant examples in `Examples/`

---

## Implementation Order Summary

| Step | Component | Key Files |
|------|-----------|-----------|
| 1 | Grammar | `grammar.js`, tree-sitter tests |
| 2 | Core Module | `Core.cppm`, `Core.cpp` |
| 3 | Parser Conversion | `Convertor.cpp`, `Formatter.cpp` |
| 4 | Type Analysis | `Analysis.cpp` |
| 5 | Validation | `Validation.cpp`, `Validation.tests.cpp` |
| 6 | Core Helpers | `Types.cpp`, `Declarations.cpp`, `Hash.cpp` |
| 7 | Compiler | `Compiler.cpp`, `Expressions.cpp`, `Types.cpp`, `Clang_code_generation.cpp`, `Debug_info.cpp` |
| 8 | Debug Info | `Types.cpp`, natvis, `Linker_coff.cpp` |
| 9 | Interoperability | C header generation, C header import, export/import tests |
| 10 | Language Server | `Inlay_hints.cpp`, `Go_to_location.cpp`, `Completion.cpp`, `Signature_help.cpp`, `Server.cpp`, new `*.tests.cpp` files |
| 11 | Docs | `Documentation/` |

Each step should be built and tested before proceeding to the next, as described in the `add-language-feature` skill.
