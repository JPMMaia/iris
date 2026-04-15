module;

#include <assert.h>

module iris.compiler.debug_info_formatter;

import llvm;
import std;

import iris.compiler.common;
import iris.compiler.types;
import iris.core;
import iris.core.declarations;

namespace iris::compiler
{
    namespace
    {
        std::pmr::string add_h_namespace_prefix(
            std::string_view const name,
            std::pmr::polymorphic_allocator<> const& allocator
        )
        {
            if (name.starts_with("iris::"))
                return std::pmr::string{ name, allocator };

            std::pmr::string output{ allocator };
            output += "iris::";
            output += name;
            return output;
        }

        std::string_view fundamental_type_name(Fundamental_type const type)
        {
            switch (type)
            {
            case Fundamental_type::Bool:
                return "Bool";
            case Fundamental_type::Byte:
                return "Byte";
            case Fundamental_type::Float16:
                return "Float16";
            case Fundamental_type::Float32:
                return "Float32";
            case Fundamental_type::Float64:
                return "Float64";
            case Fundamental_type::String:
                return "String";
            case Fundamental_type::Any_type:
                return "Any_type";
            case Fundamental_type::C_bool:
                return "C_bool";
            case Fundamental_type::C_char:
                return "C_char";
            case Fundamental_type::C_schar:
                return "C_schar";
            case Fundamental_type::C_uchar:
                return "C_uchar";
            case Fundamental_type::C_short:
                return "C_short";
            case Fundamental_type::C_ushort:
                return "C_ushort";
            case Fundamental_type::C_int:
                return "C_int";
            case Fundamental_type::C_uint:
                return "C_uint";
            case Fundamental_type::C_long:
                return "C_long";
            case Fundamental_type::C_ulong:
                return "C_ulong";
            case Fundamental_type::C_longlong:
                return "C_longlong";
            case Fundamental_type::C_ulonglong:
                return "C_ulonglong";
            case Fundamental_type::C_longdouble:
                return "C_longdouble";
            default:
                return "Unknown";
            }
        }

        std::string_view fundamental_type_c_equivalent_name(Fundamental_type const type)
        {
            switch (type)
            {
            case Fundamental_type::Bool:
                return "bool";
            case Fundamental_type::Byte:
                return "uint8_t";
            case Fundamental_type::Float32:
                return "float";
            case Fundamental_type::Float64:
                return "double";
            case Fundamental_type::C_bool:
                return "bool";
            case Fundamental_type::C_char:
                return "char";
            case Fundamental_type::C_schar:
                return "signed char";
            case Fundamental_type::C_uchar:
                return "unsigned char";
            case Fundamental_type::C_short:
                return "short";
            case Fundamental_type::C_ushort:
                return "unsigned short";
            case Fundamental_type::C_int:
                return "int";
            case Fundamental_type::C_uint:
                return "unsigned int";
            case Fundamental_type::C_long:
                return "long";
            case Fundamental_type::C_ulong:
                return "unsigned long";
            case Fundamental_type::C_longlong:
                return "long long";
            case Fundamental_type::C_ulonglong:
                return "unsigned long long";
            case Fundamental_type::C_longdouble:
                return "long double";
            default:
                return "";
            }
        }

        std::pmr::string format_debug_integer_type_name_internal(
            iris::Integer_type const& type,
            std::pmr::polymorphic_allocator<> const& output_allocator
        )
        {
            std::pmr::string output{ output_allocator };
            if (!type.is_signed)
                output += 'u';

            output += "int";
            output += std::format("{}", type.number_of_bits);
            output += "_t";
            return output;
        }

