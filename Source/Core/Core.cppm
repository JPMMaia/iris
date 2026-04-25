module;

#include <compare>

export module iris.core;

import std;



namespace iris
{
    export struct Source_location
    {
        std::optional<std::filesystem::path> file_path;
        std::uint32_t line = 0;
        std::uint32_t column = 0;

        friend bool operator==(Source_location const& lhs, Source_location const& rhs) = default;
    };

    export struct Source_position
    {
        std::uint32_t line = 0;
        std::uint32_t column = 0;

        friend bool operator==(Source_position const& lhs, Source_position const& rhs) = default;
    };

    export struct Source_range
    {
        Source_position start;
        Source_position end;

        friend bool operator==(Source_range const& lhs, Source_range const& rhs) = default;
    };

    export Source_range create_source_range(
        std::uint32_t const start_line,
        std::uint32_t const start_column,
        std::uint32_t const end_line,
        std::uint32_t const end_column
    );

    export std::optional<iris::Source_range> create_sub_source_range(
        std::optional<iris::Source_range> const& source_range,
        std::uint32_t const start_index,
        std::uint32_t const count
    );

    export bool range_contains_position(
        iris::Source_range const& range,
        iris::Source_position const& position
    );

    export bool range_contains_position_inclusive(
        iris::Source_range const& range,
        iris::Source_position const& position
    );

    export struct Source_range_location
    {
        std::optional<std::filesystem::path> file_path;
        Source_range range;

        friend bool operator==(Source_range_location const& lhs, Source_range_location const& rhs) = default;
    };

    export Source_range_location create_source_range_location(
        std::optional<std::filesystem::path> const& file_path,
        std::uint32_t const start_line,
        std::uint32_t const start_column,
        std::uint32_t const end_line,
        std::uint32_t const end_column
    );

    // Comparison operator for sorting Source_range_location objects
    export bool operator<(Source_range_location const& lhs, Source_range_location const& rhs) noexcept;

    export struct Function_declaration;
    export struct Statement;
    export struct Type_reference;
    export struct Expression;



    export enum class Fundamental_type
    {
        Bool,
        Byte,
        Float16,
        Float32,
        Float64,
        String,
        Any_type,

        C_bool,
        C_char,
        C_schar,
        C_uchar,
        C_short,
        C_ushort,
        C_int,
        C_uint,
        C_long,
        C_ulong,
        C_longlong,
        C_ulonglong,
        C_longdouble
    };

    export struct Integer_type
    {
        std::uint32_t number_of_bits;
        bool is_signed;

        friend bool operator==(Integer_type const& lhs, Integer_type const& rhs) = default;
    };

    export struct Decimal_type
    {
        std::uint32_t scale; // 1 to 18; scale N means value is (integer / 10^N)

        friend bool operator==(Decimal_type const& lhs, Decimal_type const& rhs) = default;
    };

    export struct Array_slice_type
    {
        std::pmr::vector<Type_reference> element_type;
        bool is_mutable;

        friend bool operator==(Array_slice_type const& lhs, Array_slice_type const& rhs) = default;
    };

    export struct Builtin_type_reference
    {
        std::pmr::string value;

        friend bool operator==(Builtin_type_reference const& lhs, Builtin_type_reference const& rhs) = default;
    };

    export struct Function_type
    {
        std::pmr::vector<Type_reference> input_parameter_types;
        std::pmr::vector<Type_reference> output_parameter_types;
        bool is_variadic;

        friend bool operator==(Function_type const& lhs, Function_type const& rhs) = default;
    };

    export struct Function_pointer_type
    {
        Function_type type;
        std::pmr::vector<std::pmr::string> input_parameter_names;
        std::pmr::vector<std::pmr::string> output_parameter_names;

        friend bool operator==(Function_pointer_type const& lhs, Function_pointer_type const& rhs) = default;
    };

