#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

import iris.core;
import iris.core.declarations;
import iris.c_header_exporter;
import iris.json_serializer.operators;
import iris.parser.convertor;

using iris::json::operators::operator<<;

#include <catch2/catch_all.hpp>

namespace iris::c
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
        std::optional<iris::Module> const core_module = iris::parser::parse_and_convert_to_module(source, std::nullopt, {}, {});
        REQUIRE(core_module.has_value());

        std::pmr::unordered_map<std::pmr::string, iris::Module> core_module_dependencies;
        core_module_dependencies.reserve(dependencies.size());

        for (std::pair<std::pmr::string const, std::string_view> const& dependency : dependencies)
        {
            std::optional<iris::Module> core_module_dependency = iris::parser::parse_and_convert_to_module(dependency.second, std::nullopt, {}, {});
            REQUIRE(core_module_dependency.has_value());

            core_module_dependencies[dependency.first] = std::move(core_module_dependency.value());
        }

        iris::Declaration_database declaration_database = iris::create_declaration_database();
        for (std::pair<std::pmr::string const, iris::Module> const& pair : core_module_dependencies)
            iris::add_declarations(declaration_database, pair.second);
        iris::add_declarations(declaration_database, core_module.value());

        std::pmr::string const full_expected_content = create_expected_c_header_content(core_module->name, dependencies_c_file_paths, expected_content);

        Exported_c_header const exported_c_header = export_module_as_c_header(core_module.value(), declaration_database, dependencies_c_file_paths, {}, {});
        CHECK(exported_c_header.content == full_expected_content);
    }

    static std::pmr::string export_c_header_content(
        std::string_view const source,
        std::pmr::unordered_map<std::pmr::string, std::string_view> const dependencies,
        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const dependencies_c_file_paths
    )
    {
        std::optional<iris::Module> const core_module = iris::parser::parse_and_convert_to_module(source, std::nullopt, {}, {});
        REQUIRE(core_module.has_value());

        std::pmr::unordered_map<std::pmr::string, iris::Module> core_module_dependencies;
        core_module_dependencies.reserve(dependencies.size());

        for (std::pair<std::pmr::string const, std::string_view> const& dependency : dependencies)
        {
            std::optional<iris::Module> core_module_dependency = iris::parser::parse_and_convert_to_module(dependency.second, std::nullopt, {}, {});
            REQUIRE(core_module_dependency.has_value());

            core_module_dependencies[dependency.first] = std::move(core_module_dependency.value());
        }

        iris::Declaration_database declaration_database = iris::create_declaration_database();
        for (std::pair<std::pmr::string const, iris::Module> const& pair : core_module_dependencies)
            iris::add_declarations(declaration_database, pair.second);
        iris::add_declarations(declaration_database, core_module.value());

        Exported_c_header const exported_c_header = export_module_as_c_header(core_module.value(), declaration_database, dependencies_c_file_paths, {}, {});
        return exported_c_header.content;
    }

    static std::pmr::string create_c_type_instance_name_for_test(
        iris::Type_instance const& type_instance
    )
    {
        std::pmr::string const mangled_name = iris::mangle_type_instance_name(type_instance);
        std::string_view const separator = iris::get_mangled_instance_separator();

        std::string_view suffix = mangled_name;
        std::size_t const first_separator_location = mangled_name.find(separator);
        if (first_separator_location != std::string_view::npos)
            suffix = std::string_view{mangled_name}.substr(first_separator_location + separator.size());

        std::pmr::string result;
        result.reserve(type_instance.type_constructor.module_reference.name.size() + 1 + suffix.size());

        for (char const character : type_instance.type_constructor.module_reference.name)
            result.push_back(character == '.' ? '_' : character);

        result.push_back('_');

        for (char const character : suffix)
            result.push_back(character == '.' ? '_' : character);

        return result;
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
        std::optional<iris::Module> const core_module = iris::parser::parse_and_convert_to_module(source, std::nullopt, {}, {});
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
/** IRIS_META v=1 module=my.namespace name=My_struct kind=struct */
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
/** IRIS_META v=1 module=my.namespace name=My_struct kind=struct */
struct my_namespace_My_struct
{
    int32_t a;
};

struct Array_slice_my_namespace_My_struct
{
    struct my_namespace_My_struct* data;
    uint64_t size;
};

/** IRIS_META v=1 module=my.namespace name=my_public_function kind=function */
struct my_namespace_My_struct my_namespace_my_public_function(struct my_namespace_My_struct a, struct my_namespace_My_struct const* b);
/** IRIS_META v=1 module=my.namespace name=my_public_function_2 kind=function */
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
/** IRIS_META v=1 module=my.namespace name=My_struct kind=struct */
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
/** IRIS_META v=1 module=my.namespace name=My_struct kind=struct */
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
/** IRIS_META v=1 module=my.namespace name=Allocator kind=struct */
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
/** IRIS_META v=1 module=my.namespace name=My_node kind=struct */
struct my_namespace_My_node
{
    struct my_namespace_My_node const* parent;
};

struct Array_slice_my_namespace_My_node
{
    struct my_namespace_My_node* data;
    uint64_t size;
};

/** IRIS_META v=1 module=my.namespace name=My_struct_b kind=struct */
struct my_namespace_My_struct_b
{
    int32_t v;
};

struct Array_slice_my_namespace_My_struct_b
{
    struct my_namespace_My_struct_b* data;
    uint64_t size;
};

/** IRIS_META v=1 module=my.namespace name=My_struct_a kind=struct */
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
/** IRIS_META v=1 module=my.namespace name=My_struct kind=struct */
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

    TEST_CASE("Exports instantiated types")
    {
        std::string_view const input = R"RAW(module my.namespace;

export type_constructor Vector3(Value_type: Type)
{
    return struct
    {
        x: Value_type = 0 as Value_type;
        y: Value_type = 0 as Value_type;
        z: Value_type = 0 as Value_type;
    };
}

export using Vector3f32 = Vector3::<Float32>;

export struct Transformf32
{
    translation: Vector3f32 = {};
}
)RAW";

        std::pmr::unordered_map<std::pmr::string, std::string_view> const dependencies
        {
        };

        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const dependencies_c_file_paths
        {
        };

        std::string_view const expected = R"RAW(
/** IRIS_META v=1 module=my.namespace name=Vector3f32 kind=struct */
struct my_namespace_Vector3f32
{
    float x;
    float y;
    float z;
};

struct Array_slice_my_namespace_Vector3f32
{
    struct my_namespace_Vector3f32* data;
    uint64_t size;
};

/** IRIS_META v=1 module=my.namespace name=Transformf32 kind=struct */
struct my_namespace_Transformf32
{
    struct my_namespace_Vector3f32 translation;
};

struct Array_slice_my_namespace_Transformf32
{
    struct my_namespace_Transformf32* data;
    uint64_t size;
};
)RAW";

        test_c_exporter(input, dependencies, dependencies_c_file_paths, expected);
    }

    TEST_CASE("Exports function constructor globals as C function pointers")
    {
        std::string_view const input = R"RAW(module my_namespace;

export function_constructor foo(Value_type: Type)
{
    return function(value: Value_type) -> ()
    {
    };
}

export var bar = foo::<Float32>;
)RAW";

        std::pmr::unordered_map<std::pmr::string, std::string_view> const dependencies
        {
        };

        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const dependencies_c_file_paths
        {
        };

        std::string_view const expected = R"RAW(
