#include <string_view>

#include <catch2/catch_all.hpp>

import iris.compiler.pass_test_helpers;
import iris.compiler.pipeline_pass;
import iris.core;
import iris.core.declarations;

namespace iris::compiler
{
    static void test_pipeline_pass_on_module(
        std::string_view const input_text,
        std::string_view const expected
    )
    {
        iris::compiler::tests::Parsed_module_context context = iris::compiler::tests::parse_module_context(input_text, {});

        run_pipeline_pass_on_module(context.core_module(), context.declaration_database);

        std::pmr::string const actual = iris::compiler::tests::format_core_module_to_text(context.core_module());

        CHECK(expected == actual);
    }

    TEST_CASE("Pipeline pass lowers a for-each over a single input with map and filter", "[Pipeline_pass][Passes][Pipeline]")
    {
        std::string_view const input = R"(module Pipeline_pass_test;

function square(value: Int32) -> (result: Int32)
{
    return value * value;
}

function use(value: Int32) -> ()
{
}

export function run(list: Array_slice::<Int32>, threshold: Int32) -> ()
{
    pipeline process(input: Int32)
    {
        map square(input) => v0;
        filter v0 > threshold;
    }

    for v in process(list)
    {
        use(v);
    }
}
)";

        std::string_view const expected = R"(module Pipeline_pass_test;

function square(value: Int32) -> (result: Int32)
{
    return value * value;
}

function use(value: Int32) -> ()
{
}

export function run(list: Array_slice::<Int32>, threshold: Int32) -> ()
{
    for $pipeline0_i in 0u64 to list.length
    {
        var input = list[$pipeline0_i];
        var v0 = square(input);
        if v0 > threshold
        {
            var v = v0;
            use(v);
        }
    }
}
)";

        test_pipeline_pass_on_module(input, expected);
    }

    TEST_CASE("Pipeline pass reads several inputs in lockstep and asserts equal lengths", "[Pipeline_pass][Passes][Pipeline]")
    {
        std::string_view const input = R"(module Pipeline_pass_test;

function use(value: Int32) -> ()
{
}

export function run(values: Array_slice::<Int32>, weights: Array_slice::<Int32>) -> ()
{
    pipeline weighted(value: Int32, weight: Int32)
    {
        map value * weight => product;
    }

    for product in weighted(values, weights)
    {
        use(product);
    }
}
)";

        std::string_view const expected = R"(module Pipeline_pass_test;

function use(value: Int32) -> ()
{
}

export function run(values: Array_slice::<Int32>, weights: Array_slice::<Int32>) -> ()
{
    {
        assert "Pipeline inputs have equal length" { weights.length == values.length };
        for $pipeline0_i in 0u64 to values.length
        {
            var value = values[$pipeline0_i];
            var weight = weights[$pipeline0_i];
            var product = value * weight;
            use(product);
        }
    }
}
)";

        test_pipeline_pass_on_module(input, expected);
    }

    TEST_CASE("Pipeline pass uses the constant length of a Constant_array input and binds non-variable inputs once", "[Pipeline_pass][Passes][Pipeline]")
    {
        std::string_view const input = R"(module Pipeline_pass_test;

function get_values() -> (result: Array_slice::<Int32>)
{
    return {};
}

function use(value: Int32) -> ()
{
}

export function run(fixed: Constant_array::<Int32, 4>) -> ()
{
    pipeline identity(value: Int32)
    {
        filter value > 0;
    }

    for value_out in identity(fixed)
    {
        use(value_out);
    }

    for value_out in identity(get_values())
    {
        use(value_out);
    }
}
)";

        std::string_view const expected = R"(module Pipeline_pass_test;

function get_values() -> (result: Array_slice::<Int32>)
{
    return {};
}

function use(value: Int32) -> ()
{
}