    export struct Null_pointer_type
    {
        friend bool operator==(Null_pointer_type const& lhs, Null_pointer_type const& rhs) = default;
    };

    export struct Pointer_type
    {
        std::pmr::vector<Type_reference> element_type;
        bool is_mutable;

        friend bool operator==(Pointer_type const& lhs, Pointer_type const& rhs) = default;
    };

    export struct Module_reference
    {
        std::pmr::string name;

        friend bool operator==(Module_reference const&, Module_reference const&) = default;
    };

    export struct Constant_array_type
    {
        std::pmr::vector<Type_reference> value_type;
        std::uint64_t size;

        friend bool operator==(Constant_array_type const&, Constant_array_type const&) = default;
    };

    export struct Soa_array_type
    {
        std::pmr::vector<Type_reference> value_type;
        std::uint64_t size;

        friend bool operator==(Soa_array_type const&, Soa_array_type const&) = default;
    };

    export struct Soa_array_view_type
    {
        std::pmr::vector<Type_reference> value_type;
        bool is_mutable;

        friend bool operator==(Soa_array_view_type const&, Soa_array_view_type const&) = default;
    };

    export struct Custom_type_reference
    {
        Module_reference module_reference;
        std::pmr::string name;

        friend bool operator==(Custom_type_reference const&, Custom_type_reference const&) = default;
    };

    export struct Type_instance
    {
        Custom_type_reference type_constructor;
        std::pmr::vector<Statement> arguments;

        friend bool operator==(Type_instance const& lhs, Type_instance const& rhs) = default;
    };

    export struct Parameter_type
    {
        std::pmr::string name;

        friend bool operator==(Parameter_type const&, Parameter_type const&) = default;
    };

    export struct Type_reference
    {
        using Data_type = std::variant<
            Array_slice_type,
            Builtin_type_reference,
            Constant_array_type,
            Custom_type_reference,
            Decimal_type,
            Fundamental_type,
            Function_pointer_type,
            Integer_type,
            Null_pointer_type,
            Parameter_type,
            Pointer_type,
            Soa_array_type,
            Soa_array_view_type,
            Type_instance
        >;

        Data_type data;
        std::optional<Source_range> source_range;

        friend bool operator==(Type_reference const& lhs, Type_reference const& rhs);
    };



    export struct Indexed_comment
    {
        std::uint64_t index;
        std::pmr::string comment;

        friend bool operator==(Indexed_comment const&, Indexed_comment const&) = default;
    };

    export struct Expression;

    export struct Statement
    {
        std::pmr::vector<Expression> expressions;

        friend bool operator==(Statement const& lhs, Statement const& rhs);
    };


    export enum class Global_variable_type
    {
        Constant = 0,
        Mutable,
        Macro
    };

    export struct Global_variable_declaration
    {
        std::pmr::string name;
        std::optional<std::pmr::string> unique_name;
        std::optional<Type_reference> type;
        Statement initial_value;
        Global_variable_type global_type;
        std::optional<std::pmr::string> comment;
        std::optional<Source_range_location> source_location;

        friend bool operator==(Global_variable_declaration const& lhs, Global_variable_declaration const& rhs) = default;
    };

    export struct Alias_type_declaration
    {
        std::pmr::string name;
        std::optional<std::pmr::string> unique_name;
        std::pmr::vector<Type_reference> type;
        std::optional<std::pmr::string> comment;
        std::optional<Source_range_location> source_location;

        friend bool operator==(Alias_type_declaration const& lhs, Alias_type_declaration const& rhs) = default;
    };

    export struct Enum_value
    {
        std::pmr::string name;
        std::optional<Statement> value;
        std::optional<std::pmr::string> comment;
        std::optional<Source_location> source_location;

        friend bool operator==(Enum_value const& lhs, Enum_value const& rhs) = default;
    };

