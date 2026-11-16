# Pipeline Feature Design

## Overview

Pipelines give Iris map/filter-style composition that compiles to exactly the loop you would write by hand.

```
function run(list: Array_slice::<Complex>, threshold: Float32) -> ()
{
    pipeline process(input: Complex)
    {
        map square(input) => v0;
        filter v0.real > threshold;
    }

    for v in process(list)
    {
        print_complex(v);
    }
}
```

lowers to:

```
for $pipeline0_i in 0u64 to list.length
{
    var input = list[$pipeline0_i];
    var v0 = square(input);
    if v0.real > threshold
    {
        var v = v0;
        print_complex(v);
    }
}
```

### Core ideas

1. **A pipeline is never a runtime value.** It is a compile-time recipe that is expanded inline where it is consumed by a **sink**. It has no type you can store, pass or return.
2. **The sink decides where results go.** No sink allocates. Output goes into a loop body, a caller-provided buffer, a scalar, the input itself, or a stack array whose size is known at compile time. Heap collection stays explicit: `for v in p(list) { push_back(&array, v); }`.
3. **Stages are inlined expressions, not lambdas.** Lambdas (see `lambdas_description.md`) carry a `{function_pointer, user_data}` pair and a capture struct. Stages carry neither, so a pipeline costs nothing over the hand-written loop.
4. **Pipelines are function-local.** They are declared as statements inside function bodies and read the enclosing function's locals directly. There are no module-level pipelines yet, so there is no export, no cross-module use and no binary serialization.

---

## Step 1: Syntax

### 1a. Declaration

```
pipeline <name>(<parameter>: <Type>, ...)
{
    <stage>;
    ...
}
```

Stages (v1):

| Stage | Form | Meaning |
|---|---|---|
| map | `map <expression> => <name>;` | Evaluates the expression and binds the result to `<name>`. |
| filter | `filter <expression>;` | Skips the element unless the expression is `Bool` `true`. |

Scoping:
- Every parameter and every earlier `map` binding is in scope for later stages.
- Locals of the enclosing function that are declared before the `pipeline` statement are in scope too, read at the point of use.
- The **output element** is the last `map` binding. With no `map`, it is the single parameter. A pipeline with several parameters and no `map` is an error, because it has no single output element.

Later stages: `take_while <expression>;` (stops the whole loop) and `enumerate => <name>;` (binds the original input index).

### 1b. Multiple parameters (lockstep / zip)

With several parameters, each one is fed by its own input, and all inputs are read at the same index in one loop:

```
pipeline weighted(value: Complex, weight: Float32)
{
    map scale(value, weight) => v0;
}

for v in weighted(values, weights) { ... }
```

lowers to:

```
assert "Pipeline inputs have equal length" { weights.length == values.length };
for $pipeline0_i in 0u64 to values.length
{
    var value = values[$pipeline0_i];
    var weight = weights[$pipeline0_i];
    ...
}
```

- All inputs are bound before any stage runs. That is why there is no mid-pipeline `zip` stage: after a `filter`, "the same position" would be ambiguous.
- Inputs must have equal length. An inline `assert` checks this.

### 1c. Inputs

| Input type | v1 | Notes |
|---|---|---|
| `Array_slice::<T>` | yes | Length from `.length`. |
| `Constant_array::<T, N>` | yes | Length is the constant `N`. Enables the `into_fixed` sink. |
| `Fixed_capacity_array::<T, N>` | yes | Length is its fill count, `.length`. Enables the `into_fixed` sink, so its results chain. |
| `Soa_array_view::<T>` | yes (phase 4) | The parameter is bound per field: the expansion reads only the fields the stages use, so unused columns are never loaded. |

An input's element type must equal the matching parameter type.

### 1d. Sinks