export function run(fixed: Constant_array::<Int32, 4>) -> ()
{
    for $pipeline0_i in 0u64 to 4u64
    {
        var value = fixed[$pipeline0_i];
        if value > 0
        {
            var value_out = value;
            use(value_out);
        }
    }
    {
        var $pipeline1_input0 = get_values();
        for $pipeline1_i in 0u64 to $pipeline1_input0.length
        {
            var value = $pipeline1_input0[$pipeline1_i];
            if value > 0
            {
                var value_out = value;
                use(value_out);
            }
        }
    }
}
)";

        test_pipeline_pass_on_module(input, expected);
    }

    TEST_CASE("Pipeline pass lowers nested pipelines and keeps user names", "[Pipeline_pass][Passes][Pipeline]")
    {
        std::string_view const input = R"(module Pipeline_pass_test;

function use(value: Int32) -> ()
{
}

export function run(rows: Array_slice::<Int32>, columns: Array_slice::<Int32>) -> ()
{
    var i = 0;
    var count = 1;
    pipeline doubled(row: Int32)
    {
        map row * 2 => twice;
    }

    for twice in doubled(rows)
    {
        pipeline shifted(column: Int32)
        {
            map column + twice + i + count => sum;
        }

        for sum in shifted(columns)
        {
            use(sum);
        }
    }
}
)";

        std::string_view const expected = R"(module Pipeline_pass_test;

function use(value: Int32) -> ()
{
}

export function run(rows: Array_slice::<Int32>, columns: Array_slice::<Int32>) -> ()
{
    var i = 0;
    var count = 1;
    for $pipeline0_i in 0u64 to rows.length
    {
        var row = rows[$pipeline0_i];
        var twice = row * 2;
        for $pipeline1_i in 0u64 to columns.length
        {
            var column = columns[$pipeline1_i];
            var sum = column + twice + i + count;
            use(sum);
        }
    }
}
)";

        test_pipeline_pass_on_module(input, expected);
    }

    TEST_CASE("Pipeline pass expands value sinks into statement blocks", "[Pipeline_pass][Passes][Pipeline]")
    {
        std::string_view const input = R"(module Pipeline_pass_test;

export function run(values: Array_slice::<Int32>, buffer: Array_slice::<mutable Int32>) -> ()
{
    pipeline even(value: Int32)
    {
        filter value % 2 == 0;
    }

    pipeline is_large(value: Int32)
    {
        map value > 3 => large;
    }

    var count = even(values).count();
    var any_large = is_large(values).any();
    var all_large = is_large(values).all();
    var written = even(values).into(buffer);
}
)";

        std::string_view const expected = R"(module Pipeline_pass_test;

export function run(values: Array_slice::<Int32>, buffer: Array_slice::<mutable Int32>) -> ()
{
    var count = ${
        mutable $pipeline0_count = 0u64;
        for $pipeline0_i in 0u64 to values.length
        {
            var value = values[$pipeline0_i];
            if value % 2 == 0
            {
                $pipeline0_count += 1u64;
            }
        }
        yield $pipeline0_count;
    };
    var any_large = ${
        mutable $pipeline1_result = false;
        for $pipeline1_i in 0u64 to values.length
        {
            var value = values[$pipeline1_i];
            var large = value > 3;
            if large
            {
                $pipeline1_result = true;
                break;
            }
        }
        yield $pipeline1_result;
    };
    var all_large = ${
        mutable $pipeline2_result = true;
        for $pipeline2_i in 0u64 to values.length
        {
            var value = values[$pipeline2_i];
            var large = value > 3;
            if !large
            {
                $pipeline2_result = false;
                break;
            }
        }
        yield $pipeline2_result;
    };
    var written = ${
        assert "Pipeline output buffer is large enough" { buffer.length >= values.length };
        mutable $pipeline3_count = 0u64;
        for $pipeline3_i in 0u64 to values.length
        {
            var value = values[$pipeline3_i];
            if value % 2 == 0
            {
                buffer[$pipeline3_count] = value;
                $pipeline3_count += 1u64;
            }
        }
        yield create_array_slice_from_pointer(buffer.data, $pipeline3_count);
    };
}
)";

        test_pipeline_pass_on_module(input, expected);
    }

    TEST_CASE("Pipeline pass inlines the fold lambda and expands first", "[Pipeline_pass][Passes][Pipeline]")
    {
        std::string_view const input = R"(module Pipeline_pass_test;

export function run(values: Array_slice::<Int32>) -> ()
{
    pipeline squares(value: Int32)
    {
        map value * value => squared;
    }

    var sum = squares(values).fold(0, lambda (total, element) => total + element);
    var product = squares(values).fold(1, lambda (accumulator, squared) => accumulator * squared);
    var found = squares(values).first();
}
)";

        std::string_view const expected = R"(module Pipeline_pass_test;