    export struct Enum_declaration
    {
        std::pmr::string name;
        std::optional<std::pmr::string> unique_name;
        std::pmr::vector<Enum_value> values;
        std::optional<std::pmr::string> comment;
        std::optional<Source_range_location> source_location;

        friend bool operator==(Enum_declaration const& lhs, Enum_declaration const& rhs) = default;
    };

    export struct Forward_declaration
    {
        std::pmr::string name;
        std::optional<std::pmr::string> unique_name;
        std::optional<Source_range_location> source_location;

        friend bool operator==(Forward_declaration const&, Forward_declaration const&) = default;
    };

    export struct Struct_declaration
    {
        std::pmr::string name;
        std::optional<std::pmr::string> unique_name;
        std::pmr::vector<Type_reference> member_types;
        std::pmr::vector<std::pmr::string> member_names;
        std::pmr::vector<std::optional<std::uint32_t>> member_bit_fields;
        std::pmr::vector<Statement> member_default_values;
        bool is_packed;
        bool is_literal;
        std::optional<std::pmr::string> comment;
        std::pmr::vector<Indexed_comment> member_comments;
        std::optional<Source_range_location> source_location;
        std::optional<std::pmr::vector<Source_position>> member_source_positions;

        friend bool operator==(Struct_declaration const&, Struct_declaration const&) = default;
    };

    export struct Union_declaration
    {
        std::pmr::string name;
        std::optional<std::pmr::string> unique_name;
        std::pmr::vector<Type_reference> member_types;
        std::pmr::vector<std::pmr::string> member_names;
        std::optional<std::pmr::string> comment;
        std::pmr::vector<Indexed_comment> member_comments;
        std::optional<Source_range_location> source_location;
        std::optional<std::pmr::vector<Source_position>> member_source_positions;

        friend bool operator==(Union_declaration const&, Union_declaration const&) = default;
    };

    export struct Function_condition
    {
        std::pmr::string description;
        Statement condition;
        std::optional<Source_range> source_range;

        friend bool operator==(Function_condition const& lhs, Function_condition const& rhs);
    };

    export enum class Linkage
    {
        External,
        Private
    };

    export struct Function_declaration
    {
        std::pmr::string name;
        std::optional<std::pmr::string> unique_name;
        Function_type type;
        std::pmr::vector<std::pmr::string> input_parameter_names;
        std::pmr::vector<std::pmr::string> output_parameter_names;
        Linkage linkage;
        bool is_test;
        std::pmr::vector<Function_condition> preconditions;
        std::pmr::vector<Function_condition> postconditions;
        std::optional<std::pmr::string> comment;
        std::optional<Source_range_location> source_location;
        std::optional<std::pmr::vector<Source_position>> input_parameter_source_positions;
        std::optional<std::pmr::vector<Source_position>> output_parameter_source_positions;

        friend bool operator==(Function_declaration const&, Function_declaration const&) = default;
    };

    export struct Function_definition
    {
        std::pmr::string name;
        std::pmr::vector<Statement> statements;
        std::optional<Source_range_location> source_location;

        friend bool operator==(Function_definition const&, Function_definition const&) = default;
    };

    export struct Variable_expression
    {
        std::pmr::string name;

        friend bool operator==(Variable_expression const&, Variable_expression const&) = default;
    };

    export struct Expression_index
    {
        std::uint64_t expression_index = static_cast<std::uint64_t>(-1);

        friend bool operator==(Expression_index const&, Expression_index const&) = default;
    };

    export enum class Binary_operation
    {
        Add,
        Subtract,
        Multiply,
        Divide,
        Modulus,

        Equal,
        Not_equal,
        Less_than,
        Less_than_or_equal_to,
        Greater_than,
        Greater_than_or_equal_to,

        Logical_and,
        Logical_or,

        Bitwise_and,
        Bitwise_or,
        Bitwise_xor,

        Bit_shift_left,
        Bit_shift_right,

        Has
    };

