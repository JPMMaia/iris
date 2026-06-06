module;

#include <nlohmann/json.hpp>

module iris.tools.code_generator;

import std;

namespace iris::tools::code_generator
{
    std::string indent(int const indentation)
    {
        return std::format("{:{}}", "", indentation);
    }

    std::pmr::string to_lowercase(std::string_view const string)
    {
        std::pmr::string lowercase_string;
        lowercase_string.resize(string.size());

        std::transform(
            string.begin(),
            string.end(),
            lowercase_string.begin(),
            [](char const c) { return std::tolower(c); }
        );

        return lowercase_string;
    }

    std::pmr::string generate_read_enum_json_code(
        Enum const enum_type,
        int const indentation
    )
    {
        std::stringstream output_stream;

        output_stream << indent(indentation) << "export template<>\n";
        output_stream << indent(indentation) << "    bool read_enum(" << enum_type.name << "& output, std::string_view const value)\n";
        output_stream << indent(indentation) << "{\n";

        if (!enum_type.values.empty())
        {
            std::string_view const value = enum_type.values[0];
            output_stream << indent(indentation) << "    if (value == \"" << value << "\")\n";
            output_stream << indent(indentation) << "    {\n";
            output_stream << indent(indentation) << "        output = " << enum_type.name << "::" << value << ";\n";
            output_stream << indent(indentation) << "        return true;\n";
            output_stream << indent(indentation) << "    }\n";
        }

        for (std::size_t index = 1; index < enum_type.values.size(); ++index)
        {
            std::string_view const value = enum_type.values[index];
            output_stream << indent(indentation) << "    else if (value == \"" << value << "\")\n";
            output_stream << indent(indentation) << "    {\n";
            output_stream << indent(indentation) << "        output = " << enum_type.name << "::" << value << ";\n";
            output_stream << indent(indentation) << "        return true;\n";
            output_stream << indent(indentation) << "    }\n";
        }
        output_stream << "\n";
        output_stream << indent(indentation) << "    std::cerr << std::format(\"Failed to read enum '" << enum_type.name << "' with value '{}'\\n\", value);\n";
        output_stream << indent(indentation) << "    return false;\n";
        output_stream << indent(indentation) << "}\n";

        return std::pmr::string{ output_stream.str() };
    }

    std::pmr::string generate_write_enum_json_code(
        Enum enum_type,
        int const indentation
    )
    {
        std::stringstream output_stream;

        output_stream << indent(indentation) << "export std::string_view write_enum(" << enum_type.name << " const value)\n";
        output_stream << indent(indentation) << "{\n";

        if (!enum_type.values.empty())
        {
            std::string_view const value = enum_type.values[0];
            output_stream << indent(indentation) << "    if (value == " << enum_type.name << "::" << value << ")\n";
            output_stream << indent(indentation) << "    {\n";
            output_stream << indent(indentation) << "        return \"" << value << "\";\n";
            output_stream << indent(indentation) << "    }\n";
        }

        for (std::size_t index = 1; index < enum_type.values.size(); ++index)
        {
            std::string_view const value = enum_type.values[index];
            output_stream << indent(indentation) << "    else if (value == " << enum_type.name << "::" << value << ")\n";
            output_stream << indent(indentation) << "    {\n";
            output_stream << indent(indentation) << "        return \"" << value << "\";\n";
            output_stream << indent(indentation) << "    }\n";
        }
        output_stream << "\n";
        output_stream << indent(indentation) << "    throw std::runtime_error{ \"Failed to write enum '" << enum_type.name << "'!\\n\" };\n";
        output_stream << indent(indentation) << "}\n";

        return std::pmr::string{ output_stream.str() };
    }

    namespace
    {
        bool is_enum_type(
            Type const& type,
            std::pmr::unordered_map<std::pmr::string, Enum> const& enum_types
        )
        {
            return enum_types.contains(type.name);
        }

        bool is_deque_type(
            Type const& type
        )
        {
            return type.name.starts_with("std::deque") || type.name.starts_with("std::pmr::deque");
        }

        bool is_vector_type(
            Type const& type
        )
        {
            return type.name.starts_with("std::vector") || type.name.starts_with("std::pmr::vector");
        }

        bool is_optional_type(
            Type const& type
        )
        {
            return type.name.starts_with("std::optional");
        }

        bool is_filesystem_path_type(
            Type const& type
        )
        {
            return type.name == "std::filesystem::path";
        }

        bool is_struct_type(
            Type const& type,
            std::pmr::unordered_map<std::pmr::string, Struct> const& struct_types
        )
        {
            return struct_types.contains(type.name);
        }

        bool is_variant_type(
            Type const& type
        )
        {
            return type.name.starts_with("std::variant");
        }

        bool is_bool_type(
            Type const& type
        )
        {
            return type.name == "bool";
        }

        bool is_int_type(
            Type const& type
        )
        {
            return
                type.name == "std::int8_t" ||
                type.name == "std::int16_t" ||
                type.name == "std::int32_t" ||
                type.name == "int";
        }

        bool is_int64_type(
            Type const& type
        )
        {
            return type.name == "std::int64_t";
        }

        bool is_uint_type(
            Type const& type
        )
        {
            return
                type.name == "std::uint8_t" ||
                type.name == "std::uint16_t" ||
                type.name == "std::uint32_t" ||
                type.name == "unsigned";
        }

        bool is_uint64_type(
            Type const& type
        )
        {
            return
                type.name == "std::uint64_t" ||
                type.name == "std::size_t";
        }

        bool is_double_type(
            Type const& type
        )
        {
            return
                type.name == "float" ||
                type.name == "double";
        }

        bool is_string_type(
            Type const& type
        )
        {
            return
                type.name == "std::string" ||
                type.name == "std::pmr::string";
        }

        bool is_cpp_type(
            Type const& type
        )
        {
            return
                type.name.starts_with("std::") ||
                is_bool_type(type) ||
                is_int_type(type) ||
                is_uint_type(type) ||
                is_double_type(type);
        }

        std::pmr::string get_optional_value_type(
            Type const& type
        )
        {
            auto const open_location = type.name.find_first_of('<');
            auto const close_location = type.name.find_last_of('>');
            auto const count = close_location - open_location - 1;

            return std::pmr::string{ type.name.substr(open_location + 1, count) };
        }

        std::pmr::string get_vector_value_type(
            Type const& type
        )
        {
            auto const open_location = type.name.find_first_of('<');
            auto const close_location = type.name.find_last_of('>');
            auto const count = close_location - open_location - 1;

            return std::pmr::string{ type.name.substr(open_location + 1, count) };
        }

        std::pmr::vector<std::pmr::string> get_variadic_types(
            std::string_view const type
        )
        {
            auto const open_location = type.find_first_of('<');
            auto const close_location = type.find_last_of('>');

            std::string_view const types_string = { type.begin() + open_location + 1, type.begin() + close_location };

            std::pmr::vector<std::pmr::string> variadic_types;

            {
                auto start_location = types_string.begin();

                while (true)
                {
                    auto const comma_location = std::find(start_location, types_string.end(), ',');

                    std::string_view const type = { start_location, comma_location };
                    variadic_types.push_back(std::pmr::string{ type });

                    if (comma_location == types_string.end())
                    {
                        break;
                    }

                    start_location = comma_location + 1;
                }
            }

            return variadic_types;
        }

        std::pmr::string create_formatted_variant_type(
            std::span<std::pmr::string const> const types
        )
        {
            std::stringstream stream;

            stream << "std::variant<";

            for (std::size_t index = 0; index < types.size(); ++index)
            {
                if (index != 0)
                {
                    stream << ", ";
                }

                std::pmr::string const& type = types[index];
                stream << "iris::" << type;
            }

            stream << ">";

            return std::pmr::string{ stream.str() };
        }

        int generate_read_struct_member_key_code(
            std::stringstream& output_stream,
            std::string_view const struct_name,
            Member const& member,
            int const state,
            std::pmr::unordered_map<std::pmr::string, Struct> const& struct_types,
            int const indentation,
            bool const indent_first
        )
        {
            if (is_variant_type(member.type))
            {
                if (indent_first)
                    output_stream << indent(indentation);
                output_stream << "if (event_data == \"" << member.name << "\")\n";
                output_stream << indent(indentation) << "{\n";
                output_stream << indent(indentation) << "    state = " << state << ";\n";
                output_stream << indent(indentation) << "    return true;\n";
                output_stream << indent(indentation) << "}\n";
                return 1;
            }
            else
            {
                if (indent_first)
                    output_stream << indent(indentation);
                output_stream << "if (event_data == \"" << member.name << "\")\n";
                output_stream << indent(indentation) << "{\n";
                output_stream << indent(indentation) << "    state = " << state << ";\n";
                output_stream << indent(indentation) << "    return true;\n";
                output_stream << indent(indentation) << "}\n";

                return ((is_struct_type(member.type, struct_types) || is_vector_type(member.type)) ? 2 : 1);
            }
        }

        void generate_read_object_code(
            std::stringstream& output_stream,
            std::string_view const output_name,
            int const state,
            int const end_state,
            int const stack_offset,
            int const indentation
        )
        {
            output_stream << indent(indentation) << "case " << state << ":\n";
            output_stream << indent(indentation) << "{\n";
            output_stream << indent(indentation) << "    state = " << (state + 1) << ";\n";
            output_stream << indent(indentation) << "    return read_object(" << output_name << ", event, event_data, state_stack, state_stack_position + 1 + " << stack_offset << ");\n";
            output_stream << indent(indentation) << "}\n";
            output_stream << indent(indentation) << "case " << (state + 1) << ":\n";
            output_stream << indent(indentation) << "{\n";
            output_stream << indent(indentation) << "    if ((event == Event::End_object) && (state_stack_position + 2 + " << stack_offset << " == state_stack.size()))\n";
            output_stream << indent(indentation) << "    {\n";
            output_stream << indent(indentation) << "        if (!read_object(" << output_name << ", event, event_data, state_stack, state_stack_position + 1 + " << stack_offset << "))\n";
            output_stream << indent(indentation) << "        {\n";
            output_stream << indent(indentation) << "            return false;\n";
            output_stream << indent(indentation) << "        }\n";
            output_stream << "\n";
            output_stream << indent(indentation) << "        state = " << end_state << ";\n";
            output_stream << indent(indentation) << "        return true;\n";
            output_stream << indent(indentation) << "    }\n";
            output_stream << indent(indentation) << "    else\n";
            output_stream << indent(indentation) << "    {\n";
            output_stream << indent(indentation) << "        return read_object(" << output_name << ", event, event_data, state_stack, state_stack_position + 1 + " << stack_offset << ");\n";
            output_stream << indent(indentation) << "    }\n";
            output_stream << indent(indentation) << "}\n";
        }

