module;

#include <assert.h>

#include <llvm/ADT/FunctionExtras.h>

module iris.compiler.clang_code_generation;

import std;
import llvm;
import clang;

import iris.common;
import iris.compiler.analysis;
import iris.compiler.common;
import iris.compiler.debug_info;
import iris.compiler.instructions;
import iris.compiler.types;
import iris.core;
import iris.core.declarations;
import iris.core.execution_engine;
import iris.core.string_hash;
import iris.core.types;

namespace iris::compiler
{
    static constexpr std::string_view c_builtin_module_name = "iris.builtin";

    struct Clang_data
    {
        std::unique_ptr<clang::CompilerInstance> compiler_instance;
        std::unique_ptr<clang::TargetOptions> target_options;
    };

    struct Clang_module_declarations
    {
        std::pmr::unordered_map<std::pmr::string, clang::FunctionDecl*, iris::String_hash, iris::String_equal> function_declarations;
        std::pmr::unordered_map<std::pmr::string, clang::TypedefDecl*, iris::String_hash, iris::String_equal> alias_type_declarations;
        std::pmr::unordered_map<std::pmr::string, clang::EnumDecl*, iris::String_hash, iris::String_equal> enum_declarations;
        std::pmr::unordered_map<std::pmr::string, clang::RecordDecl*, iris::String_hash, iris::String_equal> struct_declarations;
        std::pmr::unordered_map<std::pmr::string, clang::RecordDecl*, iris::String_hash, iris::String_equal> union_declarations;
    };

    struct Clang_declaration_database
    {
        std::pmr::unordered_map<std::pmr::string, Clang_module_declarations, iris::String_hash, iris::String_equal> map;
    };

    struct Clang_context
    {
        clang::ASTContext& ast_context;
        std::unique_ptr<clang::CodeGenerator> code_generator;
    };

    struct Clang_module_data
    {
        clang::ASTContext& ast_context;
        std::unique_ptr<clang::CodeGenerator> code_generator;
        Clang_declaration_database declaration_database;
    };

    clang::QualType get_opaque_forward_declaration(
        clang::ASTContext& clang_ast_context
    )
    {
        return clang_ast_context.VoidPtrTy;
    }

    static clang::QualType create_clang_soa_array_type(
        clang::ASTContext& clang_ast_context
    )
    {
        clang::IdentifierInfo* const struct_name = &clang_ast_context.Idents.get("__hl_soa_array");

        clang::RecordDecl* const record_declaration = clang::RecordDecl::Create(
            clang_ast_context,
            clang::TagTypeKind::Struct,
            clang_ast_context.getTranslationUnitDecl(),
            clang::SourceLocation(),
            clang::SourceLocation(),
            struct_name
        );

        clang::QualType const byte_type = clang_ast_context.getIntTypeForBitwidth(8, 0);
        clang::QualType const byte_pointer_type = clang_ast_context.getPointerType(byte_type);

        clang::IdentifierInfo* const field_name = &clang_ast_context.Idents.get("data");
        clang::FieldDecl* const field = clang::FieldDecl::Create(
            clang_ast_context,
            record_declaration,
            clang::SourceLocation(),
            clang::SourceLocation(),
            field_name,
            byte_pointer_type,
            nullptr,
            nullptr,
            false,
            clang::ICIS_NoInit
        );

        record_declaration->addDecl(field);
        record_declaration->completeDefinition();

        return clang_ast_context.getCanonicalTypeDeclType(record_declaration);
    }

    static clang::QualType create_clang_soa_array_view_type(
        clang::ASTContext& clang_ast_context
    )
    {
        clang::IdentifierInfo* const struct_name = &clang_ast_context.Idents.get("__hl_soa_array_view");

        clang::RecordDecl* const record_declaration = clang::RecordDecl::Create(
            clang_ast_context,
            clang::TagTypeKind::Struct,
            clang_ast_context.getTranslationUnitDecl(),
            clang::SourceLocation(),
            clang::SourceLocation(),
            struct_name
        );

        clang::QualType const uint64_type = clang_ast_context.getIntTypeForBitwidth(64, 0);
        clang::QualType const byte_type = clang_ast_context.getIntTypeForBitwidth(8, 0);
        clang::QualType const byte_pointer_type = clang_ast_context.getPointerType(byte_type);

        auto const add_field = [&](char const* const field_name, clang::QualType const field_type) {
            clang::IdentifierInfo* const identifier = &clang_ast_context.Idents.get(field_name);
            clang::FieldDecl* const field = clang::FieldDecl::Create(
                clang_ast_context,
                record_declaration,
                clang::SourceLocation(),
                clang::SourceLocation(),
                identifier,
                field_type,
                nullptr,
                nullptr,
                false,
                clang::ICIS_NoInit
            );

            record_declaration->addDecl(field);
        };

        add_field("start_index", uint64_type);
        add_field("end_index", uint64_type);
        add_field("length", uint64_type);
        add_field("data", byte_pointer_type);

        record_declaration->completeDefinition();

        return clang_ast_context.getCanonicalTypeDeclType(record_declaration);
    }

    void add_clang_alias_type_declaration(
        std::pmr::unordered_map<std::pmr::string, clang::TypedefDecl*, iris::String_hash, iris::String_equal>& clang_alias_type_declarations,
        clang::ASTContext& clang_ast_context,
        iris::Alias_type_declaration const& alias_type_declaration,
        Declaration_database const& declaration_database,
        Clang_declaration_database const& clang_declaration_database
    )
    {
        if (clang_alias_type_declarations.contains(alias_type_declaration.name))
            return;

        // TODO should we use unique_name?
        clang::IdentifierInfo* const alias_name = &clang_ast_context.Idents.get(alias_type_declaration.name.data());

        if (!alias_type_declaration.type.empty())
        {
            auto const add_underlying_alias = [&](iris::Type_reference const& type_reference) -> bool {

                if (std::holds_alternative<iris::Custom_type_reference>(type_reference.data))
                {
                    iris::Custom_type_reference const custom_type_reference = std::get<iris::Custom_type_reference>(type_reference.data);
                    std::optional<iris::Declaration> const declaration = iris::find_declaration(
                        declaration_database,
                        custom_type_reference.module_reference.name,
                        custom_type_reference.name
                    );

                    if (declaration.has_value())
                    {
                        if (std::holds_alternative<iris::Alias_type_declaration const*>(declaration->data))
                        {
                            iris::Alias_type_declaration const* underlying_alias_type_declaration = std::get<Alias_type_declaration const*>(declaration->data);

                            if (underlying_alias_type_declaration != nullptr)
                            {
                                add_clang_alias_type_declaration(
                                    clang_alias_type_declarations,
                                    clang_ast_context,
                                    *underlying_alias_type_declaration,
                                    declaration_database,
                                    clang_declaration_database
                                );
                            }
                        }
                    }
                }

                return false;
            };

            iris::visit_type_references_recursively(
                alias_type_declaration.type[0],
                add_underlying_alias
            );
        }

        std::optional<clang::QualType> const underlying_type_optional = create_type(
            clang_ast_context,
            alias_type_declaration.type,
            true,
            declaration_database,
            clang_declaration_database
        );

        clang::QualType const underlying_type =
            underlying_type_optional.has_value() ?
            underlying_type_optional.value() :
            get_opaque_forward_declaration(clang_ast_context);

        clang::TypedefDecl* const clang_alias_type_declaration = clang::TypedefDecl::Create(
            clang_ast_context,
            clang_ast_context.getTranslationUnitDecl(),
            clang::SourceLocation(),
            clang::SourceLocation(),
            alias_name,
            clang_ast_context.CreateTypeSourceInfo(underlying_type)
        );

        clang_alias_type_declarations.emplace(alias_type_declaration.name, clang_alias_type_declaration);
    }

    void add_clang_enum_declaration(
        std::pmr::unordered_map<std::pmr::string, clang::EnumDecl*, iris::String_hash, iris::String_equal>& clang_enum_declarations,
        clang::ASTContext& clang_ast_context,
        iris::Enum_declaration const& enum_declaration
    )
    {
        // TODO should we use unique_name?
        clang::IdentifierInfo* const enum_name = &clang_ast_context.Idents.get(enum_declaration.name.data());

        clang::EnumDecl* const clang_enum_declaration = clang::EnumDecl::Create(
            clang_ast_context,
            clang_ast_context.getTranslationUnitDecl(),
            clang::SourceLocation(),
            clang::SourceLocation(),
            enum_name,
            nullptr,
            false,
            false,
            false
        );

        for (std::size_t value_index = 0; value_index < enum_declaration.values.size(); ++value_index)
        {
            iris::Enum_value const& enum_value = enum_declaration.values[value_index];

            clang::IdentifierInfo* value_identifier = &clang_ast_context.Idents.get(enum_value.name.data());
            clang::EnumConstantDecl* clang_enum_value = clang::EnumConstantDecl::Create(
                clang_ast_context,
                clang_enum_declaration,
                clang::SourceLocation(),
                value_identifier,
                clang_ast_context.IntTy, // TODO
                nullptr,
                llvm::APSInt::get(0)
            );

            clang_enum_declaration->addDecl(clang_enum_value);
        }

        clang_enum_declaration->completeDefinition(
            clang_ast_context.IntTy,
            clang_ast_context.IntTy,
            0,
            32
        );

        clang_enum_declarations.emplace(enum_declaration.name, clang_enum_declaration);
    }

    clang::RecordDecl* create_clang_struct_declaration(
        clang::ASTContext& clang_ast_context,
        std::string_view const module_name,
        iris::Struct_declaration const& struct_declaration
    )
    {
        std::string const mangled_name = mangle_name(module_name, struct_declaration.name, struct_declaration.unique_name);
        clang::IdentifierInfo* const struct_name = &clang_ast_context.Idents.get(mangled_name);

        clang::RecordDecl* const record_declaration = clang::RecordDecl::Create(
            clang_ast_context,
            clang::TagTypeKind::Struct,
            clang_ast_context.getTranslationUnitDecl(),
            clang::SourceLocation(),
            clang::SourceLocation(),
            struct_name
        );

        return record_declaration;
    }

    void set_clang_struct_definition(
        clang::ASTContext& clang_ast_context,
        clang::RecordDecl& record_declaration,
        iris::Struct_declaration const& struct_declaration,
        Declaration_database const& declaration_database,
        Clang_declaration_database const& clang_declaration_database
    )
    {
        if (record_declaration.isCompleteDefinition())
            return;

        for (std::size_t member_index = 0; member_index < struct_declaration.member_types.size(); ++member_index)
        {
            std::string_view const member_name = struct_declaration.member_names[member_index];
            iris::Type_reference const& member_type = struct_declaration.member_types[member_index];
            std::optional<std::uint32_t> const member_bit_field = struct_declaration.member_bit_fields[member_index];

            std::optional<clang::QualType> const member_clang_type_optional = create_type(
                clang_ast_context,
                member_type,
                true,
                declaration_database,
                clang_declaration_database
            );
            if (!member_clang_type_optional.has_value())
                iris::common::print_message_and_exit(std::format("Could not create clang type for '{}.{}'.", struct_declaration.name, member_name));

            clang::QualType const& member_clang_type = member_clang_type_optional.value();

            clang::IdentifierInfo* field_name = &clang_ast_context.Idents.get(member_name);
            clang::FieldDecl* field = clang::FieldDecl::Create(
                clang_ast_context,
                &record_declaration,
                clang::SourceLocation(),
                clang::SourceLocation(),
                field_name,
                member_clang_type,
                nullptr,
                nullptr,
                false,
                clang::ICIS_NoInit
            );

            if (member_bit_field.has_value())
            {
                std::uint32_t const bits = member_bit_field.value();

                llvm::APInt const width{64, bits};
                clang::QualType const bit_width_type = clang_ast_context.getIntTypeForBitwidth(64, 0);

                clang::IntegerLiteral* const bit_width_expression = clang::IntegerLiteral::Create(
                    clang_ast_context,
                    width,
                    bit_width_type,
                    {}
                );

                clang::ConstantExpr* const constant_expression = clang::ConstantExpr::Create(
                    clang_ast_context,
                    bit_width_expression,
                    clang::APValue{llvm::APSInt{bit_width_expression->getValue(), true}}
                );

                field->setBitWidth(constant_expression);
                assert(field->hasConstantIntegerBitWidth());
            }

            record_declaration.addDecl(field);
        }

        record_declaration.completeDefinition();
    }

    void add_clang_union_declaration(
        std::pmr::unordered_map<std::pmr::string, clang::RecordDecl*, iris::String_hash, iris::String_equal>& clang_union_declarations,
        clang::ASTContext& clang_ast_context,
        iris::Module const& core_module,
        iris::Union_declaration const& union_declaration
    )
    {
        std::string const mangled_name = mangle_union_name(core_module, union_declaration.name);
        clang::IdentifierInfo* const union_name = &clang_ast_context.Idents.get(mangled_name);

        clang::RecordDecl* const record_declaration = clang::RecordDecl::Create(
            clang_ast_context,
            clang::TagTypeKind::Union,
            clang_ast_context.getTranslationUnitDecl(),
            clang::SourceLocation(),
            clang::SourceLocation(),
            union_name
        );

        clang_union_declarations.emplace(union_declaration.name, record_declaration);
    }

    void add_clang_union_definition(
        std::pmr::unordered_map<std::pmr::string, clang::RecordDecl*, iris::String_hash, iris::String_equal>& clang_union_declarations,
        clang::ASTContext& clang_ast_context,
        iris::Union_declaration const& union_declaration,
        Declaration_database const& declaration_database,
        Clang_declaration_database const& clang_declaration_database
    )
    {
        clang::RecordDecl* record_declaration = clang_union_declarations.at(union_declaration.name);
        if (record_declaration->isCompleteDefinition())
            return;

        for (std::size_t member_index = 0; member_index < union_declaration.member_types.size(); ++member_index)
        {
            std::string_view const member_name = union_declaration.member_names[member_index];
            iris::Type_reference const& member_type = union_declaration.member_types[member_index];

            clang::QualType const member_clang_type = *create_type(
                clang_ast_context,
                member_type,
                true,
                declaration_database,
                clang_declaration_database
            );

            clang::IdentifierInfo* field_name = &clang_ast_context.Idents.get(member_name);
            clang::FieldDecl* field = clang::FieldDecl::Create(
                clang_ast_context,
                record_declaration,
                clang::SourceLocation(),
                clang::SourceLocation(),
                field_name,
                member_clang_type,
                nullptr,
                nullptr,
                false,
                clang::ICIS_NoInit
            );

            record_declaration->addDecl(field);
        }

        record_declaration->completeDefinition();
    }