        std::pmr::string format_debug_type_name_internal(
            iris::Module const& core_module,
            iris::Type_reference const& type_reference,
            Debug_type_database const& debug_type_database,
            std::pmr::polymorphic_allocator<> const& output_allocator,
            std::pmr::polymorphic_allocator<> const& temporaries_allocator
        )
        {
            if (std::holds_alternative<Array_slice_type>(type_reference.data))
            {
                Array_slice_type const& value = std::get<Array_slice_type>(type_reference.data);
                std::pmr::string output{ output_allocator };
                output += "Array_slice::<";

                if (!value.element_type.empty())
                {
                    output += format_debug_type_name_internal(
                        core_module,
                        value.element_type[0],
                        debug_type_database,
                        output_allocator,
                        temporaries_allocator
                    );
                }
                else
                {
                    output += "Void";
                }

                output += ">";
                return output;
            }

            if (std::holds_alternative<Builtin_type_reference>(type_reference.data))
            {
                Builtin_type_reference const& value = std::get<Builtin_type_reference>(type_reference.data);
                return add_h_namespace_prefix(value.value, output_allocator);
            }

            if (std::holds_alternative<Constant_array_type>(type_reference.data))
            {
                Constant_array_type const& value = std::get<Constant_array_type>(type_reference.data);

                std::pmr::string element_type_name = !value.value_type.empty() ?
                    format_debug_type_name_internal(core_module, value.value_type[0], debug_type_database, output_allocator, temporaries_allocator) :
                    std::pmr::string{ "Void", output_allocator };

                std::pmr::string output{ output_allocator };
                output += element_type_name;
                output += std::format("[{}]", value.size);
                return output;
            }

            if (std::holds_alternative<Custom_type_reference>(type_reference.data))
            {
                Custom_type_reference const& value = std::get<Custom_type_reference>(type_reference.data);
                std::string_view const module_name = find_module_name(core_module, value.module_reference);

                std::string const mangled_name = mangle_name(debug_type_database.declaration_database, module_name, value.name);

                return std::pmr::string{mangled_name, output_allocator};
            }

            if (std::holds_alternative<Decimal_type>(type_reference.data))
            {
                Decimal_type const& value = std::get<Decimal_type>(type_reference.data);
                return add_h_namespace_prefix(std::format("Decimal{}", value.scale), output_allocator);
            }

            if (std::holds_alternative<Fundamental_type>(type_reference.data))
            {
                Fundamental_type const value = std::get<Fundamental_type>(type_reference.data);
                return format_debug_primitive_type_name(value, output_allocator);
            }

            if (std::holds_alternative<Function_pointer_type>(type_reference.data))
            {
                return std::pmr::string{ "void*", output_allocator };
            }

            if (std::holds_alternative<Integer_type>(type_reference.data))
            {
                Integer_type const& value = std::get<Integer_type>(type_reference.data);
                return format_debug_primitive_type_name(value, output_allocator);
            }

            if (std::holds_alternative<Null_pointer_type>(type_reference.data))
            {
                return std::pmr::string{ "nullptr", output_allocator };
            }

            if (std::holds_alternative<Parameter_type>(type_reference.data))
            {
                Parameter_type const& value = std::get<Parameter_type>(type_reference.data);
                return std::pmr::string{ value.name, output_allocator };
            }

            if (std::holds_alternative<Pointer_type>(type_reference.data))
            {
                Pointer_type const& value = std::get<Pointer_type>(type_reference.data);
                std::pmr::string output{ output_allocator };

                if (!value.element_type.empty())
                {
                    output += format_debug_type_name_internal(
                        core_module,
                        value.element_type[0],
                        debug_type_database,
                        output_allocator,
                        temporaries_allocator
                    );
                }
                else
                {
                    output += "Void";
                }

                output += "*";
                return output;
            }

            if (std::holds_alternative<Soa_array_type>(type_reference.data))
            {
                Soa_array_type const& value = std::get<Soa_array_type>(type_reference.data);
                return create_soa_array_debug_type_name(
                    core_module,
                    value,
                    debug_type_database,
                    output_allocator,
                    temporaries_allocator
                );
            }

            if (std::holds_alternative<Soa_array_view_type>(type_reference.data))
            {
                Soa_array_view_type const& value = std::get<Soa_array_view_type>(type_reference.data);
                return create_soa_array_view_debug_type_name(
                    core_module,
                    value,
                    debug_type_database,
                    output_allocator,
                    temporaries_allocator
                );
            }

            if (std::holds_alternative<Type_instance>(type_reference.data))
            {
                Type_instance const& value = std::get<Type_instance>(type_reference.data);
                std::string_view const module_name = find_module_name(core_module, value.type_constructor.module_reference);

                if (module_name.empty() || module_name == core_module.name)
                    return std::pmr::string{ value.type_constructor.name, output_allocator };

                return std::pmr::string{ std::format("{}.{}", module_name, value.type_constructor.name), output_allocator };
            }

            return std::pmr::string{ "Unknown", output_allocator };
        }
    }

