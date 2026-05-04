module iris.c_header_exporter;

import std;

import iris.core;
import iris.core.declarations;
import iris.core.formatter;
import iris.core.types;

namespace iris::c
{
    static bool contains_declaration(
        std::span<iris::Declaration const> const declarations,
        iris::Declaration const& declaration
    )
    {
        for (iris::Declaration const& element : declarations)
        {
            if (element.data == declaration.data)
                return true;
        }

        return false;
    }

    static void add_alias_type_declaration(
        std::pmr::vector<iris::Declaration>& sorted_declarations,
        iris::Module const& core_module,
        iris::Alias_type_declaration const& declaration
    );

    static void add_struct_declaration(
        std::pmr::vector<iris::Declaration>& sorted_declarations,
        iris::Module const& core_module,
        iris::Struct_declaration const& declaration
    );

    static void add_union_declaration(
        std::pmr::vector<iris::Declaration>& sorted_declarations,
        iris::Module const& core_module,
        iris::Union_declaration const& declaration
    );

    static void add_type_reference_declaration(
        std::pmr::vector<iris::Declaration>& sorted_declarations,
        iris::Module const& core_module,
        iris::Type_reference const& type_reference
    )
    {
        if (std::holds_alternative<iris::Constant_array_type>(type_reference.data))
        {
            iris::Constant_array_type const& constant_array_type = std::get<iris::Constant_array_type>(type_reference.data);
            if (!constant_array_type.value_type.empty())
                add_type_reference_declaration(sorted_declarations, core_module, constant_array_type.value_type[0]);
        }
        else if (std::holds_alternative<iris::Custom_type_reference>(type_reference.data))
        {
            iris::Custom_type_reference const& custom_type_reference = std::get<iris::Custom_type_reference>(type_reference.data);
            if (custom_type_reference.module_reference.name == core_module.name)
            {
                std::optional<Alias_type_declaration const*> const alias_type = find_alias_type_declaration(core_module, custom_type_reference.name);
                if (alias_type.has_value())
                {
                    add_alias_type_declaration(sorted_declarations, core_module, *alias_type.value());
                    return;
                }

                std::optional<Struct_declaration const*> const struct_type = find_struct_declaration(core_module, custom_type_reference.name);
                if (struct_type.has_value())
                {
                    add_struct_declaration(sorted_declarations, core_module, *struct_type.value());
                    return;
                }

                std::optional<Union_declaration const*> const union_type = find_union_declaration(core_module, custom_type_reference.name);
                if (union_type.has_value())
                {
                    add_union_declaration(sorted_declarations, core_module, *union_type.value());
                    return;
                }
            }
        }
        else if (std::holds_alternative<iris::Function_pointer_type>(type_reference.data))
        {
            iris::Function_pointer_type const& function_pointer_type = std::get<iris::Function_pointer_type>(type_reference.data);
            for (iris::Type_reference const& type : function_pointer_type.type.input_parameter_types)
                add_type_reference_declaration(sorted_declarations, core_module, type);
            for (iris::Type_reference const& type : function_pointer_type.type.output_parameter_types)
                add_type_reference_declaration(sorted_declarations, core_module, type);
        }
    }

    static void add_alias_type_declaration(
        std::pmr::vector<iris::Declaration>& sorted_declarations,
        iris::Module const& core_module,
        iris::Alias_type_declaration const& declaration
    )
    {
        if (contains_declaration(sorted_declarations, iris::Declaration{.data = &declaration}))
            return;

        if (!declaration.type.empty())
            add_type_reference_declaration(sorted_declarations, core_module, declaration.type[0]);
        
        sorted_declarations.push_back(iris::Declaration{.data = &declaration});
    }

    void add_struct_declaration(
        std::pmr::vector<iris::Declaration>& sorted_declarations,
        iris::Module const& core_module,
        iris::Struct_declaration const& declaration
    )
    {
        if (contains_declaration(sorted_declarations, iris::Declaration{.data = &declaration}))
            return;

        for (iris::Type_reference const& member_type : declaration.member_types)
            add_type_reference_declaration(sorted_declarations, core_module, member_type);

        sorted_declarations.push_back(iris::Declaration{.data = &declaration});
    }