    clang::QualType create_clang_function_proto_type(
        clang::ASTContext& clang_ast_context,
        iris::Function_type const& function_type,
        Declaration_database const& declaration_database,
        Clang_declaration_database const& clang_declaration_database
    )
    {
        clang::QualType const return_type = *create_type(
            clang_ast_context,
            function_type.output_parameter_types,
            false,
            declaration_database,
            clang_declaration_database
        );

        llvm::SmallVector<clang::QualType> input_parameter_types;

        for (std::size_t index = 0; index < function_type.input_parameter_types.size(); ++index)
        {
            iris::Type_reference const& input_parameter_type_reference = function_type.input_parameter_types[index];

            clang::QualType const input_parameter_type = *create_type(
                clang_ast_context,
                input_parameter_type_reference,
                false,
                declaration_database,
                clang_declaration_database
            );

            input_parameter_types.push_back(input_parameter_type);
        }

        clang::FunctionProtoType::ExtProtoInfo extra_info = {};
        extra_info.Variadic = function_type.is_variadic;

        clang::QualType const function_proto_type = clang_ast_context.getFunctionType(
            return_type,
            input_parameter_types,
            extra_info
        );

        return function_proto_type;
    }

    clang::FunctionDecl* create_clang_function_declaration(
        clang::ASTContext& clang_ast_context,
        iris::Function_declaration const& function_declaration,
        Declaration_database const& declaration_database,
        Clang_declaration_database const& clang_declaration_database
    )
    {
        clang::SourceLocation function_declaration_start_location;
        clang::SourceLocation function_declaration_end_location;

        clang::DeclarationName const declaration_name{ &clang_ast_context.Idents.get(function_declaration.name.data()) };

        clang::QualType const function_proto_type = create_clang_function_proto_type(clang_ast_context, function_declaration.type, declaration_database, clang_declaration_database);

        clang::StorageClass const storage_class = clang::StorageClass::SC_None;

        bool const is_inline_specified = false;
        bool const is_constexpr = false;

        clang::TranslationUnitDecl* const translation_unit_declaration = clang_ast_context.getTranslationUnitDecl();

        clang::FunctionDecl* clang_function_declaration = clang::FunctionDecl::Create(
            clang_ast_context,
            translation_unit_declaration,
            function_declaration_start_location,
            function_declaration_end_location,
            declaration_name,
            function_proto_type,
            nullptr,
            storage_class,
            is_inline_specified,
            is_constexpr
        );

        {
            std::pmr::vector<clang::ParmVarDecl*> clang_parameters;
            clang_parameters.reserve(function_declaration.type.input_parameter_types.size());

            for (std::size_t index = 0; index < function_declaration.type.input_parameter_types.size(); ++index)
            {
                std::string_view const input_parameter_name = function_declaration.input_parameter_names[index];
                iris::Type_reference const& input_parameter_type_reference = function_declaration.type.input_parameter_types[index];

                clang::IdentifierInfo* parameter_name = &clang_ast_context.Idents.get(input_parameter_name.data());
                clang::QualType parameter_type = *create_type(clang_ast_context, input_parameter_type_reference, false, declaration_database, clang_declaration_database);
                clang::Expr* parameter_default_argument = nullptr;

                clang::SourceLocation parameter_start_location;
                clang::SourceLocation parameter_end_location;

                clang::ParmVarDecl* parameter = clang::ParmVarDecl::Create(
                    clang_ast_context,
                    clang_function_declaration,
                    parameter_start_location,
                    parameter_end_location,
                    parameter_name,
                    parameter_type,
                    nullptr,
                    clang::StorageClass::SC_None,
                    parameter_default_argument
                );

                clang_parameters.push_back(parameter);
            }

            if (!clang_parameters.empty())
            {
                clang_function_declaration->setParams(clang_parameters);
            }
        }

        return clang_function_declaration;
    }

    void add_clang_struct_declaration(
        Clang_declaration_database& clang_declaration_database,
        clang::ASTContext& clang_ast_context,
        iris::Module const& core_module,
        iris::Struct_declaration const& struct_declaration,
        Declaration_database const& declaration_database
    )
    {
        auto iterator = clang_declaration_database.map.emplace(core_module.name, Clang_module_declarations{}).first;
        if (iterator->second.struct_declarations.contains(struct_declaration.name))
            return;

        clang::RecordDecl* const record_declaration = create_clang_struct_declaration(clang_ast_context, core_module.name, struct_declaration);
        iterator->second.struct_declarations.emplace(struct_declaration.name, record_declaration);
        set_clang_struct_definition(clang_ast_context, *record_declaration, struct_declaration, declaration_database, clang_declaration_database);
    }

    void add_clang_union_declaration(
        Clang_declaration_database& clang_declaration_database,
        clang::ASTContext& clang_ast_context,
        iris::Module const& core_module,
        iris::Union_declaration const& union_declaration,
        Declaration_database const& declaration_database
    )
    {
        auto iterator = clang_declaration_database.map.emplace(core_module.name, Clang_module_declarations{}).first;
        if (iterator->second.union_declarations.contains(union_declaration.name))
            return;

        add_clang_union_declaration(iterator->second.union_declarations, clang_ast_context, core_module, union_declaration);
        add_clang_union_definition(iterator->second.union_declarations, clang_ast_context, union_declaration, declaration_database, clang_declaration_database);
    }

    void add_clang_function_declaration(
        Clang_declaration_database& clang_declaration_database,
        clang::ASTContext& clang_ast_context,
        std::string_view const module_name,
        iris::Function_declaration const& function_declaration,
        Declaration_database const& declaration_database
    )
    {
        auto iterator = clang_declaration_database.map.emplace(module_name.data(), Clang_module_declarations{}).first;
        if (iterator->second.function_declarations.contains(function_declaration.name))
            return;

        clang::FunctionDecl* const clang_declaration = create_clang_function_declaration(clang_ast_context, function_declaration, declaration_database, clang_declaration_database);
        iterator->second.function_declarations.emplace(function_declaration.name, clang_declaration);
    }

    void add_clang_function_declarations(
        Clang_declaration_database& clang_declaration_database,
        clang::ASTContext& clang_ast_context,
        iris::Module const& core_module,
        Declaration_database const& declaration_database
    )
    {
        auto iterator = clang_declaration_database.map.emplace(core_module.name, Clang_module_declarations{}).first;

        for (iris::Function_declaration const& function_declaration : core_module.export_declarations.function_declarations)
        {
            if (iterator->second.function_declarations.contains(function_declaration.name))
                continue;

            clang::FunctionDecl* const clang_declaration = create_clang_function_declaration(clang_ast_context, function_declaration, declaration_database, clang_declaration_database);
            iterator->second.function_declarations.emplace(function_declaration.name, clang_declaration);
        }

        for (iris::Function_declaration const& function_declaration : core_module.internal_declarations.function_declarations)
        {
            if (iterator->second.function_declarations.contains(function_declaration.name))
                continue;

            clang::FunctionDecl* const clang_declaration = create_clang_function_declaration(clang_ast_context, function_declaration, declaration_database, clang_declaration_database);
            iterator->second.function_declarations.emplace(function_declaration.name, clang_declaration);
        }
    }

    void add_clang_declarations(
        Clang_declaration_database& clang_declaration_database,
        clang::ASTContext& clang_ast_context,
        iris::Module const& core_module,
        Declaration_database const& declaration_database
    )
    {
        auto iterator = clang_declaration_database.map.emplace(core_module.name, Clang_module_declarations{}).first;

        for (iris::Enum_declaration const& enum_declaration : core_module.export_declarations.enum_declarations)
        {
            if (iterator->second.enum_declarations.contains(enum_declaration.name))
                continue;
            
            add_clang_enum_declaration(iterator->second.enum_declarations, clang_ast_context, enum_declaration);
        }

        for (iris::Enum_declaration const& enum_declaration : core_module.internal_declarations.enum_declarations)
        {
            if (iterator->second.enum_declarations.contains(enum_declaration.name))
                continue;

            add_clang_enum_declaration(iterator->second.enum_declarations, clang_ast_context, enum_declaration);
        }

        for (iris::Struct_declaration const& struct_declaration : core_module.export_declarations.struct_declarations)
        {
            if (iterator->second.struct_declarations.contains(struct_declaration.name))
                continue;

            clang::RecordDecl* const record_declaration = create_clang_struct_declaration(clang_ast_context, core_module.name, struct_declaration);
            iterator->second.struct_declarations.emplace(struct_declaration.name, record_declaration);
        }

        for (iris::Struct_declaration const& struct_declaration : core_module.internal_declarations.struct_declarations)
        {
            if (iterator->second.struct_declarations.contains(struct_declaration.name))
                continue;

            clang::RecordDecl* const record_declaration = create_clang_struct_declaration(clang_ast_context, core_module.name, struct_declaration);
            iterator->second.struct_declarations.emplace(struct_declaration.name, record_declaration);
        }

        for (iris::Struct_declaration const& struct_declaration : core_module.instanced_declarations.struct_declarations)
        {
            std::optional<iris::Custom_type_reference> const instance_custom_type_reference = unmangle_type_instance_name(struct_declaration.name);
            assert(instance_custom_type_reference.has_value());
            
            clang::RecordDecl* const record_declaration = create_clang_struct_declaration(clang_ast_context, instance_custom_type_reference->module_reference.name, struct_declaration);

            auto instance_iterator = clang_declaration_database.map.emplace(instance_custom_type_reference->module_reference.name, Clang_module_declarations{}).first;
            instance_iterator->second.struct_declarations.emplace(struct_declaration.name, record_declaration);
        }

        for (iris::Union_declaration const& union_declaration : core_module.export_declarations.union_declarations)
            add_clang_union_declaration(iterator->second.union_declarations, clang_ast_context, core_module, union_declaration);

        for (iris::Union_declaration const& union_declaration : core_module.internal_declarations.union_declarations)
            add_clang_union_declaration(iterator->second.union_declarations, clang_ast_context, core_module, union_declaration);

        for (iris::Union_declaration const& union_declaration : core_module.instanced_declarations.union_declarations)
        {
            std::optional<iris::Custom_type_reference> const instance_custom_type_reference = unmangle_type_instance_name(union_declaration.name);
            assert(instance_custom_type_reference.has_value());
            
            auto instance_iterator = clang_declaration_database.map.emplace(instance_custom_type_reference->module_reference.name, Clang_module_declarations{}).first;
            add_clang_union_declaration(instance_iterator->second.union_declarations, clang_ast_context, core_module, union_declaration);
        }

        for (iris::Alias_type_declaration const& alias_type_declaration : core_module.export_declarations.alias_type_declarations)
            add_clang_alias_type_declaration(iterator->second.alias_type_declarations, clang_ast_context, alias_type_declaration, declaration_database, clang_declaration_database);

        for (iris::Alias_type_declaration const& alias_type_declaration : core_module.internal_declarations.alias_type_declarations)
            add_clang_alias_type_declaration(iterator->second.alias_type_declarations, clang_ast_context, alias_type_declaration, declaration_database, clang_declaration_database);

        for (iris::Struct_declaration const& struct_declaration : core_module.export_declarations.struct_declarations)
        {
            clang::RecordDecl* const record_declaration = iterator->second.struct_declarations.at(struct_declaration.name);
            set_clang_struct_definition(clang_ast_context, *record_declaration, struct_declaration, declaration_database, clang_declaration_database);
        }

        for (iris::Struct_declaration const& struct_declaration : core_module.internal_declarations.struct_declarations)
        {
            clang::RecordDecl* const record_declaration = iterator->second.struct_declarations.at(struct_declaration.name);
            set_clang_struct_definition(clang_ast_context, *record_declaration, struct_declaration, declaration_database, clang_declaration_database);
        }

        for (iris::Struct_declaration const& struct_declaration : core_module.instanced_declarations.struct_declarations)
        {
            std::optional<iris::Custom_type_reference> const instance_custom_type_reference = unmangle_type_instance_name(struct_declaration.name);
            assert(instance_custom_type_reference.has_value());

            auto instance_iterator = clang_declaration_database.map.emplace(instance_custom_type_reference->module_reference.name, Clang_module_declarations{}).first;
            clang::RecordDecl* const record_declaration = instance_iterator->second.struct_declarations.at(struct_declaration.name);
            set_clang_struct_definition(clang_ast_context, *record_declaration, struct_declaration, declaration_database, clang_declaration_database);
        }

        for (iris::Union_declaration const& union_declaration : core_module.export_declarations.union_declarations)
            add_clang_union_definition(iterator->second.union_declarations, clang_ast_context, union_declaration, declaration_database, clang_declaration_database);

        for (iris::Union_declaration const& union_declaration : core_module.internal_declarations.union_declarations)
            add_clang_union_definition(iterator->second.union_declarations, clang_ast_context, union_declaration, declaration_database, clang_declaration_database);

        for (iris::Union_declaration const& union_declaration : core_module.instanced_declarations.union_declarations)
        {
            std::optional<iris::Custom_type_reference> const instance_custom_type_reference = unmangle_type_instance_name(union_declaration.name);
            assert(instance_custom_type_reference.has_value());

            auto instance_iterator = clang_declaration_database.map.emplace(instance_custom_type_reference->module_reference.name, Clang_module_declarations{}).first;
            add_clang_union_definition(instance_iterator->second.union_declarations, clang_ast_context, union_declaration, declaration_database, clang_declaration_database);
        }

        for (iris::Function_declaration const& function_declaration : core_module.export_declarations.function_declarations)
        {
            if (iterator->second.function_declarations.contains(function_declaration.name))
                continue;

            clang::FunctionDecl* const clang_declaration = create_clang_function_declaration(clang_ast_context, function_declaration, declaration_database, clang_declaration_database);
            iterator->second.function_declarations.emplace(function_declaration.name, clang_declaration);
        }

        for (iris::Function_declaration const& function_declaration : core_module.internal_declarations.function_declarations)
        {
            if (iterator->second.function_declarations.contains(function_declaration.name))
                continue;

            clang::FunctionDecl* const clang_declaration = create_clang_function_declaration(clang_ast_context, function_declaration, declaration_database, clang_declaration_database);
            iterator->second.function_declarations.emplace(function_declaration.name, clang_declaration);
        }

        for (iris::Function_declaration const& function_declaration : core_module.instanced_declarations.function_declarations)
        {
            clang::FunctionDecl* const clang_declaration = create_clang_function_declaration(clang_ast_context, function_declaration, declaration_database, clang_declaration_database);

            std::optional<iris::Custom_type_reference> const instance_custom_type_reference = unmangle_type_instance_name(function_declaration.name);
            if (instance_custom_type_reference.has_value())
            {
                auto instance_iterator = clang_declaration_database.map.emplace(instance_custom_type_reference->module_reference.name, Clang_module_declarations{}).first;
                instance_iterator->second.function_declarations.emplace(function_declaration.name, clang_declaration);
            }
            else
            {
                iterator->second.function_declarations.emplace(function_declaration.name, clang_declaration);
            }
        }
    }

    void destroy_clang_context(Clang_context* data)
    {
        delete data;
    }