export function run(values: Array_slice::<Int32>) -> ()
{
    var sum = ${
        mutable total = 0;
        for $pipeline0_i in 0u64 to values.length
        {
            var value = values[$pipeline0_i];
            var squared = value * value;
            var element = squared;
            total = total + element;
        }
        yield total;
    };
    var product = ${
        mutable accumulator = 1;
        for $pipeline1_i in 0u64 to values.length
        {
            var value = values[$pipeline1_i];
            var squared = value * value;
            accumulator = accumulator * squared;
        }
        yield accumulator;
    };
    var found = ${
        mutable $pipeline2_result: Optional::<Int32> = {};
        for $pipeline2_i in 0u64 to values.length
        {
            var value = values[$pipeline2_i];
            var squared = value * value;
            $pipeline2_result = create_optional(squared);
            break;
        }
        yield $pipeline2_result;
    };
}
)";

        test_pipeline_pass_on_module(input, expected);
    }

    TEST_CASE("Pipeline pass expands into_fixed into a Fixed_capacity_array", "[Pipeline_pass][Passes][Pipeline]")
    {
        std::string_view const input = R"(module Pipeline_pass_test;

export function run(values: Constant_array::<Int32, 4>) -> ()
{
    pipeline odd_squares(value: Int32)
    {
        filter value % 2 == 1;
        map value * value => squared;
    }

    var squares = odd_squares(values).into_fixed();
    var again = odd_squares(squares).into_fixed();
}
)";

        std::string_view const expected = R"(module Pipeline_pass_test;

export function run(values: Constant_array::<Int32, 4>) -> ()
{
    var squares = ${
        mutable $pipeline0_result: Fixed_capacity_array::<Int32, 4> = {};
        for $pipeline0_i in 0u64 to 4u64
        {
            var value = values[$pipeline0_i];
            if value % 2 == 1
            {
                var squared = value * value;
                $pipeline0_result.push(squared);
            }
        }
        yield $pipeline0_result;
    };
    var again = ${
        mutable $pipeline1_result: Fixed_capacity_array::<Int32, 4> = {};
        for $pipeline1_i in 0u64 to squares.length
        {
            var value = squares[$pipeline1_i];
            if value % 2 == 1
            {
                var squared = value * value;
                $pipeline1_result.push(squared);
            }
        }
        yield $pipeline1_result;
    };
}
)";

        test_pipeline_pass_on_module(input, expected);
    }

    TEST_CASE("Pipeline pass calls a fold function that is not a lambda literal", "[Pipeline_pass][Passes][Pipeline]")
    {
        std::string_view const input = R"(module Pipeline_pass_test;

lambda Adder(a: Int32, b: Int32) -> (result: Int32);

function add(total: Int32, value: Int32) -> (result: Int32)
{
    return total + value;
}

function make_adder() -> (result: Adder)
{
    return lambda (a, b) => a + b;
}

export function run(values: Array_slice::<Int32>, add_value: Adder) -> ()
{
    pipeline squares(value: Int32)
    {
        map value * value => squared;
    }

    var sum = squares(values).fold(0, add);
    var sum_with_value = squares(values).fold(1, add_value);
    var sum_with_made = squares(values).fold(2, make_adder());
}
)";

        std::string_view const expected = R"(module Pipeline_pass_test;

lambda Adder(a: Int32, b: Int32) -> (result: Int32);

function add(total: Int32, value: Int32) -> (result: Int32)
{
    return total + value;
}

function make_adder() -> (result: Adder)
{
    return lambda(a, b) => a + b;
}