    void add_union_declaration(
        std::pmr::vector<iris::Declaration>& sorted_declarations,
        iris::Module const& core_module,
        iris::Union_declaration const& declaration
    )
    {
        if (contains_declaration(sorted_declarations, iris::Declaration{.data = &declaration}))
            return;

        for (iris::Type_reference const& member_type : declaration.member_types)
            add_type_reference_declaration(sorted_declarations, core_module, member_type);

        sorted_declarations.push_back(iris::Declaration{.data = &declaration});
    }

    static std::pmr::vector<iris::Declaration> sort_declarations(
        iris::Module const& core_module,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<iris::Declaration> sorted_declarations{temporaries_allocator};

        for (iris::Enum_declaration const& declaration : core_module.export_declarations.enum_declarations)
        {
            sorted_declarations.push_back(iris::Declaration{.data = &declaration});
        }

        for (iris::Enum_declaration const& declaration : core_module.internal_declarations.enum_declarations)
        {
            sorted_declarations.push_back(iris::Declaration{.data = &declaration});
        }

        for (iris::Alias_type_declaration const& declaration : core_module.export_declarations.alias_type_declarations)
        {
            add_alias_type_declaration(sorted_declarations, core_module, declaration);
        }

        for (iris::Alias_type_declaration const& declaration : core_module.internal_declarations.alias_type_declarations)
        {
            add_alias_type_declaration(sorted_declarations, core_module, declaration);
        }

        for (iris::Struct_declaration const& declaration : core_module.export_declarations.struct_declarations)
        {
            add_struct_declaration(sorted_declarations, core_module, declaration);
        }

        for (iris::Struct_declaration const& declaration : core_module.internal_declarations.struct_declarations)
        {
            add_struct_declaration(sorted_declarations, core_module, declaration);
        }

        for (iris::Union_declaration const& declaration : core_module.export_declarations.union_declarations)
        {
            add_union_declaration(sorted_declarations, core_module, declaration);
        }

        for (iris::Union_declaration const& declaration : core_module.internal_declarations.union_declarations)
        {
            add_union_declaration(sorted_declarations, core_module, declaration);
        }

        return std::pmr::vector<iris::Declaration>{sorted_declarations.begin(), sorted_declarations.end(), output_allocator};
    }

    using String_stream = std::basic_stringstream<char, std::char_traits<char>, std::pmr::polymorphic_allocator<char>>;

    static void write_module_namespace(
        String_stream& stream,
        std::string_view const core_module_name
    )
    {
        for (char const character : core_module_name)
        {
            if (character == '.')
                stream << "_";
            else
                stream << character;
        }
    }

    static void write_header_start(
        String_stream& stream,
        std::string_view const core_module_name,
        std::string_view const suffix
    )
    {
        stream << "#ifndef ";
        write_module_namespace(stream, core_module_name);
        stream << suffix;
        stream << "\n#define ";
        write_module_namespace(stream, core_module_name);
        stream << suffix;
        stream << "\n\n";
    }

    static void write_header_end(
        String_stream& stream
    )
    {
        stream << "#endif\n";
    }

    static void write_includes(
        String_stream& stream,
        iris::Module const& core_module,
        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const& dependencies_c_file_paths
    )
    {
        stream << "#include <iris_builtin.h>\n\n";

        for (iris::Import_module_with_alias const& import_module : core_module.dependencies.alias_imports)
        {
            auto const location = dependencies_c_file_paths.find(import_module.module_name);
            if (location != dependencies_c_file_paths.end())
            {
                stream << "#include <";
                stream << location->second.generic_string();
                stream << ">\n";
            }
        }
        if (!core_module.dependencies.alias_imports.empty())
            stream << '\n';

        stream << "#include <stdint.h>\n\n";
    }

    static void write_extern_c_begin(
        String_stream& stream
    )
    {
        stream << "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n";
    }

    static void write_extern_c_end(
        String_stream& stream
    )
    {
        stream << "#ifdef __cplusplus\n}\n#endif\n\n";
    }

    static void write_c_declaration_name(
        String_stream& stream,
        std::string_view const core_module_name,
        std::string_view const declaration_name,
        std::optional<std::string_view> const unique_name
    )
    {
        if (unique_name.has_value())
        {
            stream << unique_name.value();
            return;
        }

        write_module_namespace(stream, core_module_name);

        stream << '_';
        stream << declaration_name;
    }

