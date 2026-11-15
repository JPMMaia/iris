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