    std::unique_ptr<Clang_context, void(*)(Clang_context*)> create_clang_context(
        llvm::LLVMContext& llvm_context,
        Clang_data const& clang_data,
        std::string_view const module_name
    )
    {
        clang::ASTContext& clang_ast_context = clang_data.compiler_instance->getASTContext();

        std::unique_ptr<clang::CodeGenerator> code_generator
        {
            clang::CreateLLVMCodeGen(
                clang_data.compiler_instance->getDiagnostics(),
                module_name.data(),
                &clang_data.compiler_instance->getVirtualFileSystem(),
                clang_data.compiler_instance->getHeaderSearchOpts(),
                clang_data.compiler_instance->getPreprocessorOpts(),
                clang_data.compiler_instance->getCodeGenOpts(),
                llvm_context,
                nullptr
            )
        };
        code_generator->Initialize(clang_ast_context);

        std::unique_ptr<Clang_context, void(*)(Clang_context*)> output
        {
            new Clang_context
            {
                .ast_context = clang_ast_context,
                .code_generator = std::move(code_generator),
            },
            destroy_clang_context
        };

        return output;
    }

    Clang_module_data_pointer create_clang_module_data(
        Clang_context_pointer&& clang_context,
        std::span<iris::Module const* const> const sorted_modules,
        Declaration_database const& declaration_database
    )
    {
        clang::ASTContext& clang_ast_context = clang_context->ast_context;

        Clang_declaration_database clang_declaration_database;

        for (Module const* sorted_module : sorted_modules)
            add_clang_declarations(clang_declaration_database, clang_ast_context, *sorted_module, declaration_database);

        Clang_module_data_pointer output(
            new Clang_module_data
            {
                .ast_context = clang_ast_context,
                .code_generator = std::move(clang_context->code_generator),
                .declaration_database = std::move(clang_declaration_database),
            },
            destroy_clang_module_data
        );
        return output;
    }

    Clang_module_data_pointer create_clang_module_data(
        llvm::LLVMContext& llvm_context,
        Clang_data const& clang_data,
        std::string_view const module_name,
        std::span<iris::Module const* const> const sorted_modules,
        Declaration_database const& declaration_database
    )
    {
        Clang_context_pointer clang_context = create_clang_context(llvm_context, clang_data, module_name);
        return create_clang_module_data(std::move(clang_context), sorted_modules, declaration_database);
    }

    clang::CodeGen::CGFunctionInfo const& create_clang_function_info(
        Clang_module_data const& clang_module_data,
        iris::Function_type const& function_type,
        Declaration_database const& declaration_database
    )
    {
        clang::QualType clang_function_type = create_clang_function_proto_type(
            clang_module_data.ast_context,
            function_type,
            declaration_database,
            clang_module_data.declaration_database
        );

        clang::CanQualType clang_canonical_function_type = clang_module_data.ast_context.getCanonicalType(clang_function_type);
        clang::CanQual<clang::FunctionProtoType> clang_function_proto_type = clang_canonical_function_type->getAs<clang::FunctionProtoType>();

        return clang::CodeGen::arrangeFreeFunctionType(clang_module_data.code_generator->CGM(), clang_function_proto_type);
    }

    static llvm::FunctionType* create_llvm_function_type_auxiliary(
        Clang_module_data const& clang_module_data,
        std::string_view const module_name,
        std::string_view const function_name
    )
    {

    }

    llvm::FunctionType* create_llvm_function_type(
        Clang_module_data const& clang_module_data,
        std::string_view const module_name,
        std::string_view const function_name
    )
    {
        std::optional<iris::Custom_type_reference> const instance_type_reference = unmangle_instance_call_name(function_name);
        std::string_view const actual_module_name = instance_type_reference.has_value() ? std::string_view{instance_type_reference->module_reference.name} : module_name;

        auto const module_declarations_location = clang_module_data.declaration_database.map.find(actual_module_name);
        if (module_declarations_location == clang_module_data.declaration_database.map.end())
            throw std::runtime_error{std::format("Module '{}' not found in Clang module data!", actual_module_name)};
        Clang_module_declarations const& module_declarations = module_declarations_location->second;

        clang::FunctionDecl* const clang_function_declaration = module_declarations.function_declarations.at(function_name.data());
        return clang::CodeGen::convertFreeFunctionType(clang_module_data.code_generator->CGM(), clang_function_declaration);
    }

    llvm::FunctionType* convert_to_llvm_function_type(
        Clang_module_data const& clang_module_data,
        Declaration_database const& declaration_database,
        iris::Function_type const& function_type
    )
    {
        clang::QualType const clang_function_type = create_clang_function_proto_type(
            clang_module_data.ast_context,
            function_type,
            declaration_database,
            clang_module_data.declaration_database
        );

        llvm::Type* const llvm_type = clang::CodeGen::convertTypeForMemory(clang_module_data.code_generator->CGM(), clang_function_type);

        if (!llvm::FunctionType::classof(llvm_type))
            throw std::runtime_error{"Could not convert function type to platform ABI function type!"};

        return static_cast<llvm::FunctionType*>(llvm_type);
    }

    std::pmr::vector<llvm::Attribute> create_llvm_function_return_type_argument_attributes(
        llvm::LLVMContext& llvm_context,
        llvm::Type* return_llvm_type,
        clang::CodeGen::ABIArgInfo const& return_info
    )
    {
        std::pmr::vector<llvm::Attribute> attributes
        {
            llvm::Attribute::get(llvm_context, llvm::Attribute::DeadOnUnwind),
            llvm::Attribute::get(llvm_context, llvm::Attribute::NoAlias),
            llvm::Attribute::get(llvm_context, llvm::Attribute::Writable),
        };

        if (return_info.isIndirect())
        {
            if (return_info.getIndirectByVal())
            {
                attributes.push_back(
                    llvm::Attribute::getWithStructRetType(llvm_context, return_llvm_type)
                );
            }

            attributes.push_back(
                llvm::Attribute::getWithAlignment(llvm_context, llvm::Align(return_info.getIndirectAlign().getQuantity()))
            );
        }

        return attributes;
    }

    void set_llvm_function_argument_names(
        llvm::LLVMContext& llvm_context,
        llvm::DataLayout const& llvm_data_layout,
        Clang_module_data const& clang_module_data,
        iris::Function_declaration const& function_declaration,
        llvm::Function& llvm_function,
        Declaration_database const& declaration_database,
        Type_database const& type_database
    )
    {
        clang::CodeGen::CGFunctionInfo const& function_info = create_clang_function_info(clang_module_data, function_declaration.type, declaration_database);

        unsigned new_argument_index = 0;

        if (function_declaration.type.output_parameter_types.size() > 0)
        {
            clang::CodeGen::ABIArgInfo const& return_info = function_info.getReturnInfo();

            if (return_info.isIndirect())
            {
                // Pass return type as argument pointer

                llvm::Argument* const argument = llvm_function.getArg(new_argument_index);
                
                std::string_view const name = !function_declaration.output_parameter_names.empty() ? function_declaration.output_parameter_names[0] : std::string_view{};
                std::string const argument_name = std::format("return.{}", name);
                argument->setName(argument_name.c_str());

                llvm::Type* const return_llvm_type = type_reference_to_llvm_type(llvm_context, llvm_data_layout, function_declaration.type.output_parameter_types[0], type_database);

                std::pmr::vector<llvm::Attribute> const attributes = create_llvm_function_return_type_argument_attributes(llvm_context, return_llvm_type, return_info);
                for (llvm::Attribute const& attribute : attributes)
                    argument->addAttr(attribute);

                new_argument_index += 1;
            }
        }

        llvm::ArrayRef<clang::CodeGen::CGFunctionInfoArgInfo> const argument_infos = function_info.arguments();

        for (unsigned argument_info_index = 0; argument_info_index < argument_infos.size(); ++argument_info_index)
        {
            clang::CodeGen::CGFunctionInfoArgInfo const& argument_info = argument_infos[argument_info_index];

            std::pmr::string const& name = function_declaration.input_parameter_names[argument_info_index];

            clang::CodeGen::ABIArgInfo::Kind const kind = argument_info.info.getKind();

            switch (kind)
            {
                case clang::CodeGen::ABIArgInfo::Direct:
                case clang::CodeGen::ABIArgInfo::Extend: {
                    llvm::Type* const new_type = argument_info.info.getCoerceToType();

                    if (new_type->isStructTy())
                    {
                        llvm::StructType* const new_struct_type = static_cast<llvm::StructType*>(new_type);

                        llvm::ArrayRef<llvm::Type*> const new_elements = new_struct_type->elements();

                        for (unsigned new_element_index = 0; new_element_index < new_elements.size(); ++new_element_index)
                        {
                            std::string const argument_name = std::format("arguments[{}].{}_{}", argument_info_index, name, new_element_index);
            
                            llvm::Argument* const argument = llvm_function.getArg(new_argument_index);
                            argument->setName(argument_name.c_str());

                            new_argument_index += 1;
                        }
                    }
                    else
                    {
                        std::string const argument_name = std::format("arguments[{}].{}", argument_info_index, name);
        
                        llvm::Argument* const argument = llvm_function.getArg(new_argument_index);
                        argument->setName(argument_name.c_str());
                        argument->addAttr(llvm::Attribute::NoUndef);

                        if (argument_info.info.isExtend())
                        {
                            if (argument_info.info.isSignExt())
                            {
                                argument->addAttr(llvm::Attribute::SExt);
                            }
                            else
                            {
                                argument->addAttr(llvm::Attribute::ZExt);
                            }
                        }

                        new_argument_index += 1;
                    }

                    break;
                }
                case clang::CodeGen::ABIArgInfo::Indirect: {
                    std::string const argument_name = std::format("arguments[{}].{}", argument_info_index, name);
    
                    llvm::Argument* const argument = llvm_function.getArg(new_argument_index);
                    argument->setName(argument_name.c_str());
                    argument->addAttr(llvm::Attribute::NoUndef);

                    if (argument_info.info.getIndirectByVal())
                    {
                        iris::Type_reference const& input_parameter_type = function_declaration.type.input_parameter_types[argument_info_index];
                        llvm::Type* const original_llvm_type = type_reference_to_llvm_type(llvm_context, llvm_data_layout, input_parameter_type, type_database);
                        argument->addAttr(llvm::Attribute::getWithByValType(llvm_context, original_llvm_type));
                        argument->addAttr(llvm::Attribute::getWithAlignment(llvm_context, llvm::Align(argument_info.info.getIndirectAlign().getQuantity())));
                    }

                    new_argument_index += 1;
                    break;
                }
                case clang::CodeGen::ABIArgInfo::IndirectAliased: {
                    throw std::runtime_error{ "Clang_code_generation.set_llvm_function_argument_names(): IndirectAliased not implemented!" };
                }
                case clang::CodeGen::ABIArgInfo::Ignore: {
                    break;
                }
                case clang::CodeGen::ABIArgInfo::Expand: {
                    throw std::runtime_error{ "Clang_code_generation.set_llvm_function_argument_names(): Expand not implemented!" };
                }
                case clang::CodeGen::ABIArgInfo::CoerceAndExpand: {
                    throw std::runtime_error{ "Clang_code_generation.set_llvm_function_argument_names(): CoerceAndExpand not implemented!" };
                }
                case clang::CodeGen::ABIArgInfo::InAlloca: {
                    throw std::runtime_error{ "Clang_code_generation.set_llvm_function_argument_names(): InAlloca not implemented!" };
                }
            }
        }
    }