export function run(values: Array_slice::<Int32>, add_value: Adder) -> ()
{
    var sum = ${
        mutable $pipeline0_accumulator = 0;
        for $pipeline0_i in 0u64 to values.length
        {
            var value = values[$pipeline0_i];
            var squared = value * value;
            $pipeline0_accumulator = add($pipeline0_accumulator, squared);
        }
        yield $pipeline0_accumulator;
    };
    var sum_with_value = ${
        mutable $pipeline1_accumulator = 1;
        for $pipeline1_i in 0u64 to values.length
        {
            var value = values[$pipeline1_i];
            var squared = value * value;
            $pipeline1_accumulator = add_value($pipeline1_accumulator, squared);
        }
        yield $pipeline1_accumulator;
    };
    var sum_with_made = ${
        mutable $pipeline2_accumulator = 2;
        var $pipeline2_function = make_adder();
        for $pipeline2_i in 0u64 to values.length
        {
            var value = values[$pipeline2_i];
            var squared = value * value;
            $pipeline2_accumulator = $pipeline2_function($pipeline2_accumulator, squared);
        }
        yield $pipeline2_accumulator;
    };
}
)";

        test_pipeline_pass_on_module(input, expected);
    }

    TEST_CASE("Pipeline pass expands pipelines inside lambda bodies", "[Pipeline_pass][Passes][Pipeline]")
    {
        std::string_view const input = R"(module Pipeline_pass_test;

lambda Action(value: Int32) -> ();

lambda Counter(inputs: Array_slice::<Int32>) -> (result: Uint64);

function use(value: Int32) -> ()
{
}

export function run(values: Array_slice::<Int32>, threshold: Int32) -> ()
{
    pipeline large(input: Int32)
    {
        filter input > threshold;
    }
    var action: Action = lambda(value) => {
        pipeline shifted(input: Int32)
        {
            map input + value => next;
        }
        for next in shifted(values)
        {
            use(next);
        }
    };
    var count_large: Counter = lambda(inputs) => large(inputs).count();
}
)";

        std::string_view const expected = R"(module Pipeline_pass_test;

lambda Action(value: Int32) -> ();

lambda Counter(inputs: Array_slice::<Int32>) -> (result: Uint64);

function use(value: Int32) -> ()
{
}

export function run(values: Array_slice::<Int32>, threshold: Int32) -> ()
{
    var action: Action = lambda(value) => {
        for $pipeline0_i in 0u64 to values.length
        {
            var input = values[$pipeline0_i];
            var next = input + value;
            use(next);
        }
    };


    var count_large: Counter = lambda(inputs) => ${
        mutable $pipeline1_count = 0u64;
        for $pipeline1_i in 0u64 to inputs.length
        {
            var input = inputs[$pipeline1_i];
            if input > threshold
            {
                $pipeline1_count += 1u64;
            }
        }
        yield $pipeline1_count;
    };
}
)";

        test_pipeline_pass_on_module(input, expected);
    }

    TEST_CASE("Pipeline pass separates a pipeline into one loop per stage", "[Pipeline_pass][Passes][Pipeline]")
    {
        std::string_view const input = R"(module Pipeline_pass_test;

export function run(values: Array_slice::<Int32>, scratch: Array_slice::<mutable Int32>, output: Array_slice::<mutable Float32>, threshold: Int32) -> ()
{
    pipeline process(value: Int32)
    {
        map value * 2 => doubled;
        filter doubled > threshold;
        map doubled as Float32 => result;
    }
    var written = process(values).separate(scratch).into(output);
}
)";

        std::string_view const expected = R"(module Pipeline_pass_test;

