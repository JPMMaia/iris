module iris.compiler.test_framework;

import std;

import iris.core;
import iris.core.types;
import iris.parser.convertor;

namespace iris::compiler
{
    std::pmr::string get_test_module_name(
        std::string_view const artifact_name
    )
    {
        return std::pmr::string{std::format("{}.generated_tests_information", artifact_name)};
    }

    struct Test_info
    {
        std::uint64_t module_index;
        std::string_view module_name;
        std::pmr::string module_alias_name;
        std::string_view test_name;
        std::uint64_t source_file_line;
    };

    static std::pmr::string create_module_alias_name(std::string_view const module_name)
    {
        std::pmr::string output{module_name};
        std::replace(output.begin(), output.end(), '.', '_');
        return output;
    }

    static std::pmr::vector<Test_info> get_test_infos(
        std::span<iris::Module const* const> const core_modules,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<Test_info> test_infos;
        test_infos.reserve(core_modules.size());

        auto const add_test_infos = [&](
            std::uint64_t const module_index,
            std::string_view const module_name,
            std::pmr::string const& module_alias_name,
            std::span<iris::Function_declaration const> const declarations
        ) -> void
        {
            for (iris::Function_declaration const& declaration : declarations)
            {
                if (declaration.is_test)
                {
                    Test_info test_info
                    {
                        .module_index = module_index,
                        .module_name = module_name,
                        .module_alias_name = module_alias_name,
                        .test_name = declaration.name,
                        .source_file_line = declaration.source_location.has_value() ? declaration.source_location->range.start.line : 0
                    };

                    test_infos.push_back(std::move(test_info));
                }
            }
        };

        for (std::uint64_t module_index = 0; module_index < core_modules.size(); ++module_index)
        {
            iris::Module const& core_module = *core_modules[module_index];
            std::pmr::string const module_alias_name = create_module_alias_name(core_module.name);
            add_test_infos(module_index, core_module.name, module_alias_name, core_module.export_declarations.function_declarations);
            add_test_infos(module_index, core_module.name, module_alias_name, core_module.internal_declarations.function_declarations);
        }

        return test_infos;
    }

    static std::pmr::string create_imports(
        std::span<iris::Module const* const> const core_modules,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::stringstream stream;

        for (iris::Module const* const core_module : core_modules)
        {
            stream << "import " << core_module->name << " as " << create_module_alias_name(core_module->name) << ";\n";
        }

        return std::pmr::string{stream.str()};
    }

    static std::pmr::string create_module_source_files(
        std::span<iris::Module const* const> const core_modules,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::stringstream stream;

        for (std::size_t index = 0; index < core_modules.size(); ++index)
        {
            iris::Module const& core_module = *core_modules[index];
            std::string const source_file_path = core_module.source_file_path.has_value() ? core_module.source_file_path->generic_string() : std::string{};
            stream << "    \"" << source_file_path << "\"c";
            if (index + 1 < core_modules.size())
                stream << ',';
            stream << '\n';
        }

        return std::pmr::string{stream.str()};
    }

    static std::pmr::string create_test_names(
        std::span<Test_info const> const test_infos,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::stringstream stream;

        for (std::size_t index = 0; index < test_infos.size(); ++index)
        {
            Test_info const test_info = test_infos[index];
            stream << "    \"" << test_info.module_name << '.' << test_info.test_name << "\"c";
            if (index + 1 < test_infos.size())
                stream << ',';
            stream << '\n';
        }

        return std::pmr::string{stream.str()};
    }

    static std::pmr::string create_test_module_indices(
        std::span<Test_info const> const test_infos,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::stringstream stream;

        for (std::size_t index = 0; index < test_infos.size(); ++index)
        {
            Test_info const test_info = test_infos[index];
            stream  << test_info.module_index << "u64";
            if (index + 1 < test_infos.size())
                stream << ',';
            stream << '\n';
        }

        return std::pmr::string{stream.str()};
    }

    static std::pmr::string create_test_source_file_lines(
        std::span<Test_info const> const test_infos,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::stringstream stream;

        for (std::size_t index = 0; index < test_infos.size(); ++index)
        {
            Test_info const test_info = test_infos[index];
            stream  << test_info.source_file_line << "u64";
            if (index + 1 < test_infos.size())
                stream << ',';
            stream << '\n';
        }

        return std::pmr::string{stream.str()};
    }