    Transformed_arguments transform_arguments(
        std::pmr::vector<bool> const& is_expression_address_of,
        std::span<std::optional<Type_reference> const> const original_argument_types,
        llvm::LLVMContext& llvm_context,
        llvm::IRBuilder<>& llvm_builder,
        llvm::DataLayout const& llvm_data_layout,
        llvm::Module& llvm_module,
        llvm::Function& llvm_parent_function,
        iris::Module const& core_module,
        iris::Function_type const& function_type,
        clang::CodeGen::CGFunctionInfo const& function_info,
        std::span<llvm::Value* const> const original_arguments,
        Type_database const& type_database
    )
    {
        Transformed_arguments transformed_arguments;

        if (function_type.output_parameter_types.size() > 0)
        {
            clang::CodeGen::ABIArgInfo const& return_info = function_info.getReturnInfo();

            if (return_info.isIndirect())
            {
                // Pass return type as argument pointer

                iris::Type_reference const& original_return_type = function_type.output_parameter_types[0];
                llvm::Type* const original_return_llvm_type = type_reference_to_llvm_type(llvm_context, llvm_data_layout, original_return_type, type_database);
                
                llvm::AllocaInst* const alloca_instruction = create_alloca_instruction(llvm_builder, llvm_data_layout, llvm_parent_function, original_return_llvm_type);

                std::pmr::vector<llvm::Attribute> const attributes = create_llvm_function_return_type_argument_attributes(llvm_context, original_return_llvm_type, return_info);

                transformed_arguments.values.push_back(alloca_instruction);
                transformed_arguments.attributes.push_back(std::pmr::vector<llvm::Attribute>{attributes.begin(), attributes.end()});
                transformed_arguments.is_return_value_passed_as_first_argument = true;
            }
        }

        llvm::ArrayRef<clang::CodeGen::CGFunctionInfoArgInfo> const argument_infos = function_info.arguments();

        for (unsigned argument_index = 0; argument_index < argument_infos.size(); ++argument_index)
        {
            clang::CodeGen::CGFunctionInfoArgInfo const& argument_info = argument_infos[argument_index];

            clang::CodeGen::ABIArgInfo::Kind const kind = argument_info.info.getKind();

            switch (kind)
            {
                case clang::CodeGen::ABIArgInfo::Direct:
                case clang::CodeGen::ABIArgInfo::Extend: {
                    llvm::Type* const new_type = argument_info.info.getCoerceToType();

                    if (new_type->isStructTy())
                    {
                        llvm::StructType* const new_struct_type = static_cast<llvm::StructType*>(new_type);
                        llvm::ArrayRef<llvm::Type*> const new_elements = new_struct_type->elements();

                        llvm::Value* const original_argument = original_arguments[argument_index];

                        iris::Type_reference const& original_argument_type = function_type.input_parameter_types[argument_index];
                        llvm::Type* const original_argument_llvm_type = type_reference_to_llvm_type(llvm_context, llvm_data_layout, original_argument_type, type_database);
                        llvm::Align const original_argument_alignment = llvm_data_layout.getABITypeAlign(original_argument_llvm_type);

                        for (unsigned new_element_index = 0; new_element_index < new_elements.size(); ++new_element_index)
                        {
                            std::array<llvm::Value*, 2> const indices
                            {
                                llvm::ConstantInt::get(llvm::Type::getInt32Ty(llvm_context), 0),
                                llvm::ConstantInt::get(llvm::Type::getInt32Ty(llvm_context), new_element_index),
                            };

                            llvm::Value* const pointer_to_element = llvm_builder.CreateInBoundsGEP(new_type, original_argument, indices);

                            llvm::Type* const element_type = new_elements[new_element_index];
                            llvm::Value* const loaded_element = llvm_builder.CreateAlignedLoad(element_type, pointer_to_element, original_argument_alignment);

                            transformed_arguments.values.push_back(loaded_element);
                            transformed_arguments.attributes.push_back({});
                        }
                    }
                    else
                    {
                        llvm::Value* const original_argument = original_arguments[argument_index];
                        iris::Type_reference const& original_argument_type = function_type.input_parameter_types[argument_index];
                        llvm::Type* const original_argument_llvm_type = type_reference_to_llvm_type(llvm_context, llvm_data_layout, original_argument_type, type_database);

                        bool const is_taking_adress_of_value = is_expression_address_of[argument_index];

                        llvm::Value* transformed_argument = read_from_type(
                            llvm_context,
                            llvm_builder,
                            llvm_data_layout,
                            llvm_parent_function,
                            original_argument,
                            original_argument_llvm_type,
                            is_taking_adress_of_value,
                            new_type,
                            std::nullopt,
                            argument_info.info,
                            Convertion_type::From_original_to_abi
                        );

                        std::pmr::vector<llvm::Attribute> attributes;
                        attributes.reserve(2);
                        attributes.push_back(llvm::Attribute::get(llvm_context, llvm::Attribute::NoUndef));
                        
                        if (argument_info.info.isExtend())
                        {
                            if (argument_info.info.isSignExt())
                                attributes.push_back(llvm::Attribute::get(llvm_context, llvm::Attribute::SExt));
                            else
                                attributes.push_back(llvm::Attribute::get(llvm_context, llvm::Attribute::ZExt));
                        }

                        transformed_arguments.values.push_back(transformed_argument);
                        transformed_arguments.attributes.push_back(std::move(attributes));
                    }

                    break;
                }
                case clang::CodeGen::ABIArgInfo::Indirect: {

                    if (argument_info.info.getIndirectByVal())
                    {
                        llvm::Value* const original_argument = original_arguments[argument_index];
                        
                        iris::Type_reference const& original_argument_type = function_type.input_parameter_types[argument_index];
                        llvm::Type* const original_argument_llvm_type = type_reference_to_llvm_type(llvm_context, llvm_data_layout, original_argument_type, type_database);

                        std::pmr::vector<llvm::Attribute> attributes
                        {
                            llvm::Attribute::get(llvm_context, llvm::Attribute::NoUndef),
                            llvm::Attribute::getWithByValType(llvm_context, original_argument_llvm_type),
                            llvm::Attribute::getWithAlignment(llvm_context, llvm::Align(argument_info.info.getIndirectAlign().getQuantity()))
                        };
                        
                        transformed_arguments.values.push_back(original_argument);
                        transformed_arguments.attributes.push_back(std::move(attributes));
                    }
                    else
                    {
                        iris::Type_reference const& original_argument_type = function_type.input_parameter_types[argument_index];
                        llvm::Type* const original_argument_llvm_type = type_reference_to_llvm_type(llvm_context, llvm_data_layout, original_argument_type, type_database);
                        std::uint64_t const original_argument_size_in_bits = llvm_data_layout.getTypeAllocSize(original_argument_llvm_type);
                        llvm::Align const original_argument_alignment = llvm_data_layout.getABITypeAlign(original_argument_llvm_type);
                        
                        llvm::AllocaInst* const alloca_instruction = create_alloca_instruction(llvm_builder, llvm_data_layout, llvm_parent_function, original_argument_llvm_type);

                        create_memcpy_call(
                            llvm_context,
                            llvm_builder,
                            llvm_module,
                            alloca_instruction,
                            original_arguments[argument_index],
                            original_argument_size_in_bits,
                            original_argument_alignment
                        );

                        transformed_arguments.values.push_back(alloca_instruction);
                        transformed_arguments.attributes.push_back(std::pmr::vector<llvm::Attribute>{{ llvm::Attribute::get(llvm_context, llvm::Attribute::NoUndef) }});
                    }
                    
                    break;
                }
                case clang::CodeGen::ABIArgInfo::IndirectAliased: {
                    throw std::runtime_error{ "Clang_code_generation.transform_arguments(): IndirectAliased not implemented!" };
                }
                case clang::CodeGen::ABIArgInfo::Ignore: {
                    break;
                }
                case clang::CodeGen::ABIArgInfo::Expand: {
                    throw std::runtime_error{ "Clang_code_generation.transform_arguments(): Expand not implemented!" };
                }
                case clang::CodeGen::ABIArgInfo::CoerceAndExpand: {
                    throw std::runtime_error{ "Clang_code_generation.transform_arguments(): CoerceAndExpand not implemented!" };
                }
                case clang::CodeGen::ABIArgInfo::InAlloca: {
                    throw std::runtime_error{ "Clang_code_generation.transform_arguments(): InAlloca not implemented!" };
                }
            }
        }

        if (function_info.isVariadic())
        {
            static constexpr unsigned c_varargs_int_promotion_bits = 32;

            std::size_t const start_index = argument_infos.size();

            for (std::size_t argument_index = start_index; argument_index < original_arguments.size(); ++argument_index)
            {
                llvm::Value* const original_argument = original_arguments[argument_index];
                bool const is_taking_adress_of_value = is_expression_address_of[argument_index];

                llvm::Value* loaded_value = create_load_instruction_if_needed(
                    llvm_builder,
                    llvm_data_layout,
                    original_argument,
                    is_taking_adress_of_value
                );

                if (argument_index < original_argument_types.size())
                {
                    std::optional<Type_reference> const& original_argument_type = original_argument_types[argument_index];

                    if (original_argument_type.has_value())
                    {
                        // When the argument is an indirection expression (e.g. *y where y: *Float32),
                        // the value has already been loaded once (from y's alloca to get the pointer),
                        // but the actual pointee value still needs to be loaded before promotion.
                        if (loaded_value->getType()->isPointerTy() && !is_non_void_pointer(*original_argument_type))
                        {
                            llvm::Type* const original_argument_llvm_type = type_reference_to_llvm_type(llvm_context, llvm_data_layout, *original_argument_type, type_database);
                            loaded_value = create_load_instruction(llvm_builder, llvm_data_layout, original_argument_llvm_type, loaded_value);
                        }

                        if (is_floating_point(*original_argument_type) && loaded_value->getType()->isFloatTy())
                        {
                            loaded_value = llvm_builder.CreateFPExt(loaded_value, llvm::Type::getDoubleTy(llvm_context));
                        }
                        else if ((is_integer(*original_argument_type) || is_bool(*original_argument_type) || is_c_bool(*original_argument_type)) && loaded_value->getType()->isIntegerTy())
                        {
                            unsigned const source_bits = loaded_value->getType()->getIntegerBitWidth();

                            if (source_bits < c_varargs_int_promotion_bits)
                            {
                                llvm::Type* const promoted_type = llvm::Type::getIntNTy(llvm_context, c_varargs_int_promotion_bits);

                                if (is_signed_integer(*original_argument_type))
                                {
                                    loaded_value = llvm_builder.CreateSExt(loaded_value, promoted_type);
                                }
                                else
                                {
                                    loaded_value = llvm_builder.CreateZExt(loaded_value, promoted_type);
                                }
                            }
                        }
                    }
                }

                std::pmr::vector<llvm::Attribute> attributes;
                attributes.reserve(1);
                attributes.push_back(llvm::Attribute::get(llvm_context, llvm::Attribute::NoUndef));
                
                transformed_arguments.values.push_back(loaded_value);
                transformed_arguments.attributes.push_back(std::pmr::vector<llvm::Attribute>{attributes.begin(), attributes.end()});
            }
        }

        return transformed_arguments;

        /*clang::CodeGen::CGFunctionInfo const& FI = create_clang_function_info(clang_module_data, core_module, function_name);

        std::pmr::vector<llvm::Value*> transformedArgs;

        for (unsigned i = 0; i < FI.arg_size(); ++i) {
            const clang::CodeGen::CGFunctionInfoArgInfo& argInfo = FI.arguments()[i];
            llvm::Value* arg = arguments[i];

            switch (argInfo.info.getKind()) {
            case clang::CodeGen::ABIArgInfo::Direct: {
                // For 'Direct', we may need to handle type extension/truncation.
                llvm::Type* expectedType = argInfo.info.getCoerceToType();

                if (arg->getType() != expectedType) {
                    // Perform type casting (extension, truncation, etc.)
                    if (arg->getType()->isIntegerTy() && expectedType->isIntegerTy()) {
                        unsigned argBits = arg->getType()->getIntegerBitWidth();
                        unsigned expectedBits = expectedType->getIntegerBitWidth();

                        if (argBits < expectedBits) {
                            // Extend small integer types (e.g., char to int)
                            arg = Builder.CreateZExt(arg, expectedType);
                        }
                        else if (argBits > expectedBits) {
                            // Truncate larger integer types
                            arg = Builder.CreateTrunc(arg, expectedType);
                        }
                    }
                    else {
                        // Handle other types of casts (e.g., float to double)
                        arg = Builder.CreateBitCast(arg, expectedType);
                    }
                }
                transformedArgs.push_back(arg);
                break;
            }
            case clang::CodeGen::ABIArgInfo::Indirect: {
                // For 'Indirect', we need to pass a pointer to the argument
                llvm::Value* argPtr = Builder.CreateAlloca(arg->getType());
                Builder.CreateStore(arg, argPtr);
                transformedArgs.push_back(argPtr);
                break;
            }
            case clang::CodeGen::ABIArgInfo::Extend: {
                // For 'Extend', we need to zero-extend the integer type
                llvm::Type* expectedType = argInfo.info.getCoerceToType();
                arg = Builder.CreateZExt(arg, expectedType);
                transformedArgs.push_back(arg);
                break;
            }
            case clang::CodeGen::ABIArgInfo::Ignore: {
                // 'Ignore' means we skip this argument entirely
                break;
            }
            default: {
                llvm::errs() << "Unhandled ABIArgInfo kind!\n";
                break;
            }
            }
        }

        return transformedArgs;*/
    }

    llvm::Value* generate_function_call(
        std::pmr::vector<bool> const& is_expression_address_of,
        std::span<std::optional<Type_reference> const> const original_argument_types,
        llvm::LLVMContext& llvm_context,
        llvm::IRBuilder<>& llvm_builder,
        llvm::DataLayout const& llvm_data_layout,
        llvm::Module& llvm_module,
        llvm::Function& llvm_parent_function,
        Clang_module_data const& clang_module_data,
        iris::Module const& core_module,
        iris::Function_type const& function_type,
        llvm::FunctionType& llvm_function_type,
        llvm::Value& llvm_function_callee,
        std::span<llvm::Value* const> const llvm_arguments,
        Declaration_database const& declaration_database,
        Type_database const& type_database
    )
    {
        clang::CodeGen::CGFunctionInfo const& function_info = create_clang_function_info(clang_module_data, function_type, declaration_database);

        Transformed_arguments const transformed_arguments = transform_arguments(is_expression_address_of, original_argument_types, llvm_context, llvm_builder, llvm_data_layout, llvm_module, llvm_parent_function, core_module, function_type, function_info, llvm_arguments, type_database);

        llvm::CallInst* call_instruction = llvm_builder.CreateCall(&llvm_function_type, &llvm_function_callee, transformed_arguments.values);

        for (std::size_t argument_index = 0; argument_index <transformed_arguments.attributes.size(); ++argument_index)
        {
            std::span<llvm::Attribute const> const attributes = transformed_arguments.attributes[argument_index];

            for (llvm::Attribute const& attribute : attributes)
            {
                call_instruction->addParamAttr(static_cast<unsigned>(argument_index), attribute);
            }
        }

        if (transformed_arguments.is_return_value_passed_as_first_argument)
        {
            return transformed_arguments.values[0];
        }

        return read_function_return_instruction(
            llvm_context,
            llvm_builder,
            llvm_data_layout,
            llvm_parent_function,
            core_module,
            function_type,
            function_info,
            type_database,
            call_instruction
        );
    }

    void set_function_input_parameter_debug_information(
        llvm::LLVMContext& llvm_context,
        llvm::DataLayout const& llvm_data_layout,
        iris::Module const& core_module,
        iris::Function_declaration const& function_declaration,
        std::size_t const input_parameter_index,
        llvm::BasicBlock& llvm_block,
        llvm::Value& alloca_instruction,
        Debug_info* debug_info
    )
    {
        if (debug_info == nullptr)
            return;

        std::pmr::string const& name = function_declaration.input_parameter_names[input_parameter_index];
        Type_reference const& core_type = function_declaration.type.input_parameter_types[input_parameter_index];
        
        Source_range_location const function_declaration_source_location =
            function_declaration.source_location.value_or(Source_range_location{});

        Source_position const parameter_source_position =
            function_declaration.input_parameter_source_positions.has_value() ?
            function_declaration.input_parameter_source_positions.value()[input_parameter_index] :
            Source_position{ .line = function_declaration_source_location.range.start.line, .column = function_declaration_source_location.range.start.column };

        llvm::DIType* const llvm_argument_debug_type = type_reference_to_llvm_debug_type(
            *debug_info->llvm_builder,
            *get_debug_scope(*debug_info),
            llvm_data_layout,
            core_module,
            core_type,
            debug_info->type_database
        );

        llvm::DIScope* const debug_scope = get_debug_scope(*debug_info);

        llvm::DILocalVariable* debug_parameter_variable = debug_info->llvm_builder->createParameterVariable(
            debug_scope,
            name.c_str(),
            input_parameter_index + 1,
            debug_scope->getFile(),
            parameter_source_position.line,
            llvm_argument_debug_type,
            true
        );

        llvm::DILocation* const debug_location = llvm::DILocation::get(llvm_context, parameter_source_position.line, parameter_source_position.column, debug_scope);

        debug_info->llvm_builder->insertDeclare(
            &alloca_instruction,
            debug_parameter_variable,
            debug_info->llvm_builder->createExpression(),
            debug_location,
            &llvm_block
        );
    }