export function run(values: Array_slice::<Int32>, scratch: Array_slice::<mutable Int32>, output: Array_slice::<mutable Float32>, threshold: Int32) -> ()
{
    var written = ${
        assert "Pipeline output buffer is large enough" { output.length >= values.length };
        assert "Pipeline scratch buffer is large enough" { scratch.length >= values.length };
        mutable $pipeline0_count = values.length;
        for $pipeline0_i in 0u64 to values.length
        {
            var value = values[$pipeline0_i];
            var doubled = value * 2;
            scratch[$pipeline0_i] = doubled;
        }
        mutable $pipeline0_kept1 = 0u64;
        for $pipeline0_i1 in 0u64 to $pipeline0_count
        {
            var doubled = scratch[$pipeline0_i1];
            if doubled > threshold
            {
                scratch[$pipeline0_kept1] = doubled;
                $pipeline0_kept1 += 1u64;
            }
        }
        $pipeline0_count = $pipeline0_kept1;
        for $pipeline0_i2 in 0u64 to $pipeline0_count
        {
            var doubled = scratch[$pipeline0_i2];
            var result = doubled as Float32;
            output[$pipeline0_i2] = result;
        }
        yield create_array_slice_from_pointer(output.data, $pipeline0_count);
    };
}
)";

        test_pipeline_pass_on_module(input, expected);
    }

    TEST_CASE("Pipeline pass separates a pipeline whose elements all have the output type", "[Pipeline_pass][Passes][Pipeline]")
    {
        std::string_view const input = R"(module Pipeline_pass_test;

export function run(values: Array_slice::<Int32>, output: Array_slice::<mutable Int32>) -> ()
{
    pipeline process(value: Int32)
    {
        filter value > 0;
        map value * 2 => doubled;
        take_while doubled < 100;
    }
    var written = process(values).separate().into(output);
}
)";

        std::string_view const expected = R"(module Pipeline_pass_test;

export function run(values: Array_slice::<Int32>, output: Array_slice::<mutable Int32>) -> ()
{
    var written = ${
        assert "Pipeline output buffer is large enough" { output.length >= values.length };
        mutable $pipeline0_count = values.length;
        mutable $pipeline0_kept0 = 0u64;
        for $pipeline0_i in 0u64 to values.length
        {
            var value = values[$pipeline0_i];
            if value > 0
            {
                output[$pipeline0_kept0] = value;
                $pipeline0_kept0 += 1u64;
            }
        }
        $pipeline0_count = $pipeline0_kept0;
        for $pipeline0_i1 in 0u64 to $pipeline0_count
        {
            var value = output[$pipeline0_i1];
            var doubled = value * 2;
            output[$pipeline0_i1] = doubled;
        }
        for $pipeline0_i2 in 0u64 to $pipeline0_count
        {
            var doubled = output[$pipeline0_i2];
            if !(doubled < 100)
            {
                $pipeline0_count = $pipeline0_i2;
                break;
            }
        }
        yield create_array_slice_from_pointer(output.data, $pipeline0_count);
    };
}
)";

        test_pipeline_pass_on_module(input, expected);
    }

    TEST_CASE("Pipeline pass compacts the kept elements of an in_place sink", "[Pipeline_pass][Passes][Pipeline]")
    {
        std::string_view const input = R"(module Pipeline_pass_test;

export function run(values: Array_slice::<mutable Int32>) -> ()
{
    pipeline doubled_odds(value: Int32)
    {
        filter value % 2 == 1;
        map value * 2 => doubled;
    }

    var kept = doubled_odds.in_place(values);
}
)";

        std::string_view const expected = R"(module Pipeline_pass_test;

export function run(values: Array_slice::<mutable Int32>) -> ()
{
    var kept = ${
        mutable $pipeline0_count = 0u64;
        for $pipeline0_i in 0u64 to values.length
        {
            var value = values[$pipeline0_i];
            if value % 2 == 1
            {
                var doubled = value * 2;
                values[$pipeline0_count] = doubled;
                $pipeline0_count += 1u64;
            }
        }
        yield create_array_slice_from_pointer(values.data, $pipeline0_count);
    };
}
)";

        test_pipeline_pass_on_module(input, expected);
    }

    TEST_CASE("Pipeline pass reads only the used fields of a Soa_array_view input", "[Pipeline_pass][Passes][Pipeline]")
    {
        std::string_view const input = R"(module Pipeline_pass_test;

struct Particle
{
    x: Float32 = 0.0f32;
    y: Float32 = 0.0f32;
}

function get_x(particle: Particle) -> (result: Float32)
{
    return particle.x;
}

export function run(view: Soa_array_view::<Particle>) -> ()
{
    pipeline fast(particle: Particle)
    {
        filter particle.x > 1.5f32;
    }

    mutable total = 0.0f32;
    for fast_particle in fast(view)
    {
        total += fast_particle.y;
    }

    pipeline whole(particle: Particle)
    {
        map get_x(particle) => x;
    }

    for x in whole(view)
    {
        total += x;
    }
}
)";

        std::string_view const expected = R"(module Pipeline_pass_test;