        int generate_read_struct_member_value_code(
            std::stringstream& output_stream,
            std::string_view const struct_name,
            Member const& member,
            int const state,
            std::pmr::unordered_map<std::pmr::string, Enum> const& enum_types,
            std::pmr::unordered_map<std::pmr::string, Struct> const& struct_types,
            int const indentation
        )
        {
            if (is_struct_type(member.type, struct_types) || is_vector_type(member.type))
            {
                std::pmr::string const output_name = "output." + member.name;

                generate_read_object_code(
                    output_stream,
                    output_name,
                    state,
                    1,
                    0,
                    indentation
                );

                return 2;
            }
            else if (is_enum_type(member.type, enum_types))
            {
                output_stream << indent(indentation) << "case " << state << ":\n";
                output_stream << indent(indentation) << "{\n";
                output_stream << indent(indentation) << "    state = 1;\n";
                output_stream << indent(indentation) << "    return read_enum(output." << member.name << ", event_data);\n";
                output_stream << indent(indentation) << "}\n";

                return 1;
            }
            else if (is_variant_type(member.type))
            {
                std::pmr::vector<std::pmr::string> const variadic_types = get_variadic_types(member.type.name);

                {
                    output_stream << indent(indentation) << "case " << state << ":\n";
                    output_stream << indent(indentation) << "{\n";
                    output_stream << indent(indentation) << "    if (event == Event::Start_object)\n";
                    output_stream << indent(indentation) << "    {\n";
                    output_stream << indent(indentation) << "        state = " << (state + 2) << ";\n";
                    output_stream << indent(indentation) << "        return true;\n";
                    output_stream << indent(indentation) << "    }\n";
                    output_stream << indent(indentation) << "}\n";
                }

                const int end_object_state = (state + 1);
                {
                    output_stream << indent(indentation) << "case " << end_object_state << ":\n";
                    output_stream << indent(indentation) << "{\n";
                    output_stream << indent(indentation) << "    if (event == Event::End_object)\n";
                    output_stream << indent(indentation) << "    {\n";
                    output_stream << indent(indentation) << "        state = 1;\n";
                    output_stream << indent(indentation) << "        return true;\n";
                    output_stream << indent(indentation) << "    }\n";
                    output_stream << indent(indentation) << "}\n";
                }

                {
                    output_stream << indent(indentation) << "case " << (state + 2) << ":\n";
                    output_stream << indent(indentation) << "{\n";
                    output_stream << indent(indentation) << "    if constexpr (std::is_same_v<Event_data, std::string_view>)\n";
                    output_stream << indent(indentation) << "    {\n";
                    output_stream << indent(indentation) << "        if (event == Event::Key && event_data == \"type\")\n";
                    output_stream << indent(indentation) << "        {\n";
                    output_stream << indent(indentation) << "            state = " << (state + 3) << ";\n";
                    output_stream << indent(indentation) << "            return true;\n";
                    output_stream << indent(indentation) << "        }\n";
                    output_stream << indent(indentation) << "    }\n";
                    output_stream << indent(indentation) << "}\n";
                }

                {
                    output_stream << indent(indentation) << "case " << (state + 3) << ":\n";
                    output_stream << indent(indentation) << "{\n";
                    {
                        output_stream << indent(indentation) << "    if constexpr (std::is_same_v<Event_data, std::string_view>)\n";
                        output_stream << indent(indentation) << "    {\n";

                        for (std::size_t index = 0; index < variadic_types.size(); ++index)
                        {
                            std::string_view const type_name = variadic_types[index];
                            const int next_state = (state + 4 + 3 * index);

                            if (index == 0)
                                output_stream << indent(indentation + 8);

                            output_stream << "if (event_data == \"" << type_name << "\")\n";
                            output_stream << indent(indentation) << "        {\n";
                            output_stream << indent(indentation) << "            output." << member.name << " = " << type_name << "{};\n";
                            output_stream << indent(indentation) << "            state = " << next_state << ";\n";
                            output_stream << indent(indentation) << "            return true;\n";
                            output_stream << indent(indentation) << "        }\n";

                            if ((index + 1) != variadic_types.size())
                            {
                                output_stream << indent(indentation) << "        else ";
                            }
                        }

                        output_stream << indent(indentation) << "    }\n";
                    }
                    output_stream << indent(indentation) << "}\n";
                }

                for (std::size_t index = 0; index < variadic_types.size(); ++index)
                {
                    const int current_state = (state + 4 + 3 * index);
                    std::string_view const type_name = variadic_types[index];
                    Type const type = { .name = std::pmr::string{type_name} };

                    std::pmr::string const output_name = "std::get<" + std::pmr::string{ type_name } + ">(output." + member.name + ")";

                    output_stream << indent(indentation) << "case " << current_state << ":\n";
                    output_stream << indent(indentation) << "{\n";
                    output_stream << indent(indentation) << "    if constexpr (std::is_same_v<Event_data, std::string_view>)\n";
                    output_stream << indent(indentation) << "    {\n";
                    output_stream << indent(indentation) << "        if (event == Event::Key && event_data == \"value\")\n";
                    output_stream << indent(indentation) << "        {\n";
                    output_stream << indent(indentation) << "            state = " << (current_state + 1) << ";\n";
                    output_stream << indent(indentation) << "            return true;\n";
                    output_stream << indent(indentation) << "        }\n";
                    output_stream << indent(indentation) << "    }\n";
                    output_stream << indent(indentation) << "}\n";

                    if (is_enum_type(type, enum_types))
                    {
                        output_stream << indent(indentation) << "case " << (current_state + 1) << ":\n";
                        output_stream << indent(indentation) << "{\n";
                        output_stream << indent(indentation) << "    state = " << end_object_state << ";\n";
                        output_stream << indent(indentation) << "    return read_enum(" << output_name << ", event_data);\n";
                        output_stream << indent(indentation) << "}\n";
                    }
                    else if (is_struct_type(type, struct_types))
                    {
                        generate_read_object_code(
                            output_stream,
                            output_name,
                            current_state + 1,
                            end_object_state,
                            1,
                            indentation
                        );
                    }
                    else
                    {
                        output_stream << indent(indentation) << "case " << (current_state + 1) << ":\n";
                        output_stream << indent(indentation) << "{\n";
                        output_stream << indent(indentation) << "    state = " << end_object_state << ";\n";
                        output_stream << indent(indentation) << "    return read_value(" << output_name << ", \"" << member.name << "\", event_data);\n";
                        output_stream << indent(indentation) << "}\n";
                    }
                }

                return 4 + 3 * static_cast<int>(variadic_types.size());
            }
            else
            {
                output_stream << indent(indentation) << "case " << state << ":\n";
                output_stream << indent(indentation) << "{\n";
                output_stream << indent(indentation) << "    state = 1;\n";
                output_stream << indent(indentation) << "    return read_value(output." << member.name << ", \"" << member.name << "\", event_data);\n";
                output_stream << indent(indentation) << "}\n";

                return 1;
            }
        }
    }

    std::pmr::string generate_read_struct_json_code(
        Struct const& struct_type,
        std::pmr::unordered_map<std::pmr::string, Enum> const& enum_types,
        std::pmr::unordered_map<std::pmr::string, Struct> const& struct_types,
        int const indentation
    )
    {
        std::stringstream output_stream;

            output_stream << indent(indentation) << "export void from_json(nlohmann::json const& j, " << struct_type.name << "& output)\n";
            output_stream << indent(indentation) << "{\n";

            for (Member const& member : struct_type.members)
            {
                if (is_optional_type(member.type))
                {
                    std::pmr::string const value_type_str = get_optional_value_type(member.type);
                    Type const value_type = Type{ value_type_str };

                    output_stream << indent(indentation) << "    {\n";
                    output_stream << indent(indentation) << "        auto const it = j.find(\"" << member.name << "\");\n";
                    output_stream << indent(indentation) << "        if (it != j.end() && !it->is_null())\n";
                    output_stream << indent(indentation) << "        {\n";

                    if (is_vector_type(value_type))
                    {
                        std::pmr::string const vec_elem_str = get_vector_value_type(value_type);
                        Type const vec_elem = Type{ vec_elem_str };
                        output_stream << indent(indentation) << "            output." << member.name << ".emplace();\n";
                        output_stream << indent(indentation) << "            nlohmann::json const& arr = it->at(\"elements\");\n";
                        output_stream << indent(indentation) << "            output." << member.name << ".value().resize(arr.size());\n";
                        if (is_struct_type(vec_elem, struct_types))
                        {
                            output_stream << indent(indentation) << "            for (std::size_t i = 0; i < arr.size(); ++i)\n";
                            output_stream << indent(indentation) << "                from_json(arr[i], output." << member.name << ".value()[i]);\n";
                        }
                        else if (is_string_type(vec_elem))
                        {
                            output_stream << indent(indentation) << "            for (std::size_t i = 0; i < arr.size(); ++i)\n";
                            output_stream << indent(indentation) << "                output." << member.name << ".value()[i] = std::pmr::string{arr[i].get<std::string>()};\n";
                        }
                        else
                        {
                            output_stream << indent(indentation) << "            for (std::size_t i = 0; i < arr.size(); ++i)\n";
                            output_stream << indent(indentation) << "                output." << member.name << ".value()[i] = arr[i].get<" << vec_elem_str << ">();\n";
                        }
                    }
                    else if (is_struct_type(value_type, struct_types))
                    {
                        output_stream << indent(indentation) << "            output." << member.name << ".emplace();\n";
                        output_stream << indent(indentation) << "            from_json(*it, output." << member.name << ".value());\n";
                    }
                    else if (is_string_type(value_type))
                    {
                        output_stream << indent(indentation) << "            output." << member.name << " = std::pmr::string{it->get<std::string>()};\n";
                    }
                    else if (is_filesystem_path_type(value_type))
                    {
                        output_stream << indent(indentation) << "            output." << member.name << " = std::filesystem::path{it->get<std::string>()};\n";
                    }
                    else if (is_enum_type(value_type, enum_types))
                    {
                        output_stream << indent(indentation) << "            {\n";
                        output_stream << indent(indentation) << "                std::string const s = it->get<std::string>();\n";
                        output_stream << indent(indentation) << "                " << value_type_str << " enum_val{};\n";
                        output_stream << indent(indentation) << "                read_enum(enum_val, std::string_view{s});\n";
                        output_stream << indent(indentation) << "                output." << member.name << " = std::move(enum_val);\n";
                        output_stream << indent(indentation) << "            }\n";
                    }
                    else
                    {
                        output_stream << indent(indentation) << "            output." << member.name << " = it->get<" << value_type_str << ">();\n";
                    }

                    output_stream << indent(indentation) << "        }\n";
                    output_stream << indent(indentation) << "    }\n";
                }
                else if (is_variant_type(member.type))
                {
                    std::pmr::vector<std::pmr::string> const variadic_types = get_variadic_types(member.type.name);

                    output_stream << indent(indentation) << "    {\n";
                    output_stream << indent(indentation) << "        nlohmann::json const& data = j.at(\"" << member.name << "\");\n";
                    output_stream << indent(indentation) << "        std::string const type_str = data.at(\"type\").get<std::string>();\n";

                    for (std::size_t i = 0; i < variadic_types.size(); ++i)
                    {
                        std::pmr::string const& type_name = variadic_types[i];
                        Type const elem_type = Type{ type_name };

                        output_stream << indent(indentation) << "        ";
                        if (i != 0) output_stream << "else ";
                        output_stream << "if (type_str == \"" << type_name << "\")\n";
                        output_stream << indent(indentation) << "        {\n";

                        if (is_struct_type(elem_type, struct_types))
                        {
                            output_stream << indent(indentation) << "            output." << member.name << " = iris::" << type_name << "{};\n";
                            output_stream << indent(indentation) << "            from_json(data.at(\"value\"), std::get<iris::" << type_name << ">(output." << member.name << "));\n";
                        }
                        else if (is_enum_type(elem_type, enum_types))
                        {
                            output_stream << indent(indentation) << "            {\n";
                            output_stream << indent(indentation) << "                std::string const val_str = data.at(\"value\").get<std::string>();\n";
                            output_stream << indent(indentation) << "                iris::" << type_name << " enum_val{};\n";
                            output_stream << indent(indentation) << "                read_enum(enum_val, std::string_view{val_str});\n";
                            output_stream << indent(indentation) << "                output." << member.name << " = std::move(enum_val);\n";
                            output_stream << indent(indentation) << "            }\n";
                        }
                        else if (is_string_type(elem_type))
                        {
                            output_stream << indent(indentation) << "            output." << member.name << " = " << std::string{type_name} << "{data.at(\"value\").get<std::string>()};\n";
                        }
                        else
                        {
                            output_stream << indent(indentation) << "            output." << member.name << " = data.at(\"value\").get<" << type_name << ">();\n";
                        }

                        output_stream << indent(indentation) << "        }\n";
                    }

                    output_stream << indent(indentation) << "    }\n";
                }
                else if (is_vector_type(member.type))
                {
                    std::pmr::string const elem_type_str = get_vector_value_type(member.type);
                    Type const elem_type = Type{ elem_type_str };

                    output_stream << indent(indentation) << "    {\n";
                    output_stream << indent(indentation) << "        nlohmann::json const& arr = j.at(\"" << member.name << "\").at(\"elements\");\n";
                    output_stream << indent(indentation) << "        output." << member.name << ".resize(arr.size());\n";
                    output_stream << indent(indentation) << "        for (std::size_t i = 0; i < arr.size(); ++i)\n";
                    output_stream << indent(indentation) << "        {\n";

                    if (is_struct_type(elem_type, struct_types))
                    {
                        output_stream << indent(indentation) << "            from_json(arr[i], output." << member.name << "[i]);\n";
                    }
                    else if (is_optional_type(elem_type))
                    {
                        std::pmr::string const opt_val_str = get_optional_value_type(elem_type);
                        Type const opt_val = Type{ opt_val_str };
                        output_stream << indent(indentation) << "            if (!arr[i].is_null())\n";
                        output_stream << indent(indentation) << "            {\n";
                        if (is_struct_type(opt_val, struct_types))
                        {
                            output_stream << indent(indentation) << "                output." << member.name << "[i].emplace();\n";
                            output_stream << indent(indentation) << "                from_json(arr[i], output." << member.name << "[i].value());\n";
                        }
                        else if (is_string_type(opt_val))
                        {
                            output_stream << indent(indentation) << "                output." << member.name << "[i] = std::pmr::string{arr[i].get<std::string>()};\n";
                        }
                        else
                        {
                            output_stream << indent(indentation) << "                output." << member.name << "[i] = arr[i].get<" << opt_val_str << ">();\n";
                        }
                        output_stream << indent(indentation) << "            }\n";
                    }
                    else if (is_enum_type(elem_type, enum_types))
                    {
                        output_stream << indent(indentation) << "            { std::string s = arr[i].get<std::string>(); read_enum(output." << member.name << "[i], std::string_view{s}); }\n";
                    }
                    else if (is_string_type(elem_type))
                    {
                        output_stream << indent(indentation) << "            output." << member.name << "[i] = std::pmr::string{arr[i].get<std::string>()};\n";
                    }
                    else if (is_filesystem_path_type(elem_type))
                    {
                        output_stream << indent(indentation) << "            output." << member.name << "[i] = std::filesystem::path{arr[i].get<std::string>()};\n";
                    }
                    else
                    {
                        output_stream << indent(indentation) << "            output." << member.name << "[i] = arr[i].get<" << elem_type_str << ">();\n";
                    }

                    output_stream << indent(indentation) << "        }\n";
                    output_stream << indent(indentation) << "    }\n";
                }
                else if (is_struct_type(member.type, struct_types))
                {
                    output_stream << indent(indentation) << "    from_json(j.at(\"" << member.name << "\"), output." << member.name << ");\n";
                }
                else if (is_enum_type(member.type, enum_types))
                {
                    output_stream << indent(indentation) << "    {\n";
                    output_stream << indent(indentation) << "        std::string const s = j.at(\"" << member.name << "\").get<std::string>();\n";
                    output_stream << indent(indentation) << "        read_enum(output." << member.name << ", std::string_view{s});\n";
                    output_stream << indent(indentation) << "    }\n";
                }
                else if (is_bool_type(member.type))
                {
                    output_stream << indent(indentation) << "    output." << member.name << " = j.at(\"" << member.name << "\").get<bool>();\n";
                }
                else if (is_string_type(member.type))
                {
                    output_stream << indent(indentation) << "    output." << member.name << " = std::pmr::string{j.at(\"" << member.name << "\").get<std::string>()};\n";
                }
                else if (is_filesystem_path_type(member.type))
                {
                    output_stream << indent(indentation) << "    output." << member.name << " = std::filesystem::path{j.at(\"" << member.name << "\").get<std::string>()};\n";
                }
                else
                {
                    // int, uint, int64, uint64, double, float, etc.
                    output_stream << indent(indentation) << "    output." << member.name << " = j.at(\"" << member.name << "\").get<" << member.type.name << ">();\n";
                }
            }

            output_stream << indent(indentation) << "}\n";

        return std::pmr::string{ output_stream.str() };
    }