    std::pmr::vector<Value_and_type> generate_function_arguments(
        llvm::LLVMContext& llvm_context,
        llvm::IRBuilder<>& llvm_builder,
        llvm::DataLayout const& llvm_data_layout,
        Clang_module_data const& clang_module_data,
        iris::Module const& core_module,
        iris::Function_declaration const& function_declaration,
        llvm::Function& llvm_function,
        llvm::BasicBlock& llvm_block,
        Declaration_database const& declaration_database,
        Type_database const& type_database,
        Debug_info* debug_info
    )
    {
        clang::CodeGen::CGFunctionInfo const& function_info = create_clang_function_info(clang_module_data, function_declaration.type, declaration_database);
        llvm::ArrayRef<clang::CodeGen::CGFunctionInfoArgInfo> const argument_infos = function_info.arguments();

        std::pmr::vector<Value_and_type> restored_arguments;
        restored_arguments.reserve(argument_infos.size());

        unsigned function_argument_index = 0;
        
        {
            clang::CodeGen::ABIArgInfo const& return_info = function_info.getReturnInfo();
            if (return_info.isIndirect())
            {
                function_argument_index += 1;
            }
        }

        for (unsigned restored_argument_index = 0; restored_argument_index < argument_infos.size(); ++restored_argument_index)
        {
            clang::CodeGen::CGFunctionInfoArgInfo const& argument_info = argument_infos[restored_argument_index];
            std::pmr::string const& restored_argument_name = function_declaration.input_parameter_names[restored_argument_index];
            iris::Type_reference const& restored_argument_type = function_declaration.type.input_parameter_types[restored_argument_index];

            clang::CodeGen::ABIArgInfo::Kind const kind = argument_info.info.getKind();

            switch (kind)
            {
                case clang::CodeGen::ABIArgInfo::Direct:
                case clang::CodeGen::ABIArgInfo::Extend: {

                    llvm::Type* const restored_argument_llvm_type = type_reference_to_llvm_type(llvm_context, llvm_data_layout, restored_argument_type, type_database);
                    llvm::Align const restored_argument_alignment = llvm_data_layout.getABITypeAlign(restored_argument_llvm_type);

                    llvm::Type* const function_argument_type = argument_info.info.getCoerceToType();

                    if (function_argument_type->isStructTy())
                    {
                        llvm::StructType* const function_argument_struct_type = static_cast<llvm::StructType*>(function_argument_type);
                        llvm::ArrayRef<llvm::Type*> const function_argument_elements = function_argument_struct_type->elements();

                        llvm::AllocaInst* const alloca_instruction = create_alloca_instruction(llvm_builder, llvm_data_layout, llvm_function, restored_argument_llvm_type, restored_argument_name.data());
                        restored_arguments.push_back(Value_and_type{.name = restored_argument_name, .value = alloca_instruction, .type = restored_argument_type});

                        set_function_input_parameter_debug_information(
                            llvm_context,
                            llvm_data_layout,
                            core_module,
                            function_declaration,
                            restored_argument_index,
                            llvm_block,
                            *alloca_instruction,
                            debug_info
                        );

                        for (unsigned function_argument_element_index = 0; function_argument_element_index < function_argument_elements.size(); ++function_argument_element_index)
                        {
                            std::array<llvm::Value*, 2> const indices
                            {
                                llvm::ConstantInt::get(llvm::Type::getInt32Ty(llvm_context), 0),
                                llvm::ConstantInt::get(llvm::Type::getInt32Ty(llvm_context), function_argument_element_index),
                            };
                            llvm::Value* const pointer_to_restored_argument = llvm_builder.CreateInBoundsGEP(function_argument_type, alloca_instruction, indices);

                            llvm::Value* const function_argument = llvm_function.getArg(function_argument_index);
                            llvm_builder.CreateAlignedStore(function_argument, pointer_to_restored_argument, restored_argument_alignment);

                            function_argument_index += 1;
                        }
                    }
                    else
                    {
                        llvm::Value* const function_argument = llvm_function.getArg(function_argument_index);

                        llvm::Value* const converted_value = read_from_type(
                            llvm_context,
                            llvm_builder,
                            llvm_data_layout,
                            llvm_function,
                            function_argument,
                            function_argument_type,
                            false,
                            restored_argument_llvm_type,
                            restored_argument_name,
                            argument_info.info,
                            Convertion_type::From_abi_to_original
                        );
                        restored_arguments.push_back(Value_and_type{.name = restored_argument_name, .value = converted_value, .type = restored_argument_type});

                        set_function_input_parameter_debug_information(
                            llvm_context,
                            llvm_data_layout,
                            core_module,
                            function_declaration,
                            restored_argument_index,
                            llvm_block,
                            *converted_value,
                            debug_info
                        );

                        function_argument_index += 1;
                    }

                    break;
                }
                case clang::CodeGen::ABIArgInfo::Indirect: {

                    llvm::Value* const function_argument = llvm_function.getArg(function_argument_index);
                    restored_arguments.push_back(Value_and_type{.name = restored_argument_name, .value = function_argument, .type = restored_argument_type});

                    set_function_input_parameter_debug_information(
                        llvm_context,
                        llvm_data_layout,
                        core_module,
                        function_declaration,
                        restored_argument_index,
                        llvm_block,
                        *function_argument,
                        debug_info
                    );

                    function_argument_index += 1;

                    break;
                }
                case clang::CodeGen::ABIArgInfo::IndirectAliased: {
                    throw std::runtime_error{ "Clang_code_generation.generate_function_arguments(): IndirectAliased not implemented!" };
                }
                case clang::CodeGen::ABIArgInfo::Ignore: {
                    break;
                }
                case clang::CodeGen::ABIArgInfo::Expand: {
                    throw std::runtime_error{ "Clang_code_generation.generate_function_arguments(): Expand not implemented!" };
                }
                case clang::CodeGen::ABIArgInfo::CoerceAndExpand: {
                    throw std::runtime_error{ "Clang_code_generation.generate_function_arguments(): CoerceAndExpand not implemented!" };
                }
                case clang::CodeGen::ABIArgInfo::InAlloca: {
                    throw std::runtime_error{ "Clang_code_generation.generate_function_arguments(): InAlloca not implemented!" };
                }
            }
        }

        return restored_arguments;
    }

    llvm::Value* generate_function_return_instruction(
        llvm::LLVMContext& llvm_context,
        llvm::IRBuilder<>& llvm_builder,
        llvm::DataLayout const& llvm_data_layout,
        llvm::Module& llvm_module,
        Clang_module_data const& clang_module_data,
        iris::Module const& core_module,
        iris::Function_type const& function_type,
        llvm::Function& llvm_function,
        Declaration_database const& declaration_database,
        Type_database const& type_database,
        Value_and_type const& value_to_return,
        bool const is_taking_address_of_llvm_value
    )
    {
        clang::CodeGen::CGFunctionInfo const& function_info = create_clang_function_info(clang_module_data, function_type, declaration_database);

        if (function_type.output_parameter_types.empty())
        {
            llvm::Value* const instruction = llvm_builder.CreateRetVoid();
            return instruction;
        }

        clang::CodeGen::ABIArgInfo const& return_info = function_info.getReturnInfo();
        clang::CodeGen::ABIArgInfo::Kind const kind = return_info.getKind();

        switch (kind)
        {
            case clang::CodeGen::ABIArgInfo::Direct:
            case clang::CodeGen::ABIArgInfo::Extend: {

                iris::Type_reference const& original_return_type = function_type.output_parameter_types[0];
                llvm::Type* const original_return_llvm_type = type_reference_to_llvm_type(llvm_context, llvm_data_layout, original_return_type, type_database);

                llvm::Type* const new_return_llvm_type = return_info.getCoerceToType();

                llvm::Value* const converted_value = read_from_type(
                    llvm_context,
                    llvm_builder,
                    llvm_data_layout,
                    llvm_function,
                    value_to_return.value,
                    original_return_llvm_type,
                    is_taking_address_of_llvm_value,
                    new_return_llvm_type,
                    std::nullopt,
                    return_info,
                    Convertion_type::From_original_to_abi
                );
                
                llvm::Value* const instruction = llvm_builder.CreateRet(converted_value);
                return instruction;
            }
            case clang::CodeGen::ABIArgInfo::Indirect: {

                // Return value was passed as the first argument pointer
                // So here we need to store the result in that first argument pointer

                llvm::Argument* const return_argument = llvm_function.getArg(0);

                iris::Type_reference const& return_type = function_type.output_parameter_types[0];
                llvm::Type* const return_llvm_type = type_reference_to_llvm_type(llvm_context, llvm_data_layout, return_type, type_database);
                std::uint64_t const return_size_in_bits = llvm_data_layout.getTypeAllocSize(return_llvm_type);
                llvm::Align const return_alignment = llvm_data_layout.getABITypeAlign(return_llvm_type);

                if (value_to_return.value->getType()->isPointerTy())
                {
                    create_memcpy_call(
                        llvm_context,
                        llvm_builder,
                        llvm_module,
                        return_argument,
                        value_to_return.value,
                        return_size_in_bits,
                        return_alignment
                    );

                    llvm::Value* const return_instruction = llvm_builder.CreateRetVoid();
                    return return_instruction;
                }
                else
                {
                    llvm_builder.CreateAlignedStore(value_to_return.value, return_argument, return_alignment);

                    llvm::Value* const return_instruction = llvm_builder.CreateRetVoid();
                    return return_instruction;
                }
            }
            case clang::CodeGen::ABIArgInfo::Ignore: {
                llvm::Value* const return_instruction = llvm_builder.CreateRetVoid();
                return return_instruction;
            }
            default: {
                throw std::runtime_error{ "Clang_code_generation.generate_function_return_value(): return kind not implemented!" };
            }
        }
    }

    void set_function_definition_attributes(
        llvm::LLVMContext& llvm_context,
        Clang_module_data const& clang_module_data,
        llvm::Function& llvm_function
    )
    {
        llvm::AttrBuilder attributes_builder{llvm_context};
        clang::CodeGen::addDefaultFunctionDefinitionAttributes(clang_module_data.code_generator->CGM(), attributes_builder);
        
        llvm_function.addFnAttrs(attributes_builder);
    }

    llvm::ConstantInt* get_constant(
        llvm::LLVMContext& llvm_context,
        unsigned value
    )
    {
        return llvm::ConstantInt::get(llvm::Type::getInt32Ty(llvm_context), value);
    }

    llvm::Value* create_alloca_and_store_if_not_pointer(
        llvm::IRBuilder<>& llvm_builder,
        llvm::DataLayout const& llvm_data_layout,
        llvm::Function& llvm_parent_function,
        llvm::Value* const llvm_value,
        llvm::Type* const llvm_type
    )
    {
        if (llvm_value->getType()->isPointerTy())
            return llvm_value;

        llvm::AllocaInst* const alloca_instruction = create_alloca_instruction(llvm_builder, llvm_data_layout, llvm_parent_function, llvm_type);
        llvm_builder.CreateAlignedStore(llvm_value, alloca_instruction, llvm_data_layout.getABITypeAlign(llvm_type));
        return alloca_instruction;
    }

    llvm::Value* read_from_different_type(
        llvm::LLVMContext& llvm_context,
        llvm::IRBuilder<>& llvm_builder,
        llvm::DataLayout const& llvm_data_layout,
        llvm::Function& llvm_parent_function,
        llvm::Value* const source_llvm_value,
        llvm::Type* const source_llvm_type,
        llvm::Type* const destination_llvm_type,
        clang::CodeGen::ABIArgInfo const& abi_argument_info,
        Convertion_type const convertion_type
    )
    {
        if (source_llvm_type->isStructTy() && destination_llvm_type->isStructTy())
        {
            if (convertion_type == Convertion_type::From_original_to_abi)
            {
                llvm::Value* const source_llvm_pointer_value = create_alloca_and_store_if_not_pointer(llvm_builder, llvm_data_layout, llvm_parent_function, source_llvm_value, source_llvm_type);
                llvm::Value* const load_instruction = llvm_builder.CreateAlignedLoad(destination_llvm_type, source_llvm_pointer_value, llvm_data_layout.getABITypeAlign(source_llvm_type));
                return load_instruction;
            }
            else
            {
                llvm::AllocaInst* const destination = create_alloca_instruction(llvm_builder, llvm_data_layout, llvm_parent_function, destination_llvm_type);
            
                llvm::StructType* const source_struct_llvm_type = static_cast<llvm::StructType*>(source_llvm_type);
                llvm::ArrayRef<llvm::Type*> const source_struct_elements = source_struct_llvm_type->elements();

                for (unsigned source_element_index = 0; source_element_index < source_struct_elements.size(); ++source_element_index)
                {
                    llvm::Value* const pointer_to_destination = llvm_builder.CreateInBoundsGEP(source_llvm_type, destination, {get_constant(llvm_context, 0), get_constant(llvm_context, source_element_index)});
                    llvm::Value* const extract_value = llvm_builder.CreateExtractValue(source_llvm_value, {source_element_index});
                    llvm_builder.CreateAlignedStore(extract_value, pointer_to_destination, llvm_data_layout.getABITypeAlign(destination_llvm_type));
                }

                return destination;
            }
        }
        else if (!source_llvm_type->isStructTy() && destination_llvm_type->isStructTy())
        {
            if (convertion_type == Convertion_type::From_original_to_abi)
            {
                llvm::AllocaInst* const destination = create_alloca_instruction(llvm_builder, llvm_data_layout, llvm_parent_function, destination_llvm_type);
                llvm_builder.CreateAlignedStore(source_llvm_value, destination, llvm_data_layout.getABITypeAlign(destination_llvm_type));
                return destination;
            }
            else
            {
                llvm::AllocaInst* const destination = create_alloca_instruction(llvm_builder, llvm_data_layout, llvm_parent_function, destination_llvm_type);
                llvm::Value* const pointer_to_destination = llvm_builder.CreateInBoundsGEP(destination_llvm_type, destination, {get_constant(llvm_context, 0), get_constant(llvm_context, 0)});
                llvm_builder.CreateAlignedStore(source_llvm_value, pointer_to_destination, llvm_data_layout.getABITypeAlign(destination_llvm_type));
                return destination;
            }
        }
        else if (source_llvm_type->isStructTy() && !destination_llvm_type->isStructTy())
        {
            llvm::Value* const source_llvm_pointer_value = create_alloca_and_store_if_not_pointer(llvm_builder, llvm_data_layout, llvm_parent_function, source_llvm_value, source_llvm_type);

            llvm::Value* const pointer_to_source = llvm_builder.CreateInBoundsGEP(source_llvm_type, source_llvm_pointer_value, {get_constant(llvm_context, 0), get_constant(llvm_context, 0)});
            llvm::LoadInst* const destination_value = llvm_builder.CreateAlignedLoad(destination_llvm_type, pointer_to_source, llvm_data_layout.getABITypeAlign(source_llvm_type));
            return destination_value;
        }
        else if (source_llvm_type->isIntegerTy() && destination_llvm_type->isIntegerTy())
        {
            if (abi_argument_info.isExtend())
            {
                llvm::Value* const loaded_value =
                    (llvm::AllocaInst::classof(source_llvm_value) || llvm::GetElementPtrInst::classof(source_llvm_value)) ?
                    llvm_builder.CreateAlignedLoad(source_llvm_type, source_llvm_value, llvm_data_layout.getABITypeAlign(source_llvm_type)) :
                    source_llvm_value;

                if (abi_argument_info.isSignExt())
                {
                    return llvm_builder.CreateSExtOrTrunc(loaded_value, destination_llvm_type);
                }
                else
                {
                    return llvm_builder.CreateZExtOrTrunc(loaded_value, destination_llvm_type);
                }
            }
        }

        throw std::runtime_error{ "read_from_different_type not implemented yet!" };
    }