    static std::optional<std::string_view> get_declaration_type(iris::Declaration const& declaration)
    {
        if (std::holds_alternative<Struct_declaration const*>(declaration.data))
            return "struct";
        else if (std::holds_alternative<Union_declaration const*>(declaration.data))
            return "union";
        else
            return std::nullopt;
    }

    static void write_c_type_name(
        String_stream& stream,
        iris::Declaration_database const declaration_database,
        iris::Type_reference const& type_reference,
        std::optional<std::string_view> const variable_name
    );

    static void write_c_type_name(
        String_stream& stream,
        iris::Declaration_database const declaration_database,
        std::span<iris::Type_reference const> const type_reference,
        std::optional<std::string_view> const variable_name
    );

    static void write_fundamental_type_name(
        String_stream& stream,
        iris::Fundamental_type const value
    )
    {
        switch (value)
        {
            case iris::Fundamental_type::Bool: {
                stream << "bool";
                break;
            }
            case iris::Fundamental_type::Byte: {
                stream << "uint8_t";
                break;
            }
            case iris::Fundamental_type::Float16: {
                stream << "_float16";
                break;
            }
            case iris::Fundamental_type::Float32: {
                stream << "float";
                break;
            }
            case iris::Fundamental_type::Float64: {
                stream << "double";
                break;
            }
            case iris::Fundamental_type::String: {
                stream << "String"; // TODO
                break;
            }
            case iris::Fundamental_type::Any_type: {
                stream << "void*";
                break;
            }
            case iris::Fundamental_type::C_bool: {
                stream << "bool";
                break;
            }
            case iris::Fundamental_type::C_char: {
                stream << "char";
                break;
            }
            case iris::Fundamental_type::C_schar: {
                stream << "schar";
                break;
            }
            case iris::Fundamental_type::C_uchar: {
                stream << "uchar";
                break;
            }
            case iris::Fundamental_type::C_short: {
                stream << "short";
                break;
            }
            case iris::Fundamental_type::C_ushort: {
                stream << "ushort";
                break;
            }
            case iris::Fundamental_type::C_int: {
                stream << "int";
                break;
            }
            case iris::Fundamental_type::C_uint: {
                stream << "unsigned int";
                break;
            }
            case iris::Fundamental_type::C_long: {
                stream << "long";
                break;
            }
            case iris::Fundamental_type::C_ulong: {
                stream << "unsigned long";
                break;
            }
            case iris::Fundamental_type::C_longlong: {
                stream << "long long";
                break;
            }
            case iris::Fundamental_type::C_ulonglong: {
                stream << "unsigned long long";
                break;
            }
            case iris::Fundamental_type::C_longdouble: {
                stream << "long double";
                break;
            }
        }
    }

    static void write_integer_type_name(
        String_stream& stream,
        iris::Integer_type const& value
    )
    {
        if (!value.is_signed)
            stream << "u";
        stream << "int";
        stream << value.number_of_bits;
        stream << "_t";
    }

    static void write_array_slice_type_name(
        String_stream& stream,
        std::span<iris::Type_reference const> const array_slice_element_type,
        std::function<void(String_stream&, std::string_view, std::string_view)> const& write_declaration_name
    )
    {
        auto const write_array_slice_element_type = [&](std::span<iris::Type_reference const> const type_reference) -> void
        {
            if (type_reference.empty())
            {
                stream << "void";
                return;
            }

            if (std::holds_alternative<iris::Custom_type_reference>(type_reference[0].data))
            {
                iris::Custom_type_reference const custom_type_reference = std::get<iris::Custom_type_reference>(type_reference[0].data);
                write_declaration_name(stream, custom_type_reference.module_reference.name, custom_type_reference.name);
            }
            else if (std::holds_alternative<iris::Fundamental_type>(type_reference[0].data))
            {
                iris::Fundamental_type const fundamental_type = std::get<iris::Fundamental_type>(type_reference[0].data);
                std::string_view const type_name = format_fundamental_type(fundamental_type);
                stream << type_name;
            }
            else if (std::holds_alternative<iris::Integer_type>(type_reference[0].data))
            {
                iris::Integer_type const integer_type = std::get<iris::Integer_type>(type_reference[0].data);
                std::pmr::string const type_name = iris::format_integer_type(integer_type);
                stream << type_name;
            }
            else
            {
                throw std::runtime_error{"C Header Exporter: Array_slice type not implemented yet!"};
            }
        };

        stream << "Array_slice_";
        write_array_slice_element_type(array_slice_element_type);
    }