    namespace
    {
        void generate_write_value_json_code(
            std::stringstream& output_stream,
            Type const& type,
            std::string_view const name
        )
        {
                if (is_string_type(type))
                {
                    output_stream << "std::string{" << name << "}";
                }
                else if (is_filesystem_path_type(type))
                {
                    output_stream << name << ".generic_string()";
                }
                else
                {
                    output_stream << name;
                }
        }
    }

    std::pmr::string generate_write_struct_json_code(
        Struct const& struct_type,
        std::pmr::unordered_map<std::pmr::string, Enum> const& enum_types,
        std::pmr::unordered_map<std::pmr::string, Struct> const& struct_types,
        int const indentation
    )
    {
        std::stringstream output_stream;

            output_stream << indent(indentation) << "export void to_json(nlohmann::json& j, " << struct_type.name << " const& output)\n";
        output_stream << indent(indentation) << "{\n";
            output_stream << indent(indentation) << "    j = nlohmann::json::object();\n";

        for (Member const& member : struct_type.members)
        {
            if (is_variant_type(member.type))
            {
                    output_stream << indent(indentation) << "    {\n";
                    output_stream << indent(indentation) << "        nlohmann::json data_obj = nlohmann::json::object();\n";

                std::pmr::vector<std::pmr::string> const type_names = get_variadic_types(
                    member.type.name
                );

                for (std::size_t index = 0; index < type_names.size(); ++index)
                {
                    std::pmr::string const& type_name = type_names[index];
                    Type const underlying_type = { .name = type_name };

                        output_stream << indent(indentation + 8);
                    if (index != 0)
                    {
                        output_stream << "else ";
                    }
                    output_stream << "if (std::holds_alternative<" << type_name << ">(output." << member.name << "))\n";
                        output_stream << indent(indentation) << "        {\n";
                        output_stream << indent(indentation) << "            data_obj[\"type\"] = \"" << type_name << "\";\n";

                    if (is_enum_type(underlying_type, enum_types))
                    {
                            output_stream << indent(indentation) << "            data_obj[\"value\"] = std::string{write_enum(std::get<" << type_name << ">(output." << member.name << "))};\n";
                    }
                    else if (is_struct_type(underlying_type, struct_types))
                    {
                            output_stream << indent(indentation) << "            to_json(data_obj[\"value\"], std::get<" << type_name << ">(output." << member.name << "));\n";
                    }
                    else
                    {
                            output_stream << indent(indentation) << "            data_obj[\"value\"] = ";
                            generate_write_value_json_code(output_stream, underlying_type, "std::get<" + std::string{type_name} + ">(output." + std::string{member.name} + ")");
                            output_stream << ";\n";
                    }

                        output_stream << indent(indentation) << "        }\n";
                }

                    output_stream << indent(indentation) << "        j[\"" << member.name << "\"] = std::move(data_obj);\n";
                    output_stream << indent(indentation) << "    }\n";
            }
            else
            {
                if (is_optional_type(member.type))
                {
                    Type const value_type = Type{ get_optional_value_type(member.type) };
                    if (is_struct_type(value_type, struct_types))
                    {
                            output_stream << indent(indentation) << "    if (output." << member.name << ".has_value())\n";
                            output_stream << indent(indentation) << "    {\n";
                            output_stream << indent(indentation) << "        to_json(j[\"" << member.name << "\"], output." << member.name << ".value());\n";
                            output_stream << indent(indentation) << "    }\n";
                    }
                    else
                    {
                            output_stream << indent(indentation) << "    if (output." << member.name << ".has_value())\n";
                            output_stream << indent(indentation) << "    {\n";
                            if (is_string_type(value_type))
                            {
                                output_stream << indent(indentation) << "        j[\"" << member.name << "\"] = std::string{output." << member.name << ".value()};\n";
                            }
                            else if (is_filesystem_path_type(value_type))
                            {
                                output_stream << indent(indentation) << "        j[\"" << member.name << "\"] = output." << member.name << ".value().generic_string();\n";
                            }
                            else if (is_enum_type(value_type, enum_types))
                            {
                                output_stream << indent(indentation) << "        j[\"" << member.name << "\"] = std::string{write_enum(output." << member.name << ".value())};\n";
                            }
                            else
                            {
                                output_stream << indent(indentation) << "        j[\"" << member.name << "\"] = output." << member.name << ".value();\n";
                            }
                            output_stream << indent(indentation) << "    }\n";
                    }
                }
                else
                {
                    if (is_struct_type(member.type, struct_types) || is_vector_type(member.type))
                    {
                            if (is_vector_type(member.type))
                            {
                                std::pmr::string const elem_type = get_vector_value_type(member.type);
                                Type const elem = Type{ elem_type };
                                output_stream << indent(indentation) << "    {\n";
                                output_stream << indent(indentation) << "        nlohmann::json arr = nlohmann::json::array();\n";
                                output_stream << indent(indentation) << "        for (auto const& elem : output." << member.name << ")\n";
                                output_stream << indent(indentation) << "        {\n";
                                if (is_struct_type(elem, struct_types))
                                {
                                    output_stream << indent(indentation) << "            nlohmann::json e;\n";
                                    output_stream << indent(indentation) << "            to_json(e, elem);\n";
                                    output_stream << indent(indentation) << "            arr.push_back(std::move(e));\n";
                                }
                                else if (is_enum_type(elem, enum_types))
                                {
                                    output_stream << indent(indentation) << "            arr.push_back(std::string{write_enum(elem)});\n";
                                }
                                else if (is_string_type(elem))
                                {
                                    output_stream << indent(indentation) << "            arr.push_back(std::string{elem});\n";
                                }
                                else if (is_filesystem_path_type(elem))
                                {
                                    output_stream << indent(indentation) << "            arr.push_back(elem.generic_string());\n";
                                }
                                else
                                {
                                    output_stream << indent(indentation) << "            arr.push_back(elem);\n";
                                }
                                output_stream << indent(indentation) << "        }\n";
                                output_stream << indent(indentation) << "        j[\"" << member.name << "\"] = nlohmann::json{{\"size\", output." << member.name << ".size()}, {\"elements\", std::move(arr)}};\n";
                                output_stream << indent(indentation) << "    }\n";
                            }
                            else
                            {
                                output_stream << indent(indentation) << "    to_json(j[\"" << member.name << "\"], output." << member.name << ");\n";
                            }
                    }
                    else if (is_enum_type(member.type, enum_types))
                    {
                            output_stream << indent(indentation) << "    j[\"" << member.name << "\"] = std::string{write_enum(output." << member.name << ")};\n";
                    }
                    else
                    {
                            output_stream << indent(indentation) << "    j[\"" << member.name << "\"] = ";
                            generate_write_value_json_code(output_stream, member.type, "output." + member.name);
                            output_stream << ";\n";
                    }
                }
            }
        }

        output_stream << indent(indentation) << "}\n";

        return std::pmr::string{ output_stream.str() };
    }

    namespace
    {
        std::optional<Enum> parse_enum(std::istream& input_stream)
        {
            std::pmr::string name;
            input_stream >> name;

            if (name == "class")
            {
                input_stream >> name;
            }

            if (name.back() == ';')
            {
                return std::nullopt;
            }

            std::stringstream enum_content_string_stream;

            while (input_stream.good())
            {
                std::pmr::string string;
                input_stream >> string;
                enum_content_string_stream << string;

                if (string.back() == ';')
                {
                    break;
                }
            }

            std::string const enum_content = enum_content_string_stream.str();

            auto const open_bracket_location = std::find(enum_content.begin(), enum_content.end(), '{');
            auto const close_bracket_location = std::find(open_bracket_location + 1, enum_content.end(), '}');

            std::pmr::vector<std::pmr::string> enum_values;

            {
                auto const is_alphabetic = [](char const c) -> bool
                {
                    return std::isalpha(c) != 0;
                };

                auto const is_not_alphabetic_neither_digit = [](char const c) -> bool
                {
                    return (std::isalpha(c) == 0) && (std::isdigit(c) == 0) && (c != '_') && (c != '-');
                };

                auto current_location = open_bracket_location + 1;

                while ((current_location != close_bracket_location) && (current_location != enum_content.end()))
                {
                    auto const alphabetic_location = std::find_if(current_location, enum_content.end(), is_alphabetic);
                    auto const space_or_equal_or_comma_location = std::find_if(alphabetic_location, enum_content.end(), is_not_alphabetic_neither_digit);

                    enum_values.push_back(std::pmr::string{ alphabetic_location, space_or_equal_or_comma_location });

                    current_location = std::find(space_or_equal_or_comma_location, enum_content.end(), ',');
                }
            }

            return Enum
            {
                .name = std::move(name),
                .values = std::move(enum_values)
            };
        }

        std::optional<std::pair<std::pmr::string, Type>> parse_using_type(
            std::span<std::pmr::string const> const strings
        )
        {
            if (strings[0] != "using" || (strings.size() < 4))
            {
                return std::nullopt;
            }

            std::pmr::string const& name = strings[1];

            std::stringstream string_stream;

            for (std::size_t i = 3; i < strings.size(); ++i)
            {
                std::string_view const string = strings[i];
                if (string.back() == ';')
                {
                    std::string_view const value = { string.begin(), string.end() - 1 };
                    string_stream << value;

                    Type type
                    {
                        .name = std::pmr::string{string_stream.str()}
                    };

                    return std::make_pair(name, std::move(type));
                }

                string_stream << string;
            }

            return std::nullopt;
        }