    export bool is_bit_shift_binary_operation(iris::Binary_operation const operation);
    export bool is_bitwise_binary_operation(iris::Binary_operation const operation);
    export bool is_equality_binary_operation(iris::Binary_operation const operation);
    export bool is_comparison_binary_operation(iris::Binary_operation const operation);
    export bool is_logical_binary_operation(iris::Binary_operation const operation);
    export bool is_numeric_binary_operation(iris::Binary_operation const operation);

    export struct Access_expression
    {
        Expression_index expression;
        std::pmr::string member_name;

        friend bool operator==(Access_expression const&, Access_expression const&) = default;
    };

    export struct Access_array_expression
    {
        Expression_index expression;
        Expression_index index;

        friend bool operator==(Access_array_expression const&, Access_array_expression const&) = default;
    };

    export struct Assert_expression
    {
        std::optional<std::pmr::string> message;
        Statement statement;

        friend bool operator==(Assert_expression const&, Assert_expression const&) = default;
    };

    export struct Assignment_expression
    {
        Expression_index left_hand_side;
        Expression_index right_hand_side;
        std::optional<Binary_operation> additional_operation;

        friend bool operator==(Assignment_expression const&, Assignment_expression const&) = default;
    };

    export struct Binary_expression
    {
        Expression_index left_hand_side;
        Expression_index right_hand_side;
        Binary_operation operation;

        friend bool operator==(Binary_expression const&, Binary_expression const&) = default;
    };

    export struct Block_expression
    {
        std::pmr::vector<Statement> statements;

        friend bool operator==(Block_expression const&, Block_expression const&) = default;
    };

    export struct Break_expression
    {
        std::uint64_t loop_count;

        friend bool operator==(Break_expression const&, Break_expression const&) = default;
    };

    export struct Call_expression
    {
        Expression_index expression;
        std::pmr::vector<Expression_index> arguments;

        friend bool operator==(Call_expression const&, Call_expression const&) = default;
    };

    export enum class Cast_type
    {
        Numeric,
        BitCast
    };

    export struct Cast_expression
    {
        Expression_index source;
        Type_reference destination_type;
        Cast_type cast_type;

        friend bool operator==(Cast_expression const&, Cast_expression const&) = default;
    };

    export struct Comment_expression
    {
        std::pmr::string comment;

        friend bool operator==(Comment_expression const&, Comment_expression const&) = default;
    };

    export struct Compile_time_expression
    {
        Expression_index expression;

        friend bool operator==(Compile_time_expression const&, Compile_time_expression const&) = default;
    };

    export struct Constant_expression
    {
        Type_reference type;
        std::pmr::string data;

        friend bool operator==(Constant_expression const&, Constant_expression const&) = default;
    };

    export struct Constant_array_expression
    {
        std::pmr::vector<Statement> array_data;

        friend bool operator==(Constant_array_expression const&, Constant_array_expression const&) = default;
    };

    export struct Continue_expression
    {
        friend bool operator==(Continue_expression const&, Continue_expression const&) = default;
    };

    export struct Defer_expression
    {
        Expression_index expression_to_defer;
        
        friend bool operator==(Defer_expression const&, Defer_expression const&) = default;
    };

    export struct Dereference_and_access_expression
    {
        Expression_index expression;
        std::pmr::string member_name;

        friend bool operator==(Dereference_and_access_expression const&, Dereference_and_access_expression const&) = default;
    };

    export struct For_loop_expression
    {
        std::pmr::string variable_name;
        Expression_index range_begin;
        Statement range_end;
        Binary_operation range_comparison_operation;
        std::optional<Expression_index> step_by;
        std::pmr::vector<Statement> then_statements;

        friend bool operator==(For_loop_expression const&, For_loop_expression const&) = default;
    };

    export struct Function_expression
    {
        Function_declaration declaration;
        Function_definition definition;

        friend bool operator==(Function_expression const&, Function_expression const&) = default;
    };