    static void write_c_array_slice_type_name(
        String_stream& stream,
        iris::Declaration_database const& declaration_database,
        std::span<iris::Type_reference const> const array_slice_element_type
    )
    {
        auto const write_declaration_name = [&](String_stream& stream, std::string_view const module_name, std::string_view const declaration_name) -> void 
        {
            std::optional<Declaration> const declaration = find_declaration(declaration_database, module_name, declaration_name);
            std::optional<std::string_view> const unique_name = declaration.has_value() ? get_declaration_unique_name(declaration.value()) : std::optional<std::string_view>{std::nullopt};
            write_c_declaration_name(stream, module_name, declaration_name, unique_name);
        };

        write_array_slice_type_name(stream, array_slice_element_type, write_declaration_name);
    }

    static void write_c_type_name(
        String_stream& stream,
        iris::Declaration_database const declaration_database,
        std::span<iris::Type_reference const> const type_reference,
        std::optional<std::string_view> const variable_name
    )
    {
        if (type_reference.empty())
        {
            stream << "void";
            return;
        }

        return write_c_type_name(stream, declaration_database, type_reference[0], variable_name);
    }

    static void write_c_type_name(
        String_stream& stream,
        iris::Declaration_database const declaration_database,
        iris::Type_reference const& type_reference,
        std::optional<std::string_view> const variable_name
    )
    {
        if (std::holds_alternative<iris::Array_slice_type>(type_reference.data))
        {
            iris::Array_slice_type const& data = std::get<iris::Array_slice_type>(type_reference.data);
            stream << "struct ";
            write_c_array_slice_type_name(stream, declaration_database, data.element_type);

            if (variable_name.has_value())
                stream << ' ' << variable_name.value();
        }
        else if (std::holds_alternative<iris::Builtin_type_reference>(type_reference.data))
        {
            iris::Builtin_type_reference const& data = std::get<iris::Builtin_type_reference>(type_reference.data);
            // TODO
        }
        else if (std::holds_alternative<iris::Constant_array_type>(type_reference.data))
        {
            iris::Constant_array_type const& data = std::get<iris::Constant_array_type>(type_reference.data);
            write_c_type_name(stream, declaration_database, data.value_type, std::nullopt);

            if (variable_name.has_value())
                stream << ' ' << variable_name.value();

            stream << '[' << data.size << ']';
        }
        else if (std::holds_alternative<iris::Custom_type_reference>(type_reference.data))
        {
            iris::Custom_type_reference const& data = std::get<iris::Custom_type_reference>(type_reference.data);
            std::optional<Declaration> const declaration = find_declaration(declaration_database, data.module_reference.name, data.name);
            std::optional<Declaration> const underlying_declaration = find_underlying_declaration(declaration_database, data.module_reference.name, data.name);

            if (
                declaration.has_value() &&
                std::holds_alternative<iris::Alias_type_declaration const*>(declaration->data)
            )
            {
                iris::Alias_type_declaration const& alias_type_declaration = *std::get<iris::Alias_type_declaration const*>(declaration->data);
                if (!alias_type_declaration.type.empty() && std::holds_alternative<iris::Type_instance>(alias_type_declaration.type[0].data))
                    stream << "struct ";
            }
            else if (underlying_declaration.has_value())
            {
                std::optional<std::string_view> const declaration_type = get_declaration_type(underlying_declaration.value());
                if (declaration_type.has_value())
                    stream << declaration_type.value() << " ";
            }

            std::optional<std::string_view> const unique_name = declaration.has_value() ? get_declaration_unique_name(declaration.value()) : std::optional<std::string_view>{std::nullopt};
            write_c_declaration_name(stream, data.module_reference.name, data.name, unique_name);

            if (variable_name.has_value())
                stream << ' ' << variable_name.value();
        }
        else if (std::holds_alternative<iris::Fundamental_type>(type_reference.data))
        {
            iris::Fundamental_type const& data = std::get<iris::Fundamental_type>(type_reference.data);
            write_fundamental_type_name(stream, data);

            if (variable_name.has_value())
                stream << ' ' << variable_name.value();
        }
        else if (std::holds_alternative<iris::Function_pointer_type>(type_reference.data))
        {
            iris::Function_pointer_type const& data = std::get<iris::Function_pointer_type>(type_reference.data);
            write_c_type_name(stream, declaration_database, data.type.output_parameter_types, std::nullopt);
            stream << "(*";
            if (variable_name.has_value())
                stream << variable_name.value();
            stream << ")(";
            for (std::size_t index = 0; index < data.input_parameter_names.size(); ++index)
            {
                std::string_view const input_parameter_name = data.input_parameter_names[index];
                iris::Type_reference const& input_parameter_type = data.type.input_parameter_types[index];
                write_c_type_name(stream, declaration_database, input_parameter_type, input_parameter_name);

                if (index + 1 < data.input_parameter_names.size())
                    stream << ", ";
            }
            stream << ")";
        }
        else if (std::holds_alternative<iris::Integer_type>(type_reference.data))
        {
            iris::Integer_type const& data = std::get<iris::Integer_type>(type_reference.data);
            write_integer_type_name(stream, data);

            if (variable_name.has_value())
                stream << ' ' << variable_name.value();
        }
        else if (std::holds_alternative<iris::Null_pointer_type>(type_reference.data))
        {
            iris::Null_pointer_type const& data = std::get<iris::Null_pointer_type>(type_reference.data);
            // TODO
        }
        else if (std::holds_alternative<iris::Parameter_type>(type_reference.data))
        {
            iris::Parameter_type const& data = std::get<iris::Parameter_type>(type_reference.data);
            // TODO
        }
        else if (std::holds_alternative<iris::Pointer_type>(type_reference.data))
        {
            iris::Pointer_type const& data = std::get<iris::Pointer_type>(type_reference.data);
            write_c_type_name(stream, declaration_database, data.element_type, std::nullopt);
            if (!data.is_mutable)
                stream << " const";
            stream << "*";

            if (variable_name.has_value())
                stream << ' ' << variable_name.value();
        }
        else if (std::holds_alternative<iris::Type_instance>(type_reference.data))
        {
            iris::Type_instance const& data = std::get<iris::Type_instance>(type_reference.data);
            // TODO
        }

        // TODO
    }

