#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

import h.core;
import h.core.declarations;
import h.c_header_exporter;
import h.json_serializer.operators;
import h.parser.convertor;

using h::json::operators::operator<<;

#include <catch2/catch_all.hpp>

namespace h::c
{
    static std::pmr::string get_module_namespace(
        std::string_view const core_module_name
    )
    {
        std::pmr::string module_namespace{core_module_name};
        std::replace(module_namespace.begin(), module_namespace.end(), '.', '_');
        return module_namespace;
    }

    static std::pmr::string create_expected_c_header_content(
        std::string_view const module_name,
        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const dependencies_c_file_paths,
        std::string_view const content
    )
    {
        constexpr char const* const template_string =
            "#ifndef {}\n"
            "#define {}\n\n"
            "{}\n"
            "#ifdef __cplusplus\n"
            "extern \"C\" {{\n"
            "#endif\n"
            "{}\n"
            "#ifdef __cplusplus\n"
            "}}\n"
            "#endif\n\n"
            "#endif\n";

        std::stringstream include_stream;
        include_stream << "#include <iris_builtin.h>\n\n";
        for (std::pair<std::pmr::string const, std::filesystem::path> const& pair : dependencies_c_file_paths)
            include_stream << "#include <" << pair.second.generic_string() << ">\n";
        if (!dependencies_c_file_paths.empty())
            include_stream << '\n';
        include_stream << "#include <stdint.h>\n";
        std::string const includes = include_stream.str();

        std::pmr::string const include_guard_name = get_module_namespace(module_name);
        return std::pmr::string{std::vformat(template_string, std::make_format_args(include_guard_name, include_guard_name, includes, content))};
    }

    static void test_c_exporter(
        std::string_view const source,
        std::pmr::unordered_map<std::pmr::string, std::string_view> const dependencies,
        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const dependencies_c_file_paths,
        std::string_view const expected_content
    )
    {
        std::optional<h::Module> const core_module = h::parser::parse_and_convert_to_module(source, std::nullopt, {}, {});
        REQUIRE(core_module.has_value());

        std::pmr::unordered_map<std::pmr::string, h::Module> core_module_dependencies;
        core_module_dependencies.reserve(dependencies.size());

        for (std::pair<std::pmr::string const, std::string_view> const& dependency : dependencies)
        {
            std::optional<h::Module> core_module_dependency = h::parser::parse_and_convert_to_module(dependency.second, std::nullopt, {}, {});
            REQUIRE(core_module_dependency.has_value());

            core_module_dependencies[dependency.first] = std::move(core_module_dependency.value());
        }

        h::Declaration_database declaration_database = h::create_declaration_database();
        for (std::pair<std::pmr::string const, h::Module> const& pair : core_module_dependencies)
            h::add_declarations(declaration_database, pair.second);
        h::add_declarations(declaration_database, core_module.value());

        std::pmr::string const full_expected_content = create_expected_c_header_content(core_module->name, dependencies_c_file_paths, expected_content);

        Exported_c_header const exported_c_header = export_module_as_c_header(core_module.value(), declaration_database, dependencies_c_file_paths, {}, {});
        CHECK(exported_c_header.content == full_expected_content);
    }

    static std::pmr::string create_expected_cpp_header_content(
        std::string_view const module_name,
        std::filesystem::path const& c_header_file_path,
        std::string_view const content
    )
    {
        constexpr char const* const template_string =
            "#ifndef {}\n"
            "#define {}\n\n"
            "#include <{}>\n"
            "{}\n"
            "#endif\n";

        std::pmr::string const include_guard_name = get_module_namespace(module_name) + "_HPP";
        std::string const c_header_file_path_string = c_header_file_path.generic_string();
        return std::pmr::string{std::vformat(template_string, std::make_format_args(include_guard_name, include_guard_name, c_header_file_path_string, content))};
    }

    static void test_cpp_exporter(
        std::string_view const source,
        std::filesystem::path const& c_header_file_path,
        std::string_view const expected_content
    )
    {
        std::optional<h::Module> const core_module = h::parser::parse_and_convert_to_module(source, std::nullopt, {}, {});
        REQUIRE(core_module.has_value());

        std::pmr::string const full_expected_content = create_expected_cpp_header_content(core_module->name, c_header_file_path, expected_content);

        Exported_cpp_header const exported_cpp_header = export_module_as_cpp_header(core_module.value(), c_header_file_path, {}, {});
        CHECK(exported_cpp_header.content == full_expected_content);
    }

    TEST_CASE("Export structs")
    {
        std::string_view const input = R"RAW(module my.namespace;
export struct My_struct
{
    a: Int32 = 0u64;
}
)RAW";

        std::string_view const expected = R"RAW(
struct my_namespace_My_struct
{
    int32_t a;
};

struct Array_slice_my_namespace_My_struct
{
    struct my_namespace_My_struct* data;
    uint64_t size;
};
)RAW";

        std::string_view const cpp_expected = R"RAW(