struct Particle
{
    x: Float32 = 0.0f32;
    y: Float32 = 0.0f32;
}

function get_x(particle: Particle) -> (result: Float32)
{
    return particle.x;
}

export function run(view: Soa_array_view::<Particle>) -> ()
{
    mutable total = 0.0f32;
    for $pipeline0_i in 0u64 to view.length
    {
        var $pipeline0_particle_x = view->x[$pipeline0_i];
        var $pipeline0_particle_y = view->y[$pipeline0_i];
        if $pipeline0_particle_x > 1.5f32
        {
            total += $pipeline0_particle_y;
        }
    }
    for $pipeline1_i in 0u64 to view.length
    {
        var particle = view[$pipeline1_i];
        var x = get_x(particle);
        total += x;
    }
}
)";

        test_pipeline_pass_on_module(input, expected);
    }

    TEST_CASE("Pipeline pass reads the element in place when only its fields are used", "[Pipeline_pass][Passes][Pipeline]")
    {
        std::string_view const input = R"(module Pipeline_pass_test;

struct Particle
{
    x: Float32 = 0.0f32;
    y: Float32 = 0.0f32;
    history: Constant_array::<Float32, 4> = zero_initialized {};
}

function get_x(particle: Particle) -> (result: Float32)
{
    return particle.x;
}

function read(value: *Float32) -> (result: Float32)
{
    return *value;
}

export function run(list: Array_slice::<Particle>) -> ()
{
    pipeline fast(particle: Particle)
    {
        filter particle.x > 1.5f32;
    }

    mutable total = 0.0f32;
    for fast_particle in fast(list)
    {
        total += fast_particle.y + read(&fast_particle.history[0u64]);
    }

    var fast_count = fast(list).count();

    pipeline whole(particle: Particle)
    {
        map get_x(particle) => x;
    }

    for x in whole(list)
    {
        total += x;
    }
}
)";

        std::string_view const expected = R"(module Pipeline_pass_test;

struct Particle
{
    x: Float32 = 0.0f32;
    y: Float32 = 0.0f32;
    history: Constant_array::<Float32, 4> = zero_initialized {};
}

function get_x(particle: Particle) -> (result: Float32)
{
    return particle.x;
}

function read(value: *Float32) -> (result: Float32)
{
    return *value;
}

export function run(list: Array_slice::<Particle>) -> ()
{
    mutable total = 0.0f32;
    for $pipeline0_i in 0u64 to list.length
    {
        var $pipeline0_particle = &list[$pipeline0_i];
        if $pipeline0_particle->x > 1.5f32
        {
            total += $pipeline0_particle->y + read(&$pipeline0_particle->history[0u64]);
        }
    }
    var fast_count = ${
        mutable $pipeline1_count = 0u64;
        for $pipeline1_i in 0u64 to list.length
        {
            var $pipeline1_particle = &list[$pipeline1_i];
            if $pipeline1_particle->x > 1.5f32
            {
                $pipeline1_count += 1u64;
            }
        }
        yield $pipeline1_count;
    };
    for $pipeline2_i in 0u64 to list.length
    {
        var particle = list[$pipeline2_i];
        var x = get_x(particle);
        total += x;
    }
}
)";

        test_pipeline_pass_on_module(input, expected);
    }

    TEST_CASE("Pipeline pass lowers a range input alone to a loop over the range", "[Pipeline_pass][Passes][Pipeline]")
    {
        std::string_view const input = R"(module Pipeline_pass_test;

function next(value: Uint32) -> (result: Uint32)
{
    return value + 1u32;
}

export function run(count: Uint32) -> ()
{
    pipeline even(row: Uint32)
    {
        filter row % 2u32 == 0u32;
    }

    mutable total = 0u32;
    for row in even(0u32 to count)
    {
        total += row;
    }

    var even_count = even(1u32 to next(count)).count();
}
)";

        std::string_view const expected = R"(module Pipeline_pass_test;