    void write_c_array_slice_struct(
        String_stream& stream,
        iris::Declaration_database const declaration_database,
        std::string_view const declaration_type,
        std::string_view const core_module_name,
        std::string_view const declaration_name,
        std::optional<std::pmr::string> const unique_name
    )
    {
        iris::Type_reference const element_type = iris::create_custom_type_reference(core_module_name, declaration_name);

        stream << "struct ";
        
        write_c_array_slice_type_name(stream, declaration_database, {&element_type, 1});
        stream << "\n{\n";
        
        stream << "    " << declaration_type << " ";
        write_c_declaration_name(stream, core_module_name, declaration_name, unique_name);
        stream << "* data;\n";
        
        stream << "    uint64_t size;\n";

        stream << "};\n\n";
    }

    void write_c_struct_or_union_declaration(
        String_stream& stream,
        iris::Declaration_database const declaration_database,
        std::string_view const declaration_type,
        std::string_view const core_module_name,
        std::string_view const declaration_name,
        std::optional<std::pmr::string> const unique_name,
        std::span<std::pmr::string const> const member_names,
        std::span<iris::Type_reference const> const member_types
    )
    {
        stream << declaration_type << " ";
        write_c_declaration_name(stream, core_module_name, declaration_name, unique_name);
        stream << "\n{\n";
        
        for (std::size_t member_index = 0; member_index < member_names.size(); ++member_index)
        {
            iris::Type_reference const& member_type = member_types[member_index];

            stream << "    ";
            write_c_type_name(stream, declaration_database, member_type, member_names[member_index]);
            stream << ";\n";
        }
        
        stream << "};\n\n";
        
        write_c_array_slice_struct(stream, declaration_database, declaration_type, core_module_name, declaration_name, unique_name);
    }

