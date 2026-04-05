module;

#include <filesystem>
#include <memory_resource>
#include <optional>
#include <span>
#include <string_view>

export module h.parser.convertor;

import h.core;
import h.parser.parse_tree;

namespace h::parser
{
    export struct Module_info
    {
        std::string_view module_name;
        std::optional<std::filesystem::path> source_file_path;
        std::span<Import_module_with_alias const> alias_imports;
    };

    export Module_info create_module_info(
        h::Module const& core_module
    );

    export std::optional<h::Module> parse_and_convert_to_module(
        std::string_view const source,
        std::optional<std::filesystem::path> const& source_file_path,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::optional<h::Module> parse_and_convert_to_module(
        std::filesystem::path const& source_file_path,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::optional<h::Module> parse_node_to_module(
        Parse_tree const& tree,
        Parse_node const& node,
        std::optional<std::filesystem::path> const& source_file_path,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    std::pmr::vector<Import_module_with_alias> create_import_modules(
        Parse_tree const& tree,
        std::optional<Parse_node> const& module_head_node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export std::optional<Import_module_with_alias> node_to_import_module_with_alias(
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator
    );

    bool is_variadic(
        Parse_tree const& tree,
        std::span<Parse_node const> const parameter_nodes
    );

    std::optional<h::Type_reference> node_to_type_reference(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    void node_to_declaration(
        h::Module& core_module,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    h::Alias_type_declaration node_to_alias_type_declaration(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::optional<std::string_view> const unique_name,
        std::optional<std::pmr::string> const& comment,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    h::Enum_declaration node_to_enum_declaration(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::optional<std::string_view> const unique_name,
        std::optional<std::pmr::string> const& comment,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    h::Function_declaration node_to_function_declaration(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        h::Linkage const linkage,
        std::optional<std::string_view> const& unique_name,
        bool const is_test,
        std::optional<std::pmr::string> const& comment,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    h::Function_definition node_to_function_definition(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::string_view const function_name,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    h::Function_constructor node_to_function_constructor_declaration(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::optional<std::pmr::string> const& comment,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    h::Global_variable_declaration node_to_global_variable_declaration(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::optional<std::string_view> const unique_name,
        std::optional<std::pmr::string> const& comment,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    h::Struct_declaration node_to_struct_declaration(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::optional<std::string_view> const unique_name,
        std::optional<std::pmr::string> const& comment,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    h::Type_constructor node_to_type_constructor_declaration(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::optional<std::pmr::string> const& comment,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    h::Union_declaration node_to_union_declaration(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::optional<std::string_view> const unique_name,
        std::optional<std::pmr::string> const& comment,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    h::Statement node_to_statement(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    std::pmr::vector<h::Statement> node_to_block(
        Module_info const& module_info,
        Parse_tree const& tree,
        std::span<Parse_node const> const statement_nodes,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    std::pmr::vector<h::Statement> node_to_block(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    export h::Expression_index node_to_expression(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    h::Access_expression node_to_expression_access(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    h::Access_array_expression node_to_expression_access_array(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    h::Assert_expression node_to_expression_assert(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    h::Assignment_expression node_to_expression_assignment(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    h::Binary_expression node_to_expression_binary(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    h::Block_expression node_to_expression_block(
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    h::Break_expression node_to_expression_break(
        Parse_tree const& tree,
        Parse_node const& node
    );

    h::Call_expression node_to_expression_call(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    h::Cast_expression node_to_expression_cast(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    h::Compile_time_expression node_to_expression_compile_time(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    h::Constant_expression node_to_expression_constant(
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator
    );

    h::Constant_array_expression node_to_expression_constant_array(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    h::Comment_expression node_to_expression_comment(
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    h::Continue_expression node_to_expression_continue(
        Parse_tree const& tree,
        Parse_node const& node
    );

    h::Defer_expression node_to_expression_defer(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    h::Dereference_and_access_expression node_to_expression_dereference_and_access(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    h::For_loop_expression node_to_expression_for_loop(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    h::Function_expression node_to_expression_function(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    h::If_expression node_to_expression_if(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    h::Instance_call_expression node_to_expression_instance_call(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    h::Instantiate_expression node_to_expression_instantiate(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    h::Null_pointer_expression node_to_expression_null_pointer();

    h::Parenthesis_expression node_to_expression_parenthesis(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    h::Reflection_expression node_to_expression_reflection(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    h::Return_expression node_to_expression_return(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    h::Struct_expression node_to_expression_struct(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    h::Switch_case_expression_pair node_to_expression_switch_case(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    h::Switch_expression node_to_expression_switch(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    h::Ternary_condition_expression node_to_expression_ternary_condition(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    h::Type_expression node_to_expression_type(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    h::Unary_expression node_to_expression_unary(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    h::Variable_expression node_to_expression_variable(
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator
    );

    h::Variable_declaration_expression node_to_expression_variable_declaration(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    h::Variable_declaration_with_type_expression node_to_expression_variable_declaration_with_type(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );

    h::While_loop_expression node_to_expression_while_loop(
        h::Statement& statement,
        Module_info const& module_info,
        Parse_tree const& tree,
        Parse_node const& node,
        std::pmr::polymorphic_allocator<> const& output_allocator,
        std::pmr::polymorphic_allocator<> const& temporaries_allocator
    );
}