| Sink | Syntax | Result | Memory |
|---|---|---|---|
| for-loop | `for v in p(inputs) { ... }` | — | None. Stages are fused into the loop body. |
| into | `p(inputs).into(out)` | `Array_slice::<mutable U>`, the written prefix of `out` | Caller-provided `out: Array_slice::<mutable U>` |
| count | `p(inputs).count()` | `Uint64` | None |
| any / all | `p(inputs).any()` / `.all()` | `Bool` | None. The output element must be `Bool`. Short-circuits with `break`. |
| first | `p(inputs).first()` | `Optional::<U>` (see `optional_type.md`) | None |
| fold | `p(inputs).fold(init, lambda (acc, v) => expression)` or `p(inputs).fold(init, f)` | type of `init` | None |
| in place | `p.in_place(list)` | `Array_slice::<mutable T>`, the kept prefix | Overwrites `list` |
| into fixed | `p(inputs).into_fixed()` | `Fixed_capacity_array::<U, N>`, where every input is a `Constant_array::<_, N>` or a `Fixed_capacity_array::<_, N>` | Stack |
| separated into | `p(inputs).separate([scratch]).into(out)` | as `into` | `out`, plus `scratch` when a stage produces a type other than `U` (see Step 5) |

Bounds:
- `into`: `map` preserves the element count and `filter` only removes elements, so `out.length >= input length` is always sufficient. The expansion asserts it. A later `into_bounded(out)` can stop when `out` is full and return `{ written, consumed }` for chunked streaming.
- `in place`: requires a single parameter, an output element type equal to the input element type, and a mutable slice. The write index never passes the read index, so overwriting is safe.

### 1e. Grammar notes

**Files to edit:**
- `Source/Parser/tree-sitter-iris/grammar.js`
- `Source/Parser/tree-sitter-iris/test/corpus/pipelines.txt` (new)

- `pipeline` becomes a statement-level rule, and it needs to be added to the reserved keywords in `Source/Compiler/Validation.cpp` (`get_reserved_keywords`).
- The map binding uses `=>`, not `->`: `->` is also the pointer member-access operator and binds tighter than binary operators, so `map value * weight -> product;` would parse as `value * (weight->product)`.
- `map` and `filter` are only keywords at the start of a statement inside a pipeline body. They **must not** become global reserved words, because `filter` and `map` are common function names (`dynamic_array.iris`-style code would break). This must be verified in tree-sitter, which extracts keywords globally by default. If it causes conflicts, fall back to `@map` / `@filter`.
- `for <name> in <expression>` (with no `to`) is a new for-loop form. It is only valid when the expression is a pipeline call.
- Sink calls (`.into(out)`, `.count()`, ...) parse as ordinary instance calls on a call expression. The pipeline pass recognizes them by name.

---

## Step 2: Core representation

### 2a. Pipeline nodes

**Files to edit:** `Source/Core/Core.cppm`, `Source/Core/Expressions.cpp`, `Source/Core/Formatter.cpp`, `Source/Core/Hash.cpp`, `Source/Core/Expressions_visitor.cppm`, and the parser converter.

```cpp
export enum class Pipeline_stage_kind { Map, Filter };

export struct Pipeline_stage
{
    Pipeline_stage_kind kind;
    Statement expression;
    std::optional<std::pmr::string> binding_name; // Map only
    std::optional<Source_range> source_range;
};

export struct Pipeline_expression
{
    std::pmr::string name;
    std::pmr::vector<std::pmr::string> input_parameter_names;   // mirrors Function_declaration
    std::pmr::vector<Type_reference> input_parameter_types;
    std::pmr::vector<Pipeline_stage> stages;
};

// for <variable_name> in <range_expression>
export struct For_each_expression
{
    std::pmr::string variable_name;
    Expression_index range_expression;
    std::pmr::vector<Statement> then_statements;
};
```

### 2b. Internal statement-block expression

A new expression that only compiler passes create; the parser never produces it:

```cpp
// Runs `statements` in a new scope, then evaluates `result` as the value of the whole expression.
export struct Statement_block_expression
{
    std::pmr::vector<Statement> statements;
    Statement result;
};
```

- **Codegen** (`Source/Compiler/Expressions.cpp`): open a scope, emit `statements` into the current basic block, then return the value of `result`. Debug-info lexical blocks follow `Block_expression`.
- **Analysis**: its type is the type of `result`.
- **Validation**: no user rules apply, because only passes generate it. A debug assert checks that the statements contain no `return` and no `break` / `continue` aimed at a loop outside the block.
- **Formatter**: prints `${ ...; yield <result>; }` for compiler dumps and tests only.