    void write_c_declaration(
        String_stream& stream,
        iris::Declaration_database const declaration_database,
        std::string_view const core_module_name,
        iris::Declaration const& declaration
    )
    {
        if (std::holds_alternative<iris::Alias_type_declaration const*>(declaration.data))
        {
            iris::Alias_type_declaration const& alias_type_declaration = *std::get<iris::Alias_type_declaration const*>(declaration.data);
            if (!alias_type_declaration.type.empty() && std::holds_alternative<iris::Type_instance>(alias_type_declaration.type[0].data))
            {
                iris::Type_instance const& type_instance = std::get<iris::Type_instance>(alias_type_declaration.type[0].data);
                iris::Declaration_instance_storage const storage = iris::instantiate_type_instance(declaration_database, type_instance);
                iris::Struct_declaration const& instantiated = std::get<iris::Struct_declaration>(storage.data);
                write_c_struct_or_union_declaration(
                    stream,
                    declaration_database,
                    "struct",
                    core_module_name,
                    alias_type_declaration.name,
                    std::nullopt,
                    instantiated.member_names,
                    instantiated.member_types
                );
            }
        }
        else if (std::holds_alternative<iris::Struct_declaration const*>(declaration.data))
        {
            iris::Struct_declaration const& struct_declaration = *std::get<iris::Struct_declaration const*>(declaration.data);
            write_c_struct_or_union_declaration(
                stream, 
                declaration_database,
                "struct",
                core_module_name, 
                struct_declaration.name,
                struct_declaration.unique_name,
                struct_declaration.member_names,
                struct_declaration.member_types
            );
        }
        else if (std::holds_alternative<iris::Union_declaration const*>(declaration.data))
        {
            iris::Union_declaration const& union_declaration = *std::get<iris::Union_declaration const*>(declaration.data);
            write_c_struct_or_union_declaration(
                stream, 
                declaration_database, 
                "union",
                core_module_name, 
                union_declaration.name,
                union_declaration.unique_name,
                union_declaration.member_names,
                union_declaration.member_types
            );
        }
    }

    void write_c_function_declaration(
        String_stream& stream,
        iris::Declaration_database const declaration_database,
        std::string_view const core_module_name,
        iris::Function_declaration const& declaration
    )
    {
        write_c_type_name(stream, declaration_database, declaration.type.output_parameter_types, std::nullopt);

        stream << " ";
        write_c_declaration_name(stream, core_module_name, declaration.name, declaration.unique_name);

        stream << '(';
        for (std::size_t index = 0; index < declaration.input_parameter_names.size(); ++index)
        {
            write_c_type_name(stream, declaration_database, declaration.type.input_parameter_types[index], declaration.input_parameter_names[index]);
            if (index + 1 < declaration.input_parameter_names.size())
                stream << ", ";
        }
        stream << ");\n";
    }

    Exported_c_header export_module_as_c_header(
        iris::Module const& core_module,
        iris::Declaration_database const& declaration_database,
        std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const& dependencies_c_file_paths,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<iris::Declaration> const declarations = sort_declarations(core_module, temporaries_allocator, temporaries_allocator);

        String_stream stream{std::ios_base::in | std::ios_base::out, temporaries_allocator};

        write_header_start(stream, core_module.name, "");
        write_includes(stream, core_module, dependencies_c_file_paths);
        write_extern_c_begin(stream);

        for (iris::Declaration const& declaration : declarations)
            write_c_declaration(stream, declaration_database, core_module.name, declaration);

        for (iris::Function_declaration const& declaration : core_module.export_declarations.function_declarations)
            write_c_function_declaration(stream, declaration_database, core_module.name, declaration);

        if (!core_module.export_declarations.function_declarations.empty())
            stream << '\n';

        write_extern_c_end(stream);
        write_header_end(stream);

        std::string_view const content = stream.view();
        return {
            .content = std::pmr::string{content.begin(), content.end(), output_allocator}
        };
    }

    static void write_cpp_module_namespace(
        String_stream& stream,
        std::string_view const core_module_name
    )
    {
        for (char const character : core_module_name)
        {
            if (character == '.')
                stream << "::";
            else
                stream << character;
        }
    }

    static void write_cpp_type(
        String_stream& stream,
        iris::Type_reference const& type_reference,
        std::optional<std::string_view> const variable_name
    );

    static void write_cpp_type(
        String_stream& stream,
        std::span<iris::Type_reference const> const type_reference,
        std::optional<std::string_view> const variable_name
    );

    static void write_cpp_array_slice_type_name(
        String_stream& stream,
        std::span<iris::Type_reference const> const array_slice_element_type
    )
    {
        auto const write_declaration_name = [&](String_stream& stream, std::string_view const module_name, std::string_view const declaration_name) -> void 
        {
            iris::Type_reference const type_reference = iris::create_custom_type_reference(module_name, declaration_name);
            write_cpp_type(stream, type_reference, std::nullopt);
        };

        write_array_slice_type_name(stream, array_slice_element_type, write_declaration_name);
    }