function next(value: Uint32) -> (result: Uint32)
{
    return value + 1u32;
}

export function run(count: Uint32) -> ()
{
    mutable total = 0u32;
    for row in 0u32 to count
    {
        if row % 2u32 == 0u32
        {
            total += row;
        }
    }
    var even_count = ${
        var $pipeline1_input0_end = next(count);
        mutable $pipeline1_count = 0u64;
        for row in 1u32 to $pipeline1_input0_end
        {
            if row % 2u32 == 0u32
            {
                $pipeline1_count += 1u64;
            }
        }
        yield $pipeline1_count;
    };
}
)";

        test_pipeline_pass_on_module(input, expected);
    }

    TEST_CASE("Pipeline pass reads a range in lockstep with other inputs by index", "[Pipeline_pass][Passes][Pipeline]")
    {
        std::string_view const input = R"(module Pipeline_pass_test;

export function run(first: Uint32, end: Uint32, values: Array_slice::<Int32>) -> ()
{
    pipeline weighted(row: Uint32, value: Int32)
    {
        map value * (row as Int32) => product;
    }

    mutable total = 0;
    for product in weighted(first to end, values)
    {
        total += product;
    }
}
)";

        std::string_view const expected = R"(module Pipeline_pass_test;

export function run(first: Uint32, end: Uint32, values: Array_slice::<Int32>) -> ()
{
    mutable total = 0;
    {
        mutable $pipeline0_input0_length = 0u64;
        if end > first
        {
            $pipeline0_input0_length = (end - first) as Uint64;
        }
        assert "Pipeline inputs have equal length" { values.length == $pipeline0_input0_length };
        for $pipeline0_i in 0u64 to $pipeline0_input0_length
        {
            var row = first + $pipeline0_i as Uint32;
            var value = values[$pipeline0_i];
            var product = value * (row as Int32);
            total += product;
        }
    }
}
)";

        test_pipeline_pass_on_module(input, expected);
    }

    TEST_CASE("Pipeline pass writes mutable elements in place", "[Pipeline_pass][Passes][Pipeline]")
    {
        std::string_view const input = R"(module Pipeline_pass_test;

struct Body
{
    mass: Float32 = 0.0f32;
    speed: Float32 = 0.0f32;
}

function keep(body: Body) -> ()
{
}

export function run(bodies: Array_slice::<mutable Body>, view: Soa_array_view::<mutable Body>) -> ()
{
    pipeline moving(body: mutable Body)
    {
        filter body.speed > 0.0f32;
    }

    for body in moving(bodies)
    {
        body.mass += body.speed;
        keep(body);
    }

    for body in moving(view)
    {
        body.mass = 1.0f32;
        keep(body);
    }
}
)";

        std::string_view const expected = R"(module Pipeline_pass_test;

struct Body
{
    mass: Float32 = 0.0f32;
    speed: Float32 = 0.0f32;
}

function keep(body: Body) -> ()
{
}

export function run(bodies: Array_slice::<mutable Body>, view: Soa_array_view::<mutable Body>) -> ()
{
    for $pipeline0_i in 0u64 to bodies.length
    {
        var $pipeline0_body = &bodies[$pipeline0_i];
        if $pipeline0_body->speed > 0.0f32
        {
            $pipeline0_body->mass += $pipeline0_body->speed;
            keep(*$pipeline0_body);
        }
    }
    for $pipeline1_i in 0u64 to view.length
    {
        if view->speed[$pipeline1_i] > 0.0f32
        {
            view->mass[$pipeline1_i] = 1.0f32;
            keep(view[$pipeline1_i]);
        }
    }
}
)";

        test_pipeline_pass_on_module(input, expected);
    }

    TEST_CASE("Pipeline pass lowers min_by and max_by to the best-so-far loop", "[Pipeline_pass][Passes][Pipeline]")
    {
        std::string_view const input = R"(module Pipeline_pass_test;

function negate(value: Int32) -> (result: Int32)
{
    return -value;
}

export function run(values: Array_slice::<Int32>) -> ()
{
    pipeline positive(value: Int32)
    {
        filter value > 0;
    }

    var smallest = positive(values).min_by(lambda (value) => value * value);
    var largest = positive(values).max_by(negate);
}
)";

        std::string_view const expected = R"(module Pipeline_pass_test;