/** IRIS_META v=1 module=my_namespace name=bar kind=global */
extern void(*my_namespace_bar)(float value);
)RAW";

        std::string_view const cpp_expected = R"RAW(
namespace my_namespace
{

    inline auto& bar = ::my_namespace_bar;
}
)RAW";

        test_c_exporter(input, dependencies, dependencies_c_file_paths, expected);
        test_cpp_exporter(input, "input.h", cpp_expected);
    }

    TEST_CASE("Exports Type_instance in direct struct members")
    {
        std::string_view const input = R"RAW(module my.namespace;

export type_constructor Vector3(Value_type: Type)
{
    return struct
    {
        x: Value_type = 0 as Value_type;
        y: Value_type = 0 as Value_type;
        z: Value_type = 0 as Value_type;
    };
}

export struct Transformf32
{
    translation: Vector3::<Float32> = {};
}
)RAW";

        std::optional<iris::Module> const core_module = iris::parser::parse_and_convert_to_module(input, std::nullopt, {}, {});
        REQUIRE(core_module.has_value());

        iris::Type_instance const& type_instance = std::get<iris::Type_instance>(core_module->export_declarations.struct_declarations[0].member_types[0].data);
        std::pmr::string const type_instance_name = create_c_type_instance_name_for_test(type_instance);

        std::pmr::string const expected = std::pmr::string{std::format(
            R"RAW(
/** IRIS_META v=1 module=my.namespace name=Transformf32 kind=struct */
struct my_namespace_Transformf32
{{
    struct {} translation;
}};

struct Array_slice_my_namespace_Transformf32
{{
    struct my_namespace_Transformf32* data;
    uint64_t size;
}};
)RAW",
            type_instance_name
        )};

        test_c_exporter(input, {}, {}, expected);
    }

    TEST_CASE("Exports Type_instance in function signatures")
    {
        std::string_view const input = R"RAW(module my.namespace;

export type_constructor Vector3(Value_type: Type)
{
    return struct
    {
        x: Value_type = 0 as Value_type;
        y: Value_type = 0 as Value_type;
        z: Value_type = 0 as Value_type;
    };
}

export function make_vector() -> (value: Vector3::<Float32>);
export function process_vector(value: Vector3::<Float32>) -> (result: Vector3::<Float32>);
)RAW";

        std::optional<iris::Module> const core_module = iris::parser::parse_and_convert_to_module(input, std::nullopt, {}, {});
        REQUIRE(core_module.has_value());

        iris::Type_instance const& type_instance = std::get<iris::Type_instance>(core_module->export_declarations.function_declarations[0].type.output_parameter_types[0].data);
        std::pmr::string const type_instance_name = create_c_type_instance_name_for_test(type_instance);

        std::pmr::string const expected = std::pmr::string{std::format(
            R"RAW(
/** IRIS_META v=1 module=my.namespace name=make_vector kind=function */
struct {} my_namespace_make_vector();
/** IRIS_META v=1 module=my.namespace name=process_vector kind=function */
struct {} my_namespace_process_vector(struct {} value);
)RAW",
            type_instance_name,
            type_instance_name,
            type_instance_name
        )};

        test_c_exporter(input, {}, {}, expected);
    }

    TEST_CASE("Exports Type_instance inside Array_slice")
    {
        std::string_view const input = R"RAW(module my.namespace;

export type_constructor Vector3(Value_type: Type)
{
    return struct
    {
        x: Value_type = 0 as Value_type;
        y: Value_type = 0 as Value_type;
        z: Value_type = 0 as Value_type;
    };
}

export struct Buffer
{
    elements: Array_slice::<Vector3::<Float32>> = {};
}
)RAW";

        std::optional<iris::Module> const core_module = iris::parser::parse_and_convert_to_module(input, std::nullopt, {}, {});
        REQUIRE(core_module.has_value());

        iris::Array_slice_type const& array_slice_type = std::get<iris::Array_slice_type>(core_module->export_declarations.struct_declarations[0].member_types[0].data);
        iris::Type_instance const& type_instance = std::get<iris::Type_instance>(array_slice_type.element_type[0].data);
        std::pmr::string const type_instance_name = create_c_type_instance_name_for_test(type_instance);

        std::pmr::string const expected = std::pmr::string{std::format(
            R"RAW(
/** IRIS_META v=1 module=my.namespace name=Buffer kind=struct */
struct my_namespace_Buffer
{{
    struct Array_slice_{} elements;
}};

struct Array_slice_my_namespace_Buffer
{{
    struct my_namespace_Buffer* data;
    uint64_t size;
}};
)RAW",
            type_instance_name
        )};

        test_c_exporter(input, {}, {}, expected);
    }

    TEST_CASE("Prefers alias name when Type_instance alias exists")
    {
        std::string_view const input = R"RAW(module my.namespace;

export type_constructor Vector3(Value_type: Type)
{
    return struct
    {
        x: Value_type = 0 as Value_type;
        y: Value_type = 0 as Value_type;
        z: Value_type = 0 as Value_type;
    };
}

export using Vector3f32 = Vector3::<Float32>;

export function process_vector(value: Vector3f32) -> (result: Vector3f32);
)RAW";

        std::string_view const expected = R"RAW(