This is the first step toward user-facing value blocks (see Future work).

### 2c. Pipeline calls

Pipeline uses (`p(list)`, `p(list).into(out)`) remain `Call_expression` / `Instance_call_expression`. Analysis and the pass recognize them by resolving the callee name to a `Pipeline_expression` in scope.

---

## Step 3: Analysis and validation

**Files to edit:** `Source/Compiler/Analysis.cpp`, `Source/Compiler/Validation.cpp`, and the matching `.tests.cpp`.

Analysis computes the type of each `map` binding in order and the output element type. Pipeline calls resolve to that internal information, not to a real type.

Validation errors:
- A `filter` expression is not `Bool`.
- A `map` binding shadows a parameter or an earlier binding.
- A pipeline call is used without a sink (`var x = p(list);`, or passed as an argument).
- `into_fixed` is used with an input whose size is not known at compile time, or with inputs of different sizes.
- An input element type does not match its parameter type.
- An in-place sink is used with several parameters, mismatched types or an immutable slice.
- `any` / `all` are used when the output element is not `Bool`.
- A pipeline is referenced before its declaration or outside the function that declares it.

---

## Step 4: Lowering — `Pipeline_pass`

**Files to add/edit:** `Source/Compiler/Passes/Pipeline_pass.{cppm,cpp,tests.cpp}`, `Source/Compiler/Passes/All_passes.cpp`, `Source/Compiler/CMakeLists.txt`.

The pass runs first in `run_all_passes_on_module` / `run_all_passes_on_function`, before the compile-time and instantiate passes. After expansion a pipeline is ordinary code, so the instantiate pass then handles `function_constructor` calls in stages and the lambda pass handles lambdas in stages, without either having to understand pipelines. It rewrites into constructs that already exist (`For_loop_expression`, `If_expression`, `Variable_declaration_expression`, `Assignment_expression`, `Assert_expression`, `Break_expression`), plus the internal `Statement_block_expression` from 2b. The only codegen change is that node. There are no debug info or Clang interop changes beyond it. The `Pipeline_expression` statement itself is then removed.

### 4a. For-loop sink

The `For_each_expression` is replaced in place by a `For_loop_expression` over `0u64 to length`. Its body binds the inputs, emits each `map` as a `var`, and wraps the remainder in an `if` for each `filter`. The innermost body binds the loop variable to the output element and then holds the user's statements.

User `break` / `continue` inside the body are fine, because the expansion is a single loop. Nested `if`s do not count as loop levels for `break`'s `loop_count`.

### 4b. Value sinks: statement-block expressions

Value sinks expand into a `Statement_block_expression` (see 2b), an internal expression that runs a list of statements and then evaluates a final result expression. The expansion stays exactly where the sink was written:

```
var written = process(list).into(out);
```

becomes (written in pseudo-syntax; the node has no user-facing syntax)

```
var written = ${
    assert "Pipeline output buffer is large enough" { out.length >= list.length };
    mutable $pipeline0_count = 0u64;
    for $pipeline0_i in 0u64 to list.length
    {
        var input = list[$pipeline0_i];
        var v0 = square(input);
        if v0.real > threshold
        {
            out[$pipeline0_count] = v0;
            $pipeline0_count += 1u64;
        }
    }
    yield create_array_slice_from_pointer(out.data, $pipeline0_count);
};
```

Because nothing is moved, value sinks are valid in **any expression position** with the expected semantics:
- `while process(list).any() { ... }` re-runs the pipeline on every condition check.
- `a && process(list).any()` only runs the pipeline when `a` is true.
- Sinks nested in call arguments are evaluated in normal argument order.

Inputs and `out` are evaluated once each. When they are not plain variables, the expansion first binds them to generated `var`s at the top of the block, so side effects run exactly once and in source order.

### 4c. Generated names