    static std::pmr::string create_test_function_pointers(
        std::span<Test_info const> const test_infos,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::stringstream stream;

        for (std::size_t index = 0; index < test_infos.size(); ++index)
        {
            Test_info const test_info = test_infos[index];
            stream << "    " << test_info.module_alias_name << '.' << test_info.test_name << "";
            if (index + 1 < test_infos.size())
                stream << ',';
            stream << '\n';
        }

        return std::pmr::string{stream.str()};
    }

    std::optional<iris::Module> create_test_module(
        std::string_view const artifact_name,
        std::span<iris::Module const* const> const core_modules,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        constexpr char const* test_template = R"RAW(module {};

{}
export using Test_function = function<() -> ()>;

var g_module_source_files: Constant_array::<*C_char> = [{}];
var g_test_names: Constant_array::<*C_char> = [{}];
var g_test_module_indices: Constant_array::<Uint64> = [{}];
var g_test_source_file_lines: Constant_array::<Uint64> = [{}];
var g_tests: Constant_array::<Test_function> = [{}];

@unique_name("iris_get_test_count")
export function get_test_count() -> (result: Uint64)
{{
    return {}u64;
}}

@unique_name("iris_get_test_names")
export function get_test_names() -> (result: **C_char)
{{
    return &g_test_names[0];
}}

@unique_name("iris_get_test_source_file")
export function get_test_source_file(test_index: Uint64) -> (source_file_path: *C_char)
{{
    var module_index = g_test_module_indices[test_index];
    return g_module_source_files[module_index];
}}

@unique_name("iris_get_test_source_file_lines")
export function get_test_source_file_lines() -> (result: *Uint64)
{{
    return &g_test_source_file_lines[0];
}}

@unique_name("iris_get_tests")
export function get_tests() -> (result: *Test_function)
{{
    return &g_tests[0];
}}
)RAW";

        std::pmr::vector<Test_info> const test_infos = get_test_infos(core_modules, temporaries_allocator);
        if (test_infos.empty())
            return std::nullopt;

        std::pmr::string const test_module_name = get_test_module_name(artifact_name);
        std::pmr::string const imports = create_imports(core_modules, temporaries_allocator);
        std::pmr::string const module_source_files = create_module_source_files(core_modules, temporaries_allocator);
        std::pmr::string const test_names = create_test_names(test_infos, temporaries_allocator);
        std::pmr::string const test_module_indices = create_test_module_indices(test_infos, temporaries_allocator);
        std::pmr::string const test_source_file_lines = create_test_source_file_lines(test_infos, temporaries_allocator);
        std::pmr::string const test_pointers = create_test_function_pointers(test_infos, temporaries_allocator);

        std::string const test_module_code = std::format(test_template, test_module_name, imports, module_source_files, test_names, test_module_indices, test_source_file_lines, test_pointers, test_infos.size());

        std::optional<iris::Module> const test_module = iris::parser::parse_and_convert_to_module(
            test_module_code,
            std::nullopt,
            temporaries_allocator,
            temporaries_allocator
        );

        return test_module;
    }

    iris::Function_pointer_type create_test_check_function_pointer_type()
    {
        return iris::Function_pointer_type {
            .type = {
                .input_parameter_types = {
                    iris::create_bool_type_reference(),
                    iris::create_c_string_type_reference(false),
                    iris::create_integer_type_type_reference(64, false),
                },
                .output_parameter_types = {},
                .is_variadic = false,
            },
            .input_parameter_names = {"condition", "source_file_path", "line"},
            .output_parameter_names = {},
        };
    }

    iris::Function_declaration create_test_check_function_declaration()
    {
        iris::Function_pointer_type function_pointer_type = create_test_check_function_pointer_type();

        return iris::Function_declaration{
            .name = "iris_test_check",
            .unique_name = "iris_test_check",
            .type = std::move(function_pointer_type.type),
            .input_parameter_names = std::move(function_pointer_type.input_parameter_names),
            .output_parameter_names = std::move(function_pointer_type.output_parameter_names),
            .linkage = iris::Linkage::External,
            .is_test = false
        };
    }
}