    llvm::Value* read_from_type(
        llvm::LLVMContext& llvm_context,
        llvm::IRBuilder<>& llvm_builder,
        llvm::DataLayout const& llvm_data_layout,
        llvm::Function& llvm_parent_function,
        llvm::Value* const source_llvm_value,
        llvm::Type* const source_llvm_type,
        bool const is_taking_address_of_source_llvm_value,
        llvm::Type* const destination_llvm_type,
        std::optional<std::string_view> const alloca_name,
        clang::CodeGen::ABIArgInfo const& abi_argument_info,
        Convertion_type const convertion_type
    )
    {   
        auto const is_pointer_valued_global_variable = [](llvm::Value* const value) -> bool
        {
            if (!llvm::GlobalVariable::classof(value))
                return false;

            llvm::GlobalVariable const* const global_variable = static_cast<llvm::GlobalVariable const*>(value);
            return global_variable->getValueType()->isPointerTy();
        };

        if (source_llvm_type == destination_llvm_type)
        {
            if (source_llvm_value->getType() != source_llvm_type && source_llvm_value->getType()->isPointerTy())
            {
                llvm::Value* const loaded_value = create_load_instruction(llvm_builder, llvm_data_layout, destination_llvm_type, source_llvm_value);
                return loaded_value;
            }
            else
            {
                if (alloca_name.has_value())
                {
                    llvm::AllocaInst* const destination = create_alloca_instruction(llvm_builder, llvm_data_layout, llvm_parent_function, destination_llvm_type, alloca_name->data());
                    llvm_builder.CreateAlignedStore(source_llvm_value, destination, llvm_data_layout.getABITypeAlign(destination_llvm_type));
                    return destination;
                }
                else
                {
                    if (llvm::AllocaInst::classof(source_llvm_value) || llvm::GetElementPtrInst::classof(source_llvm_value) || is_pointer_valued_global_variable(source_llvm_value))
                    {
                        if (!is_taking_address_of_source_llvm_value)
                        {
                            llvm::Value* const loaded_value = create_load_instruction(llvm_builder, llvm_data_layout, destination_llvm_type, source_llvm_value);
                            return loaded_value;
                        }
                    }

                    return source_llvm_value;
                }
            }
        }

        llvm::Value* converted_value = read_from_different_type(
            llvm_context,
            llvm_builder,
            llvm_data_layout,
            llvm_parent_function,
            source_llvm_value,
            source_llvm_type,
            destination_llvm_type,
            abi_argument_info,
            convertion_type
        );

        if (alloca_name.has_value() && !llvm::AllocaInst::classof(converted_value))
        {
            llvm::AllocaInst* const destination = create_alloca_instruction(llvm_builder, llvm_data_layout, llvm_parent_function, destination_llvm_type, alloca_name->data());
            llvm_builder.CreateAlignedStore(converted_value, destination, llvm_data_layout.getABITypeAlign(destination_llvm_type));
            return destination;
        }
        else
        {
            return converted_value;
        }
    }

    llvm::Value* read_function_return_instruction(
        llvm::LLVMContext& llvm_context,
        llvm::IRBuilder<>& llvm_builder,
        llvm::DataLayout const& llvm_data_layout,
        llvm::Function& llvm_parent_function,
        iris::Module const& core_module,
        iris::Function_type const& function_type,
        clang::CodeGen::CGFunctionInfo const& function_info,
        Type_database const& type_database,
        llvm::Value* const call_instruction
    )
    {
        if (function_type.output_parameter_types.empty())
        {
            return call_instruction;
        }

        clang::CodeGen::ABIArgInfo const& return_info = function_info.getReturnInfo();
        clang::CodeGen::ABIArgInfo::Kind const kind = return_info.getKind();

        switch (kind)
        {
            case clang::CodeGen::ABIArgInfo::Direct:
            case clang::CodeGen::ABIArgInfo::Extend: {

                iris::Type_reference const& original_return_type = function_type.output_parameter_types[0];
                llvm::Type* const original_return_llvm_type = type_reference_to_llvm_type(llvm_context, llvm_data_layout, original_return_type, type_database);

                llvm::Type* const new_return_llvm_type = return_info.getCoerceToType();

                return read_from_type(
                    llvm_context,
                    llvm_builder,
                    llvm_data_layout,
                    llvm_parent_function,
                    call_instruction,
                    new_return_llvm_type,
                    false,
                    original_return_llvm_type,
                    std::nullopt,
                    return_info,
                    Convertion_type::From_abi_to_original
                );
            }
            case clang::CodeGen::ABIArgInfo::Ignore: {
                return call_instruction;
            }
            default: {
                throw std::runtime_error{ "Clang_code_generation.read_function_return_instruction(): return kind not implemented!" };
            }
        }
    }

    llvm::Type* convert_type(
        Clang_module_data const& clang_module_data,
        std::string_view const module_name,
        std::string_view const declaration_name
    )
    {
        std::optional<iris::Custom_type_reference> const instance_type_reference = unmangle_type_instance_name(declaration_name);
        std::string_view const actual_module_name = instance_type_reference.has_value() ? std::string_view{instance_type_reference->module_reference.name} : module_name;

        Clang_module_declarations const& clang_declarations = clang_module_data.declaration_database.map.find(actual_module_name)->second;

        {
            auto const location = clang_declarations.alias_type_declarations.find(declaration_name);
            if (location != clang_declarations.alias_type_declarations.end())
            {
                clang::TypedefDecl* const typedef_declaration = location->second;
                clang::QualType const qual_type = clang_module_data.ast_context.getCanonicalTypeDeclType(typedef_declaration);
                llvm::Type* const clang_type = clang::CodeGen::convertTypeForMemory(clang_module_data.code_generator->CGM(), qual_type);
                return clang_type;
            }
        }

        {
            auto const location = clang_declarations.enum_declarations.find(declaration_name);
            if (location != clang_declarations.enum_declarations.end())
            {
                clang::EnumDecl* const enum_declaration = location->second;
                clang::QualType const qual_type = clang_module_data.ast_context.getCanonicalTypeDeclType(enum_declaration);
                llvm::Type* const clang_type = clang::CodeGen::convertTypeForMemory(clang_module_data.code_generator->CGM(), qual_type);
                return clang_type;
            }
        }

        {
            auto const location = clang_declarations.struct_declarations.find(declaration_name);
            if (location != clang_declarations.struct_declarations.end())
            {
                clang::RecordDecl* const record_declaration = location->second;
                return convert_type(clang_module_data, record_declaration);
            }
        }

        {
            auto const location = clang_declarations.union_declarations.find(declaration_name);
            if (location != clang_declarations.union_declarations.end())
            {
                clang::RecordDecl* const record_declaration = location->second;
                return convert_type(clang_module_data, record_declaration);
            }
        }

        throw std::runtime_error{ std::format("Could not find type '{}.{}'", module_name, declaration_name) };
    }

    static clang::RecordDecl* create_on_demand_clang_union_declaration(
        clang::ASTContext& clang_ast_context,
        std::string_view const module_name,
        iris::Union_declaration const& union_declaration
    )
    {
        std::string const mangled_name = mangle_name(module_name, union_declaration.name, union_declaration.unique_name);
        clang::IdentifierInfo* const union_name = &clang_ast_context.Idents.get(mangled_name);

        clang::RecordDecl* const record_declaration = clang::RecordDecl::Create(
            clang_ast_context,
            clang::TagTypeKind::Union,
            clang_ast_context.getTranslationUnitDecl(),
            clang::SourceLocation(),
            clang::SourceLocation(),
            union_name
        );

        return record_declaration;
    }

    static void ensure_clang_custom_type_declaration(
        clang::ASTContext& clang_ast_context,
        Declaration_database const& declaration_database,
        Clang_declaration_database& clang_declaration_database,
        std::string_view const module_name,
        std::string_view const declaration_name
    )
    {
        auto module_location = clang_declaration_database.map.find(module_name);
        if (module_location != clang_declaration_database.map.end())
        {
            Clang_module_declarations const& clang_declarations = module_location->second;

            if (clang_declarations.alias_type_declarations.contains(declaration_name))
                return;

            if (clang_declarations.enum_declarations.contains(declaration_name))
                return;

            if (clang_declarations.struct_declarations.contains(declaration_name))
                return;

            if (clang_declarations.union_declarations.contains(declaration_name))
                return;
        }

        std::optional<Declaration> const declaration = find_declaration(declaration_database, module_name, declaration_name);
        if (!declaration.has_value())
            throw std::runtime_error{ std::format("Could not find declaration '{}.{}'", module_name, declaration_name) };

        auto const ensure_nested_custom_type_declarations = [&](Type_reference const& type_reference) -> bool
        {
            if (std::holds_alternative<Array_slice_type>(type_reference.data))
            {
                ensure_clang_custom_type_declaration(
                    clang_ast_context,
                    declaration_database,
                    clang_declaration_database,
                    "iris.builtin",
                    "Generic_array_slice"
                );
                return false;
            }

            if (!std::holds_alternative<Custom_type_reference>(type_reference.data))
                return false;

            Custom_type_reference const& custom_type_reference = std::get<Custom_type_reference>(type_reference.data);
            ensure_clang_custom_type_declaration(
                clang_ast_context,
                declaration_database,
                clang_declaration_database,
                custom_type_reference.module_reference.name,
                custom_type_reference.name
            );

            return false;
        };

        Clang_module_declarations& clang_declarations = clang_declaration_database.map.emplace(module_name, Clang_module_declarations{}).first->second;

        if (std::holds_alternative<iris::Alias_type_declaration const*>(declaration->data))
        {
            Alias_type_declaration const* const alias_declaration = std::get<iris::Alias_type_declaration const*>(declaration->data);

            if (!alias_declaration->type.empty())
            {
                Type_reference const& alias_underlying_type = alias_declaration->type[0];
                if (std::holds_alternative<iris::Type_instance>(alias_underlying_type.data))
                {
                    Type_instance const& type_instance = std::get<iris::Type_instance>(alias_underlying_type.data);
                    Declaration_instance_storage storage = instantiate_type_instance(declaration_database, type_instance);
                    if (std::holds_alternative<iris::Struct_declaration>(storage.data))
                    {
                        Struct_declaration const& struct_decl = std::get<iris::Struct_declaration>(storage.data);
                        std::string_view const tc_module = type_instance.type_constructor.module_reference.name;

                        for (Type_reference const& member_type : struct_decl.member_types)
                            visit_type_references_recursively(member_type, ensure_nested_custom_type_declarations);

                        Clang_module_declarations& tc_clang_decls = clang_declaration_database.map.emplace(
                            tc_module, Clang_module_declarations{}).first->second;

                        if (!tc_clang_decls.struct_declarations.contains(struct_decl.name))
                        {
                            clang::RecordDecl* const record_decl = create_clang_struct_declaration(
                                clang_ast_context, tc_module, struct_decl);
                            tc_clang_decls.struct_declarations.emplace(struct_decl.name, record_decl);
                            set_clang_struct_definition(clang_ast_context, *record_decl, struct_decl,
                                declaration_database, clang_declaration_database);
                        }
                    }
                }
                else
                {
                    visit_type_references_recursively(alias_underlying_type, ensure_nested_custom_type_declarations);
                }
            }

            // Re-acquire the alias module's declarations in case the map was rehashed above.
            Clang_module_declarations& alias_clang_decls = clang_declaration_database.map.emplace(
                module_name, Clang_module_declarations{}).first->second;
            add_clang_alias_type_declaration(alias_clang_decls.alias_type_declarations, clang_ast_context, *alias_declaration, declaration_database, clang_declaration_database);
            return;
        }

        if (std::holds_alternative<iris::Enum_declaration const*>(declaration->data))
        {
            Enum_declaration const* const enum_declaration = std::get<iris::Enum_declaration const*>(declaration->data);
            add_clang_enum_declaration(clang_declarations.enum_declarations, clang_ast_context, *enum_declaration);
            return;
        }

        if (std::holds_alternative<iris::Forward_declaration const*>(declaration->data))
        {
            return;
        }

        if (std::holds_alternative<iris::Struct_declaration const*>(declaration->data))
        {
            Struct_declaration const* const struct_declaration = std::get<iris::Struct_declaration const*>(declaration->data);

            clang::RecordDecl* record_declaration = nullptr;
            auto const struct_location = clang_declarations.struct_declarations.find(declaration_name);
            if (struct_location == clang_declarations.struct_declarations.end())
            {
                record_declaration = create_clang_struct_declaration(clang_ast_context, module_name, *struct_declaration);
                clang_declarations.struct_declarations.emplace(declaration_name, record_declaration);
            }
            else
            {
                record_declaration = struct_location->second;
            }

            for (Type_reference const& member_type : struct_declaration->member_types)
                visit_type_references_recursively(member_type, ensure_nested_custom_type_declarations);

            if (!record_declaration->isCompleteDefinition())
                set_clang_struct_definition(clang_ast_context, *record_declaration, *struct_declaration, declaration_database, clang_declaration_database);

            return;
        }

        if (std::holds_alternative<iris::Union_declaration const*>(declaration->data))
        {
            Union_declaration const* const union_declaration = std::get<iris::Union_declaration const*>(declaration->data);

            clang::RecordDecl* record_declaration = nullptr;
            auto const union_location = clang_declarations.union_declarations.find(declaration_name);
            if (union_location == clang_declarations.union_declarations.end())
            {
                record_declaration = create_on_demand_clang_union_declaration(clang_ast_context, module_name, *union_declaration);
                clang_declarations.union_declarations.emplace(declaration_name, record_declaration);
            }
            else
            {
                record_declaration = union_location->second;
            }

            for (Type_reference const& member_type : union_declaration->member_types)
                visit_type_references_recursively(member_type, ensure_nested_custom_type_declarations);

            if (!record_declaration->isCompleteDefinition())
                add_clang_union_definition(clang_declarations.union_declarations, clang_ast_context, *union_declaration, declaration_database, clang_declaration_database);

            return;
        }

        if (std::holds_alternative<iris::Type_constructor const*>(declaration->data))
        {
            // Type constructors are templates, not concrete types — no Clang representation needed.
            return;
        }

        throw std::runtime_error{ std::format("'{}.{}' is not a type declaration", module_name, declaration_name) };
    }

    llvm::Type* convert_type_on_demand(
        Clang_context const& clang_context,
        Declaration_database const& declaration_database,
        std::string_view const module_name,
        std::string_view const declaration_name
    )
    {
        Clang_declaration_database transient_clang_declaration_database;

        ensure_clang_custom_type_declaration(
            clang_context.ast_context,
            declaration_database,
            transient_clang_declaration_database,
            module_name,
            declaration_name
        );

        Type_reference const type_reference = create_custom_type_reference(module_name, declaration_name);
        std::optional<clang::QualType> const clang_type = create_type(
            clang_context.ast_context,
            type_reference,
            true,
            declaration_database,
            transient_clang_declaration_database
        );

        if (!clang_type.has_value())
            throw std::runtime_error{ std::format("Could not create clang type '{}.{}'", module_name, declaration_name) };

        return clang::CodeGen::convertTypeForMemory(clang_context.code_generator->CGM(), *clang_type);
    }