namespace my::namespace
{
    using My_struct = ::my_namespace_My_struct;
    using Array_slice_My_struct = ::Array_slice_my_namespace_My_struct;
}
)RAW";

        test_c_exporter(input, {}, {}, expected);
        test_cpp_exporter(input, "input.h", cpp_expected);
    }

    TEST_CASE("Export functions")
    {
        std::string_view const input = R"RAW(module my.namespace;
export struct My_struct
{
    a: Int32 = 0;
}

function my_private_function() -> ();

export function my_public_function(a: My_struct, b: *My_struct) -> (result: My_struct);
export function my_public_function_2() -> ();
)RAW";

        std::string_view const expected = R"RAW(
struct my_namespace_My_struct
{
    int32_t a;
};

struct Array_slice_my_namespace_My_struct
{
    struct my_namespace_My_struct* data;
    uint64_t size;
};

struct my_namespace_My_struct my_namespace_my_public_function(struct my_namespace_My_struct a, struct my_namespace_My_struct const* b);
void my_namespace_my_public_function_2();
)RAW";

        std::string_view const cpp_expected = R"RAW(
namespace my::namespace
{
    using My_struct = ::my_namespace_My_struct;
    using Array_slice_My_struct = ::Array_slice_my_namespace_My_struct;

    inline auto my_public_function(My_struct a, My_struct const* b) -> My_struct { return ::my_namespace_my_public_function(a, b); }
    inline auto my_public_function_2() -> void { ::my_namespace_my_public_function_2(); }
}
)RAW";

        test_c_exporter(input, {}, {}, expected);
        test_cpp_exporter(input, "input.h", cpp_expected);
    }

    TEST_CASE("Export Constant_array")
    {
        std::string_view const input = R"RAW(module my.namespace;
export struct My_struct
{
    a: Constant_array::<Int32, 4> = [0, 1, 2, 3];
}
)RAW";

        std::string_view const expected = R"RAW(
struct my_namespace_My_struct
{
    int32_t a[4];
};

struct Array_slice_my_namespace_My_struct
{
    struct my_namespace_My_struct* data;
    uint64_t size;
};
)RAW";

        test_c_exporter(input, {}, {}, expected);
    }

    TEST_CASE("Export Array_slice")
    {
        std::string_view const input = R"RAW(module my.namespace;

export struct My_struct
{
    elements: Array_slice::<Int32> = {};
}
)RAW";

        std::string_view const expected = R"RAW(
struct my_namespace_My_struct
{
    struct Array_slice_Int32 elements;
};

struct Array_slice_my_namespace_My_struct
{
    struct my_namespace_My_struct* data;
    uint64_t size;
};
)RAW";

        test_c_exporter(input, {}, {}, expected);
    }

    TEST_CASE("Export Function Pointers")
    {
        std::string_view const input = R"RAW(module my.namespace;

export struct Allocator
{
    allocate: function<(size: Uint64, alignment: Uint64) -> (memory: *mutable Void)> = null;
    deallocate: function<(memory: *mutable Void) -> ()> = null;
}
)RAW";

        std::string_view const expected = R"RAW(
struct my_namespace_Allocator
{
    void*(*allocate)(uint64_t size, uint64_t alignment);
    void(*deallocate)(void* memory);
};

struct Array_slice_my_namespace_Allocator
{
    struct my_namespace_Allocator* data;
    uint64_t size;
};
)RAW";

        test_c_exporter(input, {}, {}, expected);
    }

    TEST_CASE("Export declarations in the correct order")
    {
        std::string_view const input = R"RAW(module my.namespace;

export struct My_node
{
    parent: *My_node = null;
}

export struct My_struct_a
{
    b: My_struct_b = {};
}

export struct My_struct_b
{
    v: Int32 = 0;
}
)RAW";

        std::string_view const expected = R"RAW(
struct my_namespace_My_node
{
    struct my_namespace_My_node const* parent;
};

struct Array_slice_my_namespace_My_node
{
    struct my_namespace_My_node* data;
    uint64_t size;
};

struct my_namespace_My_struct_b
{
    int32_t v;
};

struct Array_slice_my_namespace_My_struct_b
{
    struct my_namespace_My_struct_b* data;
    uint64_t size;
};

struct my_namespace_My_struct_a
{
    struct my_namespace_My_struct_b b;
};

struct Array_slice_my_namespace_My_struct_a
{
    struct my_namespace_My_struct_a* data;
    uint64_t size;
};
)RAW";

        test_c_exporter(input, {}, {}, expected);
    }

    TEST_CASE("Exports includes")
    {
        std::string_view const module_a = R"RAW(module my_library.module_a;
export struct My_struct
{
    a: Int32 = 0;
}
)RAW";

        std::string_view const input = R"RAW(module my.namespace;

import my_library.module_a as ma;

export struct My_struct
{
    b: ma.My_struct = {};
}
)RAW";

        std::pmr::unordered_map<std::pmr::string, std::string_view> const dependencies
        {
            { "my_library.module_a", module_a }
        };

        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const dependencies_c_file_paths
        {
            { "my_library.module_a", std::filesystem::path{"my_library/module_a.h"} }
        };

        std::string_view const expected = R"RAW(
struct my_namespace_My_struct
{
    struct my_library_module_a_My_struct b;
};

struct Array_slice_my_namespace_My_struct
{
    struct my_namespace_My_struct* data;
    uint64_t size;
};
)RAW";

        test_c_exporter(input, dependencies, dependencies_c_file_paths, expected);
    }
}