        std::optional<Struct> parse_struct(std::istream& input_stream)
        {
            std::pmr::string name;
            input_stream >> name;

            if (name.back() == ';')
            {
                return std::nullopt;
            }

            std::pmr::vector<std::pmr::string> strings;

            while (input_stream.good())
            {
                std::pmr::string string;
                input_stream >> string;

                if (string.empty())
                {
                    continue;
                }

                if (string.front() == '#' || string.starts_with("//"))
                {
                    std::getline(input_stream, string);
                    continue;
                }

                strings.push_back(string);

                if (string == "};")
                {
                    break;
                }
            }

            strings.erase(strings.begin());
            strings.pop_back();

            std::pmr::vector<std::pair<std::pmr::string, Type>> using_types;
            std::pmr::vector<std::pmr::string> member_strings;

            for (std::size_t i = 0; i < strings.size();)
            {
                auto const begin_location = strings.begin() + i;

                auto const semicolon_location = std::find_if(
                    begin_location + 1,
                    strings.end(),
                    [](std::pmr::string const& string) -> bool { return string.back() == ';'; }
                );

                if (*begin_location == "using")
                {
                    std::optional<std::pair<std::pmr::string, Type>> const using_type =
                        parse_using_type({ begin_location, semicolon_location + 1 });

                    if (using_type)
                    {
                        using_types.push_back(*using_type);
                    }
                    else
                    {
                        throw std::runtime_error{ "Failed to parse 'using <type> = <type>;' expression." };
                    }

                    i += 1 + std::distance(begin_location, semicolon_location);
                }
                else if (*begin_location == "friend" || *begin_location == "auto")
                {
                    i += 1 + std::distance(begin_location, semicolon_location);
                }
                else
                {
                    for (auto iterator = begin_location; iterator != semicolon_location; ++iterator)
                    {
                        member_strings.push_back(*iterator);
                        ++i;
                    }
                    member_strings.push_back(*semicolon_location);
                    ++i;
                }
            }

            std::pmr::vector<std::pair<std::pmr::string, std::pmr::string>> values;

            for (std::size_t i = 0; i < member_strings.size();)
            {
                std::pmr::string const& type_name = member_strings[i];

                auto const using_type_location = std::find_if(
                    using_types.begin(),
                    using_types.end(),
                    [&type_name](std::pair<std::pmr::string, Type> const& pair) { return pair.first == type_name; }
                );

                if (using_type_location != using_types.end())
                {
                    std::pmr::string const& member_name = member_strings[i + 1];
                    if (member_name.back() == ';')
                    {
                        std::pmr::string value = { member_name.begin(), member_name.end() - 1 };
                        values.push_back(std::make_pair(using_type_location->second.name, value));
                    }
                    else
                    {
                        values.push_back(std::make_pair(using_type_location->second.name, member_name));
                    }
                }
                else
                {
                    std::pmr::string const& member_name = member_strings[i + 1];
                    if (member_name.back() == ';')
                    {
                        std::pmr::string value = { member_name.begin(), member_name.end() - 1 };
                        values.push_back(std::make_pair(type_name, value));
                    }
                    else
                    {
                        values.push_back(std::make_pair(type_name, member_name));
                    }
                }

                auto const begin_location = member_strings.begin() + i;
                auto const semicolon_location = std::find_if(
                    begin_location + 1,
                    member_strings.end(),
                    [](std::pmr::string const& string) -> bool { return string.back() == ';'; }
                );

                i += 1 + std::distance(begin_location, semicolon_location);
            }

            std::pmr::vector<Member> members;

            for (std::size_t i = 0; i < values.size(); ++i)
            {
                std::pmr::string const& type_name = values[i].first;
                std::pmr::string const& member_name = values[i].second;

                members.push_back(
                    Member
                    {
                        .type = Type
                        {
                            .name = type_name,
                        },
                        .name = member_name
                    }
                );
            }

            return Struct
            {
                .name = std::move(name),
                .members = std::move(members)
            };
        }

        template<typename Value_type>
        std::pmr::unordered_map<std::pmr::string, Value_type> create_name_map(
            std::span<Value_type const> const values
        )
        {
            std::pmr::unordered_map<std::pmr::string, Value_type> map;
            map.reserve(values.size());

            for (Value_type const& value : values)
            {
                map.insert(std::make_pair(value.name, value));
            }

            return map;
        }

        void generate_write_forward_declarations(
            std::ostream& output_stream,
            std::span<Struct const> const structs,
            int const indentation
        )
        {
            for (Struct const struct_type : structs)
            {
                output_stream << indent(indentation) << "export template<typename Writer_type>\n";
                output_stream << indent(indentation) << "    void write_object(\n";
                output_stream << indent(indentation) << "        Writer_type& writer,\n";
                output_stream << indent(indentation) << "        " << struct_type.name << " const& input\n";
                output_stream << indent(indentation) << "    );\n";
                output_stream << "\n";
            }
        }
    }

    File_types identify_file_types(
        std::istream& input_stream
    )
    {
        std::pmr::vector<Enum> enums;
        std::pmr::vector<Struct> structs;

        while (input_stream.good())
        {
            std::pmr::string value;
            input_stream >> value;

            if (value == "enum")
            {
                std::optional<Enum> const enum_type = parse_enum(input_stream);

                if (enum_type)
                {
                    enums.push_back(*enum_type);
                }
            }
            else if (value == "struct")
            {
                std::optional<Struct> const struct_type = parse_struct(input_stream);

                if (struct_type)
                {
                    structs.push_back(*struct_type);
                }
            }
        }

        std::pmr::unordered_map<std::pmr::string, Enum> const enum_map = create_name_map<Enum>(
            enums
        );

        std::pmr::unordered_map<std::pmr::string, Struct> struct_map = create_name_map<Struct>(
            structs
        );

        return File_types
        {
            .enums = std::move(enums),
            .structs = std::move(structs)
        };
    }

    void generate_read_json_code(
        std::istream& input_stream,
        std::ostream& output_stream,
        std::string_view const export_module_name,
        std::string_view const module_name_to_import,
        std::string_view const namespace_name
    )
    {
        File_types const file_types = identify_file_types(
            input_stream
        );

        std::pmr::unordered_map<std::pmr::string, Enum> const enum_map = create_name_map<Enum>(
            file_types.enums
        );

        std::pmr::unordered_map<std::pmr::string, Struct> struct_map = create_name_map<Struct>(
            file_types.structs
        );

        output_stream << "module;\n";
        output_stream << '\n';
        output_stream << "#include <filesystem>\n";
        output_stream << "#include <format>\n";
        output_stream << "#include <iostream>\n";
        output_stream << "#include <memory_resource>\n";
        output_stream << "#include <optional>\n";
        output_stream << "#include <variant>\n";
        output_stream << "#include <vector>\n";
        output_stream << '\n';
        output_stream << "export module " << export_module_name << ";\n";
            output_stream << "#include <nlohmann/json.hpp>\n";
            output_stream << '\n';
            output_stream << "module;\n";
            output_stream << '\n';
            output_stream << "#include <filesystem>\n";
            output_stream << "#include <memory_resource>\n";
            output_stream << "#include <optional>\n";
            output_stream << "#include <variant>\n";
            output_stream << "#include <vector>\n";
            output_stream << '\n';
            output_stream << "#include <nlohmann/json.hpp>\n";
            output_stream << '\n';
            output_stream << "export module " << export_module_name << ";\n";
            output_stream << '\n';
            output_stream << "module;\n";
            output_stream << '\n';
            output_stream << "#include <filesystem>\n";
            output_stream << "#include <memory_resource>\n";
            output_stream << "#include <optional>\n";
            output_stream << "#include <variant>\n";
            output_stream << "#include <vector>\n";
            output_stream << '\n';
            output_stream << "#include <nlohmann/json.hpp>\n";
            output_stream << '\n';
            output_stream << "export module " << export_module_name << ";\n";
            output_stream << '\n';
            output_stream << "import " << module_name_to_import << ";\n";
        output_stream << '\n';
        output_stream << "namespace " << namespace_name << '\n';
        output_stream << "{\n";

        // Generate read_enum()
        output_stream << "    export template<typename Enum_type, typename Event_value>\n";
        output_stream << "        bool read_enum(Enum_type& output, Event_value const value)\n";
        output_stream << "    {\n";
        output_stream << "        return false;\n";
        output_stream << "    };\n";
        output_stream << "\n";
        for (Enum const& enum_type : file_types.enums)
        {
            output_stream << generate_read_enum_json_code(enum_type, 4);
            output_stream << "\n";
        }

            // Forward declare from_json for all structs
            for (Struct const& struct_info : file_types.structs)
            {
                output_stream << "    export void from_json(nlohmann::json const& j, iris::" << struct_info.name << "& output);\n";
            }
            output_stream << "\n";

            // Generate from_json for each struct
            for (Struct const& struct_type : file_types.structs)
            {
                output_stream << generate_read_struct_json_code(struct_type, enum_map, struct_map, 4);
                output_stream << "\n";
            }

        output_stream << "}\n";

    }

    void generate_write_json_code(
        std::istream& input_stream,
        std::ostream& output_stream,
        std::string_view const export_module_name,
        std::string_view const module_name_to_import,
        std::string_view const namespace_name
    )
    {
        /*File_types const file_types = identify_file_types(
            input_stream
        );

        std::string_view const initial_code = R"(module;

#include <compare>

#include <nlohmann/json.hpp>

export module iris.json_serializer.operators;

import std;

import iris.binary_serializer.generics;
import iris.core;

namespace iris::binary_serializer
{

)";

        for (Enum const& enum_type : file_types.enums)
        {
            output_stream << generate_write_enum_json_code(enum_type, 4);
            output_stream << "\n";
        }

        // Forward declare to_json for all structs
        for (Struct const& struct_info : file_types.structs)
        {
            output_stream << "    export void to_json(nlohmann::json& j, " << struct_info.name << " const& output);\n";
        }
        output_stream << "\n";

        std::pmr::unordered_map<std::pmr::string, Enum> const enum_map = create_name_map<Enum>(
            file_types.enums
        );

        std::pmr::unordered_map<std::pmr::string, Struct> struct_map = create_name_map<Struct>(
            file_types.structs
        );

        for (Struct const& struct_type : file_types.structs)
        {
            output_stream << generate_write_struct_json_code(struct_type, enum_map, struct_map, 4);
            output_stream << "\n";
        }

        output_stream << "}\n";*/
    }

    void generate_json_data(
        std::istream& input_stream,
        std::ostream& output_stream
    )
    {
        File_types const file_types = identify_file_types(
            input_stream
        );

            nlohmann::json j;

            j["enums"] = nlohmann::json::array();
            for (Enum const& enum_info : file_types.enums)
            {
                nlohmann::json enum_json;
                enum_json["name"] = std::string{ enum_info.name };
                enum_json["values"] = nlohmann::json::array();
                for (std::pmr::string const& value : enum_info.values)
                    enum_json["values"].push_back(std::string{ value });
                j["enums"].push_back(std::move(enum_json));
            }

            j["structs"] = nlohmann::json::array();
            for (Struct const& struct_info : file_types.structs)
            {
                nlohmann::json struct_json;
                struct_json["name"] = std::string{ struct_info.name };
                struct_json["members"] = nlohmann::json::array();
                for (Member const& member : struct_info.members)
                {
                    nlohmann::json member_json;
                    member_json["type"] = nlohmann::json{ {"name", std::string{ member.type.name }} };
                    member_json["name"] = std::string{ member.name };
                    struct_json["members"].push_back(std::move(member_json));
                }
                j["structs"].push_back(std::move(struct_json));
            }

            output_stream << j;
    }

    std::pmr::string join(std::span<std::pmr::string const> const strings, std::string_view const delimiter)
    {
        std::pmr::string output;

        for (unsigned int i = 0; i < strings.size(); ++i)
        {
            output += strings[i];

            if ((i + 1) == strings.size())
            {
                break;
            }

            output += delimiter;
        }

        return output;
    }

    std::pmr::string generate_variant_types_enum_name(std::string_view const parent_type_name, std::span<std::pmr::string const> const variant_type_names)
    {
        if (parent_type_name == "Type_reference")
        {
            return std::pmr::string{ "Type_reference_enum" };
        }
        else if (parent_type_name == "Expression")
        {
            return std::pmr::string{ "Expression_enum" };
        }
        else
        {
            return join(variant_type_names, "_") + "_enum";
        }
    }

    std::pmr::string to_typescript_type(
        Type const& type,
        std::string_view const parent_type_name,
        std::pmr::unordered_map<std::pmr::string, Enum> const& enum_map,
        std::pmr::unordered_map<std::pmr::string, Struct> const& struct_map,
        std::pmr::unordered_map<std::pmr::string, std::pmr::string> const& replace_map,
        bool const intermediate_representation
    )
    {
        {
            auto const location = replace_map.find(type.name);
            if (location != replace_map.end())
            {
                std::pmr::string const& new_type = location->second;
                return to_typescript_type(Type{ .name = new_type.c_str() }, parent_type_name, enum_map, struct_map, replace_map, intermediate_representation);
            }
        }

        if (is_int_type(type) || is_int64_type(type) || is_uint_type(type) || is_uint64_type(type) || is_double_type(type))
        {
            return std::pmr::string{ "number" };
        }
        else if (is_string_type(type))
        {
            return std::pmr::string{ "string" };
        }
        else if (is_bool_type(type))
        {
            return std::pmr::string{ "boolean" };
        }
        else if (is_enum_type(type, enum_map))
        {
            return type.name;
        }
        else if (is_struct_type(type, struct_map))
        {
            return type.name;
        }
        else if (is_vector_type(type))
        {
            std::pmr::string const value_type = get_vector_value_type(type);
            std::pmr::string const typescript_type = to_typescript_type(Type{ .name = value_type }, parent_type_name, enum_map, struct_map, replace_map, intermediate_representation);

            if (intermediate_representation)
            {
                return std::pmr::string(std::format("{}[]", typescript_type));
            }
            else
            {
                return std::pmr::string{ std::format("Vector<{}>", typescript_type) };
            }
        }
        else if (is_variant_type(type))
        {
            std::pmr::vector<std::pmr::string> const variant_type_names = get_variadic_types(
                type.name
            );

            std::pmr::string const variant_type_enum_name = generate_variant_types_enum_name(parent_type_name, variant_type_names);

            std::pmr::string const typescript_variant_type = join(variant_type_names, " | ");

            return std::pmr::string{ std::format("Variant<{}, {}>", variant_type_enum_name, typescript_variant_type) };
        }
        else if (is_optional_type(type))
        {
            std::pmr::string const value_type = get_optional_value_type(type);

            if (intermediate_representation)
            {
                auto const location = replace_map.find(value_type);
                if (location != replace_map.end())
                {
                    std::pmr::string const& new_type = location->second;
                    return to_typescript_type(Type{ .name = new_type }, parent_type_name, enum_map, struct_map, replace_map, intermediate_representation);
                }
                else
                {
                    return to_typescript_type(Type{ .name = value_type }, parent_type_name, enum_map, struct_map, replace_map, intermediate_representation);
                }
            }
            else
            {
                return to_typescript_type(Type{ .name = value_type }, parent_type_name, enum_map, struct_map, replace_map, intermediate_representation);
            }
        }
        else if (is_filesystem_path_type(type))
        {
            return "string";
        }
        else
        {
            throw std::runtime_error{ "Type not handled!" };
        }
    }