/** IRIS_META v=1 module=my.namespace name=Vector3f32 kind=struct */
struct my_namespace_Vector3f32
{
    float x;
    float y;
    float z;
};

struct Array_slice_my_namespace_Vector3f32
{
    struct my_namespace_Vector3f32* data;
    uint64_t size;
};

/** IRIS_META v=1 module=my.namespace name=process_vector kind=function */
struct my_namespace_Vector3f32 my_namespace_process_vector(struct my_namespace_Vector3f32 value);
)RAW";

        test_c_exporter(input, {}, {}, expected);
    }

    TEST_CASE("Exports IRIS_META comments for struct and function declarations")
    {
        std::string_view const input = R"RAW(module my.namespace;

export struct Camera
{
    id: Int32 = 0;
}

export function create_camera() -> (result: Camera);
)RAW";

        std::string_view const expected = R"RAW(
/** IRIS_META v=1 module=my.namespace name=Camera kind=struct */
struct my_namespace_Camera
{
    int32_t id;
};

struct Array_slice_my_namespace_Camera
{
    struct my_namespace_Camera* data;
    uint64_t size;
};

/** IRIS_META v=1 module=my.namespace name=create_camera kind=function */
struct my_namespace_Camera my_namespace_create_camera();
)RAW";

        test_c_exporter(input, {}, {}, expected);
    }

    TEST_CASE("Exports IRIS_META comments for global variables")
    {
        std::string_view const input = R"RAW(module my.namespace;

export function_constructor make_callback(Value_type: Type)
{
    return function(value: Value_type) -> ()
    {
    };
}

export var on_update = make_callback::<Float32>;
)RAW";

        std::string_view const expected = R"RAW(
/** IRIS_META v=1 module=my.namespace name=on_update kind=global */
extern void(*my_namespace_on_update)(float value);
)RAW";

        test_c_exporter(input, {}, {}, expected);
    }
}