    static std::pmr::vector<Soa_debug_member_info> get_soa_debug_member_infos(
        iris::Module const& core_module,
        std::span<iris::Type_reference const> const value_type,
        Debug_type_database const& debug_type_database
    )
    {
        if (value_type.empty())
            return {};

        if (!std::holds_alternative<Custom_type_reference>(value_type[0].data))
            return {};

        Custom_type_reference const& element_custom_type_reference = std::get<Custom_type_reference>(value_type[0].data);
        std::string_view const element_module_name = find_module_name(core_module, element_custom_type_reference.module_reference);
        std::pmr::string const element_module_name_key = std::pmr::string{ element_module_name };

        auto const module_location = debug_type_database.name_to_llvm_debug_type.find(element_module_name_key);
        if (module_location == debug_type_database.name_to_llvm_debug_type.end())
            return {};

        auto const type_location = module_location->second.find(element_custom_type_reference.name);
        if (type_location == module_location->second.end())
            return {};

        llvm::DIType* const element_debug_type = type_location->second;
        llvm::DICompositeType const* const composite_type = llvm::dyn_cast<llvm::DICompositeType>(element_debug_type);
        if (composite_type == nullptr)
            return {};

        std::pmr::vector<Soa_debug_member_info> member_infos;
        member_infos.reserve(composite_type->getElements().size());

        for (llvm::Metadata* const element : composite_type->getElements())
        {
            llvm::DIDerivedType const* const member_type = llvm::dyn_cast<llvm::DIDerivedType>(element);
            if (member_type == nullptr || member_type->getTag() != llvm::dwarf::DW_TAG_member)
                continue;

            std::string member_type_name = "Unknown";
            llvm::DIType const* member_base_type = member_type->getBaseType();
            while (llvm::DIDerivedType const* const derived_base_type = llvm::dyn_cast_or_null<llvm::DIDerivedType>(member_base_type))
            {
                member_base_type = derived_base_type->getBaseType();
            }

            if (member_base_type != nullptr)
            {
                std::string const candidate = member_base_type->getName().str();
                if (!candidate.empty())
                    member_type_name = candidate;
            }

            member_infos.push_back(
                Soa_debug_member_info
                {
                    .name = std::pmr::string{ member_type->getName().str() },
                    .type_name = std::pmr::string{ member_type_name },
                }
            );
        }

        return member_infos;
    }

    std::pmr::string format_debug_primitive_type_name(
        iris::Fundamental_type const type,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        std::string_view const c_equivalent_name = fundamental_type_c_equivalent_name(type);
        if (!c_equivalent_name.empty())
            return std::pmr::string{ c_equivalent_name, output_allocator };

        return add_h_namespace_prefix(fundamental_type_name(type), output_allocator);
    }

    std::pmr::string format_debug_primitive_type_name(
        iris::Integer_type const& type,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        return format_debug_integer_type_name_internal(type, output_allocator);
    }

    std::pmr::string format_debug_type_name(
        iris::Module const& core_module,
        iris::Type_reference const& type_reference,
        Debug_type_database const& debug_type_database,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        return format_debug_type_name_internal(
            core_module,
            type_reference,
            debug_type_database,
            output_allocator,
            temporaries_allocator
        );
    }

    std::pmr::string create_soa_array_debug_type_name(
        iris::Module const& core_module,
        iris::Soa_array_type const& type,
        Debug_type_database const& debug_type_database,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<Soa_debug_member_info> const member_infos = get_soa_debug_member_infos(
            core_module,
            type.value_type,
            debug_type_database
        );

        std::pmr::string element_type_name{ "Unknown", output_allocator };
        if (!type.value_type.empty())
        {
            element_type_name = format_debug_type_name_internal(
                core_module,
                iris::Type_reference{ .data = type.value_type[0].data },
                debug_type_database,
                output_allocator,
                temporaries_allocator
            );
        }

        std::pmr::string output{ std::format("Soa_array<{},{},{}", member_infos.size(), element_type_name, type.size), output_allocator };
        for (Soa_debug_member_info const& member_info : member_infos)
        {
            output += std::format(",{},{}", member_info.name, member_info.type_name);
        }

        output += ">";
        return output;
    }

    std::pmr::string create_soa_array_view_debug_type_name(
        iris::Module const& core_module,
        iris::Soa_array_view_type const& type,
        Debug_type_database const& debug_type_database,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    )
    {
        std::pmr::vector<Soa_debug_member_info> const member_infos = get_soa_debug_member_infos(
            core_module,
            type.value_type,
            debug_type_database
        );

       std::pmr::string element_type_name{ "Unknown", output_allocator };
        if (!type.value_type.empty())
        {
            element_type_name = format_debug_type_name_internal(
                core_module,
                iris::Type_reference{ .data = type.value_type[0].data },
                debug_type_database,
                output_allocator,
                temporaries_allocator
            );
        }

        std::pmr::string output{ std::format("Soa_array_view<{},{}", member_infos.size(), element_type_name), output_allocator };
        for (Soa_debug_member_info const& member_info : member_infos)
        {
            output += std::format(",{},{}", member_info.name, member_info.type_name);
        }

        output += ">";
        return output;
    }
}