    export struct Instance_call_expression
    {
        Expression_index left_hand_side;
        std::pmr::vector<Statement> arguments;

        friend bool operator==(Instance_call_expression const&, Instance_call_expression const&) = default;
    };

    export struct Instance_call_key
    {
        std::pmr::string module_name;
        std::pmr::string function_constructor_name;
        std::pmr::vector<Statement> arguments;

        friend bool operator==(Instance_call_key const&, Instance_call_key const&) = default;
    };

    export struct Condition_statement_pair
    {
        std::optional<Statement> condition;
        std::pmr::vector<Statement> then_statements;
        std::optional<Source_range> block_source_range;

        friend bool operator==(Condition_statement_pair const&, Condition_statement_pair const&) = default;
    };

    export struct If_expression
    {
        std::pmr::vector<Condition_statement_pair> series;

        friend bool operator==(If_expression const&, If_expression const&) = default;
    };

    export enum class Instantiate_expression_type
    {
        Default,
        Explicit,
        Uninitialized,
        Zero_initialized
    };

    export struct Instantiate_member_value_pair
    {
        std::pmr::string member_name;
        Expression_index value;
        std::optional<Source_range> source_range;

        friend bool operator==(Instantiate_member_value_pair const&, Instantiate_member_value_pair const&) = default;
    };

    export struct Instantiate_expression
    {
        Instantiate_expression_type type;
        std::pmr::vector<Instantiate_member_value_pair> members;

        friend bool operator==(Instantiate_expression const&, Instantiate_expression const&) = default;
    };

    export struct Invalid_expression
    {
        std::pmr::string value;

        friend bool operator==(Invalid_expression const&, Invalid_expression const&) = default;
    };

    export struct Null_pointer_expression
    {
        friend bool operator==(Null_pointer_expression const&, Null_pointer_expression const&) = default;
    };

    export struct Parenthesis_expression
    {
        Expression_index expression;

        friend bool operator==(Parenthesis_expression const&, Parenthesis_expression const&) = default;
    };

    export struct Reflection_expression
    {
        std::pmr::string name;
        std::pmr::vector<Type_reference> type_arguments;
        std::pmr::vector<Expression_index> arguments;

        friend bool operator==(Reflection_expression const&, Reflection_expression const&) = default;
    };

    export struct Return_expression
    {
        std::optional<Expression_index> expression;

        friend bool operator==(Return_expression const&, Return_expression const&) = default;
    };

    export struct Struct_expression
    {
        Struct_declaration declaration;

        friend bool operator==(Struct_expression const&, Struct_expression const&) = default;
    };

    export struct Switch_case_expression_pair
    {
        std::optional<Expression_index> case_value;
        std::pmr::vector<Statement> statements;

        friend bool operator==(Switch_case_expression_pair const&, Switch_case_expression_pair const&) = default;
    };

    export struct Switch_expression
    {
        Expression_index value;
        std::pmr::vector<Switch_case_expression_pair> cases;

        friend bool operator==(Switch_expression const&, Switch_expression const&) = default;
    };

    export struct Ternary_condition_expression
    {
        Expression_index condition;
        Statement then_statement;
        Statement else_statement;

        friend bool operator==(Ternary_condition_expression const&, Ternary_condition_expression const&) = default;
    };

    export struct Type_expression
    {
        Type_reference type;

        friend bool operator==(Type_expression const&, Type_expression const&) = default;
    };

    export enum class Unary_operation
    {
        Not,
        Bitwise_not,
        Minus,
        Pre_increment,
        Post_increment,
        Pre_decrement,
        Post_decrement,
        Indirection,
        Address_of
    };

    export struct Unary_expression
    {
        Expression_index expression;
        Unary_operation operation;

        friend bool operator==(Unary_expression const&, Unary_expression const&) = default;
    };

    export struct Union_expression
    {
        Union_declaration declaration;