    void generate_variant_enums(
        std::ostream& output_stream,
        std::span<Struct const> const struct_infos
    )
    {
        for (Struct const& struct_info : struct_infos)
        {
            for (Member const& member : struct_info.members)
            {
                if (is_variant_type(member.type))
                {
                    std::string_view const variant_string = member.type.name;

                    std::pmr::vector<std::pmr::string> const variant_type_names = get_variadic_types(
                        variant_string
                    );

                    std::pmr::string const variant_type_enum_name = generate_variant_types_enum_name(struct_info.name, variant_type_names);

                    output_stream << "export enum " << variant_type_enum_name << " {\n";
                    {
                        for (std::string_view const name : variant_type_names)
                        {
                            output_stream << std::format("    {} = \"{}\",\n", name, name);
                        }
                    }
                    output_stream << "}\n\n";
                }
            }
        }
    }

    bool is_expression_type(std::string_view const type_name)
    {
        return type_name.starts_with("Expression") || type_name.ends_with("expression");
    }

    bool contains_expressions(std::pmr::string const& type_name, std::pmr::unordered_map<std::pmr::string, Struct> const& struct_map)
    {
        if (type_name != "Statement")
        {
            auto const location = struct_map.find(type_name);
            if (location != struct_map.end())
            {
                Struct const& struct_info = location->second;
                for (Member const& member : struct_info.members)
                {
                    if (is_expression_type(member.type.name))
                    {
                        return true;
                    }
                    else if (is_vector_type(member.type))
                    {
                        std::pmr::string const value_type = get_vector_value_type(member.type);
                        if (is_expression_type(value_type))
                        {
                            return true;
                        }
                    }
                    else if (is_optional_type(member.type))
                    {
                        std::pmr::string const value_type = get_optional_value_type(member.type);
                        if (is_expression_type(value_type))
                        {
                            return true;
                        }
                    }
                }
            }
        }

        return is_expression_type(type_name);
    }

    void generate_typescript_interface(
        std::istream& input_stream,
        std::ostream& output_stream
    )
    {
        File_types const file_types = identify_file_types(
            input_stream
        );


        std::pmr::unordered_map<std::pmr::string, Enum> const enum_map = create_name_map<Enum>(
            file_types.enums
        );

        std::pmr::unordered_map<std::pmr::string, Struct> struct_map = create_name_map<Struct>(
            file_types.structs
        );

        {
            output_stream << "export interface Vector<T> {\n";
            output_stream << "    size: number;\n";
            output_stream << "    elements: T[];\n";
            output_stream << "}\n\n";
        }

        {
            output_stream << "export interface Variant<Type_enum, T> {\n";
            output_stream << "    type: Type_enum;\n";
            output_stream << "    value: T;\n";
            output_stream << "}\n\n";
        }

        for (Enum const& enum_info : file_types.enums)
        {
            output_stream << "export enum " << enum_info.name << " {\n";
            {
                for (std::pmr::string const& value : enum_info.values)
                {
                    output_stream << std::format("    {} = \"{}\",\n", value, value);
                }
            }
            output_stream << "}\n\n";
        }

        generate_variant_enums(output_stream, file_types.structs);

        for (Struct const& struct_info : file_types.structs)
        {
            output_stream << "export interface " << struct_info.name << " {\n";
            {
                for (Member const& member : struct_info.members)
                {
                    output_stream << std::format("    {}{}: {};\n", member.name, is_optional_type(member.type) ? "?" : "", to_typescript_type(member.type, struct_info.name, enum_map, struct_map, {}, false));
                }
            }
            output_stream << "}\n\n";
        }
    }

    void generate_variant_core_to_intermediate_representation(
        std::ostream& output_stream,
        Type const& type,
        std::string_view const parent_type_name,
        std::pmr::unordered_map<std::pmr::string, Enum> const& enum_map,
        std::pmr::unordered_map<std::pmr::string, Struct> const& struct_map,
        int const indentation
    )
    {
        std::pmr::vector<std::pmr::string> const variant_types = get_variadic_types(type.name);
        std::pmr::string const variant_type_enum_name = generate_variant_types_enum_name(parent_type_name, variant_types);

        output_stream << indent(indentation) << "switch (core_value.data.type) {\n";

        for (std::pmr::string const& variant_type : variant_types)
        {
            output_stream << indent(indentation) << std::format("    case Core.{}.{}: {{\n", variant_type_enum_name, variant_type);
            output_stream << indent(indentation) << "        return {\n";
            output_stream << indent(indentation) << "            type: core_value.data.type,\n";

            if (contains_expressions(variant_type, struct_map))
            {
                output_stream << indent(indentation) << std::format("            value: core_to_intermediate_{}(core_value.data.value as Core.{}, statement)\n", to_lowercase(variant_type), variant_type);
            }
            else if (is_enum_type(Type{ .name = variant_type }, enum_map))
            {
                output_stream << indent(indentation) << std::format("            value: core_value.data.value as {}\n", variant_type);
            }
            else
            {
                output_stream << indent(indentation) << std::format("            value: core_to_intermediate_{}(core_value.data.value as Core.{})\n", to_lowercase(variant_type), variant_type);
            }

            output_stream << indent(indentation) << "        };\n";
            output_stream << indent(indentation) << "    }\n";
        }

        output_stream << indent(indentation) << "}\n";
    }

    void generate_variant_intermediate_to_core_representation(
        std::ostream& output_stream,
        Type const& type,
        std::string_view const parent_type_name,
        std::pmr::unordered_map<std::pmr::string, Enum> const& enum_map,
        int const indentation
    )
    {
        std::pmr::vector<std::pmr::string> const variant_types = get_variadic_types(type.name);
        std::pmr::string const variant_type_enum_name = generate_variant_types_enum_name(parent_type_name, variant_types);

        if (variant_type_enum_name == "Expression_enum")
        {
            output_stream << indent(indentation) << "const expression_index = expressions.length;\n\n";
        }

        output_stream << indent(indentation) << "switch (intermediate_value.data.type) {\n";

        if (variant_type_enum_name == "Expression_enum")
        {
            for (std::pmr::string const& variant_type : variant_types)
            {
                output_stream << indent(indentation) << std::format("    case {}.{}: {{\n", variant_type_enum_name, variant_type);
                output_stream << indent(indentation) << std::format("        intermediate_to_core_{}(intermediate_value.data.value as {}, expressions);\n", to_lowercase(variant_type), variant_type);
                output_stream << indent(indentation) << "        break;\n";
                output_stream << indent(indentation) << "    }\n";
            }
        }
        else
        {
            for (std::pmr::string const& variant_type : variant_types)
            {
                output_stream << indent(indentation) << std::format("    case {}.{}: {{\n", variant_type_enum_name, variant_type);
                output_stream << indent(indentation) << "        return {\n";
                output_stream << indent(indentation) << "            data: {\n";
                output_stream << indent(indentation) << "                type: intermediate_value.data.type,\n";

                if (is_enum_type(Type{ .name = variant_type }, enum_map))
                {
                    output_stream << indent(indentation) << std::format("                value: intermediate_value.data.value as {}\n", variant_type);
                }
                else
                {
                    output_stream << indent(indentation) << std::format("                value: intermediate_to_core_{}(intermediate_value.data.value as {})\n", to_lowercase(variant_type), variant_type);
                }


                output_stream << indent(indentation) << "            }\n";
                output_stream << indent(indentation) << "        };\n";
                output_stream << indent(indentation) << "    }\n";
            }
        }

        output_stream << indent(indentation) << "}\n";

        if (variant_type_enum_name == "Expression_enum")
        {
            output_stream << "\n";
            output_stream << indent(indentation) << "if (intermediate_value.source_position !== undefined) {\n";
            output_stream << indent(indentation) << "    expressions[expression_index].source_position = intermediate_value.source_position;\n";
            output_stream << indent(indentation) << "}\n";
        }
    }