Generated locals (`$pipeline<N>_i`, `$pipeline<N>_count`, `$pipeline<N>_input<K>`) use a character the grammar's `Identifier` (`/[a-zA-Z_][a-zA-Z_0-9]*/`) cannot produce, so they can never collide with or shadow user names. `N` is a per-function counter, following `get_lambda_function_name` in `Lambda_pass.cpp`. Debug info shows these names as-is, which is acceptable.

Stage bindings (`v0`) and parameter names (`input`) keep their source names, so they show up in the debugger. Validation (Step 3) ensures they do not shadow anything.

---

## Step 5: `separate`

`p(list).separate().into(out)` and `p(list).separate(scratch).into(out)` produce one loop per stage, which helps vectorization when one stage is expensive. Iris has no named arguments, so the scratch buffer is the argument of `separate` itself.

It is a linear chain:
- The first loop reads the inputs and runs the first stage. Each later stage sees only the element the stage before it produced (the latest map binding, or the parameter before any map), plus the enclosing function's locals. Reading any other parameter or binding is an error.
- `enumerate` is rejected. With several inputs, the first stage must be a `map`.
- Between loops, each element lives at its index in a buffer chosen by its type: the output type `U` in `out`, and any other type in `scratch`. So the chain may produce at most one type other than `U`. `scratch` is required exactly when it produces one, and must be `Array_slice::<mutable T>` of that type.
- `map` writes the next buffer at the same index (in place when the type stays the same). `filter` compacts its buffer in place with a `kept` counter. `take_while` sets the count to the failing index and breaks. A `$pipeline<N>_count` tracks the live elements, and the result is `out`'s first `count` elements.
- `out` and `scratch` are asserted to be at least as long as the first input ("Pipeline scratch buffer is large enough").
- Only `into` supports `separate`, and `separate` must be followed by a sink.

```
var written = ${
    assert "Pipeline output buffer is large enough" { output.length >= values.length };
    assert "Pipeline scratch buffer is large enough" { scratch.length >= values.length };
    mutable $pipeline0_count = values.length;
    for $pipeline0_i in 0u64 to values.length { var value = values[$pipeline0_i]; var doubled = value * 2; scratch[$pipeline0_i] = doubled; }
    mutable $pipeline0_kept1 = 0u64;
    for $pipeline0_i1 in 0u64 to $pipeline0_count { var doubled = scratch[$pipeline0_i1]; if doubled > threshold { scratch[$pipeline0_kept1] = doubled; $pipeline0_kept1 += 1u64; } }
    $pipeline0_count = $pipeline0_kept1;
    for $pipeline0_i2 in 0u64 to $pipeline0_count { var doubled = scratch[$pipeline0_i2]; var result = doubled as Float32; output[$pipeline0_i2] = result; }
    yield create_array_slice_from_pointer(output.data, $pipeline0_count);
};
```

---

## Phasing

1. Grammar, core representation, analysis and validation, plus the for-loop sink with `map` / `filter` and single or multiple parameters.
2. `Statement_block_expression` (core, analysis, codegen), then the `into` sink with its inline assert, `count`, `any`, `all`, `fold`, `first`.
3. In place, then the `Fixed_capacity_array` builtin type and the `into_fixed` sink.
4. `Soa_array_view` inputs.
5. `separate`, `take_while`, `enumerate`.

## Implementation status

Phase 1 is implemented (grammar, core nodes, converter, formatter, analysis, validation, `Pipeline_pass` for the for-loop sink with `map` / `filter` and one or several inputs).

Phase 2 is implemented: `Statement_block_expression` (core, analysis, codegen, formatter) and the `count`, `any`, `all`, `into`, `fold` and `first` value sinks. They work in any expression position: runtime tests cover a `while` condition (re-run on every check), the right of `&&` (short-circuited) and call arguments (evaluated in order). The equal-length and `into` capacity asserts are covered by `Examples/Pipeline_asserts`, run one test at a time because an abort ends the run.

Phase 3 is implemented: the `in_place` sink, the `Fixed_capacity_array::<T, N>` builtin type and the `into_fixed` sink (see "Fixed_capacity_array and into_fixed" below).

Phase 4 is implemented: `Soa_array_view::<T>` inputs (mutable or not), alone or in lockstep with other inputs.