        friend bool operator==(Union_expression const&, Union_expression const&) = default;
    };

    export struct Variable_declaration_expression
    {
        std::pmr::string name;
        bool is_mutable;
        Expression_index right_hand_side;

        friend bool operator==(Variable_declaration_expression const&, Variable_declaration_expression const&) = default;
    };

    export struct Variable_declaration_with_type_expression
    {
        std::pmr::string name;
        bool is_mutable;
        Expression_index type;
        Expression_index right_hand_side;

        friend bool operator==(Variable_declaration_with_type_expression const&, Variable_declaration_with_type_expression const&) = default;
    };

    export std::optional<Type_reference> get_variable_declaration_with_type_expression_type(
        Statement const& statement,
        Variable_declaration_with_type_expression const& expression
    );

    export struct While_loop_expression
    {
        Statement condition;
        std::pmr::vector<Statement> then_statements;

        friend bool operator==(While_loop_expression const&, While_loop_expression const&) = default;
    };

    export struct Expression
    {
        using Data_type = std::variant <
            Access_expression,
            Access_array_expression,
            Assert_expression,
            Assignment_expression,
            Binary_expression,
            Block_expression,
            Break_expression,
            Call_expression,
            Cast_expression,
            Comment_expression,
            Compile_time_expression,
            Constant_expression,
            Constant_array_expression,
            Continue_expression,
            Defer_expression,
            Dereference_and_access_expression,
            For_loop_expression,
            Function_expression,
            Instance_call_expression,
            If_expression,
            Instantiate_expression,
            Invalid_expression,
            Null_pointer_expression,
            Parenthesis_expression,
            Reflection_expression,
            Return_expression,
            Struct_expression,
            Switch_expression,
            Ternary_condition_expression,
            Type_expression,
            Unary_expression,
            Union_expression,
            Variable_declaration_expression,
            Variable_declaration_with_type_expression,
            Variable_expression,
            While_loop_expression
        > ;

        Data_type data;
        std::optional<Source_range> source_range;

        friend bool operator==(Expression const& lhs, Expression const& rhs);
    };


    export struct Type_constructor_parameter
    {
        std::pmr::string name;
        Type_reference type;

        friend bool operator==(Type_constructor_parameter const&, Type_constructor_parameter const&) = default;
    };

    export struct Type_constructor
    {
        std::pmr::string name;
        std::pmr::vector<Type_constructor_parameter> parameters;
        std::pmr::vector<Statement> statements;
        std::optional<std::pmr::string> comment;
        std::optional<Source_range_location> source_location;

        friend bool operator==(Type_constructor const&, Type_constructor const&) = default;
    };

    export struct Function_constructor_parameter
    {
        std::pmr::string name;
        Type_reference type;

        friend bool operator==(Function_constructor_parameter const&, Function_constructor_parameter const&) = default;
    };

    export struct Function_constructor
    {
        std::pmr::string name;
        std::pmr::vector<Function_constructor_parameter> parameters;
        std::pmr::vector<Statement> statements;
        std::optional<std::pmr::string> comment;
        std::optional<Source_range_location> source_location;

        friend bool operator==(Function_constructor const&, Function_constructor const&) = default;
    };

    export struct Language_version
    {
        std::uint32_t major;
        std::uint32_t minor;
        std::uint32_t patch;

        friend bool operator==(Language_version const&, Language_version const&) = default;
    };

    export struct Import_module_with_alias
    {
        std::pmr::string module_name;
        std::pmr::string alias;
        std::pmr::vector<std::pmr::string> usages;
        std::optional<Source_range> source_range;

        friend bool operator==(Import_module_with_alias const& lhs, Import_module_with_alias const& rhs);
    };

    export struct Module_dependencies
    {
        std::pmr::vector<Import_module_with_alias> alias_imports;

        friend bool operator==(Module_dependencies const&, Module_dependencies const&) = default;
    };