    llvm::Type* convert_type_instance_on_demand(
        Clang_context const& clang_context,
        Declaration_database const& declaration_database,
        iris::Type_instance const& type_instance
    )
    {
        Clang_declaration_database transient_clang_declaration_database;

        iris::Declaration_instance_storage storage = iris::instantiate_type_instance(declaration_database, type_instance);
        if (!std::holds_alternative<iris::Struct_declaration>(storage.data))
            throw std::runtime_error{"convert_type_instance_on_demand: expected Struct_declaration"};

        iris::Struct_declaration const& struct_declaration = std::get<iris::Struct_declaration>(storage.data);
        std::string_view const transient_module_name = type_instance.type_constructor.module_reference.name;

        auto const ensure_member_type = [&](iris::Type_reference const& type_ref) -> bool
        {
            if (!std::holds_alternative<iris::Custom_type_reference>(type_ref.data))
                return false;
            iris::Custom_type_reference const& ctr = std::get<iris::Custom_type_reference>(type_ref.data);
            ensure_clang_custom_type_declaration(
                clang_context.ast_context,
                declaration_database,
                transient_clang_declaration_database,
                ctr.module_reference.name,
                ctr.name
            );
            return false;
        };
        for (iris::Type_reference const& member_type : struct_declaration.member_types)
            iris::visit_type_references_recursively(member_type, ensure_member_type);

        Clang_module_declarations& transient_clang_declarations = transient_clang_declaration_database.map
            .emplace(transient_module_name, Clang_module_declarations{}).first->second;

        clang::RecordDecl* record_declaration = nullptr;
        if (!transient_clang_declarations.struct_declarations.contains(struct_declaration.name))
        {
            record_declaration = create_clang_struct_declaration(clang_context.ast_context, transient_module_name, struct_declaration);
            transient_clang_declarations.struct_declarations.emplace(struct_declaration.name, record_declaration);
            set_clang_struct_definition(clang_context.ast_context, *record_declaration, struct_declaration,
                declaration_database, transient_clang_declaration_database);
        }
        else
        {
            record_declaration = transient_clang_declarations.struct_declarations.at(struct_declaration.name);
        }

        clang::QualType const qual_type = clang_context.ast_context.getCanonicalTypeDeclType(record_declaration);
        return clang::CodeGen::convertTypeForMemory(clang_context.code_generator->CGM(), qual_type);
    }

    llvm::Type* convert_type(
        Clang_module_data const& clang_module_data,
        clang::RecordDecl* const record_declaration
    )
    {
        clang::QualType const qual_type = clang_module_data.ast_context.getCanonicalTypeDeclType(record_declaration);
        llvm::Type* const clang_type = clang::CodeGen::convertTypeForMemory(clang_module_data.code_generator->CGM(), qual_type);
        return clang_type;
    }

    llvm::FunctionType* convert_function_type(
        Clang_module_data const& clang_module_data,
        clang::FunctionDecl* const function_declaration
    )
    {
        return clang::CodeGen::convertFreeFunctionType(clang_module_data.code_generator->CGM(), function_declaration);
    }

    clang::RecordDecl* get_record_declaration(
        std::string_view const module_name,
        std::string_view const declaration_name,
        Clang_declaration_database const& clang_declaration_database
    )
    {
        {
            auto const clang_declarations_location = clang_declaration_database.map.find(module_name);
            if (clang_declarations_location != clang_declaration_database.map.end())
            {
                Clang_module_declarations const& clang_declarations = clang_declarations_location->second;

                auto const location = clang_declarations.struct_declarations.find(declaration_name);
                if (location != clang_declarations.struct_declarations.end())
                {
                    clang::RecordDecl* const record_declaration = location->second;
                    return record_declaration;
                }
            }
        }

        return nullptr;
    }

    std::pmr::vector<Clang_struct_member_info> get_clang_struct_member_infos(
        Clang_module_data const& clang_module_data,
        std::string_view const module_name,
        Struct_declaration const& struct_declaration,
        std::pmr::polymorphic_allocator<> const& output_allocator
    )
    {
        std::pmr::vector<Clang_struct_member_info> output{output_allocator};
        output.reserve(struct_declaration.member_names.size());

        clang::CodeGen::CodeGenModule& code_gen_module = clang_module_data.code_generator->CGM();
        clang::CodeGen::CodeGenTypes& code_gen_types = code_gen_module.getTypes();

        clang::RecordDecl* const record_declaration = get_record_declaration(
            module_name,
            struct_declaration.name,
            clang_module_data.declaration_database
        );
        if (record_declaration == nullptr)
            throw std::runtime_error{ "Cannot find struct record." };

        auto field_location = record_declaration->field_begin();

        for (std::size_t member_index = 0; member_index < struct_declaration.member_names.size(); ++member_index)
        {
            clang::FieldDecl* const field_declaration = *field_location;

            unsigned const llvm_struct_member_index = clang::CodeGen::getLLVMFieldNumber(code_gen_module, record_declaration, field_declaration);

            if (field_declaration->isBitField())
            {
                clang::CodeGen::CGRecordLayout const& record_layout = code_gen_types.getCGRecordLayout(record_declaration);
                clang::CodeGen::CGBitFieldInfo const& bit_field_info = record_layout.getBitFieldInfo(field_declaration);

                unsigned const bit_field_offset_in_bits = bit_field_info.Offset;
                unsigned const bit_field_size_in_bits = bit_field_info.Size;

                Clang_struct_member_info const member_info =
                {
                    .llvm_struct_member_index = llvm_struct_member_index,
                    .bit_field_info = Clang_struct_member_bit_field_info
                    {
                        .bit_field_offset_in_bits = static_cast<std::uint32_t>(bit_field_offset_in_bits),
                        .bit_field_size_in_bits = static_cast<std::uint32_t>(bit_field_size_in_bits),
                    },
                };
                
                output.push_back(member_info);
            }
            else
            {
                unsigned const llvm_struct_member_index = clang::CodeGen::getLLVMFieldNumber(code_gen_module, record_declaration, field_declaration);

                Clang_struct_member_info const member_info =
                {
                    .llvm_struct_member_index = llvm_struct_member_index,
                };
                
                output.push_back(member_info);
            }

            ++field_location;
        }

        return output;
    }

    Value_and_type generate_load_struct_member_instructions(
        Clang_module_data const& clang_module_data,
        llvm::LLVMContext& llvm_context,
        llvm::IRBuilder<>& llvm_builder,
        llvm::DataLayout const& llvm_data_layout,
        llvm::Value* const struct_alloca,
        std::string_view const access_member_name,
        std::string_view const module_name,
        Struct_declaration const& struct_declaration,
        Type_database const& type_database
    )
    {
        auto const member_location = std::find(struct_declaration.member_names.begin(), struct_declaration.member_names.end(), access_member_name);
        if (member_location == struct_declaration.member_names.end())
            throw std::runtime_error{ std::format("'{}' does not exist in struct type '{}'.", access_member_name, struct_declaration.name) };

        unsigned const member_index = static_cast<unsigned>(std::distance(struct_declaration.member_names.begin(), member_location));

        Type_reference const& member_type = struct_declaration.member_types[member_index];
        llvm::Type* const member_llvm_type = type_reference_to_llvm_type(llvm_context, llvm_data_layout, member_type, type_database);

        clang::CodeGen::CodeGenModule& code_gen_module = clang_module_data.code_generator->CGM();
        clang::CodeGen::CodeGenTypes& code_gen_types = code_gen_module.getTypes();
        
        clang::RecordDecl* const record_declaration = get_record_declaration(
            module_name,
            struct_declaration.name,
            clang_module_data.declaration_database
        );
        if (record_declaration == nullptr)
            throw std::runtime_error{ "Cannot find struct record." };

        auto field_location = record_declaration->field_begin();
        for (std::uint32_t index = 0; index < member_index; ++index)
            ++field_location;    
        clang::FieldDecl* const field_declaration = *field_location;
        
        llvm::Type* const struct_llvm_type = type_reference_to_llvm_type(
            llvm_context,
            llvm_data_layout,
            create_custom_type_reference(module_name, struct_declaration.name),
            type_database
        );
        if (struct_llvm_type == nullptr)
            throw std::runtime_error{ std::format("Cannot find llvm struct type for '{}'.", struct_declaration.name) };

        unsigned const llvm_struct_member_index = clang::CodeGen::getLLVMFieldNumber(code_gen_module, record_declaration, field_declaration);
        
        if (field_declaration->isBitField())
        {
            clang::CodeGen::CGRecordLayout const& record_layout = code_gen_types.getCGRecordLayout(record_declaration);
            clang::CodeGen::CGBitFieldInfo const& bit_field_info = record_layout.getBitFieldInfo(field_declaration);

            std::array<llvm::Value*, 2> const indices
            {
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(llvm_context), 0),
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(llvm_context), llvm_struct_member_index),
            };

            llvm::Value* const get_element_pointer_instruction = llvm_builder.CreateGEP(struct_llvm_type, struct_alloca, indices, "", true);

            std::uint64_t const storage_size_in_bits = 8*llvm_data_layout.getTypeAllocSize(member_llvm_type);
            llvm::Type* const member_storage_llvm_type = member_llvm_type;
            llvm::Value* const loaded_value = llvm_builder.CreateLoad(member_storage_llvm_type, get_element_pointer_instruction);

            unsigned const bit_field_offset = bit_field_info.Offset;
            unsigned const bit_field_size = bit_field_info.Size;
            bool const is_signed = bit_field_info.IsSigned == 1;

