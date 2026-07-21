export module iris.parser.convertor;

import std;

import iris.core;
import iris.parser.parse_tree;

namespace iris::parser
{
    export struct Module_info
    {
        std::string_view module_name;
        std::optional<std::filesystem::path> source_file_path;
        std::span<Import_module_with_alias const> alias_imports;
    };

    export Module_info create_module_info(
        iris::Module const& core_module
    );

    export std::optional<iris::Module> parse_and_convert_to_module(
        std::string_view const source,
        std::optional<std::filesystem::path> const& source_file_path,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::optional<iris::Module> parse_and_convert_to_module(
        std::filesystem::path const& source_file_path,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::optional<iris::Module> parse_node_to_module(
        Parse_tree const& tree,
        Parse_node const& node,
        std::optional<std::filesystem::path> const& source_file_path,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    std::pmr::vector<Import_module_with_alias> create_import_modules(
        Parse_tree const& tree,
        Parse_node const& module_node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::optional<Import_module_with_alias> node_to_import_module_with_alias(
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    bool is_variadic(
        Parse_tree const& tree,
        std::span<Parse_node const> const parameter_nodes
    );

    std::optional<iris::Type_reference> node_to_type_reference(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    void node_to_declaration(
        iris::Module& core_module,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    iris::Alias_type_declaration node_to_alias_type_declaration(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::optional<std::string_view> const unique_name,
        std::optional<std::pmr::string> const& comment,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    iris::Enum_declaration node_to_enum_declaration(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::optional<std::string_view> const unique_name,
        std::optional<std::pmr::string> const& comment,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    iris::Function_declaration node_to_function_declaration(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        iris::Linkage const linkage,
        std::optional<std::string_view> const& unique_name,
        bool const is_test,
        std::optional<std::pmr::string> const& comment,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    iris::Function_definition node_to_function_definition(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::string_view const function_name,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    iris::Function_constructor node_to_function_constructor_declaration(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::optional<std::pmr::string> const& comment,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    iris::Global_variable_declaration node_to_global_variable_declaration(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::optional<std::string_view> const unique_name,
        std::optional<std::pmr::string> const& comment,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    iris::Struct_declaration node_to_struct_declaration(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::optional<std::string_view> const unique_name,
        std::optional<std::pmr::string> const& comment,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    iris::Type_constructor node_to_type_constructor_declaration(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::optional<std::pmr::string> const& comment,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    iris::Union_declaration node_to_union_declaration(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::optional<std::string_view> const unique_name,
        std::optional<std::pmr::string> const& comment,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    iris::Statement node_to_statement(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    std::pmr::vector<iris::Statement> node_to_block(
        Module_info const& module_info,
        Parse_tree const& tree,
        std::span<Parse_node const> const statement_nodes,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    std::pmr::vector<iris::Statement> node_to_block(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export iris::Expression_index node_to_expression(
        iris::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    iris::Access_expression node_to_expression_access(
        iris::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    iris::Access_array_expression node_to_expression_access_array(
        iris::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    iris::Assert_expression node_to_expression_assert(
        iris::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    iris::Assignment_expression node_to_expression_assignment(
        iris::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    iris::Binary_expression node_to_expression_binary(
        iris::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    iris::Block_expression node_to_expression_block(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    iris::Break_expression node_to_expression_break(
        Parse_tree const& tree,
        Parse_node const& node
    );

    iris::Call_expression node_to_expression_call(
        iris::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    iris::Cast_expression node_to_expression_cast(
        iris::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    iris::Compile_time_expression node_to_expression_compile_time(
        iris::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    iris::Constant_expression node_to_expression_constant(
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator
    );

    iris::Constant_array_expression node_to_expression_constant_array(
        iris::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    iris::Comment_expression node_to_expression_comment(
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    iris::Continue_expression node_to_expression_continue(
        Parse_tree const& tree,
        Parse_node const& node
    );

    iris::Defer_expression node_to_expression_defer(
        iris::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    iris::Dereference_and_access_expression node_to_expression_dereference_and_access(
        iris::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    iris::For_loop_expression node_to_expression_for_loop(
        iris::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    iris::Function_expression node_to_expression_function(
        iris::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    iris::If_expression node_to_expression_if(
        iris::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    iris::Instance_call_expression node_to_expression_instance_call(
        iris::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    iris::Instantiate_expression node_to_expression_instantiate(
        iris::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    iris::Null_pointer_expression node_to_expression_null_pointer();

    iris::Parenthesis_expression node_to_expression_parenthesis(
        iris::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    iris::Reflection_expression node_to_expression_reflection(
        iris::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    iris::Return_expression node_to_expression_return(
        iris::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    iris::Struct_expression node_to_expression_struct(
        iris::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    iris::Switch_case_expression_pair node_to_expression_switch_case(
        iris::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    iris::Switch_expression node_to_expression_switch(
        iris::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    iris::Ternary_condition_expression node_to_expression_ternary_condition(
        iris::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    iris::Type_expression node_to_expression_type(
        iris::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    iris::Unary_expression node_to_expression_unary(
        iris::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    iris::Variable_expression node_to_expression_variable(
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator
    );

    iris::Variable_declaration_expression node_to_expression_variable_declaration(
        iris::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    iris::Variable_declaration_with_type_expression node_to_expression_variable_declaration_with_type(
        iris::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    iris::While_loop_expression node_to_expression_while_loop(
        iris::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );
}