Phase 5 is implemented: the `take_while` and `enumerate` stages, and `separate` (see Step 5).

Pipelines can be declared and consumed inside lambda bodies, and `fold` also takes a function name or a lambda value.

Decisions taken while implementing:
- Map syntax is `map <expression> => <name>;` (see 1e).
- Pipelines inside lambda bodies:
  - `Pipeline_pass` walks with `visit_statements_using_scope_including_lambda_bodies`, which moved from the language server to `iris.compiler.validation` and is shared by both. It enters each lambda body with the lambda's parameters in scope, so a pipeline is expanded inside the lambda before the lambda pass turns the lambda into a function.
  - A lambda that consumes a pipeline declared outside it reads, once expanded, whatever that pipeline's stages read. So capture analysis (`compute_lambda_captures`, which now takes the enclosing `scope.pipelines`) adds the pipeline's free names (`get_pipeline_free_names`) when the body names an enclosing pipeline. This is computed before expansion, so the "cannot return a capturing lambda" check sees those captures too.
  - Locals cannot shadow, but lambda parameters can. `Scope::pipelines` holds `Scope_pipeline { pipeline, visible_variable_count }`, and a sink or for-each is an error ("Pipeline 'p' reads 't', which is hidden here by a variable of the same name.") when a name the stages read resolves to a variable declared after the pipeline.