            if (is_signed)
            {
                std::uint64_t const bits_to_shift_left = storage_size_in_bits - (bit_field_offset + bit_field_size);
                llvm::Value* const shift_left_value = llvm::ConstantInt::get(member_storage_llvm_type, bits_to_shift_left, is_signed);
                llvm::Value* const left_shifted_value = llvm_builder.CreateShl(loaded_value, shift_left_value);

                std::uint64_t const bits_to_shift_right = bit_field_offset + bits_to_shift_left;
                llvm::Value* const shift_right_value = llvm::ConstantInt::get(member_storage_llvm_type, bits_to_shift_right, is_signed);
                llvm::Value* const right_shifted_value = llvm_builder.CreateAShr(left_shifted_value, shift_right_value);

                return
                {
                    .name = "",
                    .value = right_shifted_value,
                    .type = member_type
                };
            }
            else
            {
                std::uint64_t const bits_to_shift_right = bit_field_offset;
                llvm::Value* const shift_right_value = llvm::ConstantInt::get(member_storage_llvm_type, bits_to_shift_right, is_signed);
                llvm::Value* const right_shifted_value = llvm_builder.CreateLShr(loaded_value, shift_right_value);

                std::uint64_t const bit_mask = (1 << bit_field_size) - 1;
                llvm::Value* const mask_value = llvm::ConstantInt::get(member_storage_llvm_type, bit_mask, is_signed);
                llvm::Value* const masked_value = llvm_builder.CreateAnd(right_shifted_value, mask_value);

                return
                {
                    .name = "",
                    .value = masked_value,
                    .type = member_type
                };
            }
        }
        else
        {
            std::array<llvm::Value*, 2> const indices
            {
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(llvm_context), 0),
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(llvm_context), llvm_struct_member_index),
            };

            llvm::Value* const get_element_pointer_instruction = llvm_builder.CreateGEP(struct_llvm_type, struct_alloca, indices, "", true);
            
            return
            {
                .name = "",
                .value = get_element_pointer_instruction,
                .type = member_type
            };
        }
    }

    Value_and_type generate_store_struct_member_instructions(
        Clang_module_data const& clang_module_data,
        llvm::LLVMContext& llvm_context,
        llvm::IRBuilder<>& llvm_builder,
        llvm::DataLayout const& llvm_data_layout,
        llvm::Value* const struct_alloca,
        std::string_view const access_member_name,
        std::string_view const module_name,
        Struct_declaration const& struct_declaration,
        Value_and_type const& value_to_store,
        Type_database const& type_database
    )
    {
        auto const member_location = std::find(struct_declaration.member_names.begin(), struct_declaration.member_names.end(), access_member_name);
        if (member_location == struct_declaration.member_names.end())
            throw std::runtime_error{ std::format("'{}' does not exist in struct type '{}'.", access_member_name, struct_declaration.name) };

        unsigned const member_index = static_cast<unsigned>(std::distance(struct_declaration.member_names.begin(), member_location));

        Type_reference const& member_type = struct_declaration.member_types[member_index];
        llvm::Type* const member_llvm_type = type_reference_to_llvm_type(llvm_context, llvm_data_layout, member_type, type_database);

        clang::CodeGen::CodeGenModule& code_gen_module = clang_module_data.code_generator->CGM();
        clang::CodeGen::CodeGenTypes& code_gen_types = code_gen_module.getTypes();
        
        clang::RecordDecl* const record_declaration = get_record_declaration(
            module_name,
            struct_declaration.name,
            clang_module_data.declaration_database
        );
        if (record_declaration == nullptr)
            throw std::runtime_error{ "Cannot find struct record." };

        auto field_location = record_declaration->field_begin();
        for (std::uint32_t index = 0; index < member_index; ++index)
            ++field_location;    
        clang::FieldDecl* const field_declaration = *field_location;
        
        llvm::Type* const struct_llvm_type = type_reference_to_llvm_type(
            llvm_context,
            llvm_data_layout,
            create_custom_type_reference(module_name, struct_declaration.name),
            type_database
        );
        if (struct_llvm_type == nullptr)
            throw std::runtime_error{ std::format("Cannot find llvm struct type for '{}'.", struct_declaration.name) };

        unsigned const llvm_struct_member_index = clang::CodeGen::getLLVMFieldNumber(code_gen_module, record_declaration, field_declaration);

        if (field_declaration->isBitField())
        {
            clang::CodeGen::CGRecordLayout const& record_layout = code_gen_types.getCGRecordLayout(record_declaration);
            clang::CodeGen::CGBitFieldInfo const& bit_field_info = record_layout.getBitFieldInfo(field_declaration);

            std::array<llvm::Value*, 2> const indices
            {
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(llvm_context), 0),
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(llvm_context), llvm_struct_member_index),
            };

            llvm::Value* const get_element_pointer_instruction = llvm_builder.CreateGEP(struct_llvm_type, struct_alloca, indices, "", true);

            llvm::Type* const member_storage_llvm_type = member_llvm_type;

            llvm::Value* const loaded_value = llvm_builder.CreateLoad(member_storage_llvm_type, get_element_pointer_instruction);

            unsigned const bit_field_offset = bit_field_info.Offset;
            unsigned const bit_field_size = bit_field_info.Size;
            bool const is_signed = bit_field_info.IsSigned == 1;

            std::uint64_t const bit_mask = ((1 << bit_field_size) - 1) << bit_field_offset;
            std::uint64_t const reset_bit_mask = ~bit_mask;
            llvm::Value* const reset_mask_value = llvm::ConstantInt::get(member_storage_llvm_type, reset_bit_mask, is_signed);
            llvm::Value* const reset_loaded_value = llvm_builder.CreateAnd(loaded_value, reset_mask_value);

            std::uint64_t const bits_to_shift_left = bit_field_offset;
            llvm::Value* const shift_left_value = llvm::ConstantInt::get(member_storage_llvm_type, bits_to_shift_left, is_signed);
            llvm::Value* const left_shifted_value = llvm_builder.CreateShl(value_to_store.value, shift_left_value);

            llvm::Value* const mask_value = llvm::ConstantInt::get(member_storage_llvm_type, bit_mask, is_signed);
            llvm::Value* const masked_value = llvm_builder.CreateAnd(left_shifted_value, mask_value);

            llvm::Value* const new_value_to_store = llvm_builder.CreateOr(reset_loaded_value, masked_value);
            
            llvm::Value* const store_instruction = create_store_instruction(llvm_builder, llvm_data_layout, new_value_to_store, get_element_pointer_instruction);

            return
            {
                .name = "",
                .value = store_instruction,
                .type = std::nullopt
            };
        }
        else
        {
            std::array<llvm::Value*, 2> const indices
            {
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(llvm_context), 0),
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(llvm_context), llvm_struct_member_index),
            };

            llvm::Value* const get_element_pointer_instruction = llvm_builder.CreateGEP(struct_llvm_type, struct_alloca, indices, "", true);
            llvm::Value* const store_instruction = create_store_instruction(llvm_builder, llvm_data_layout, value_to_store.value, get_element_pointer_instruction);
            
            return
            {
                .name = "",
                .value = store_instruction,
                .type = std::nullopt
            };
        }
    }

    std::optional<clang::QualType> create_type(
        clang::ASTContext& clang_ast_context,
        iris::Type_reference const& type_reference,
        bool const alloca_type,
        Declaration_database const& declaration_database,
        Clang_declaration_database const& clang_declaration_database
    )
    {
        if (std::holds_alternative<iris::Array_slice_type>(type_reference.data))
        {
            auto const module_declarations_location = clang_declaration_database.map.find("iris.builtin");
            if (module_declarations_location == clang_declaration_database.map.end())
                throw std::runtime_error{"Cannot find Builtin module!"};
            Clang_module_declarations const& module_declarations = module_declarations_location->second;

            auto const array_slice_type_location = module_declarations.struct_declarations.find("Generic_array_slice");
            if (array_slice_type_location == module_declarations.struct_declarations.end())
                throw std::runtime_error{"Cannot find Builtin.Generic_array_slice!"};

            clang::RecordDecl* const record_declaration = array_slice_type_location->second;
            return clang_ast_context.getCanonicalTypeDeclType(record_declaration);
        }
        else if (std::holds_alternative<iris::Builtin_type_reference>(type_reference.data))
        {
            iris::Builtin_type_reference const& builtin_type = std::get<iris::Builtin_type_reference>(type_reference.data);
            if (builtin_type.value == "__builtin_va_list")
                return clang_ast_context.getBuiltinVaListType();
        }
        else if (std::holds_alternative<iris::Constant_array_type>(type_reference.data))
        {
            iris::Constant_array_type const& constant_array_type = std::get<iris::Constant_array_type>(type_reference.data);

            if (constant_array_type.value_type.empty())
                throw std::runtime_error{"Cannot create constant array type if value_type is not specified."};

            std::optional<clang::QualType> const element_type = create_type(
                clang_ast_context,
                constant_array_type.value_type[0],
                true,
                declaration_database,
                clang_declaration_database
            );
            if (!element_type.has_value())
                throw std::runtime_error{"Cannot create constant array type. Failed to create element type."};

            llvm::APInt const array_size_value(64, constant_array_type.size, false);

            return clang_ast_context.getConstantArrayType(*element_type, array_size_value, nullptr, clang::ArraySizeModifier::Normal, 0);
        }
        else if (std::holds_alternative<iris::Soa_array_type>(type_reference.data))
        {
            return create_clang_soa_array_type(clang_ast_context);
        }
        else if (std::holds_alternative<iris::Soa_array_view_type>(type_reference.data))
        {
            return create_clang_soa_array_view_type(clang_ast_context);
        }
        else if (std::holds_alternative<iris::Decimal_type>(type_reference.data))
        {
            iris::Decimal_type const decimal_type = std::get<iris::Decimal_type>(type_reference.data);
            std::uint32_t const number_of_bits = get_decimal_size_in_bits(decimal_type.scale);
            return clang_ast_context.getIntTypeForBitwidth(number_of_bits, 1);
        }
        else if (std::holds_alternative<iris::Fundamental_type>(type_reference.data))
        {
            iris::Fundamental_type const fundamental_type = std::get<iris::Fundamental_type>(type_reference.data);
            switch (fundamental_type)
            {
                case iris::Fundamental_type::Bool:
                case iris::Fundamental_type::C_bool: {
                    return alloca_type ? clang_ast_context.getIntTypeForBitwidth(8, 0) : clang_ast_context.BoolTy;
                }
                case iris::Fundamental_type::Byte: {
                    return clang_ast_context.getIntTypeForBitwidth(8, 0);
                }
                case iris::Fundamental_type::Float16: {
                    return clang_ast_context.getRealTypeForBitwidth(16, clang::FloatModeKind::Half);
                }
                case iris::Fundamental_type::Float32: {
                    return clang_ast_context.getRealTypeForBitwidth(32, clang::FloatModeKind::Float);
                }
                case iris::Fundamental_type::Float64: {
                    return clang_ast_context.getRealTypeForBitwidth(64, clang::FloatModeKind::Double);
                }
                case iris::Fundamental_type::String: {
                    Clang_module_declarations const& clang_declarations = clang_declaration_database.map.find(c_builtin_module_name)->second;
                    clang::RecordDecl* const record_declaration = clang_declarations.struct_declarations.at("String");

                    return clang_ast_context.getCanonicalTypeDeclType(record_declaration);
                }
                case iris::Fundamental_type::Any_type: {
                    return clang_ast_context.VoidPtrTy;
                }
                case iris::Fundamental_type::C_char: {
                    return clang_ast_context.CharTy;
                }
                case iris::Fundamental_type::C_schar: {
                    return clang_ast_context.SignedCharTy;
                }
                case iris::Fundamental_type::C_uchar: {
                    return clang_ast_context.UnsignedCharTy;
                }
                case iris::Fundamental_type::C_short: {
                    return clang_ast_context.ShortTy;
                }
                case iris::Fundamental_type::C_ushort: {
                    return clang_ast_context.UnsignedShortTy;
                }
                case iris::Fundamental_type::C_int: {
                    return clang_ast_context.IntTy;
                }
                case iris::Fundamental_type::C_uint: {
                    return clang_ast_context.UnsignedIntTy;
                }
                case iris::Fundamental_type::C_long: {
                    return clang_ast_context.LongTy;
                }
                case iris::Fundamental_type::C_ulong: {
                    return clang_ast_context.UnsignedLongTy;
                }
                case iris::Fundamental_type::C_longlong: {
                    return clang_ast_context.LongLongTy;
                }
                case iris::Fundamental_type::C_ulonglong: {
                    return clang_ast_context.UnsignedLongLongTy;
                }
                case iris::Fundamental_type::C_longdouble: {
                    return clang_ast_context.LongDoubleTy;
                }
            }
        }
        else if (std::holds_alternative<iris::Function_pointer_type>(type_reference.data))
        {
            iris::Function_pointer_type const& function_pointer_type = std::get<iris::Function_pointer_type>(type_reference.data);

            clang::QualType const function_proto_type = create_clang_function_proto_type(
                clang_ast_context,
                function_pointer_type.type,
                declaration_database,
                clang_declaration_database
            );

            return clang_ast_context.getPointerType(function_proto_type);
        }
        else if (std::holds_alternative<iris::Integer_type>(type_reference.data))
        {
            iris::Integer_type const integer_type = std::get<iris::Integer_type>(type_reference.data);

            /*auto const get_number_of_bits = [](iris::Integer_type const& integer_type) -> unsigned int
            {
                if (integer_type.number_of_bits == 8 || integer_type.number_of_bits == 16 || integer_type.number_of_bits == 32 || integer_type.number_of_bits == 64)
                    return integer_type.number_of_bits;
                else if (integer_type.number_of_bits <= 32) // TODO we assume that bitfields are always packed into 32-bit integers (except when number of bits is greater than 32)
                    return 32;
                else
                    return 64;
            };

            unsigned int const number_of_bits = get_number_of_bits(integer_type);*/

            return clang_ast_context.getIntTypeForBitwidth(integer_type.number_of_bits, integer_type.is_signed ? 1 : 0);
        }
        else if (std::holds_alternative<iris::Type_instance>(type_reference.data))
        {
            iris::Type_instance const& type_instance = std::get<iris::Type_instance>(type_reference.data);
            std::pmr::string const mangled_name = mangle_type_instance_name(type_instance);
            std::string_view const tc_module = type_instance.type_constructor.module_reference.name;

            auto const module_location = clang_declaration_database.map.find(tc_module);
            if (module_location == clang_declaration_database.map.end())
                return std::nullopt;

            auto const struct_location = module_location->second.struct_declarations.find(mangled_name);
            if (struct_location == module_location->second.struct_declarations.end())
                return std::nullopt;

            return clang_ast_context.getCanonicalTypeDeclType(struct_location->second);
        }
        else if (std::holds_alternative<iris::Custom_type_reference>(type_reference.data))
        {
            iris::Custom_type_reference const custom_type_reference = std::get<iris::Custom_type_reference>(type_reference.data);
            std::optional<iris::Declaration> const declaration = iris::find_declaration(
                declaration_database,
                custom_type_reference.module_reference.name,
                custom_type_reference.name
            );

            if (declaration.has_value())
            {
                if (std::holds_alternative<iris::Alias_type_declaration const*>(declaration->data))
                {
                    Clang_module_declarations const& clang_declarations = clang_declaration_database.map.at(custom_type_reference.module_reference.name);
                    clang::TypedefDecl* const typedef_declaration = clang_declarations.alias_type_declarations.at(custom_type_reference.name);

                    return clang_ast_context.getCanonicalTypeDeclType(typedef_declaration);
                }
                else if (std::holds_alternative<iris::Enum_declaration const*>(declaration->data))
                {
                    Clang_module_declarations const& clang_declarations = clang_declaration_database.map.at(custom_type_reference.module_reference.name);
                    clang::EnumDecl* const enum_declaration = clang_declarations.enum_declarations.at(custom_type_reference.name);

                    return clang_ast_context.getCanonicalTypeDeclType(enum_declaration);
                }
                else if (std::holds_alternative<iris::Struct_declaration const*>(declaration->data))
                {
                    Clang_module_declarations const& clang_declarations = clang_declaration_database.map.at(custom_type_reference.module_reference.name);
                    auto const location = clang_declarations.struct_declarations.find(custom_type_reference.name);
                    if (location == clang_declarations.struct_declarations.end())
                        return std::nullopt;
                    
                    clang::RecordDecl* const record_declaration = location->second;
                    return clang_ast_context.getCanonicalTypeDeclType(record_declaration);
                }
                else if (std::holds_alternative<iris::Union_declaration const*>(declaration->data))
                {
                    Clang_module_declarations const& clang_declarations = clang_declaration_database.map.at(custom_type_reference.module_reference.name);
                    clang::RecordDecl* const record_declaration = clang_declarations.union_declarations.at(custom_type_reference.name);

                    return clang_ast_context.getCanonicalTypeDeclType(record_declaration);
                }
            }
        }
        else if (std::holds_alternative<iris::Pointer_type>(type_reference.data))
        {
            iris::Pointer_type const pointer_type = std::get<iris::Pointer_type>(type_reference.data);
            if (pointer_type.element_type.empty())
                return clang_ast_context.getPointerType(clang_ast_context.VoidTy);

            std::optional<clang::QualType> const element_type = create_type(
                clang_ast_context,
                pointer_type.element_type[0],
                true,
                declaration_database,
                clang_declaration_database
            );
            if (element_type.has_value())
                return clang_ast_context.getPointerType(*element_type);
            else
                return clang_ast_context.getPointerType(clang_ast_context.VoidTy);
        }

        return std::nullopt;
    }

    std::optional<clang::QualType> create_type(
        clang::ASTContext& clang_ast_context,
        std::span<iris::Type_reference const> const type_reference,
        bool const alloca_type,
        Declaration_database const& declaration_database,
        Clang_declaration_database const& clang_declaration_database
    )
    {
        if (type_reference.size() == 0) {
            return clang_ast_context.VoidTy;
        }

        return create_type(clang_ast_context, type_reference[0], alloca_type, declaration_database, clang_declaration_database);
    }

    void destroy_clang_data(Clang_data* data)
    {
        delete data;
    }

    Clang_data_pointer create_clang_data(
        llvm::LLVMContext& llvm_context,
        llvm::Triple const& llvm_triple,
        unsigned int const optimization_level
    )
    {
        std::unique_ptr<clang::CompilerInstance> compiler_instance = std::make_unique<clang::CompilerInstance>();

        compiler_instance->createDiagnostics();

        std::unique_ptr<clang::TargetOptions> target_options = std::make_unique<clang::TargetOptions>();
        target_options->Triple = llvm_triple.str();
        clang::TargetInfo* target_info = clang::TargetInfo::CreateTargetInfo(compiler_instance->getDiagnostics(), *target_options.get());
        compiler_instance->setTarget(target_info);

        compiler_instance->createVirtualFileSystem();
        compiler_instance->createFileManager();
        compiler_instance->createSourceManager();

        clang::LangOptions language_options;
        std::vector<std::string> language_option_includes;
        clang::LangOptions::setLangDefaults(language_options, clang::Language::C, llvm_triple, language_option_includes, clang::LangStandard::Kind::lang_c17);

        compiler_instance->createPreprocessor(clang::TU_Complete);
        compiler_instance->getPreprocessorOpts().UsePredefines = false;

        compiler_instance->createASTContext();

        Clang_data_pointer output
        (
            new Clang_data
            {
                .compiler_instance = std::move(compiler_instance),
                .target_options = std::move(target_options),
            },
            destroy_clang_data
        );

        return output;
    }

    void destroy_clang_module_data(Clang_module_data* data)
    {
        delete data;
    }

    clang::CompilerInstance& get_compiler_instance(Clang_data const& clang_data)
    {
        return *clang_data.compiler_instance.get();
    }

    Clang_declaration_database& get_clang_declaration_database(Clang_module_data& clang_module_data)
    {
        return clang_module_data.declaration_database;
    }

    Clang_declaration_database const& get_clang_declaration_database(Clang_module_data const& clang_module_data)
    {
        return clang_module_data.declaration_database;
    }

    clang::ASTContext& get_clang_ast_context(Clang_module_data& clang_module_data)
    {
        return clang_module_data.ast_context;
    }

    clang::ASTContext const& get_clang_ast_context(Clang_module_data const& clang_module_data)
    {
        return clang_module_data.ast_context;
    }

    llvm::Function& to_function(
        llvm::LLVMContext& llvm_context,
        llvm::DataLayout const& llvm_data_layout,
        Clang_module_data const& clang_module_data,
        std::string_view const module_name,
        llvm::FunctionType& llvm_function_type,
        Function_declaration const& function_declaration,
        Type_database const& type_database,
        Declaration_database const& declaration_database
    )
    {
        llvm::GlobalValue::LinkageTypes const linkage = to_linkage(function_declaration.linkage, function_declaration.is_test);

        std::string const mangled_name = mangle_name(module_name, function_declaration.name, function_declaration.unique_name);

        llvm::Function* const llvm_function = llvm::Function::Create(
            &llvm_function_type,
            linkage,
            mangled_name.c_str(),
            nullptr
        );

        if (!llvm_function)
        {
            throw std::runtime_error{ "Could not create function." };
        }

        set_llvm_function_argument_names(
            llvm_context,
            llvm_data_layout,
            clang_module_data,
            function_declaration,
            *llvm_function,
            declaration_database,
            type_database
        );

        llvm_function->setCallingConv(llvm::CallingConv::C);

        set_function_definition_attributes(llvm_context, clang_module_data, *llvm_function);

        return *llvm_function;
    }
}