    static void write_cpp_type(
        String_stream& stream,
        std::span<iris::Type_reference const> const type_reference,
        std::optional<std::string_view> const variable_name
    )
    {
        if (type_reference.empty())
        {
            stream << "void";
            return;
        }

        return write_cpp_type(stream, type_reference[0], variable_name);
    }

    static void write_cpp_type(
        String_stream& stream,
        iris::Type_reference const& type_reference,
        std::optional<std::string_view> const variable_name
    )
    {
        if (std::holds_alternative<iris::Array_slice_type>(type_reference.data))
        {
            iris::Array_slice_type const& data = std::get<iris::Array_slice_type>(type_reference.data);
            write_cpp_array_slice_type_name(stream, data.element_type);

            if (variable_name.has_value())
                stream << ' ' << variable_name.value();
        }
        else if (std::holds_alternative<iris::Builtin_type_reference>(type_reference.data))
        {
            iris::Builtin_type_reference const& data = std::get<iris::Builtin_type_reference>(type_reference.data);
            // TODO
        }
        else if (std::holds_alternative<iris::Constant_array_type>(type_reference.data))
        {
            iris::Constant_array_type const& data = std::get<iris::Constant_array_type>(type_reference.data);
            write_cpp_type(stream, data.value_type, std::nullopt);

            if (variable_name.has_value())
                stream << ' ' << variable_name.value();

            stream << '[' << data.size << ']';
        }
        else if (std::holds_alternative<iris::Custom_type_reference>(type_reference.data))
        {
            iris::Custom_type_reference const& data = std::get<iris::Custom_type_reference>(type_reference.data);
            stream << data.name;

            if (variable_name.has_value())
                stream << ' ' << variable_name.value();
        }
        else if (std::holds_alternative<iris::Fundamental_type>(type_reference.data))
        {
            iris::Fundamental_type const& data = std::get<iris::Fundamental_type>(type_reference.data);
            write_fundamental_type_name(stream, data);

            if (variable_name.has_value())
                stream << ' ' << variable_name.value();
        }
        else if (std::holds_alternative<iris::Function_pointer_type>(type_reference.data))
        {
            iris::Function_pointer_type const& data = std::get<iris::Function_pointer_type>(type_reference.data);
            write_cpp_type(stream, data.type.output_parameter_types, std::nullopt);
            stream << "(*";
            if (variable_name.has_value())
                stream << variable_name.value();
            stream << ")(";
            for (std::size_t index = 0; index < data.input_parameter_names.size(); ++index)
            {
                std::string_view const input_parameter_name = data.input_parameter_names[index];
                iris::Type_reference const& input_parameter_type = data.type.input_parameter_types[index];
                write_cpp_type(stream, input_parameter_type, input_parameter_name);

                if (index + 1 < data.input_parameter_names.size())
                    stream << ", ";
            }
            stream << ")";
        }
        else if (std::holds_alternative<iris::Integer_type>(type_reference.data))
        {
            iris::Integer_type const& data = std::get<iris::Integer_type>(type_reference.data);
            write_integer_type_name(stream, data);

            if (variable_name.has_value())
                stream << ' ' << variable_name.value();
        }
        else if (std::holds_alternative<iris::Null_pointer_type>(type_reference.data))
        {
            iris::Null_pointer_type const& data = std::get<iris::Null_pointer_type>(type_reference.data);
            // TODO
        }
        else if (std::holds_alternative<iris::Parameter_type>(type_reference.data))
        {
            iris::Parameter_type const& data = std::get<iris::Parameter_type>(type_reference.data);
            // TODO
        }
        else if (std::holds_alternative<iris::Pointer_type>(type_reference.data))
        {
            iris::Pointer_type const& data = std::get<iris::Pointer_type>(type_reference.data);
            write_cpp_type(stream, data.element_type, std::nullopt);
            if (!data.is_mutable)
                stream << " const";
            stream << "*";

            if (variable_name.has_value())
                stream << ' ' << variable_name.value();
        }
        else if (std::holds_alternative<iris::Type_instance>(type_reference.data))
        {
            iris::Type_instance const& data = std::get<iris::Type_instance>(type_reference.data);
            // TODO
        }

        // TODO
    }