- `fold(init, f)` with anything but a lambda literal is called, not inlined: `mutable $pipeline<N>_accumulator = init;` and `$pipeline<N>_accumulator = f($pipeline<N>_accumulator, <output>);` per element. A variable or member-access path (`add`, `Math.add`) is called as written. Any other expression (`make_adder()`) is bound once to `$pipeline<N>_function`. Validation requires a function or lambda type `(type of init, U) -> type of init`. At O2 a named function inlines to the hand-written loop (`sum_of_odd_cubes` in the zero-cost test).
- `separate` is recognized by `get_pipeline_sink_call` as `p(inputs).separate([scratch]).<sink>(...)`. `Pipeline_sink_call::separate` holds the `separate` call and its scratch argument, and `pipeline_call` stays `p(inputs)`. `get_separated_pipeline_layout` decides which buffer holds each stage's element, and both validation and the pass use it. The bare `.separate(...)` call is validated by the sink called on it.
- `For_each_expression` and `Pipeline_expression` are appended at the end of the `Expression` variant instead of sorted in, because expression hashes (instance names) and binary serialization use the variant index.
- `Scope` gained a `pipelines` list. Analysis, validation and `visit_statements_using_scope` push a pipeline when its statement is reached and drop it at the end of the enclosing block.
- The loop variable may reuse the output binding's name (`for v0 in p(list)`), in which case no copy is emitted. Any other collision between the loop variable, pipeline parameters, bindings and visible locals is an error, both at the declaration and at the for-each site.
- The generated loop and block carry no source range, so formatter dumps have no stray blank lines.
- A value sink is recognized as `p(inputs).<name>(...)`: a call whose callee is an access on a pipeline call. Analysis types it (`count` → `Uint64`, `any` / `all` → `Bool`, `into` → `Array_slice::<mutable U>`), so `var n = p(list).count();` needs no annotation.
- The pass replaces the sink call in place with a `Statement_block_expression` and invalidates its old sub-expressions. Inputs and the `into` buffer that are not plain variables are bound once, in source order, at the top of the block.
- `fold` takes an ordinary lambda literal, `fold(init, lambda (acc, v) => expression)`, which gives it the `=>` of Iris lambdas without new grammar. Validation supplies the lambda's expected type `(type of init, U) -> type of init`, so parameter and return types are inferred and the body is checked like any lambda body. The pass inlines it rather than calling it: `mutable acc = init;` before the loop, `acc = <body>;` per element, and `v` bound to the output element (no copy when `v` is the output binding's name). It must be a literal with two parameters and an expression body, and its parameter names follow the same collision rules as pipeline bindings.
- `in_place` is written on the pipeline itself, `p.in_place(values)`, with the inputs as its arguments, so `get_pipeline_sink_call` recognizes both `p(inputs).<sink>(...)` and `p.<sink>(...)`, and `Pipeline_sink_call` carries the input expressions. Writing it as `p(values).in_place()`, or any other sink as `p.count()`, is an error. It needs one input of type `Array_slice::<mutable T>` and an output element of type `T`. It expands like `into` with the input as the buffer and no capacity assert.
- A `Soa_array_view` parameter `p` is bound per field when every use of it, and of its immutable copies (`var q = p;`, which is what a for-each loop variable or a `map p => q` becomes), is a member access `p.field`. Each used field is read once at the top of the iteration, `var $pipeline<N>_<p>_<field> = input->field[$pipeline<N>_i];`, the accesses are rewritten to that local, and the copies are removed. Reading at the top keeps the semantics of `var p = input[i];` even when the body writes to the view. Any other use (passing `p` to a function, storing it, a `mutable` copy, a lambda parameter of the same name) falls back to `var p = input[i];`. So does any expression whose nested statements the pass's walker does not know how to reach, so a use can never be missed.
- `take_while <expression>;` expands to `if !(<expression>) { break; }` at its position among the stages, so it stops the whole loop, and after a `filter` it only sees the kept elements. Its expression must be `Bool`. `enumerate => <name>;` expands to `var <name> = $pipeline<N>_i;`: the index in the inputs, before any filter, typed `Uint64`. It is a binding like a map binding (same collision rules) but never the output element. Both are keywords only inside a pipeline body; the corpus checks they stay usable as identifiers elsewhere. The new `Pipeline_stage_kind` values are appended.
- The JSON serializer's `Generated.cppm` is regenerated from `Core.cppm` at build time, and its parser takes a comment inside an enum body for an enum value, so enum comments go above the enum.
- Zero cost is tested in `Compiler.tests.cpp` ("Pipelines compile to the same optimized code as the hand-written loops"). `Examples/txt/pipeline_zero_cost.iris` and `pipeline_zero_cost_hand_written.iris` define the same functions (a for-each with filter and map, a two-input `fold`, `count`, `any`, `in_place`, a `Soa_array_view` input, a `fold` with a named function, and `into_fixed`), as pipelines and as hand-written loops. Both are compiled at O2, with and without bounds checks, and each pair must be the same code. They are separate modules so that `MergeFunctionsPass` cannot fold one into the other. The comparison renames locals and labels in order of appearance, drops IR comments, masks error-message lengths (they embed the module name and path) and lets the `phi`s at the start of a block appear in any order (that order follows variable declaration order). The hand-written SoA loop reads its fields at the top of the iteration, as the expansion does; reading one inside the `if` gives the same instructions without bounds checks but a different block layout with them.
- Getting O2 to run at all fixed two latent bugs in `initialize_llvm` / `optimize_llvm_module`, since nothing had ever compiled with `is_optimized` (`Source/Builder/main.cpp` still hard-codes it to false). The `PassBuilder` was a local, but the analyses it registers call back into it, so it now lives in `Optimization_managers` (declared first, so it outlives the managers) and gets the target machine. And the module pass manager was built once per `LLVM_data` and reused; the O1+ pipelines contain `ModuleInlinerWrapperPass`, which moves its inner CGSCC pipeline out on its first run, so the second module crashed on null passes. The pipeline is now built on each `optimize_llvm_module` call, and the analysis caches are cleared afterwards, since they are keyed by IR addresses a later module may reuse.
- `first` expands to `mutable $r: Optional::<U> = {};` and, for the first kept element, `$r = create_optional(<output>); break;`.
- The test runner (`share/iris/source/tests_main.cpp`) calls `_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT)` on Windows. The Debug CRT otherwise shows an "abort() has been called" message box before raising `SIGABRT`, which hung every headless run with a failing `assert`.
- `visit_statements_using_scope` now also visits `while` conditions and the statements and result of statement blocks, so later passes (lambda, instantiate) see code inside them.
- `Statement_block_expression` codegen emits the statements into the current basic block and passes the result value through unloaded, like a parenthesis. It pushes no block info, because passes never put `return`, `defer` or an outward `break` / `continue` inside it.

