
# 1. Core Concept

A lambda value is a struct internally:

```c
struct Lambda_T {
    function_pointer: function<(Args...) -> (Return_type)> = null;
    user_data: *mutable Void = null;
}
```

When called:

```c
lambda.function_pointer(args..., lambda.user_data);
```

This means:

* **captured variables live inside `userdata`**
* `userdata` may be `NULL` if nothing captured

---

# 2. Lambda Type Declaration (Named)

 To help with C header generarion, we need to support **named lambda interfaces**.

Example syntax:

```
lambda Comparator(a: Int32, b: Int32) -> (result: Int32)
```

Generated C:

```c
struct Comparator {
    function_pointer: function<(a: Int32, b: Int32, user_data: *mutable Void) -> (result: Int32)> = null;
    user_data: *mutable Void = null;
}
```

This makes the type **stable and C-exposable**.

---

# 3. Using the Lambda Type

Example:

```
function sort(array: Array_view::<Int32>, cmp: Comparator)
```

Call site:

```
sort(numbers, lambda(a, b) => a - b)
```

Compiler expands this into:

```
function __lambda1(a: Int32, b: Int32, user_data: *mutable Void) -> (result: Int32) {
    return a - b;
}

var lambda_temporary: Comparator = { __lambda1, null };
sort(numbers, lambda_temporary);
```

---

# 4. Capturing Variables

Example:

```
function threshold_filter(values: Array_slice::<Int32>, threshold: Int32) -> () {
    filter(values, lambda(x) => x > threshold)
}
```

Compiler generates:

```
struct __lambda_env1 {
    threshold: Int32 = 0;
};

function __lambda1(x: Int32, user_data: *mutable Void) -> (result: Bool) {
    env: *mutable __lambda_env1 = user_data as *mutable __lambda_env1;
    return x > env->threshold;
}
```

Call site:

```
env: __lambda_env1 = {};
env.threshold = threshold;

lambda_temporary: Predicate = { __lambda1, &env };
filter(values, lambda_temporary);
```

---

# 5. Lambda Literal Syntax

Capture of external variables is automatic.

## Option A (inline)

```
lambda(x, y) => x + y
```

### Option B (block body)

```
lambda(x, y) => {
    return x * y;
}
```

---

# 6. Lambda Call Syntax

Calling a lambda should be natural:

```
cmp(a, b)
```

Compiler lowers to:

```
cmp.function_pointer(a, b, cmp.userdata)
```

---

# 7. C Header Generation

Language:

```
lambda Predicate(x: Int32) -> (result: Bool)
```

Generated C header:

```c
struct Predicate {
    bool (*function_pointer)(int32_t x, void* user_data);
    void* user_data;
};
```

Example exported function:

```
export function filter(values: *Int32, count: Uint64, predicate: Predicate)
```

C header:

```c
void filter(int32_t* values, uint64_t count, Predicate predicate);
```

---

# 8. LLVM Representation

LLVM struct type:

```
%Predicate = type {
    i1 (i32, ptr)*,
    ptr
}
```

Example call lowering:

```
%fn = extractvalue %Predicate %pred, 0
%ud = extractvalue %Predicate %pred, 1

%result = call i1 %fn(i32 %x, ptr %ud)
```

---

# 9. Advantages of This Model

✔ Fully **C ABI compatible**
✔ Works with **capturing lambdas**
✔ Easy **header generation**
✔ No runtime needed
✔ Simple LLVM lowering

---

# 11. Example End-to-End

Language:

```
lambda Predicate(x: Int32) -> (result: Bool)

fn filter(arr: []i32, pred: Predicate)

fn main() {
    let limit = 10

    filter(numbers, lambda(x) => x > limit)
}
```

Compiler generates roughly:

```
struct __env {
    int limit;
};

bool __lambda(int x, void* userdata) {
    struct __env* e = userdata;
    return x > e->limit;
}
```