    void generate_typescript_intermediate_representation(
        std::istream& input_stream,
        std::ostream& output_stream
    )
    {
        File_types const file_types = identify_file_types(
            input_stream
        );


        std::pmr::unordered_map<std::pmr::string, Enum> const enum_map = create_name_map<Enum>(
            file_types.enums
        );

        std::pmr::unordered_map<std::pmr::string, Struct> struct_map = create_name_map<Struct>(
            file_types.structs
        );

        char const* const head = R"(
import * as Core from "./Core_interface";

export interface Variant<Type_enum, T> {
    type: Type_enum;
    value: T;
}

export interface Module {
    name: string;
    imports: Import_module_with_alias[];
    declarations: Declaration[];
    comment?: string;
    source_file_path?: string;
}

export function create_intermediate_representation(core_module: Core.Module): Module {

    const imports = core_module.dependencies.alias_imports.elements.map(value => core_to_intermediate_import_module_with_alias(value));
    const declarations = create_declarations(core_module);

    return {
        name: core_module.name,
        imports: imports,
        declarations: declarations,
        comment: core_module.comment,
        source_file_path: core_module.source_file_path
    };
}

export function create_core_module(module: Module, language_version: Core.Language_version): Core.Module {

    const alias_imports = module.imports.map(value => intermediate_to_core_import_module_with_alias(value));

    const export_alias: Core.Alias_type_declaration[] = [];
    const internal_alias: Core.Alias_type_declaration[] = [];
    const export_enums: Core.Enum_declaration[] = [];
    const internal_enums: Core.Enum_declaration[] = [];
    const export_functions: Core.Function_declaration[] = [];
    const internal_functions: Core.Function_declaration[] = [];
    const export_function_constructors: Core.Function_constructor[] = [];
    const internal_function_constructors: Core.Function_constructor[] = [];
    const export_global_variables: Core.Global_variable_declaration[] = [];
    const internal_global_variables: Core.Global_variable_declaration[] = [];
    const export_structs: Core.Struct_declaration[] = [];
    const internal_structs: Core.Struct_declaration[] = [];
    const export_type_constructors: Core.Type_constructor[] = [];
    const internal_type_constructors: Core.Type_constructor[] = [];
    const export_unions: Core.Union_declaration[] = [];
    const internal_unions: Core.Union_declaration[] = [];
    const function_definitions: Core.Function_definition[] = [];

    for (const declaration of module.declarations) {
        switch (declaration.type) {
            case Declaration_type.Alias: {
                const array = declaration.is_export ? export_alias : internal_alias;
                array.push(intermediate_to_core_alias_type_declaration(declaration.value as Alias_type_declaration));
                break;
            }
            case Declaration_type.Enum: {
                const array = declaration.is_export ? export_enums : internal_enums;
                array.push(intermediate_to_core_enum_declaration(declaration.value as Enum_declaration));
                break;
            }
            case Declaration_type.Function: {
                const array = declaration.is_export ? export_functions : internal_functions;
                const function_value = declaration.value as Function;
                array.push(intermediate_to_core_function_declaration(function_value.declaration));
                if (function_value.definition !== undefined) {
                    function_definitions.push(intermediate_to_core_function_definition(function_value.definition));
                }
                break;
            }
            case Declaration_type.Function_constructor: {
                const array = declaration.is_export ? export_function_constructors : internal_function_constructors;
                array.push(intermediate_to_core_function_constructor(declaration.value as Function_constructor));
                break;
            }
            case Declaration_type.Global_variable: {
                const array = declaration.is_export ? export_global_variables : internal_global_variables;
                array.push(intermediate_to_core_global_variable_declaration(declaration.value as Global_variable_declaration));
                break;
            }
            case Declaration_type.Struct: {
                const array = declaration.is_export ? export_structs : internal_structs;
                array.push(intermediate_to_core_struct_declaration(declaration.value as Struct_declaration));
                break;
            }
            case Declaration_type.Type_constructor: {
                const array = declaration.is_export ? export_type_constructors : internal_type_constructors;
                array.push(intermediate_to_core_type_constructor(declaration.value as Type_constructor));
                break;
            }
            case Declaration_type.Union: {
                const array = declaration.is_export ? export_unions : internal_unions;
                array.push(intermediate_to_core_union_declaration(declaration.value as Union_declaration));
                break;
            }
        }
    }

    return {
        language_version: language_version,
        name: module.name,
        dependencies: {
            alias_imports: {
                size: alias_imports.length,
                elements: alias_imports
            }
        },
        export_declarations: {
            alias_type_declarations: {
                size: export_alias.length,
                elements: export_alias
            },
            enum_declarations: {
                size: export_enums.length,
                elements: export_enums
            },
            function_constructors: {
                size: export_function_constructors.length,
                elements: export_function_constructors
            },
            function_declarations: {
                size: export_functions.length,
                elements: export_functions
            },
            global_variable_declarations: {
                size: export_global_variables.length,
                elements: export_global_variables,
            },
            struct_declarations: {
                size: export_structs.length,
                elements: export_structs
            },
            type_constructors: {
                size: export_type_constructors.length,
                elements: export_type_constructors
            },
            union_declarations: {
                size: export_unions.length,
                elements: export_unions
            }
        },
        internal_declarations: {
            alias_type_declarations: {
                size: internal_alias.length,
                elements: internal_alias
            },
            enum_declarations: {
                size: internal_enums.length,
                elements: internal_enums
            },
            function_constructors: {
                size: export_function_constructors.length,
                elements: export_function_constructors
            },
            function_declarations: {
                size: internal_functions.length,
                elements: internal_functions
            },
            global_variable_declarations: {
                size: internal_global_variables.length,
                elements: internal_global_variables,
            },
            struct_declarations: {
                size: internal_structs.length,
                elements: internal_structs
            },
            type_constructors: {
                size: internal_type_constructors.length,
                elements: internal_type_constructors
            },
            union_declarations: {
                size: internal_unions.length,
                elements: internal_unions
            }
        },
        definitions: {
            function_definitions: {
                size: function_definitions.length,
                elements: function_definitions
            }
        },
        comment: module.comment,
        source_file_path: module.source_file_path
    };
}

export enum Declaration_type {
    Alias,
    Enum,
    Function,
    Function_constructor,
    Global_variable,
    Struct,
    Type_constructor,
    Union
}

export interface Declaration {
    name: string;
    type: Declaration_type;
    is_export: boolean;
    value: Alias_type_declaration | Enum_declaration | Function | Function_constructor | Global_variable_declaration | Struct_declaration | Type_constructor | Union_declaration
}

function create_declarations(module: Core.Module): Declaration[] {

    const declarations: Declaration[] = [
        ...module.export_declarations.alias_type_declarations.elements.map((value, index): Declaration => { return { name: value.name, type: Declaration_type.Alias, is_export: true, value: core_to_intermediate_alias_type_declaration(value) }; }),
        ...module.export_declarations.enum_declarations.elements.map((value, index): Declaration => { return { name: value.name, type: Declaration_type.Enum, is_export: true, value: core_to_intermediate_enum_declaration(value) }; }),
        ...module.export_declarations.function_declarations.elements.map((value, index): Declaration => { return { name: value.name, type: Declaration_type.Function, is_export: true, value: core_to_intermediate_function(module, value) }; }),
        ...module.export_declarations.function_constructors.elements.map((value, index): Declaration => { return { name: value.name, type: Declaration_type.Function_constructor, is_export: true, value: core_to_intermediate_function_constructor(value) }; }),
        ...module.export_declarations.global_variable_declarations.elements.map((value, index): Declaration => { return { name: value.name, type: Declaration_type.Global_variable, is_export: true, value: core_to_intermediate_global_variable_declaration(value) }; }),
        ...module.export_declarations.struct_declarations.elements.map((value, index): Declaration => { return { name: value.name, type: Declaration_type.Struct, is_export: true, value: core_to_intermediate_struct_declaration(value) }; }),
        ...module.export_declarations.type_constructors.elements.map((value, index): Declaration => { return { name: value.name, type: Declaration_type.Type_constructor, is_export: true, value: core_to_intermediate_type_constructor(value) }; }),
        ...module.export_declarations.union_declarations.elements.map((value, index): Declaration => { return { name: value.name, type: Declaration_type.Union, is_export: true, value: core_to_intermediate_union_declaration(value) }; }),
        ...module.internal_declarations.alias_type_declarations.elements.map((value, index): Declaration => { return { name: value.name, type: Declaration_type.Alias, is_export: false, value: core_to_intermediate_alias_type_declaration(value) }; }),
        ...module.internal_declarations.enum_declarations.elements.map((value, index): Declaration => { return { name: value.name, type: Declaration_type.Enum, is_export: false, value: core_to_intermediate_enum_declaration(value) }; }),
        ...module.internal_declarations.function_declarations.elements.map((value, index): Declaration => { return { name: value.name, type: Declaration_type.Function, is_export: false, value: core_to_intermediate_function(module, value) }; }),
        ...module.internal_declarations.function_constructors.elements.map((value, index): Declaration => { return { name: value.name, type: Declaration_type.Function_constructor, is_export: false, value: core_to_intermediate_function_constructor(value) }; }),
        ...module.internal_declarations.global_variable_declarations.elements.map((value, index): Declaration => { return { name: value.name, type: Declaration_type.Global_variable, is_export: false, value: core_to_intermediate_global_variable_declaration(value) }; }),
        ...module.internal_declarations.struct_declarations.elements.map((value, index): Declaration => { return { name: value.name, type: Declaration_type.Struct, is_export: false, value: core_to_intermediate_struct_declaration(value) }; }),
        ...module.internal_declarations.type_constructors.elements.map((value, index): Declaration => { return { name: value.name, type: Declaration_type.Type_constructor, is_export: false, value: core_to_intermediate_type_constructor(value) }; }),
        ...module.internal_declarations.union_declarations.elements.map((value, index): Declaration => { return { name: value.name, type: Declaration_type.Union, is_export: false, value: core_to_intermediate_union_declaration(value) }; }),
    ];

    return declarations;
}

export interface Function {
    declaration: Function_declaration;
    definition: Function_definition | undefined;
}

function core_to_intermediate_function(module: Core.Module, declaration: Core.Function_declaration): Function {

    const definition_index = module.definitions.function_definitions.elements.findIndex(value => value.name === declaration.name);
    const definition = definition_index !== -1 ? module.definitions.function_definitions.elements[definition_index] : undefined;

    const value: Function = {
        declaration: core_to_intermediate_function_declaration(declaration),
        definition: definition !== undefined ? core_to_intermediate_function_definition(definition) : undefined
    };

    return value;
}

export interface Statement {
    expression: Expression;

}

function core_to_intermediate_statement(core_value: Core.Statement): Statement {
    return {
        expression: core_to_intermediate_expression(core_value.expressions.elements[0], core_value)
    };
}

function intermediate_to_core_statement(intermediate_value: Statement): Core.Statement {

    const expressions: Core.Expression[] = [];
    intermediate_to_core_expression(intermediate_value.expression, expressions);

    return {
        expressions: {
            size: expressions.length,
            elements: expressions
        }
    };
}

)";

        output_stream << head;

        for (Enum const& enum_info : file_types.enums)
        {
            output_stream << "export enum " << enum_info.name << " {\n";
            {
                for (std::pmr::string const& value : enum_info.values)
                {
                    output_stream << std::format("    {} = \"{}\",\n", value, value);
                }
            }
            output_stream << "}\n\n";
        }

        generate_variant_enums(output_stream, file_types.structs);

        std::array<char const*, 6> const struct_ignore_list = {
            "Module",
            "Module_declarations",
            "Module_definitions",
            "Module_dependencies",
            "Statement",
            "Expression_index"
        };

        std::pmr::unordered_map<std::pmr::string, std::pmr::string> const replace_type_map = {
            { "Expression_index", "Expression" }
        };