- Two bugs fixed along the way, both on HEAD before this work:
  - `Lambda_pass` appended to the module's declaration vectors while the declaration database pointed into them. Once a capturing lambda reallocated a vector, a later function that read a struct's members read freed memory. Lambda functions and environment structs now reserve room first and repoint the moved entries (`reserve_declarations_keeping_database_pointers`).
  - `get_expected_expression_type` gave every expression of a statement the statement's expected type. So in `lambda (xs) => p(xs).fold(0, lambda (a, b) => a + b)`, the inner lambda was "expected" to be the outer lambda's return type. A lambda literal nested in a statement is now typed by its context (call argument, fold, typed declaration) first, and falls back to the statement's type only when nothing applies.
  - Capture analysis did not look inside ternary branches or switch cases, which are statements of their own. So `lambda (v) => v > 0 ? weight : 0` failed with "Undefined variable 'weight'". It now collects names from them too.

Language server:
- `Pipeline_expression::name_source_position`, `Pipeline_stage::binding_name_source_position` and `For_each_expression::variable_name_source_position` record where names are written, so go-to lands on the name rather than on `pipeline`, `map` or `for`.
- `visit_statements_using_scope` adds the parameters, then visits each stage before pushing its binding (`add_pipeline_parameters_to_scope`, `add_pipeline_stage_binding_to_scope`), so a stage sees only earlier bindings. `is_add_scope_expression` includes `Pipeline_expression`, so `calculate_scope` descends into stages.
- Hover: the pipeline name (declaration or use) shows `pipeline p(input: T) => U`, where `U` is the output element type. Parameters, bindings and the for-each variable show `name: Type`. A sink name shows its signature, such as `into(out: Array_slice::<mutable U>) -> Array_slice::<mutable U>`. These come from `create_pipeline_signature` and `create_pipeline_sink_signature` in `iris.language_server.location`, which signature help shares.
- Go-to: a pipeline use goes to its name. Go-to now walks lambda bodies too.
- Completion: pipeline names appear in value lists. `p(xs).` offers the sinks plus `separate` (`any` / `all` only for a `Bool` output, `into_fixed` only when the first input has a static size), `p.` offers `in_place`, and `p(xs).separate(s).` offers `into`. The start of a stage offers `map`, `filter`, `take_while` and `enumerate`. Access completion after a call now completes on the whole call, not on its `)` token.
- Signature help covers `p(`, `p.in_place(` and sink arguments. The innermost call wins, so `p(f(|))` is answered for `f`.
- Inlay hints: `: Type` after each map binding and after the for-each variable.
- Tests are tagged `[Pipeline]` in the language server test files.

Known gaps:
- `separate` only feeds `into`, and its stages form a linear chain (see Step 5).

## Fixed_capacity_array and into_fixed

`Fixed_capacity_array::<T, N>` is a builtin type: a stack array of capacity `N` with a fill count. `into_fixed` collects a pipeline into one, so a pipeline over `Constant_array` inputs needs no caller-provided buffer.

```
mutable values: Fixed_capacity_array::<Int32, 4> = {};
values.push(1);
var last = values.pop();
values[0] = 2;
var n = values.length;      // Uint64, read-only
var c = values.capacity;    // Uint64, the constant N
values.clear();
sum(values);                // converts to Array_slice::<Int32> of the filled part

var squares = odd_squares(constant_array).into_fixed();   // Fixed_capacity_array::<Int32, N>
```