function negate(value: Int32) -> (result: Int32)
{
    return -value;
}

export function run(values: Array_slice::<Int32>) -> ()
{
    var smallest = ${
        mutable $pipeline0_result: Optional::<Int32> = {};
        mutable $pipeline0_key: Optional::<Int32> = {};
        for $pipeline0_i in 0u64 to values.length
        {
            var value = values[$pipeline0_i];
            if value > 0
            {
                var $pipeline0_candidate = value * value;
                if !$pipeline0_key.has_value || $pipeline0_candidate < $pipeline0_key.value
                {
                    $pipeline0_key = create_optional($pipeline0_candidate);
                    $pipeline0_result = create_optional(value);
                }
            }
        }
        yield $pipeline0_result;
    };
    var largest = ${
        mutable $pipeline1_result: Optional::<Int32> = {};
        mutable $pipeline1_key: Optional::<Int32> = {};
        for $pipeline1_i in 0u64 to values.length
        {
            var value = values[$pipeline1_i];
            if value > 0
            {
                var $pipeline1_candidate = negate(value);
                if !$pipeline1_key.has_value || $pipeline1_key.value < $pipeline1_candidate
                {
                    $pipeline1_key = create_optional($pipeline1_candidate);
                    $pipeline1_result = create_optional(value);
                }
            }
        }
        yield $pipeline1_result;
    };
}
)";

        test_pipeline_pass_on_module(input, expected);
    }

    TEST_CASE("Pipeline pass expands a module-level pipeline in every function that consumes it", "[Pipeline_pass][Passes][Pipeline]")
    {
        std::string_view const input = R"(module Pipeline_pass_test;

pipeline positive(value: Int32)
{
    filter value > 0;
}

export function count_positive(values: Array_slice::<Int32>) -> (result: Uint64)
{
    return positive(values).count();
}

export function sum_positive(values: Array_slice::<Int32>) -> (result: Int32)
{
    mutable sum = 0;
    for value in positive(values)
    {
        sum += value;
    }
    return sum;
}
)";

        std::string_view const expected = R"(module Pipeline_pass_test;

export function count_positive(values: Array_slice::<Int32>) -> (result: Uint64)
{
    return ${
        mutable $pipeline0_count = 0u64;
        for $pipeline0_i in 0u64 to values.length
        {
            var value = values[$pipeline0_i];
            if value > 0
            {
                $pipeline0_count += 1u64;
            }
        }
        yield $pipeline0_count;
    };
}

export function sum_positive(values: Array_slice::<Int32>) -> (result: Int32)
{
    mutable sum = 0;
    for $pipeline0_i in 0u64 to values.length
    {
        var value = values[$pipeline0_i];
        if value > 0
        {
            sum += value;
        }
    }
    return sum;
}
)";

        test_pipeline_pass_on_module(input, expected);
    }

    TEST_CASE("Pipeline pass stops the loop at take_while and binds the index of enumerate", "[Pipeline_pass][Passes][Pipeline]")
    {
        std::string_view const input = R"(module Pipeline_pass_test;

export function run(values: Array_slice::<Int32>) -> ()
{
    pipeline positive_prefix(value: Int32)
    {
        enumerate => index;
        filter value % 2 == 0;
        take_while value > 0;
    }

    var count = positive_prefix(values).count();
}
)";

        std::string_view const expected = R"(module Pipeline_pass_test;

export function run(values: Array_slice::<Int32>) -> ()
{
    var count = ${
        mutable $pipeline0_count = 0u64;
        for $pipeline0_i in 0u64 to values.length
        {
            var value = values[$pipeline0_i];
            var index = $pipeline0_i;
            if value % 2 == 0
            {
                if !(value > 0)
                {
                    break;
                }
                $pipeline0_count += 1u64;
            }
        }
        yield $pipeline0_count;
    };
}
)";

        test_pipeline_pass_on_module(input, expected);
    }
}
