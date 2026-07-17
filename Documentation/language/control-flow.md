---
sidebar_position: 3
---

# Control Flow

## If / Else

```iris
if value == 0
{
    print_message("zero"c);
}

if value < 0
{
    print_message("negative"c);
}
else if value > 0
{
    print_message("positive"c);
}
else
{
    print_message("zero"c);
}
```

Conditions do not need parentheses. The branches must be enclosed in curly braces.

## Switch

```iris
switch enum_argument
{
case My_enum.Value_0:
case My_enum.Value_1:
    return 0;
case My_enum.Value_10:
    return 1;
}
```

Multiple `case` labels can share a body. Each case falls through automatically to the next case label until a statement is reached.

## For Loop

The `for…in` loop iterates over an integer range:

```iris
// 0, 1, 2  (exclusive upper bound)
for index in 0 to 3
{
    print_integer(index);
}
```

### Step

```iris
// 0, 1, 2, 3  (with explicit step)
for index in 0 to 4 step_by 1
{
    print_integer(index);
}
```

### Reverse

```iris
// 4, 3, 2, 1, 0
for index in 4 to 0 step_by -1 reverse
{
    print_integer(index);
}

// also valid without step_by
for index in 4 to 0 reverse
{
    print_integer(index);
}
```

## While Loop

```iris
while condition
{
    // body
}
```

## Break & Continue

```iris
while condition
{
    if should_stop { break; }
    if should_skip { continue; }
}
```

`break N` breaks out of `N` nested loops at once:

```iris
for i in 0 to 10
{
    for j in 0 to 10
    {
        if j % 2 == 0
        {
            break 2;   // exits both loops
        }
    }
}
```

## Defer

`defer` schedules a statement to run at the end of the **current scope**, regardless of how that scope exits (normal return, early return, break, continue):

```iris
function run(condition: Bool, value: Int32) -> ()
{
    defer do_defer(0);   // runs last
    defer do_defer(1);   // runs second-to-last

    if condition
    {
        defer do_defer(2);   // runs when this if-scope ends
    }

    if condition
    {
        return;   // deferred items 0 and 1 still run
    }
}
```

Deferred statements execute in **LIFO order** (last registered, first executed). The deferral is always to the scope in which `defer` appears, not the function.

To defer more than one statement, follow `defer` with a block. The block form takes no trailing semicolon:

```iris
function run() -> ()
{
    var id = create_object();

    defer
    {
        log("destroying");
        destroy_object(id);
    }
}
```

The whole block runs as a unit when the scope exits, and the statements inside it run in the order written. A block counts as a single entry for LIFO purposes, so it is ordered against other `defer`s by where it was registered.

A deferred block introduces its own scope, so it may declare locals and may itself contain a `defer`:

```iris
defer
{
    do_defer(0);

    defer do_defer(1);   // runs when the deferred block ends, so after 0
}
```