Decisions taken while implementing:
- Layout `{ values: Constant_array::<T, N>; length: Uint64 }`. `values` is internal: validation rejects it ("Member 'values' does not exist ..."). `{}` is the only initializer; any member gives "A Fixed_capacity_array can only be initialized empty, with '{}'. Use push() to add elements.".
- `N` is a literal, like `Constant_array`. A generic `N` is not supported.
- `Fixed_capacity_array_type` is appended at the end of the `Type_reference` variant, because the hash includes the variant index. Like `Optional::<T>`, its record is synthesized per `T, N` by `try_get_builtin_struct_declaration`, named `Fixed_capacity_array_<T>_<N>` (the C header uses the same name), and collected per module for LLVM, Clang and the C header exporter. The exporter writes these records after the records their members use.
- `push`, `pop` and `clear` are typed as builtin references (`fixed_capacity_array_push` and so on), like `Soa_array.view`, and are emitted inline in `create_builtin_call_expression_value`. They need a mutable receiver ("Cannot call 'push' on an immutable Fixed_capacity_array."). `pop` returns `T`.
- Through a pointer, `p->push(v)`, `p->pop()`, `p->clear()`, `p->length` and `p->capacity` work too. The methods need a `*mutable` pointer ("Cannot call 'push' through a pointer that is not mutable. ..."), and `p->values` is rejected like `a.values`.
- With bounds checks on, `a[i]` checks `i < length` ("Out-of-bounds Fixed_capacity_array access"), `push` checks `length < N` ("Fixed_capacity_array push on a full array") and `pop` checks `length > 0` ("Fixed_capacity_array pop on an empty array"). Without bounds checks they are unchecked, like other indexing.
- The conversion to `Array_slice::<T>` follows `Constant_array`'s: `can_assign_type` allows it, and `convert_to_expected_type_if_needed` builds `{ &values[0], length }`.
- The sink is an explicit call, `p(inputs).into_fixed()`, like the other value sinks, rather than the pipeline call used as a value. So "Pipeline 'p' is not a value" stays unconditional. It expands to:

  ```
  var result = ${
      mutable $pipeline0_result: Fixed_capacity_array::<U, N> = {};
      for $pipeline0_i in 0u64 to N
      {
          ... stages ...
          $pipeline0_result.push(<output>);
      }
      yield $pipeline0_result;
  };
  ```

  It uses `push` rather than writing to `values`, so the field stays internal. This costs nothing: with bounds checks, `push`'s `length < N` check is the same one a `values[length]` store would make. The zero-cost test compares it against a hand-written `push` loop.
- Taking the address of an access whose value is computed rather than stored (`.capacity`, `Soa_array.length`) now spills it to a temporary. `check(a.capacity == 4u64)` needed this, because the test-mode rewrite of `check(x == y)` passes `&x` and `&y` to `print_json_difference`.
- Debug info names the record `Fixed_capacity_array<T,N>`, which `share/iris/visualizers/iris_fixed_capacity_array.natvis` matches, showing the first `length` values.
- Not done: compile-time reflection reports the type kind as `Custom`, because a new kind would change `Builtin.iris`. `p[i]` on a pointer stays pointer indexing; write `(*p)[i]` to index the array.

## Verification

- `Examples/txt/pipeline_*.iris`: one file per sink, plus multiple parameters, captured locals and nested pipelines inside `for` bodies.
- `Pipeline_pass.tests.cpp`: assert the expanded core AST. Use `Pass_test_helpers.cppm` as the other passes do.
- **Zero-cost proof** in `Compiler.tests.cpp`: compile a pipeline and the equivalent hand-written loop, and assert identical optimized IR. Give each helper a distinct body, because `MergeFunctionsPass` collapses identical functions. Regenerate pinned IR with the dump-hook script, never by hand.
- Test-framework runs that must trigger asserts: an `into` buffer that is too small, and two inputs of different lengths.
- Value sinks in nested positions: inside a `while` condition (re-evaluated every iteration), on the right of `&&` (short-circuited), and as call arguments (evaluated in order).
- `Statement_block_expression` codegen tests in `Compiler.tests.cpp`, built directly in core form, since there is no syntax for it.
- Name hygiene: user locals named `i`, `count` and `pipeline0_i` next to a pipeline expansion.
- Tree-sitter: confirm `map` / `filter` still work as ordinary identifiers outside pipeline bodies.

## Future work

- **User-facing value blocks** (separate plan). Expose `Statement_block_expression` as syntax. Open questions: an explicit `yield value;` (preferred, it fits Iris's explicitness) or a trailing expression without `;`; what `return`, `break`, `continue` and `defer` do inside a value block; and whether `if` / `switch` should become value-producing on top of it.
- **Module-level and exported pipelines.** These need a parameter story for outside values and binary serialization.