    void write_cpp_using_array_slice(
        String_stream& stream,
        std::string_view const core_module_name,
        std::string_view const declaration_name,
        std::optional<std::string_view> const unique_name
    )
    {
        iris::Type_reference const element_type = iris::create_custom_type_reference(core_module_name, declaration_name);
        
        stream << "    using ";
        write_cpp_array_slice_type_name(stream, {&element_type, 1});
        stream << " = ::";

        auto const write_declaration_name = [&](String_stream& stream, std::string_view const module_name, std::string_view const declaration_name) -> void 
        {
            write_c_declaration_name(stream, module_name, declaration_name, unique_name);
        };
        write_array_slice_type_name(stream, {&element_type, 1}, write_declaration_name);

        stream << ";\n";
    }

    void write_cpp_using_declaration(
        String_stream& stream,
        std::string_view const core_module_name,
        std::string_view const declaration_name,
        std::optional<std::string_view> const unique_name,
        bool const write_builtin_structs
    )
    {
        stream << "    using " << declaration_name << " = ::";
        write_c_declaration_name(stream, core_module_name, declaration_name, unique_name);
        stream << ";\n";

        if (write_builtin_structs)
        {
            write_cpp_using_array_slice(stream, core_module_name, declaration_name, unique_name);
        }
    }

    void write_cpp_function_declaration(
        String_stream& stream,
        std::string_view const core_module_name,
        iris::Function_declaration const& declaration
    )
    {
        stream << "    inline auto " << declaration.name << "(";
        for (std::size_t index = 0; index < declaration.input_parameter_names.size(); ++index)
        {
            write_cpp_type(stream, declaration.type.input_parameter_types[index], declaration.input_parameter_names[index]);
            if (index + 1 < declaration.input_parameter_names.size())
                stream << ", ";
        }
        stream << ") -> ";
        
        write_cpp_type(stream, declaration.type.output_parameter_types, std::nullopt);
        
        stream << " { ";
        if (!declaration.type.output_parameter_types.empty())
            stream << "return ";
        stream << "::";
        write_c_declaration_name(stream, core_module_name, declaration.name, declaration.unique_name);
        stream << "(";
        for (std::size_t index = 0; index < declaration.input_parameter_names.size(); ++index)
        {
            stream << declaration.input_parameter_names[index];
            if (index + 1 < declaration.input_parameter_names.size())
                stream << ", ";
        }
        stream << "); }";
        stream << "\n";
    }

    Exported_cpp_header export_module_as_cpp_header(
        iris::Module const& core_module,
        std::filesystem::path const& c_header_file_path,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<iris::Declaration> const declarations = sort_declarations(core_module, temporaries_allocator, temporaries_allocator);

        String_stream stream{std::ios_base::in | std::ios_base::out, temporaries_allocator};

        write_header_start(stream, core_module.name, "_HPP");
        stream << "#include <" << c_header_file_path.generic_string() << ">\n\n";

        stream << "namespace ";
        write_cpp_module_namespace(stream, core_module.name);
        stream << "\n{\n";

        for (iris::Alias_type_declaration const& declaration : core_module.export_declarations.alias_type_declarations)
            write_cpp_using_declaration(stream, core_module.name, declaration.name, declaration.unique_name, false);

        for (iris::Enum_declaration const& declaration : core_module.export_declarations.enum_declarations)
            write_cpp_using_declaration(stream, core_module.name, declaration.name, declaration.unique_name, true);

        for (iris::Struct_declaration const& declaration : core_module.export_declarations.struct_declarations)
            write_cpp_using_declaration(stream, core_module.name, declaration.name, declaration.unique_name, true);

        for (iris::Union_declaration const& declaration : core_module.export_declarations.union_declarations)
            write_cpp_using_declaration(stream, core_module.name, declaration.name, declaration.unique_name, true);

        if (!core_module.export_declarations.function_declarations.empty())
            stream << '\n';

        for (iris::Function_declaration const& declaration : core_module.export_declarations.function_declarations)
            write_cpp_function_declaration(stream, core_module.name, declaration);

        stream << "}\n\n";

        write_header_end(stream);

        std::string_view const content = stream.view();
        return {
            .content = std::pmr::string{content.begin(), content.end(), output_allocator}
        };
    }
}