    export struct Module_declarations
    {
        std::pmr::vector<Alias_type_declaration> alias_type_declarations;
        std::pmr::vector<Enum_declaration> enum_declarations;
        std::pmr::vector<Forward_declaration> forward_declarations;
        std::pmr::vector<Global_variable_declaration> global_variable_declarations;
        std::pmr::vector<Struct_declaration> struct_declarations;
        std::pmr::vector<Union_declaration> union_declarations;
        std::pmr::vector<Function_declaration> function_declarations;
        std::pmr::vector<Function_constructor> function_constructors;
        std::pmr::vector<Type_constructor> type_constructors;

        friend bool operator==(Module_declarations const&, Module_declarations const&) = default;
    };

    export struct Module_instanced_declarations
    {
        std::pmr::deque<Struct_declaration> struct_declarations;
        std::pmr::deque<Union_declaration> union_declarations;
        std::pmr::deque<Function_declaration> function_declarations;

        friend bool operator==(Module_instanced_declarations const&, Module_instanced_declarations const&) = default;
    };

    export struct Module_definitions
    {
        std::pmr::deque<Function_definition> function_definitions;

        friend bool operator==(Module_definitions const&, Module_definitions const&) = default;
    };

    export struct Module
    {
        Language_version language_version;
        std::pmr::string name;
        std::optional<std::uint64_t> content_hash;
        Module_dependencies dependencies;
        Module_declarations export_declarations;
        Module_declarations internal_declarations;
        Module_instanced_declarations instanced_declarations;
        Module_definitions definitions;
        std::optional<std::pmr::string> comment;
        std::optional<std::filesystem::path> source_file_path;

        friend bool operator==(Module const&, Module const&) = default;
    };

    export Module const& find_module(
        Module const& core_module,
        std::pmr::unordered_map<std::pmr::string, Module> const& core_module_dependencies,
        std::string_view name
    );

    export std::string_view find_module_name(
        Module const& core_module,
        Module_reference const& module_reference
    );

    export Custom_type_reference const* find_declaration_type_reference(
        Type_reference const& type_reference
    );

    export std::optional<Alias_type_declaration const*> find_alias_type_declaration(Module const& module, std::string_view name);
    export std::optional<Enum_declaration const*> find_enum_declaration(Module const& module, std::string_view name);
    export std::optional<Forward_declaration const*> find_forward_declaration(Module const& module, std::string_view name);
    export std::optional<Function_declaration const*> find_function_declaration(Module const& module, std::string_view name);
    export std::optional<Function_definition const*> find_function_definition(Module const& module, std::string_view name);
    export std::optional<Global_variable_declaration const*> find_global_variable_declaration(Module const& module, std::string_view name);
    export std::optional<Struct_declaration const*> find_struct_declaration(Module const& module, std::string_view name);
    export std::optional<Union_declaration const*> find_union_declaration(Module const& module, std::string_view name);

    export Import_module_with_alias const* find_import_module_with_alias(
        iris::Module_dependencies const& dependencies,
        std::string_view const alias_name
    );

    export Import_module_with_alias* find_import_module_with_alias(
        iris::Module_dependencies& dependencies,
        std::string_view const alias_name
    );

    export Import_module_with_alias const* find_import_module_with_module_name(
        iris::Module_dependencies const& dependencies,
        std::string_view const module_name
    );

    export iris::Expression_index copy_expressions_to_new_statement(
        iris::Statement& destination_statement,
        iris::Statement const& source_statement,
        iris::Expression_index const source_expression_index
    );

    export bool is_builtin_function_name(
        std::string_view const name
    );
    
    export bool is_expression_address_of(
        iris::Expression const& expression
    );

    export bool is_offset_pointer(
        iris::Statement const& statement,
        iris::Expression const& expression
    );

    export bool is_add_scope_expression(
        iris::Expression const& expression
    );
}