        for (Struct const& struct_info : file_types.structs)
        {
            if (std::find(struct_ignore_list.begin(), struct_ignore_list.end(), struct_info.name) != struct_ignore_list.end())
            {
                continue;
            }

            output_stream << "export interface " << struct_info.name << " {\n";
            {
                for (Member const& member : struct_info.members)
                {
                    output_stream << std::format("    {}{}: {};\n", member.name, is_optional_type(member.type) ? "?" : "", to_typescript_type(member.type, struct_info.name, enum_map, struct_map, replace_type_map, true));
                }
            }
            output_stream << "}\n\n";

            {
                if (contains_expressions(struct_info.name, struct_map))
                {
                    output_stream << std::format("function core_to_intermediate_{}(core_value: Core.{}, statement: Core.Statement): {} {{\n", to_lowercase(struct_info.name), struct_info.name, struct_info.name);
                }
                else
                {
                    output_stream << std::format("function core_to_intermediate_{}(core_value: Core.{}): {} {{\n", to_lowercase(struct_info.name), struct_info.name, struct_info.name);
                }

                {
                    output_stream << "    return {\n";
                    for (Member const& member : struct_info.members)
                    {
                        if (member.type.name == "Expression_index")
                        {
                            output_stream << std::format("        {}: core_to_intermediate_expression(statement.expressions.elements[core_value.{}.expression_index], statement),\n", member.name, member.name);
                        }
                        else if (member.type.name == "std::optional<Expression_index>")
                        {
                            output_stream << std::format("        {}: core_value.{} !== undefined ? core_to_intermediate_expression(statement.expressions.elements[core_value.{}.expression_index], statement) : undefined,\n", member.name, member.name, member.name);
                        }
                        else if (is_variant_type(member.type))
                        {
                            output_stream << std::format("        {}: (() => {{\n", member.name);
                            generate_variant_core_to_intermediate_representation(output_stream, member.type, struct_info.name, enum_map, struct_map, 12);
                            output_stream << std::format("        }})(),\n", member.name);
                        }
                        else if (is_vector_type(member.type))
                        {
                            Type const vector_value_type = Type{ .name = get_vector_value_type(member.type) };
                            if (vector_value_type.name == "Expression_index")
                            {
                                output_stream << std::format("        {}: core_value.{}.elements.map(value => core_to_intermediate_expression(statement.expressions.elements[value.expression_index], statement)),\n", member.name, member.name);
                            }
                            else if (is_struct_type(vector_value_type, struct_map))
                            {
                                if (contains_expressions(vector_value_type.name, struct_map))
                                {
                                    output_stream << std::format("        {}: core_value.{}.elements.map(value => core_to_intermediate_{}(value, statement)),\n", member.name, member.name, to_lowercase(vector_value_type.name));
                                }
                                else
                                {
                                    output_stream << std::format("        {}: core_value.{}.elements.map(value => core_to_intermediate_{}(value)),\n", member.name, member.name, to_lowercase(vector_value_type.name));
                                }
                            }
                            else
                            {
                                output_stream << std::format("        {}: core_value.{}.elements,\n", member.name, member.name);
                            }
                        }
                        else if (is_struct_type(member.type, struct_map))
                        {
                            output_stream << std::format("        {}: core_to_intermediate_{}(core_value.{}),\n", member.name, to_lowercase(member.type.name), member.name);
                        }
                        else if (is_optional_type(member.type))
                        {
                            Type const value_type = Type{ get_optional_value_type(member.type) };
                            if (is_struct_type(value_type, struct_map))
                            {
                                output_stream << std::format("        {}: core_value.{} !== undefined ? core_to_intermediate_{}(core_value.{}) : undefined,\n", member.name, member.name, to_lowercase(value_type.name), member.name);
                            }
                            else if (is_vector_type(value_type))
                            {
                                Type const vector_value_type = Type{ get_vector_value_type(value_type) };
                                if (is_struct_type(vector_value_type, struct_map))
                                {
                                    output_stream << std::format("        {}: core_value.{} !== undefined ? core_value.{}.elements.map(value => core_to_intermediate_{}(value)) : undefined,\n", member.name, member.name, member.name, to_lowercase(vector_value_type.name));
                                }
                                else
                                {
                                    output_stream << std::format("        {}: core_value.{} !== undefined ? core_value.{}.elements : undefined,\n", member.name, member.name, member.name);
                                }
                            }
                            else
                            {
                                output_stream << std::format("        {}: core_value.{},\n", member.name, member.name);
                            }
                        }
                        else
                        {
                            output_stream << std::format("        {}: core_value.{},\n", member.name, member.name);
                        }

                    }
                    output_stream << "    };\n";
                }
                output_stream << "}\n\n";
            }

            {
                if (contains_expressions(struct_info.name, struct_map))
                {
                    std::string const return_type = is_expression_type(struct_info.name) ? "void" : std::format("Core.{}", struct_info.name);
                    output_stream << std::format("function intermediate_to_core_{}(intermediate_value: {}, expressions: Core.Expression[]): {} {{\n", to_lowercase(struct_info.name), struct_info.name, return_type);
                }
                else
                {
                    output_stream << std::format("function intermediate_to_core_{}(intermediate_value: {}): Core.{} {{\n", to_lowercase(struct_info.name), struct_info.name, struct_info.name);
                }

                if (struct_info.name == "Expression")
                {
                    generate_variant_intermediate_to_core_representation(output_stream, struct_info.members[0].type, struct_info.name, enum_map, 4);
                }
                else if (struct_info.name == "Type_reference")
                {
                    generate_variant_intermediate_to_core_representation(output_stream, struct_info.members[0].type, struct_info.name, enum_map, 4);
                }
                else if (contains_expressions(struct_info.name, struct_map))
                {
                    bool const is_expression = is_expression_type(struct_info.name);

                    std::pmr::string const core_value_type = is_expression ? "Expression" : struct_info.name;

                    if (is_expression)
                    {
                        output_stream << "    const index = expressions.length;\n";
                        output_stream << "    expressions.push({} as Core.Expression);\n";
                        output_stream << "    const core_value: Core.Expression = {\n";
                        output_stream << "        data: {\n";
                        output_stream << std::format("            type: Core.Expression_enum.{},\n", struct_info.name);
                        output_stream << "            value: {\n";
                    }
                    else
                    {
                        output_stream << std::format("    const core_value: Core.{} = {{\n", core_value_type);
                    }

                    int const indentation = is_expression ? 8 : 0;

                    for (Member const& member : struct_info.members)
                    {
                        if (member.type.name == "Expression_index")
                        {
                            output_stream << indent(indentation) << std::format("        {}: {{\n", member.name);
                            output_stream << indent(indentation) << "            expression_index: -1\n";
                            output_stream << indent(indentation) << "        },\n";
                        }
                        else if (member.type.name == "std::optional<Expression_index>")
                        {
                            output_stream << indent(indentation) << std::format("        {}: intermediate_value.{} !== undefined ? {{ expression_index: -1 }} : undefined,\n", member.name, member.name);
                        }
                        else if (is_vector_type(member.type) && get_vector_value_type(member.type) == "Expression_index")
                        {
                            output_stream << indent(indentation) << std::format("        {}: {{\n", member.name);
                            output_stream << indent(indentation) << "            size: 0,\n";
                            output_stream << indent(indentation) << "            elements: []\n";
                            output_stream << indent(indentation) << "        }\n";
                        }
                        else if (is_vector_type(member.type))
                        {
                            std::pmr::string const value_type = get_vector_value_type(member.type);
                            output_stream << indent(indentation) << std::format("        {}: {{\n", member.name);
                            output_stream << indent(indentation) << std::format("            size: intermediate_value.{}.length,\n", member.name);

                            if (contains_expressions(value_type, struct_map))
                            {
                                output_stream << indent(indentation) << std::format("            elements: intermediate_value.{}.map(value => intermediate_to_core_{}(value, expressions))\n", member.name, to_lowercase(value_type));
                            }
                            else
                            {
                                output_stream << indent(indentation) << std::format("            elements: intermediate_value.{}.map(value => intermediate_to_core_{}(value))\n", member.name, to_lowercase(value_type));
                            }

                            output_stream << indent(indentation) << "        },\n";
                        }
                        else if (is_struct_type(member.type, struct_map))
                        {
                            output_stream << indent(indentation) << std::format("        {}: intermediate_to_core_{}(intermediate_value.{}),\n", member.name, to_lowercase(member.type.name), member.name);
                        }
                        else
                        {
                            output_stream << indent(indentation) << std::format("        {}: intermediate_value.{},\n", member.name, member.name);
                        }
                    }

                    if (is_expression)
                    {
                        output_stream << "            }\n";
                        output_stream << "        }\n";
                    }
                    output_stream << "    };\n";

                    if (is_expression)
                    {
                        output_stream << "\n    expressions[index] = core_value;\n";
                    }

                    for (Member const& member : struct_info.members)
                    {
                        std::string const core_value_member = is_expression ? std::format("(core_value.data.value as Core.{}).{}", struct_info.name, member.name) : std::format("core_value.{}", member.name);

                        if (member.type.name == "Expression_index")
                        {
                            output_stream << "\n";
                            output_stream << std::format("    {}.expression_index = expressions.length;\n", core_value_member);
                            output_stream << std::format("    intermediate_to_core_expression(intermediate_value.{}, expressions);\n", member.name);
                        }
                        else if (member.type.name == "std::optional<Expression_index>")
                        {
                            output_stream << "\n";
                            output_stream << std::format("    if (intermediate_value.{} !== undefined) {{\n", member.name);
                            output_stream << std::format("        {} = {{ expression_index: expressions.length }};\n", core_value_member);
                            output_stream << std::format("        intermediate_to_core_expression(intermediate_value.{}, expressions);\n", member.name);
                            output_stream << "    }\n";
                        }
                        else if (is_vector_type(member.type) && get_vector_value_type(member.type) == "Expression_index")
                        {
                            output_stream << "\n";
                            output_stream << std::format("    for (const element of intermediate_value.{}) {{\n", member.name);
                            output_stream << std::format("        {}.elements.push({{ expression_index: expressions.length }});\n", core_value_member);
                            output_stream << "        intermediate_to_core_expression(element, expressions);\n";
                            output_stream << "    }\n";
                            output_stream << std::format("    {}.size = {}.elements.length;\n", core_value_member, core_value_member);
                        }
                    }

                    if (!is_expression)
                    {
                        output_stream << "\n    return core_value;\n";
                    }
                }
                else
                {
                    output_stream << "    return {\n";
                    for (Member const& member : struct_info.members)
                    {
                        if (is_vector_type(member.type))
                        {
                            Type const vector_value_type = Type{ .name = get_vector_value_type(member.type) };
                            if (vector_value_type.name == "Expression_index")
                            {
                                output_stream << std::format("                {}: {{\n", member.name);
                                output_stream << "                    size: 0,\n";
                                output_stream << "                    elements: []\n";
                                output_stream << "                }\n";
                            }
                            else if (is_struct_type(vector_value_type, struct_map))
                            {
                                output_stream << std::format("        {}: {{\n", member.name);
                                output_stream << std::format("            size: intermediate_value.{}.length,\n", member.name);
                                output_stream << std::format("            elements: intermediate_value.{}.map(value => intermediate_to_core_{}(value)),\n", member.name, to_lowercase(vector_value_type.name));
                                output_stream << "        },\n";
                            }
                            else
                            {
                                output_stream << std::format("        {}: {{\n", member.name);
                                output_stream << std::format("            size: intermediate_value.{}.length,\n", member.name);
                                output_stream << std::format("            elements: intermediate_value.{},\n", member.name);
                                output_stream << "        },\n";
                            }
                        }
                        else if (member.type.name == "Expression_index")
                        {
                            output_stream << std::format("                {}: {{\n", member.name);
                            output_stream << "                    expression_index: -1\n";
                            output_stream << "                },\n";
                        }
                        else if (is_struct_type(member.type, struct_map))
                        {
                            output_stream << std::format("        {}: intermediate_to_core_{}(intermediate_value.{}),\n", member.name, to_lowercase(member.type.name), member.name);
                        }
                        else if (is_optional_type(member.type))
                        {
                            Type const value_type = Type{ get_optional_value_type(member.type) };
                            if (is_struct_type(value_type, struct_map))
                            {
                                output_stream << std::format("        {}: intermediate_value.{} !== undefined ? intermediate_to_core_{}(intermediate_value.{}) : undefined,\n", member.name, member.name, to_lowercase(value_type.name), member.name);
                            }
                            else if (is_vector_type(value_type))
                            {
                                output_stream << std::format("        {}: intermediate_value.{} !== undefined ? {{ size: intermediate_value.{}.length, elements : intermediate_value.{} }} : undefined,\n", member.name, member.name, member.name, member.name);
                            }
                            else
                            {
                                output_stream << std::format("        {}: intermediate_value.{},\n", member.name, member.name);
                            }
                        }
                        else
                        {
                            output_stream << std::format("        {}: intermediate_value.{},\n", member.name, member.name);
                        }

                    }
                    output_stream << "    };\n";
                }

                output_stream << "}\n\n";
            }

            if (is_expression_type(struct_info.name) && struct_info.name != "Expression")
            {
                auto const replace_by_valid_name = [](std::pmr::string const& name) -> std::pmr::string
                {
                    if (name == "arguments")
                        return "args";

                    return name;
                };

                output_stream << std::format("export function create_{}(", to_lowercase(struct_info.name));
                for (std::size_t member_index = 0; member_index < struct_info.members.size(); ++member_index)
                {
                    Member const& member = struct_info.members[member_index];

                    std::pmr::string const transformed_member_type = to_typescript_type(member.type, struct_info.name, enum_map, struct_map, replace_type_map, true);
                    output_stream << std::format("{}: {}{}", replace_by_valid_name(member.name), transformed_member_type, is_optional_type(member.type) ? " | undefined" : "");

                    if ((member_index + 1) < struct_info.members.size())
                        output_stream << ", ";
                }
                output_stream << "): Expression {\n";
                output_stream << std::format("    const {}: {} = {{\n", to_lowercase(struct_info.name), struct_info.name);
                for (std::size_t member_index = 0; member_index < struct_info.members.size(); ++member_index)
                {
                    Member const& member = struct_info.members[member_index];
                    output_stream << std::format("        {}: {},\n", member.name, replace_by_valid_name(member.name));
                }
                output_stream << "    };\n";
                output_stream << "    return {\n";
                output_stream << "        data: {\n";
                output_stream << std::format("            type: Expression_enum.{},\n", struct_info.name);
                output_stream << std::format("            value: {}\n", to_lowercase(struct_info.name));
                output_stream << "        }\n";
                output_stream << "    };\n";
                output_stream << "}\n";
            }
        }
    }

    void generate_serialize_binary_code(
        std::istream& input_stream,
        std::ostream& output_stream
    )
    {
        File_types const file_types = identify_file_types(
            input_stream
        );

        std::string_view const initial_code = R"(module;

#include <compare>

export module iris.binary_serializer.generated;

import iris.binary_serializer.generics;
import iris.core;

namespace iris::binary_serializer
{
)";

        output_stream << initial_code;

        std::string_view const function_template = R"(    template <>
    void serialize(Serializer& serializer, {} const& value)
    {{
{}
    }}

    template <>
    void deserialize(Deserializer& deserializer, {}& value)
    {{
{}
    }}

)";

        for (Struct const& struct_info : file_types.structs)
        {
            std::pmr::string serialize_body;

            for (std::size_t member_index = 0; member_index < struct_info.members.size(); ++member_index)
            {
                Member const& member = struct_info.members[member_index];
                serialize_body += std::format("        serialize(serializer, value.{});", member.name);

                if (member_index + 1 < struct_info.members.size())
                    serialize_body += '\n';
            }

            std::pmr::string deserialize_body;

            for (std::size_t member_index = 0; member_index < struct_info.members.size(); ++member_index)
            {
                Member const& member = struct_info.members[member_index];
                deserialize_body += std::format("        deserialize(deserializer, value.{});", member.name);

                if (member_index + 1 < struct_info.members.size())
                    deserialize_body += '\n';
            }

            std::string const output = std::vformat(function_template, std::make_format_args(struct_info.name, serialize_body, struct_info.name, deserialize_body));

            output_stream << output;
        }

        output_stream << "}\n";

        output_stream.flush();
    }

    void generate_serialize_json_code(
        std::istream& input_stream,
        std::ostream& output_stream
    )
    {
        File_types const file_types = identify_file_types(
            input_stream
        );

        std::string_view const initial_code = R"(module;

#define EXPORT export
#include "Generics.h"

#include <compare>

export module iris.json_serializer.generated;

//import iris.json_serializer.generics;
import iris.core;

namespace iris::json
{
)";

        std::string_view const enum_template = R"(    EXPORT template <>
    JSON to_json({} const& value)
    {{
        switch (value)
        {{
{}
        }}
    }}

    EXPORT template <>
    void from_json(JSON const& data, {}& output)
    {{
        std::string const& value = data.get<std::string>();
{}
    }}

)";

        std::string_view const function_template = R"(    EXPORT template <>
    JSON to_json({} const& value)
    {{
        JSON data;
{}
        return data;
    }}

    EXPORT template <>
    void from_json(JSON const& data, {}& value)
    {{
{}
    }}

)";

        output_stream << initial_code;

        for (Enum const& enum_info : file_types.enums)
        {
            std::pmr::string to_json_body;

            for (std::size_t enum_value_index = 0; enum_value_index < enum_info.values.size(); ++enum_value_index)
            {
                std::string_view const enum_value = enum_info.values[enum_value_index];
                to_json_body += std::format("            case {}::{}: return \"{}\";", enum_info.name, enum_value, enum_value);

                if (enum_value_index + 1 < enum_info.values.size())
                    to_json_body += '\n';
            }
            if (!enum_info.values.empty())
                to_json_body += std::format("\n        default: return \"{}\";", enum_info.values[0]);

            std::pmr::string from_json_body;

            for (std::size_t enum_value_index = 0; enum_value_index < enum_info.values.size(); ++enum_value_index)
            {
                std::string_view const enum_value = enum_info.values[enum_value_index];
                from_json_body += std::format("        if (value == \"{}\") {{ output = {}::{}; return; }}", enum_value, enum_info.name, enum_value);

                if (enum_value_index + 1 < enum_info.values.size())
                    from_json_body += '\n';
            }
            if (!enum_info.values.empty())
                from_json_body += std::format("\n        output = {}::{};", enum_info.name, enum_info.values[0]);

            std::string const output = std::vformat(enum_template, std::make_format_args(enum_info.name, to_json_body, enum_info.name, from_json_body));

            output_stream << output;
        }

        for (Struct const& struct_info : file_types.structs)
        {
            std::pmr::string to_json_body;

            for (std::size_t member_index = 0; member_index < struct_info.members.size(); ++member_index)
            {
                Member const& member = struct_info.members[member_index];
                if (is_optional_type(member.type))
                    to_json_body += std::format("        if (value.{}.has_value()) data[\"{}\"] = to_json(value.{});", member.name, member.name, member.name);
                else
                    to_json_body += std::format("        data[\"{}\"] = to_json(value.{});", member.name, member.name);

                if (member_index + 1 < struct_info.members.size())
                    to_json_body += '\n';
            }

            std::pmr::string from_json_body;

            for (std::size_t member_index = 0; member_index < struct_info.members.size(); ++member_index)
            {
                Member const& member = struct_info.members[member_index];
                if (is_optional_type(member.type) || is_vector_type(member.type) || is_deque_type(member.type))
                    from_json_body += std::format("        if (data.contains(\"{}\")) from_json(data.at(\"{}\"), value.{});", member.name, member.name, member.name);
                else
                    from_json_body += std::format("        from_json(data.at(\"{}\"), value.{});", member.name, member.name);

                if (member_index + 1 < struct_info.members.size())
                    from_json_body += '\n';
            }

            std::string const output = std::vformat(function_template, std::make_format_args(struct_info.name, to_json_body, struct_info.name, from_json_body));

            output_stream << output;
        }

        output_stream << "}\n";

        output_stream.flush();
    }

    void generate_json_operators_code(
        std::istream& input_stream,
        std::ostream& output_stream
    )
    {
        File_types const file_types = identify_file_types(
            input_stream
        );

        std::string_view const initial_code = R"(module;

#include "Generics.h"

#include <compare>
#include <variant>

#include <nlohmann/json.hpp>

export module iris.json_serializer.operators;

import iris.json_serializer.generated;
//import iris.json_serializer.generics;
import iris.core;

namespace iris::json::operators
{
)";

    std::string_view const enum_template = R"(
        export std::istream& operator>>(std::istream& input_stream, {}& value)
        {{
            JSON data{{}};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }}

        export std::ostream& operator<<(std::ostream& output_stream, {} const value)
        {{
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }}
)";

    std::string_view const struct_template = R"(
        export std::istream& operator>>(std::istream& input_stream, {}& value)
        {{
            JSON data{{}};
            input_stream >> data;

            from_json(data, value);

            return input_stream;
        }}

        export std::ostream& operator<<(std::ostream& output_stream, {} const& value)
        {{
            JSON const data = to_json(value);

            output_stream << data.dump(4) << '\n';

            return output_stream;
        }}
)";

        output_stream << initial_code;

        for (Enum const& enum_type : file_types.enums)
        {
            std::string const output = std::vformat(enum_template, std::make_format_args(enum_type.name, enum_type.name));
            output_stream << output;
        }

        for (Struct const& struct_type : file_types.structs)
        {
            std::string const output = std::vformat(struct_template, std::make_format_args(struct_type.name, struct_type.name));
            output_stream << output;
        }

        output_stream << "}\n";
    }

    void generate_expressions_visitor(
        std::istream& input_stream,
        std::ostream& output_stream
    )
    {
        File_types const file_types = identify_file_types(
            input_stream
        );

        std::string_view const initial_code = R"(export module iris.core.expressions_visitor;

import std;
import iris.core;

namespace iris
{
)";

    std::string_view const function_template = R"(
    export void visit_expressions_recursively(iris::Statement const& statement, iris::Expression const& expression, std::function<void(iris::Statement const& statement, iris::Expression const& expression)> const& predicate)
    {{
        predicate(statement, expression);
{}
    }}
)";

        std::string_view const if_expression_type_template = R"(
        if (std::holds_alternative<{}>(expression.data))
        {{
            {} const& data = std::get<{}>(expression.data);
{}
        }}
        )";

        std::string_view const visit_expression_index_template = R"(
            visit_expressions_recursively(statement, statement.expressions[data.{}.expression_index], predicate);)";

        std::string_view const visit_optional_expression_index_template = R"(
            if (data.{}.has_value())
                visit_expressions_recursively(statement, statement.expressions[data.{}->expression_index], predicate);)";

        std::string_view const visit_vector_expression_index_template = R"(
            for (std::size_t index = 0; index < data.{}.size(); ++index)
                visit_expressions_recursively(statement, statement.expressions[data.{}[index].expression_index], predicate);)";

        output_stream << initial_code;

        std::stringstream if_stream;

        for (Struct const& struct_type : file_types.structs)
        {
            if (struct_type.name.ends_with("_expression"))
            {
                std::stringstream visit_expression_stream;

                bool at_least_one = false;
                
                for (Member const& member : struct_type.members)
                {
                    if (member.type.name == "Expression_index")
                    {
                        visit_expression_stream << std::vformat(visit_expression_index_template, std::make_format_args(member.name));
                        at_least_one = true;
                    }
                    else if (member.type.name == "std::optional<Expression_index>")
                    {
                        visit_expression_stream << std::vformat(visit_optional_expression_index_template, std::make_format_args(member.name, member.name));
                        at_least_one = true;
                    }
                    else if (member.type.name == "std::pmr::vector<Expression_index>")
                    {
                        visit_expression_stream << std::vformat(visit_vector_expression_index_template, std::make_format_args(member.name, member.name));
                        at_least_one = true;
                    }
                }
                
                if (at_least_one)
                {
                    std::string const visit_stream_expression_string = visit_expression_stream.str();
                    if_stream << std::vformat(if_expression_type_template, std::make_format_args(struct_type.name, struct_type.name, struct_type.name, visit_stream_expression_string));
                }
            }
        }

        std::string const if_stream_string = if_stream.str();
        output_stream << std::vformat(function_template, std::make_format_args(if_stream_string));

        output_stream << "}\n";
    }

    void generate_soa_array_natvis(
        std::istream& input_stream,
        std::ostream& output_stream
    )
    {
        (void)input_stream;

        output_stream << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
        output_stream << "<AutoVisualizer xmlns=\"http://schemas.microsoft.com/vstudio/debugger/natvis/2010\">\n";
        output_stream << "  <Intrinsic Name=\"calculate_soa_data_pointer\" Expression=\"((char*)data) + count*stride\">\n";
        output_stream << "    <Parameter Name=\"data\" Type=\"void*\" />\n";
        output_stream << "    <Parameter Name=\"count\" Type=\"int\" />\n";
        output_stream << "    <Parameter Name=\"stride\" Type=\"int\" />\n";
        output_stream << "  </Intrinsic>\n";
        output_stream << "\n";

        for (int member_count = 0; member_count <= 18; ++member_count)
        {
            output_stream << std::format("  <!-- {} elements -->\n", member_count);

            std::stringstream type_name;
            type_name << "iris::Soa_array&lt;" << member_count;
            for (int index = 0; index < (2 * member_count + 2); ++index)
            {
                type_name << ",*";
            }
            type_name << "&gt;";

            output_stream << std::format("  <Type Name=\"{}\">\n", type_name.str());
            output_stream << "    <DisplayString>Soa_array::&lt;{\"$T1\",sb}, {$T2}&gt;</DisplayString>\n";

            if (member_count > 0)
            {
                output_stream << "    <Expand>\n";

                for (int member_index = 0; member_index < member_count; ++member_index)
                {
                    int const name_type_index = 3 + (2 * member_index);
                    int const data_type_index = name_type_index + 1;

                    std::stringstream offset_stream;
                    if (member_index == 0)
                    {
                        offset_stream << "0";
                    }
                    else
                    {
                        for (int previous_member_index = 0; previous_member_index < member_index; ++previous_member_index)
                        {
                            if (previous_member_index != 0)
                            {
                                offset_stream << "+";
                            }

                            int const previous_data_type_index = 4 + (2 * previous_member_index);
                            offset_stream << std::format("sizeof($T{})", previous_data_type_index);
                        }
                    }

                    output_stream << std::format("      <Synthetic Name=\"{}\">\n", member_index);
                    output_stream << std::format("        <DisplayString>{{\"$T{}: $T{}[]\",sb}}</DisplayString>\n", name_type_index, data_type_index);
                    output_stream << "        <Expand>\n";
                    output_stream << "          <IndexListItems>\n";
                    output_stream << "            <Size>$T2</Size>\n";
                    output_stream << std::format("            <ValueNode>(($T{}*)calculate_soa_data_pointer(data, $T2, {}))[$i]</ValueNode>\n", data_type_index, offset_stream.str());
                    output_stream << "          </IndexListItems>\n";
                    output_stream << "        </Expand>\n";
                    output_stream << "      </Synthetic>\n";
                    output_stream << "\n";
                }

                output_stream << "    </Expand>\n";
            }

            output_stream << "  </Type>\n";
        }

        output_stream << "</AutoVisualizer>\n";
    }

    void generate_soa_array_view_natvis(
        std::istream& input_stream,
        std::ostream& output_stream
    )
    {
        (void)input_stream;

        output_stream << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
        output_stream << "<AutoVisualizer xmlns=\"http://schemas.microsoft.com/vstudio/debugger/natvis/2010\">\n";
        output_stream << "  <Intrinsic Name=\"calculate_soa_data_pointer\" Expression=\"((char*)data) + count*previous_stride + start_index*current_stride\">\n";
        output_stream << "    <Parameter Name=\"data\" Type=\"void*\" />\n";
        output_stream << "    <Parameter Name=\"count\" Type=\"int\" />\n";
        output_stream << "    <Parameter Name=\"start_index\" Type=\"int\" />\n";
        output_stream << "    <Parameter Name=\"current_stride\" Type=\"int\" />\n";
        output_stream << "    <Parameter Name=\"previous_stride\" Type=\"int\" />\n";
        output_stream << "  </Intrinsic>\n";
        output_stream << "\n";

        for (int member_count = 0; member_count <= 18; ++member_count)
        {
            output_stream << std::format("  <!-- {} elements -->\n", member_count);

            std::stringstream type_name;
            type_name << "iris::Soa_array_view&lt;" << member_count;
            for (int index = 0; index < (2 * member_count + 1); ++index)
            {
                type_name << ",*";
            }
            type_name << "&gt;";

            output_stream << std::format("  <Type Name=\"{}\">\n", type_name.str());
            output_stream << "    <DisplayString>Soa_array_view::&lt;{\"$T1\",sb}&gt;</DisplayString>\n";

            if (member_count > 0)
            {
                output_stream << "    <Expand>\n";

                for (int member_index = 0; member_index < member_count; ++member_index)
                {
                    int const member_name_type_index = 2 + (2 * member_index);
                    int const member_data_type_index = member_name_type_index + 1;

                    std::stringstream offset_stream;
                    if (member_index == 0)
                    {
                        offset_stream << "0";
                    }
                    else
                    {
                        for (int previous_member_index = 0; previous_member_index < member_index; ++previous_member_index)
                        {
                            if (previous_member_index != 0)
                            {
                                offset_stream << "+";
                            }

                            int const previous_member_data_type_index = 3 + (2 * previous_member_index);
                            offset_stream << std::format("sizeof($T{})", previous_member_data_type_index);
                        }
                    }

                    output_stream << std::format("      <Synthetic Name=\"{}\">\n", member_index);
                    output_stream << std::format("        <DisplayString>{{\"$T{}: $T{}[]\",sb}}</DisplayString>\n", member_name_type_index, member_data_type_index);
                    output_stream << "        <Expand>\n";
                    output_stream << "          <IndexListItems>\n";
                    output_stream << "            <Size>end_index - start_index</Size>\n";
                    output_stream << std::format("            <ValueNode>(($T{}*)calculate_soa_data_pointer(data, length, start_index, sizeof($T{}), {}))[$i]</ValueNode>\n", member_data_type_index, member_data_type_index, offset_stream.str());
                    output_stream << "          </IndexListItems>\n";
                    output_stream << "        </Expand>\n";
                    output_stream << "      </Synthetic>\n";
                    output_stream << "\n";
                }

                output_stream << "    </Expand>\n";
            }

            output_stream << "  </Type>\n";
        }

        output_stream << "</AutoVisualizer>\n";
    }
}
