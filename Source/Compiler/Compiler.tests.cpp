#include <cstdio>
#include <filesystem>
#include <memory_resource>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/Analysis/LoopAnalysisManager.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassInstrumentation.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/StandardInstrumentations.h>
#include <llvm/Target/TargetMachine.h>

import iris.binary_serializer;
import iris.core;
import iris.core.declarations;
import iris.core.struct_layout;
import iris.common;
import iris.common.filesystem;
import iris.compiler;
import iris.compiler.artifact;
import iris.compiler.clang_code_generation;
import iris.compiler.clang_data;
import iris.compiler.common;
import iris.compiler.expressions;
import iris.compiler.types;
import iris.json_serializer.operators;
import iris.c_header_converter;
import iris.parser.convertor;
import iris.parser.parse_tree;
import iris.parser.parser;

using iris::json::operators::operator<<;

#include <catch2/catch_all.hpp>

namespace iris
{
  static std::filesystem::path const g_test_source_files_path = std::filesystem::path{ TEST_SOURCE_FILES_PATH };
  static std::filesystem::path const g_standard_library_path = std::filesystem::path{ C_STANDARD_LIBRARY_PATH };
  static std::filesystem::path const g_tests_output_directory_path = std::filesystem::path{ TESTS_OUTPUT_DIRECTORY_PATH };

  std::filesystem::path find_c_header_path(std::string_view const filename)
  {
    std::pmr::vector<std::filesystem::path> header_search_directories = iris::common::get_default_header_search_directories();
    std::optional<std::filesystem::path> const header_path = iris::compiler::find_c_header_path(filename, header_search_directories);
    REQUIRE(header_path.has_value());
    return header_path.value();
  }

  std::filesystem::path import_c_header_and_get_file_path(
    std::string_view const header_name,
    std::string_view const filename
  )
  {
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock{mutex};

    std::filesystem::path const input_file_path = find_c_header_path(filename);
    std::filesystem::path const output_file_path = (g_standard_library_path / filename).replace_extension(".irisb");

    if (std::filesystem::exists(output_file_path))
      return output_file_path;

    if (!std::filesystem::exists(output_file_path.parent_path()))
      std::filesystem::create_directories(output_file_path.parent_path());

    iris::c::Options const options = {};
    std::optional<iris::Module> const header_module = iris::c::import_header_and_write_to_file(
      header_name,
      input_file_path,
      output_file_path,
      options
    );
    REQUIRE(header_module.has_value());

    return output_file_path;
  }

  std::filesystem::path create_and_import_c_header(
    std::string_view const header_content,
    std::string_view const header_filename,
    std::string_view const header_module_filename,
    std::string_view const header_module_name,
    std::filesystem::path const output_directory
  )
  {
    std::filesystem::path const header_file_path = output_directory / header_filename;
    iris::common::write_to_file(header_file_path, header_content);

    std::filesystem::path const header_module_file_path = output_directory / header_module_filename;
    std::optional<iris::Module> const header_module = iris::c::import_header_and_write_to_file(header_module_name, header_file_path, header_module_file_path, {});
    REQUIRE(header_module.has_value());

    return header_module_file_path;
  }

  std::filesystem::path parse_and_get_file_path(
    std::filesystem::path const& source_file_path
  )
  {
    std::optional<iris::Module> const core_module = iris::parser::parse_and_convert_to_module(
        source_file_path,
        {},
        {}
    );
    REQUIRE(core_module.has_value());

    std::filesystem::path const output_file_path = g_tests_output_directory_path / std::format("{}.irisb", core_module.value().name);
    if (!std::filesystem::exists(output_file_path.parent_path()))
      std::filesystem::create_directories(output_file_path.parent_path());

    iris::binary_serializer::write_module_to_file(output_file_path, core_module.value(), {});

    return output_file_path;
  }

  std::string_view exclude_header(std::string_view const llvm_ir)
  {
    std::size_t current_index = 0;

    std::size_t const location = llvm_ir.find("\n\n", current_index);
    if (location == std::string_view::npos)
      return "";

    current_index = location + 1;
    return llvm_ir.substr(current_index, llvm_ir.size());
  }

  std::optional<std::string_view> find_metadata_id_for_file(
    std::string_view const llvm_ir,
    std::string_view const filename
  )
  {
    std::string const token = std::format("= !DIFile(filename: \"{}\"", filename);
    std::size_t const location = llvm_ir.find(token);
    if (location == std::string_view::npos)
      return std::nullopt;

    std::size_t const line_start = llvm_ir.rfind('\n', location);
    if (line_start == std::string_view::npos)
      return std::nullopt;

    std::size_t const id_start = line_start + 1;
    std::size_t const id_end = llvm_ir.find(' ', id_start);
    if (id_end == std::string_view::npos || id_end <= id_start)
      return std::nullopt;

    return llvm_ir.substr(id_start, id_end - id_start);
  }

  struct Test_options
  {
    bool debug = false;
    std::string_view target_triple = "x86_64-pc-linux-gnu";
    iris::compiler::Contract_options contract_options = iris::compiler::Contract_options::Log_error_and_abort;
    bool enable_bounds_checks = false;
    bool is_test_mode = false;
  };

  static void add_test_mode_dependencies(
    iris::Module& core_module,
    std::pmr::unordered_map<std::pmr::string, std::filesystem::path>& module_name_to_file_path_map
  )
  {
    module_name_to_file_path_map.emplace("c.inttypes", import_c_header_and_get_file_path("c.inttypes", "inttypes.h"));
    module_name_to_file_path_map.emplace("c.stdio", import_c_header_and_get_file_path("c.stdio", "stdio.h"));
    module_name_to_file_path_map.emplace("c.string", import_c_header_and_get_file_path("c.string", "string.h"));
    module_name_to_file_path_map.emplace("iris.json", parse_and_get_file_path(iris::common::get_json_module_file_path()));
    core_module.dependencies.alias_imports.push_back({"iris.json", "iris_json"});
  }

  std::string create_llvm_ir_body(
    std::string_view const input_file,
    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> module_name_to_file_path_map,
    Test_options const test_options = {}
  )
  {
    std::filesystem::path const input_file_path = g_test_source_files_path / input_file;
    std::optional<std::pmr::u8string> input_content = iris::common::get_file_utf8_contents(input_file_path);
    REQUIRE(input_content.has_value());

    iris::parser::Parser parser = iris::parser::create_parser();

    iris::parser::Parse_tree parse_tree = iris::parser::parse(parser, std::move(input_content.value()));

    iris::parser::Parse_node const root = get_root_node(parse_tree);

    std::optional<iris::Module> core_module = iris::parser::parse_node_to_module(
      parse_tree,
      root,
      input_file_path,
      {},
      {}
    );
    REQUIRE(core_module.has_value());

    if (test_options.is_test_mode)
      add_test_mode_dependencies(core_module.value(), module_name_to_file_path_map);

    iris::compiler::Compilation_options const compilation_options
    {
      .target_triple = test_options.target_triple,
      .is_optimized = false,
      .debug = test_options.debug,
      .contract_options = test_options.contract_options,
      .enable_bounds_checks = test_options.enable_bounds_checks,
      .is_test_mode = test_options.is_test_mode,
    };

    iris::compiler::LLVM_data llvm_data = iris::compiler::initialize_llvm(compilation_options);

    // Read header/dependency modules from the file-path map.
    std::pmr::vector<iris::Module> core_modules;
    core_modules.reserve(module_name_to_file_path_map.size());
    iris::compiler::add_builtin_module(core_modules);
    
    std::pmr::vector<iris::Module> header_modules;
    header_modules.reserve(module_name_to_file_path_map.size());
    
    for (auto const& [name, path] : module_name_to_file_path_map)
    {
      std::optional<iris::Module> dependency = iris::compiler::read_core_module(path);
      if (dependency.has_value())
      {
        if (path.extension().generic_string() == ".h")
          header_modules.push_back(std::move(dependency.value()));
        else
          core_modules.push_back(std::move(dependency.value()));
      }
    }
    core_modules.push_back(core_module.value());

    std::pmr::vector<iris::Module const*> header_module_pointers;
    header_module_pointers.reserve(header_modules.size());
    for (iris::Module const& header_module : header_modules)
      header_module_pointers.push_back(&header_module);

    iris::compiler::Preprocessed_modules preprocessed = iris::compiler::preprocess_modules(
      llvm_data,
      header_module_pointers,
      core_modules,
      compilation_options,
      {},
      {}
    );

    auto const transformed_location = std::find_if(
      preprocessed.transformed_core_modules.begin(),
      preprocessed.transformed_core_modules.end(),
      [&](iris::Module const& transformed) -> bool { return transformed.name == core_module->name; }
    );
    REQUIRE(transformed_location != preprocessed.transformed_core_modules.end());
    iris::Module const& transformed_core_module = *transformed_location;

    std::unique_ptr<llvm::Module> llvm_module = iris::compiler::create_llvm_module(
      llvm_data,
      transformed_core_module,
      preprocessed.sorted_modules,
      module_name_to_file_path_map,
      preprocessed.declaration_database,
      compilation_options
    );
    std::string const llvm_ir = iris::compiler::to_string(*llvm_module);

    iris::parser::destroy_tree(std::move(parse_tree));
    iris::parser::destroy_parser(std::move(parser));

    return std::string{exclude_header(llvm_ir)};
  }

  void test_create_llvm_module(
    std::string_view const input_file,
    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> module_name_to_file_path_map,
    std::string_view const expected_llvm_ir,
    Test_options const test_options = {}
  )
  {
    std::string const llvm_ir_body = create_llvm_ir_body(
      input_file,
      std::move(module_name_to_file_path_map),
      test_options
    );

    CHECK(llvm_ir_body == expected_llvm_ir);
  }

  TEST_CASE("Compile Address Of", "[LLVM_IR]")
  {
    char const* const input_file = "address_of.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
%struct.Address_of_My_struct = type { i32, i32 }

; Function Attrs: convergent
define void @Address_of_take(ptr noundef %"arguments[0].integers", i64 noundef %"arguments[1].instance") #0 {
entry:
  %integers = alloca ptr, align 8
  %0 = alloca %struct.Address_of_My_struct, align 4
  %p0 = alloca ptr, align 8
  %p1 = alloca ptr, align 8
  store ptr %"arguments[0].integers", ptr %integers, align 8
  %1 = getelementptr inbounds %struct.Address_of_My_struct, ptr %0, i32 0, i32 0
  store i64 %"arguments[1].instance", ptr %1, align 4
  %2 = load ptr, ptr %integers, align 8
  %array_element_pointer = getelementptr i32, ptr %2, i32 1
  store ptr %array_element_pointer, ptr %p0, align 8
  %3 = getelementptr inbounds %struct.Address_of_My_struct, ptr %0, i32 0, i32 1
  store ptr %3, ptr %p1, align 8
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Array Slices", "[LLVM_IR]")
  {
    char const* const input_file = "array_slices.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    std::string const expected_llvm_ir = std::format(R"(
%struct.iris_builtin_Generic_array_slice = type {{ ptr, i64 }}

; Function Attrs: convergent
define void @Array_slices_take(ptr %"arguments[0].integers_0", i64 %"arguments[0].integers_1") #0 !dbg !3 {{
entry:
  %integers = alloca %struct.iris_builtin_Generic_array_slice, align 8
  call void @llvm.dbg.declare(metadata ptr %integers, metadata !14, metadata !DIExpression()), !dbg !19
  %data = alloca ptr, align 8, !dbg !20
  %length = alloca i64, align 8, !dbg !21
  %v0 = alloca i32, align 4, !dbg !21
  %v1 = alloca i32, align 4, !dbg !22
  %v2 = alloca i32, align 4, !dbg !23
  %index = alloca i32, align 4, !dbg !24
  %v3 = alloca i32, align 4, !dbg !24
  %0 = getelementptr inbounds {{ ptr, i64 }}, ptr %integers, i32 0, i32 0
  store ptr %"arguments[0].integers_0", ptr %0, align 8
  %1 = getelementptr inbounds {{ ptr, i64 }}, ptr %integers, i32 0, i32 1
  store i64 %"arguments[0].integers_1", ptr %1, align 8
  %2 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %integers, i32 0, i32 0, !dbg !20
  %3 = load ptr, ptr %2, align 8, !dbg !20
  call void @llvm.dbg.declare(metadata ptr %data, metadata !25, metadata !DIExpression()), !dbg !26
  store ptr %3, ptr %data, align 8, !dbg !26
  %4 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %integers, i32 0, i32 1, !dbg !26
  %5 = load i64, ptr %4, align 8, !dbg !26
  call void @llvm.dbg.declare(metadata ptr %length, metadata !27, metadata !DIExpression()), !dbg !21
  store i64 %5, ptr %length, align 8, !dbg !21
  %6 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %integers, i32 0, i32 0, !dbg !21
  %7 = load ptr, ptr %6, align 8, !dbg !21
  %array_slice_element_pointer = getelementptr i32, ptr %7, i32 0, !dbg !21
  %8 = load i32, ptr %array_slice_element_pointer, align 4, !dbg !21
  call void @llvm.dbg.declare(metadata ptr %v0, metadata !28, metadata !DIExpression()), !dbg !22
  store i32 %8, ptr %v0, align 4, !dbg !22
  %9 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %integers, i32 0, i32 0, !dbg !22
  %10 = load ptr, ptr %9, align 8, !dbg !22
  %array_slice_element_pointer1 = getelementptr i32, ptr %10, i32 1, !dbg !22
  %11 = load i32, ptr %array_slice_element_pointer1, align 4, !dbg !22
  call void @llvm.dbg.declare(metadata ptr %v1, metadata !29, metadata !DIExpression()), !dbg !23
  store i32 %11, ptr %v1, align 4, !dbg !23
  %12 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %integers, i32 0, i32 0, !dbg !23
  %13 = load ptr, ptr %12, align 8, !dbg !23
  %array_slice_element_pointer2 = getelementptr i32, ptr %13, i32 2, !dbg !23
  %14 = load i32, ptr %array_slice_element_pointer2, align 4, !dbg !23
  call void @llvm.dbg.declare(metadata ptr %v2, metadata !30, metadata !DIExpression()), !dbg !31
  store i32 %14, ptr %v2, align 4, !dbg !31
  call void @llvm.dbg.declare(metadata ptr %index, metadata !32, metadata !DIExpression()), !dbg !24
  store i32 3, ptr %index, align 4, !dbg !24
  %15 = load i32, ptr %index, align 4, !dbg !24
  %16 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %integers, i32 0, i32 0, !dbg !24
  %17 = load ptr, ptr %16, align 8, !dbg !24
  %array_slice_element_pointer3 = getelementptr i32, ptr %17, i32 %15, !dbg !24
  %18 = load i32, ptr %array_slice_element_pointer3, align 4, !dbg !24
  call void @llvm.dbg.declare(metadata ptr %v3, metadata !33, metadata !DIExpression()), !dbg !34
  store i32 %18, ptr %v3, align 4, !dbg !34
  ret void, !dbg !34
}}

; Function Attrs: convergent
define void @Array_slices_run() #0 !dbg !35 {{
entry:
  %array = alloca [4 x i32], i64 4, align 4, !dbg !39
  %a = alloca [4 x i32], align 4, !dbg !39
  %0 = alloca %struct.iris_builtin_Generic_array_slice, align 8, !dbg !40
  %1 = alloca %struct.iris_builtin_Generic_array_slice, align 8, !dbg !41
  %array4 = alloca [1 x i32], i64 1, align 4, !dbg !42
  %2 = alloca %struct.iris_builtin_Generic_array_slice, align 8, !dbg !42
  %b = alloca i32, align 4, !dbg !43
  %c = alloca ptr, align 8, !dbg !43
  %3 = alloca %struct.iris_builtin_Generic_array_slice, align 8, !dbg !44
  %d = alloca %struct.iris_builtin_Generic_array_slice, align 8, !dbg !45
  %f = alloca i32, align 4, !dbg !46
  %g = alloca ptr, align 8, !dbg !46
  %4 = alloca %struct.iris_builtin_Generic_array_slice, align 8, !dbg !47
  %h = alloca %struct.iris_builtin_Generic_array_slice, align 8, !dbg !47
  %i = alloca ptr, align 8, !dbg !48
  %j = alloca ptr, align 8, !dbg !49
  %array_element_pointer = getelementptr [4 x i32], ptr %array, i32 0, i32 0, !dbg !39
  store i32 0, ptr %array_element_pointer, align 4, !dbg !39
  %array_element_pointer1 = getelementptr [4 x i32], ptr %array, i32 0, i32 1, !dbg !39
  store i32 1, ptr %array_element_pointer1, align 4, !dbg !39
  %array_element_pointer2 = getelementptr [4 x i32], ptr %array, i32 0, i32 2, !dbg !39
  store i32 2, ptr %array_element_pointer2, align 4, !dbg !39
  %array_element_pointer3 = getelementptr [4 x i32], ptr %array, i32 0, i32 3, !dbg !39
  store i32 3, ptr %array_element_pointer3, align 4, !dbg !39
  %5 = load [4 x i32], ptr %array, align 4, !dbg !39
  call void @llvm.dbg.declare(metadata ptr %a, metadata !50, metadata !DIExpression()), !dbg !40
  store [4 x i32] %5, ptr %a, align 4, !dbg !40
  %data_pointer = getelementptr [4 x i32], ptr %a, i32 0, i32 0, !dbg !40
  %6 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %0, i32 0, i32 0, !dbg !40
  store ptr %data_pointer, ptr %6, align 8, !dbg !40
  %7 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %0, i32 0, i32 1, !dbg !40
  store i64 4, ptr %7, align 8, !dbg !40
  %8 = getelementptr inbounds {{ ptr, i64 }}, ptr %0, i32 0, i32 0, !dbg !41
  %9 = load ptr, ptr %8, align 8, !dbg !41
  %10 = getelementptr inbounds {{ ptr, i64 }}, ptr %0, i32 0, i32 1, !dbg !41
  %11 = load i64, ptr %10, align 8, !dbg !41
  call void @Array_slices_take(ptr %9, i64 %11), !dbg !41
  %12 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %1, i32 0, i32 0, !dbg !41
  store ptr null, ptr %12, align 8, !dbg !41
  %13 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %1, i32 0, i32 1, !dbg !41
  store i64 0, ptr %13, align 8, !dbg !41
  %14 = getelementptr inbounds {{ ptr, i64 }}, ptr %1, i32 0, i32 0, !dbg !42
  %15 = load ptr, ptr %14, align 8, !dbg !42
  %16 = getelementptr inbounds {{ ptr, i64 }}, ptr %1, i32 0, i32 1, !dbg !42
  %17 = load i64, ptr %16, align 8, !dbg !42
  call void @Array_slices_take(ptr %15, i64 %17), !dbg !42
  %array_element_pointer5 = getelementptr [1 x i32], ptr %array4, i32 0, i32 0, !dbg !42
  store i32 4, ptr %array_element_pointer5, align 4, !dbg !42
  %data_pointer6 = getelementptr [1 x i32], ptr %array4, i32 0, i32 0, !dbg !42
  %18 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %2, i32 0, i32 0, !dbg !42
  store ptr %data_pointer6, ptr %18, align 8, !dbg !42
  %19 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %2, i32 0, i32 1, !dbg !42
  store i64 1, ptr %19, align 8, !dbg !42
  %20 = getelementptr inbounds {{ ptr, i64 }}, ptr %2, i32 0, i32 0, !dbg !52
  %21 = load ptr, ptr %20, align 8, !dbg !52
  %22 = getelementptr inbounds {{ ptr, i64 }}, ptr %2, i32 0, i32 1, !dbg !52
  %23 = load i64, ptr %22, align 8, !dbg !52
  call void @Array_slices_take(ptr %21, i64 %23), !dbg !52
  call void @llvm.dbg.declare(metadata ptr %b, metadata !53, metadata !DIExpression()), !dbg !43
  store i32 0, ptr %b, align 4, !dbg !43
  call void @llvm.dbg.declare(metadata ptr %c, metadata !54, metadata !DIExpression()), !dbg !44
  store ptr %b, ptr %c, align 8, !dbg !44
  %24 = load ptr, ptr %c, align 8, !dbg !44
  %25 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %3, i32 0, i32 0, !dbg !44
  store ptr %24, ptr %25, align 8, !dbg !44
  %26 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %3, i32 0, i32 1, !dbg !44
  store i64 1, ptr %26, align 8, !dbg !44
  %27 = load %struct.iris_builtin_Generic_array_slice, ptr %3, align 8, !dbg !44
  call void @llvm.dbg.declare(metadata ptr %d, metadata !55, metadata !DIExpression()), !dbg !45
  store %struct.iris_builtin_Generic_array_slice %27, ptr %d, align 8, !dbg !45
  %28 = getelementptr inbounds {{ ptr, i64 }}, ptr %d, i32 0, i32 0, !dbg !60
  %29 = load ptr, ptr %28, align 8, !dbg !60
  %30 = getelementptr inbounds {{ ptr, i64 }}, ptr %d, i32 0, i32 1, !dbg !60
  %31 = load i64, ptr %30, align 8, !dbg !60
  call void @Array_slices_take(ptr %29, i64 %31), !dbg !60
  call void @llvm.dbg.declare(metadata ptr %f, metadata !61, metadata !DIExpression()), !dbg !46
  store i32 0, ptr %f, align 4, !dbg !46
  call void @llvm.dbg.declare(metadata ptr %g, metadata !62, metadata !DIExpression()), !dbg !47
  store ptr %f, ptr %g, align 8, !dbg !47
  %32 = load ptr, ptr %g, align 8, !dbg !47
  %33 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %4, i32 0, i32 0, !dbg !47
  store ptr %32, ptr %33, align 8, !dbg !47
  %34 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %4, i32 0, i32 1, !dbg !47
  store i64 1, ptr %34, align 8, !dbg !47
  %35 = load %struct.iris_builtin_Generic_array_slice, ptr %4, align 8, !dbg !47
  call void @llvm.dbg.declare(metadata ptr %h, metadata !63, metadata !DIExpression()), !dbg !48
  store %struct.iris_builtin_Generic_array_slice %35, ptr %h, align 8, !dbg !48
  %36 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %h, i32 0, i32 0, !dbg !48
  %37 = load ptr, ptr %36, align 8, !dbg !48
  call void @llvm.dbg.declare(metadata ptr %i, metadata !64, metadata !DIExpression()), !dbg !49
  store ptr %37, ptr %i, align 8, !dbg !49
  %38 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %h, i32 0, i32 0, !dbg !49
  %39 = load ptr, ptr %38, align 8, !dbg !49
  %array_element_pointer7 = getelementptr i32, ptr %39, i32 0, !dbg !49
  call void @llvm.dbg.declare(metadata ptr %j, metadata !65, metadata !DIExpression()), !dbg !66
  store ptr %array_element_pointer7, ptr %j, align 8, !dbg !66
  ret void, !dbg !66
}}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

attributes #0 = {{ convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }}
attributes #1 = {{ nocallback nofree nosync nounwind speculatable willreturn memory(none) }}

!llvm.module.flags = !{{!0}}
!llvm.dbg.cu = !{{!1}}

!0 = !{{i32 2, !"Debug Info Version", i32 3}}
!1 = distinct !DICompileUnit(language: DW_LANG_C, file: !2, producer: "Iris Compiler", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!2 = !DIFile(filename: "array_slices.iris", directory: "{}")
!3 = distinct !DISubprogram(name: "take", linkageName: "Array_slices_take", scope: null, file: !2, line: 3, type: !4, scopeLine: 4, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !1, retainedNodes: !13)
!4 = !DISubroutineType(types: !5)
!5 = !{{null, !6}}
!6 = !DICompositeType(tag: DW_TAG_structure_type, name: "Array_slice::<int32_t>", size: 128, align: 64, elements: !7)
!7 = !{{!8, !11}}
!8 = !DIDerivedType(tag: DW_TAG_member, name: "data", baseType: !9, size: 64, align: 64)
!9 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !10, size: 64)
!10 = !DIBasicType(name: "int32_t", size: 32, encoding: DW_ATE_signed)
!11 = !DIDerivedType(tag: DW_TAG_member, name: "length", baseType: !12, size: 64, align: 64, offset: 64)
!12 = !DIBasicType(name: "uint64_t", size: 64, encoding: DW_ATE_unsigned)
!13 = !{{!14}}
!14 = !DILocalVariable(name: "integers", arg: 1, scope: !3, file: !2, line: 3, type: !15)
!15 = !DICompositeType(tag: DW_TAG_structure_type, name: "Array_slice::<int32_t>", scope: !3, size: 128, align: 64, elements: !16)
!16 = !{{!17, !18}}
!17 = !DIDerivedType(tag: DW_TAG_member, name: "data", scope: !3, baseType: !9, size: 64, align: 64)
!18 = !DIDerivedType(tag: DW_TAG_member, name: "length", scope: !3, baseType: !12, size: 64, align: 64, offset: 64)
!19 = !DILocation(line: 3, column: 22, scope: !3)
!20 = !DILocation(line: 4, column: 1, scope: !3)
!21 = !DILocation(line: 6, column: 5, scope: !3)
!22 = !DILocation(line: 8, column: 5, scope: !3)
!23 = !DILocation(line: 9, column: 5, scope: !3)
!24 = !DILocation(line: 12, column: 5, scope: !3)
!25 = !DILocalVariable(name: "data", scope: !3, file: !2, line: 5, type: !9)
!26 = !DILocation(line: 5, column: 5, scope: !3)
!27 = !DILocalVariable(name: "length", scope: !3, file: !2, line: 6, type: !12)
!28 = !DILocalVariable(name: "v0", scope: !3, file: !2, line: 8, type: !10)
!29 = !DILocalVariable(name: "v1", scope: !3, file: !2, line: 9, type: !10)
!30 = !DILocalVariable(name: "v2", scope: !3, file: !2, line: 10, type: !10)
!31 = !DILocation(line: 10, column: 5, scope: !3)
!32 = !DILocalVariable(name: "index", scope: !3, file: !2, line: 12, type: !10)
!33 = !DILocalVariable(name: "v3", scope: !3, file: !2, line: 13, type: !10)
!34 = !DILocation(line: 13, column: 5, scope: !3)
!35 = distinct !DISubprogram(name: "run", linkageName: "Array_slices_run", scope: null, file: !2, line: 16, type: !36, scopeLine: 17, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !1, retainedNodes: !38)
!36 = !DISubroutineType(types: !37)
!37 = !{{null}}
!38 = !{{}}
!39 = !DILocation(line: 17, column: 1, scope: !35)
!40 = !DILocation(line: 18, column: 5, scope: !35)
!41 = !DILocation(line: 19, column: 5, scope: !35)
!42 = !DILocation(line: 20, column: 5, scope: !35)
!43 = !DILocation(line: 23, column: 5, scope: !35)
!44 = !DILocation(line: 24, column: 5, scope: !35)
!45 = !DILocation(line: 25, column: 5, scope: !35)
!46 = !DILocation(line: 28, column: 5, scope: !35)
!47 = !DILocation(line: 29, column: 5, scope: !35)
!48 = !DILocation(line: 30, column: 5, scope: !35)
!49 = !DILocation(line: 31, column: 5, scope: !35)
!50 = !DILocalVariable(name: "a", scope: !35, file: !2, line: 18, type: !51)
!51 = !DICompositeType(tag: DW_TAG_array_type, baseType: !10, size: 4)
!52 = !DILocation(line: 21, column: 5, scope: !35)
!53 = !DILocalVariable(name: "b", scope: !35, file: !2, line: 23, type: !10)
!54 = !DILocalVariable(name: "c", scope: !35, file: !2, line: 24, type: !9)
!55 = !DILocalVariable(name: "d", scope: !35, file: !2, line: 25, type: !56)
!56 = !DICompositeType(tag: DW_TAG_structure_type, name: "Array_slice::<int32_t>", scope: !35, size: 128, align: 64, elements: !57)
!57 = !{{!58, !59}}
!58 = !DIDerivedType(tag: DW_TAG_member, name: "data", scope: !35, baseType: !9, size: 64, align: 64)
!59 = !DIDerivedType(tag: DW_TAG_member, name: "length", scope: !35, baseType: !12, size: 64, align: 64, offset: 64)
!60 = !DILocation(line: 26, column: 5, scope: !35)
!61 = !DILocalVariable(name: "f", scope: !35, file: !2, line: 28, type: !10)
!62 = !DILocalVariable(name: "g", scope: !35, file: !2, line: 29, type: !9)
!63 = !DILocalVariable(name: "h", scope: !35, file: !2, line: 30, type: !56)
!64 = !DILocalVariable(name: "i", scope: !35, file: !2, line: 31, type: !9)
!65 = !DILocalVariable(name: "j", scope: !35, file: !2, line: 32, type: !9)
!66 = !DILocation(line: 32, column: 5, scope: !35)
)", g_test_source_files_path.generic_string());

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir, { .debug = true });
  }

  TEST_CASE("Compile Array Slices Instantiate", "[LLVM_IR]")
  {
    char const* const input_file = "array_slices_instantiate.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    std::string const expected_llvm_ir = std::format(R"(
%struct.Array_slices_instantiate_My_struct = type {{ %struct.iris_builtin_Generic_array_slice }}
%struct.iris_builtin_Generic_array_slice = type {{ ptr, i64 }}

; Function Attrs: convergent
define void @Array_slices_instantiate_run() #0 !dbg !3 {{
entry:
  %v0 = alloca %struct.Array_slices_instantiate_My_struct, align 8, !dbg !7
  %0 = alloca %struct.iris_builtin_Generic_array_slice, align 8, !dbg !7
  %value = alloca i32, align 4, !dbg !8
  %s0 = alloca %struct.iris_builtin_Generic_array_slice, align 8, !dbg !9
  %1 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %0, i32 0, i32 0, !dbg !7
  store ptr null, ptr %1, align 8, !dbg !7
  %2 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %0, i32 0, i32 1, !dbg !7
  store i64 0, ptr %2, align 8, !dbg !7
  %3 = load %struct.iris_builtin_Generic_array_slice, ptr %0, align 8, !dbg !7
  %4 = getelementptr inbounds %struct.Array_slices_instantiate_My_struct, ptr %v0, i32 0, i32 0, !dbg !7
  store %struct.iris_builtin_Generic_array_slice %3, ptr %4, align 8, !dbg !7
  call void @llvm.dbg.declare(metadata ptr %v0, metadata !10, metadata !DIExpression()), !dbg !7
  call void @llvm.dbg.declare(metadata ptr %value, metadata !21, metadata !DIExpression()), !dbg !8
  store i32 0, ptr %value, align 4, !dbg !8
  %5 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %s0, i32 0, i32 0, !dbg !9
  store ptr %value, ptr %5, align 8, !dbg !9
  %6 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %s0, i32 0, i32 1, !dbg !9
  store i64 1, ptr %6, align 8, !dbg !9
  call void @llvm.dbg.declare(metadata ptr %s0, metadata !22, metadata !DIExpression()), !dbg !9
  ret void, !dbg !9
}}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

attributes #0 = {{ convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }}
attributes #1 = {{ nocallback nofree nosync nounwind speculatable willreturn memory(none) }}

!llvm.module.flags = !{{!0}}
!llvm.dbg.cu = !{{!1}}

!0 = !{{i32 2, !"Debug Info Version", i32 3}}
!1 = distinct !DICompileUnit(language: DW_LANG_C, file: !2, producer: "Iris Compiler", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!2 = !DIFile(filename: "array_slices_instantiate.iris", directory: "{}")
!3 = distinct !DISubprogram(name: "run", linkageName: "Array_slices_instantiate_run", scope: null, file: !2, line: 8, type: !4, scopeLine: 9, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !1, retainedNodes: !6)
!4 = !DISubroutineType(types: !5)
!5 = !{{null}}
!6 = !{{}}
!7 = !DILocation(line: 10, column: 5, scope: !3)
!8 = !DILocation(line: 12, column: 5, scope: !3)
!9 = !DILocation(line: 13, column: 5, scope: !3)
!10 = !DILocalVariable(name: "v0", scope: !3, file: !2, line: 10, type: !11)
!11 = !DICompositeType(tag: DW_TAG_structure_type, name: "Array_slices_instantiate_My_struct", file: !2, line: 3, size: 128, align: 8, elements: !12)
!12 = !{{!13}}
!13 = !DIDerivedType(tag: DW_TAG_member, name: "slice", file: !2, line: 5, baseType: !14, size: 128, align: 64)
!14 = !DICompositeType(tag: DW_TAG_structure_type, name: "Array_slice::<int32_t>", size: 128, align: 64, elements: !15)
!15 = !{{!16, !19}}
!16 = !DIDerivedType(tag: DW_TAG_member, name: "data", baseType: !17, size: 64, align: 64)
!17 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !18, size: 64)
!18 = !DIBasicType(name: "int32_t", size: 32, encoding: DW_ATE_signed)
!19 = !DIDerivedType(tag: DW_TAG_member, name: "length", baseType: !20, size: 64, align: 64, offset: 64)
!20 = !DIBasicType(name: "uint64_t", size: 64, encoding: DW_ATE_unsigned)
!21 = !DILocalVariable(name: "value", scope: !3, file: !2, line: 12, type: !18)
!22 = !DILocalVariable(name: "s0", scope: !3, file: !2, line: 13, type: !23)
!23 = !DICompositeType(tag: DW_TAG_structure_type, name: "Array_slice::<int32_t>", scope: !3, size: 128, align: 64, elements: !24)
!24 = !{{!25, !26}}
!25 = !DIDerivedType(tag: DW_TAG_member, name: "data", scope: !3, baseType: !17, size: 64, align: 64)
!26 = !DIDerivedType(tag: DW_TAG_member, name: "length", scope: !3, baseType: !20, size: 64, align: 64, offset: 64)
)", g_test_source_files_path.generic_string());

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir, { .debug = true });
  }

  TEST_CASE("Compile Asserts", "[LLVM_IR]")
  {
    char const* const input_file = "assert_expressions.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
@function_contract_error_string = private unnamed_addr constant [69 x i8] c"In function 'Assert_expressions.run' assert 'Value is not 0' failed!\00"
@function_contract_error_string.1 = private unnamed_addr constant [55 x i8] c"In function 'Assert_expressions.run' assert '' failed!\00"

; Function Attrs: convergent
define void @Assert_expressions_run(i32 noundef %"arguments[0].value") #0 {
entry:
  %value = alloca i32, align 4
  store i32 %"arguments[0].value", ptr %value, align 4
  %0 = load i32, ptr %value, align 4
  %1 = icmp ne i32 %0, 0
  br i1 %1, label %condition_success, label %condition_fail

condition_success:                                ; preds = %entry
  %2 = load i32, ptr %value, align 4
  %3 = icmp ne i32 %2, 1
  br i1 %3, label %condition_success1, label %condition_fail2

condition_fail:                                   ; preds = %entry
  %4 = call i32 @puts(ptr @function_contract_error_string)
  call void @abort()
  unreachable

condition_success1:                               ; preds = %condition_success
  ret void

condition_fail2:                                  ; preds = %condition_success
  %5 = call i32 @puts(ptr @function_contract_error_string.1)
  call void @abort()
  unreachable
}

declare i32 @puts(ptr)

declare void @abort()

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Assignments", "[LLVM_IR]")
  {
    char const* const input_file = "assignment_expressions.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define void @Assignment_expressions_integer_operations(i32 noundef %"arguments[0].other_signed_integer", i32 noundef %"arguments[1].other_unsigned_integer") #0 {
entry:
  %other_signed_integer = alloca i32, align 4
  %other_unsigned_integer = alloca i32, align 4
  %my_signed_integer = alloca i32, align 4
  %my_unsigned_integer = alloca i32, align 4
  store i32 %"arguments[0].other_signed_integer", ptr %other_signed_integer, align 4
  store i32 %"arguments[1].other_unsigned_integer", ptr %other_unsigned_integer, align 4
  store i32 1, ptr %my_signed_integer, align 4
  store i32 1, ptr %my_unsigned_integer, align 4
  store i32 2, ptr %my_signed_integer, align 4
  store i32 2, ptr %my_unsigned_integer, align 4
  %0 = load i32, ptr %my_signed_integer, align 4
  %1 = load i32, ptr %other_signed_integer, align 4
  %2 = add i32 %0, %1
  store i32 %2, ptr %my_signed_integer, align 4
  %3 = load i32, ptr %my_signed_integer, align 4
  %4 = load i32, ptr %other_signed_integer, align 4
  %5 = sub i32 %3, %4
  store i32 %5, ptr %my_signed_integer, align 4
  %6 = load i32, ptr %my_signed_integer, align 4
  %7 = load i32, ptr %other_signed_integer, align 4
  %8 = mul i32 %6, %7
  store i32 %8, ptr %my_signed_integer, align 4
  %9 = load i32, ptr %my_signed_integer, align 4
  %10 = load i32, ptr %other_signed_integer, align 4
  %11 = sdiv i32 %9, %10
  store i32 %11, ptr %my_signed_integer, align 4
  %12 = load i32, ptr %my_unsigned_integer, align 4
  %13 = load i32, ptr %other_unsigned_integer, align 4
  %14 = udiv i32 %12, %13
  store i32 %14, ptr %my_unsigned_integer, align 4
  %15 = load i32, ptr %my_signed_integer, align 4
  %16 = load i32, ptr %other_signed_integer, align 4
  %17 = srem i32 %15, %16
  store i32 %17, ptr %my_signed_integer, align 4
  %18 = load i32, ptr %my_unsigned_integer, align 4
  %19 = load i32, ptr %other_unsigned_integer, align 4
  %20 = urem i32 %18, %19
  store i32 %20, ptr %my_unsigned_integer, align 4
  %21 = load i32, ptr %my_signed_integer, align 4
  %22 = load i32, ptr %other_signed_integer, align 4
  %23 = and i32 %21, %22
  store i32 %23, ptr %my_signed_integer, align 4
  %24 = load i32, ptr %my_signed_integer, align 4
  %25 = load i32, ptr %other_signed_integer, align 4
  %26 = or i32 %24, %25
  store i32 %26, ptr %my_signed_integer, align 4
  %27 = load i32, ptr %my_signed_integer, align 4
  %28 = load i32, ptr %other_signed_integer, align 4
  %29 = xor i32 %27, %28
  store i32 %29, ptr %my_signed_integer, align 4
  %30 = load i32, ptr %my_signed_integer, align 4
  %31 = load i32, ptr %other_signed_integer, align 4
  %32 = shl i32 %30, %31
  store i32 %32, ptr %my_signed_integer, align 4
  %33 = load i32, ptr %my_signed_integer, align 4
  %34 = load i32, ptr %other_signed_integer, align 4
  %35 = ashr i32 %33, %34
  store i32 %35, ptr %my_signed_integer, align 4
  %36 = load i32, ptr %my_unsigned_integer, align 4
  %37 = load i32, ptr %other_unsigned_integer, align 4
  %38 = lshr i32 %36, %37
  store i32 %38, ptr %my_unsigned_integer, align 4
  ret void
}

; Function Attrs: convergent
define void @Assignment_expressions_float32_operations(float noundef %"arguments[0].other_float") #0 {
entry:
  %other_float = alloca float, align 4
  %my_float = alloca float, align 4
  store float %"arguments[0].other_float", ptr %other_float, align 4
  store float 1.000000e+00, ptr %my_float, align 4
  store float 2.000000e+00, ptr %my_float, align 4
  %0 = load float, ptr %my_float, align 4
  %1 = load float, ptr %other_float, align 4
  %2 = fadd float %0, %1
  store float %2, ptr %my_float, align 4
  %3 = load float, ptr %my_float, align 4
  %4 = load float, ptr %other_float, align 4
  %5 = fsub float %3, %4
  store float %5, ptr %my_float, align 4
  %6 = load float, ptr %my_float, align 4
  %7 = load float, ptr %other_float, align 4
  %8 = fmul float %6, %7
  store float %8, ptr %my_float, align 4
  %9 = load float, ptr %my_float, align 4
  %10 = load float, ptr %other_float, align 4
  %11 = fdiv float %9, %10
  store float %11, ptr %my_float, align 4
  %12 = load float, ptr %my_float, align 4
  %13 = load float, ptr %other_float, align 4
  %14 = frem float %12, %13
  store float %14, ptr %my_float, align 4
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Binary Expressions Precedence", "[LLVM_IR]")
  {
    char const* const input_file = "binary_expressions_precedence.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define void @Binary_expressions_operator_precedence_foo(i32 noundef %"arguments[0].a", i32 noundef %"arguments[1].b", i32 noundef %"arguments[2].c") #0 {
entry:
  %a = alloca i32, align 4
  %b = alloca i32, align 4
  %c = alloca i32, align 4
  %case_0 = alloca i32, align 4
  %case_1 = alloca i32, align 4
  %case_2 = alloca i32, align 4
  %case_3 = alloca i32, align 4
  %case_4 = alloca i32, align 4
  %pointer_a = alloca ptr, align 8
  %pointer_b = alloca ptr, align 8
  %case_7 = alloca i32, align 4
  %case_8 = alloca i32, align 4
  %case_9 = alloca i32, align 4
  %case_10 = alloca i1, align 1
  %case_11 = alloca i1, align 1
  %case_12 = alloca i1, align 1
  %case_13 = alloca i1, align 1
  %case_14 = alloca i32, align 4
  store i32 %"arguments[0].a", ptr %a, align 4
  store i32 %"arguments[1].b", ptr %b, align 4
  store i32 %"arguments[2].c", ptr %c, align 4
  %0 = load i32, ptr %a, align 4
  %1 = load i32, ptr %b, align 4
  %2 = load i32, ptr %c, align 4
  %3 = mul i32 %1, %2
  %4 = add i32 %0, %3
  store i32 %4, ptr %case_0, align 4
  %5 = load i32, ptr %a, align 4
  %6 = load i32, ptr %b, align 4
  %7 = mul i32 %5, %6
  %8 = load i32, ptr %c, align 4
  %9 = add i32 %7, %8
  store i32 %9, ptr %case_1, align 4
  %10 = load i32, ptr %a, align 4
  %11 = load i32, ptr %b, align 4
  %12 = sdiv i32 %10, %11
  %13 = load i32, ptr %c, align 4
  %14 = mul i32 %12, %13
  store i32 %14, ptr %case_2, align 4
  %15 = load i32, ptr %a, align 4
  %16 = load i32, ptr %b, align 4
  %17 = mul i32 %15, %16
  %18 = load i32, ptr %c, align 4
  %19 = sdiv i32 %17, %18
  store i32 %19, ptr %case_3, align 4
  %20 = load i32, ptr %a, align 4
  %21 = call i32 @Binary_expressions_operator_precedence_other_function()
  %22 = mul i32 %20, %21
  %23 = load i32, ptr %b, align 4
  %24 = add i32 %22, %23
  store i32 %24, ptr %case_4, align 4
  store ptr %case_0, ptr %pointer_a, align 8
  store ptr %case_1, ptr %pointer_b, align 8
  %25 = load ptr, ptr %pointer_a, align 8
  %26 = load i32, ptr %25, align 4
  %27 = load ptr, ptr %pointer_b, align 8
  %28 = load i32, ptr %27, align 4
  %29 = mul i32 %26, %28
  store i32 %29, ptr %case_7, align 4
  %30 = load i32, ptr %a, align 4
  %31 = load i32, ptr %b, align 4
  %32 = add i32 %30, %31
  %33 = load i32, ptr %c, align 4
  %34 = mul i32 %32, %33
  store i32 %34, ptr %case_8, align 4
  %35 = load i32, ptr %a, align 4
  %36 = load i32, ptr %b, align 4
  %37 = load i32, ptr %c, align 4
  %38 = add i32 %36, %37
  %39 = mul i32 %35, %38
  store i32 %39, ptr %case_9, align 4
  %40 = load i32, ptr %a, align 4
  %41 = icmp eq i32 %40, 0
  %42 = load i32, ptr %b, align 4
  %43 = icmp eq i32 %42, 1
  %44 = and i1 %41, %43
  store i1 %44, ptr %case_10, align 1
  %45 = load i32, ptr %a, align 4
  %46 = load i32, ptr %b, align 4
  %47 = and i32 %45, %46
  %48 = load i32, ptr %b, align 4
  %49 = load i32, ptr %a, align 4
  %50 = and i32 %48, %49
  %51 = icmp eq i32 %47, %50
  store i1 %51, ptr %case_11, align 1
  %52 = load i32, ptr %a, align 4
  %53 = load i32, ptr %b, align 4
  %54 = icmp slt i32 %52, %53
  %55 = load i32, ptr %b, align 4
  %56 = load i32, ptr %c, align 4
  %57 = icmp slt i32 %55, %56
  %58 = and i1 %54, %57
  store i1 %58, ptr %case_12, align 1
  %59 = load i32, ptr %a, align 4
  %60 = load i32, ptr %b, align 4
  %61 = add i32 %59, %60
  %62 = load i32, ptr %b, align 4
  %63 = load i32, ptr %c, align 4
  %64 = add i32 %62, %63
  %65 = icmp eq i32 %61, %64
  store i1 %65, ptr %case_13, align 1
  %66 = load i32, ptr %a, align 4
  %67 = sub i32 0, %66
  %68 = load i32, ptr %b, align 4
  %69 = sub i32 0, %68
  %70 = add i32 %67, %69
  store i32 %70, ptr %case_14, align 4
  ret void
}

; Function Attrs: convergent
define private i32 @Binary_expressions_operator_precedence_other_function() #0 {
entry:
  ret i32 1
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Binary Expressions", "[LLVM_IR]")
  {
    char const* const input_file = "binary_expressions.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define void @Binary_expressions_integer_operations(i32 noundef %"arguments[0].first_signed_integer", i32 noundef %"arguments[1].second_signed_integer", i32 noundef %"arguments[2].first_unsigned_integer", i32 noundef %"arguments[3].second_unsigned_integer") #0 {
entry:
  %first_signed_integer = alloca i32, align 4
  %second_signed_integer = alloca i32, align 4
  %first_unsigned_integer = alloca i32, align 4
  %second_unsigned_integer = alloca i32, align 4
  %add = alloca i32, align 4
  %subtract = alloca i32, align 4
  %multiply = alloca i32, align 4
  %signed_divide = alloca i32, align 4
  %unsigned_divide = alloca i32, align 4
  %signed_modulus = alloca i32, align 4
  %unsigned_modulus = alloca i32, align 4
  %equal = alloca i1, align 1
  %not_equal = alloca i1, align 1
  %signed_less_than = alloca i1, align 1
  %unsigned_less_than = alloca i1, align 1
  %signed_less_than_or_equal_to = alloca i1, align 1
  %unsigned_less_than_or_equal_to = alloca i1, align 1
  %signed_greater_than = alloca i1, align 1
  %unsigned_greater_than = alloca i1, align 1
  %signed_greater_than_or_equal_to = alloca i1, align 1
  %unsigned_greater_than_or_equal_to = alloca i1, align 1
  %bitwise_and = alloca i32, align 4
  %bitwise_or = alloca i32, align 4
  %bitwise_xor = alloca i32, align 4
  %bit_shift_left = alloca i32, align 4
  %signed_bit_shift_right = alloca i32, align 4
  %unsigned_bit_shift_right = alloca i32, align 4
  store i32 %"arguments[0].first_signed_integer", ptr %first_signed_integer, align 4
  store i32 %"arguments[1].second_signed_integer", ptr %second_signed_integer, align 4
  store i32 %"arguments[2].first_unsigned_integer", ptr %first_unsigned_integer, align 4
  store i32 %"arguments[3].second_unsigned_integer", ptr %second_unsigned_integer, align 4
  %0 = load i32, ptr %first_signed_integer, align 4
  %1 = load i32, ptr %second_signed_integer, align 4
  %2 = add i32 %0, %1
  store i32 %2, ptr %add, align 4
  %3 = load i32, ptr %first_signed_integer, align 4
  %4 = load i32, ptr %second_signed_integer, align 4
  %5 = sub i32 %3, %4
  store i32 %5, ptr %subtract, align 4
  %6 = load i32, ptr %first_signed_integer, align 4
  %7 = load i32, ptr %second_signed_integer, align 4
  %8 = mul i32 %6, %7
  store i32 %8, ptr %multiply, align 4
  %9 = load i32, ptr %first_signed_integer, align 4
  %10 = load i32, ptr %second_signed_integer, align 4
  %11 = sdiv i32 %9, %10
  store i32 %11, ptr %signed_divide, align 4
  %12 = load i32, ptr %first_unsigned_integer, align 4
  %13 = load i32, ptr %second_unsigned_integer, align 4
  %14 = udiv i32 %12, %13
  store i32 %14, ptr %unsigned_divide, align 4
  %15 = load i32, ptr %first_signed_integer, align 4
  %16 = load i32, ptr %second_signed_integer, align 4
  %17 = srem i32 %15, %16
  store i32 %17, ptr %signed_modulus, align 4
  %18 = load i32, ptr %first_unsigned_integer, align 4
  %19 = load i32, ptr %second_unsigned_integer, align 4
  %20 = urem i32 %18, %19
  store i32 %20, ptr %unsigned_modulus, align 4
  %21 = load i32, ptr %first_signed_integer, align 4
  %22 = load i32, ptr %second_signed_integer, align 4
  %23 = icmp eq i32 %21, %22
  store i1 %23, ptr %equal, align 1
  %24 = load i32, ptr %first_signed_integer, align 4
  %25 = load i32, ptr %second_signed_integer, align 4
  %26 = icmp ne i32 %24, %25
  store i1 %26, ptr %not_equal, align 1
  %27 = load i32, ptr %first_signed_integer, align 4
  %28 = load i32, ptr %second_signed_integer, align 4
  %29 = icmp slt i32 %27, %28
  store i1 %29, ptr %signed_less_than, align 1
  %30 = load i32, ptr %first_unsigned_integer, align 4
  %31 = load i32, ptr %second_unsigned_integer, align 4
  %32 = icmp ult i32 %30, %31
  store i1 %32, ptr %unsigned_less_than, align 1
  %33 = load i32, ptr %first_signed_integer, align 4
  %34 = load i32, ptr %second_signed_integer, align 4
  %35 = icmp sle i32 %33, %34
  store i1 %35, ptr %signed_less_than_or_equal_to, align 1
  %36 = load i32, ptr %first_unsigned_integer, align 4
  %37 = load i32, ptr %second_unsigned_integer, align 4
  %38 = icmp ule i32 %36, %37
  store i1 %38, ptr %unsigned_less_than_or_equal_to, align 1
  %39 = load i32, ptr %first_signed_integer, align 4
  %40 = load i32, ptr %second_signed_integer, align 4
  %41 = icmp sgt i32 %39, %40
  store i1 %41, ptr %signed_greater_than, align 1
  %42 = load i32, ptr %first_unsigned_integer, align 4
  %43 = load i32, ptr %second_unsigned_integer, align 4
  %44 = icmp ugt i32 %42, %43
  store i1 %44, ptr %unsigned_greater_than, align 1
  %45 = load i32, ptr %first_signed_integer, align 4
  %46 = load i32, ptr %second_signed_integer, align 4
  %47 = icmp sge i32 %45, %46
  store i1 %47, ptr %signed_greater_than_or_equal_to, align 1
  %48 = load i32, ptr %first_unsigned_integer, align 4
  %49 = load i32, ptr %second_unsigned_integer, align 4
  %50 = icmp uge i32 %48, %49
  store i1 %50, ptr %unsigned_greater_than_or_equal_to, align 1
  %51 = load i32, ptr %first_signed_integer, align 4
  %52 = load i32, ptr %second_signed_integer, align 4
  %53 = and i32 %51, %52
  store i32 %53, ptr %bitwise_and, align 4
  %54 = load i32, ptr %first_signed_integer, align 4
  %55 = load i32, ptr %second_signed_integer, align 4
  %56 = or i32 %54, %55
  store i32 %56, ptr %bitwise_or, align 4
  %57 = load i32, ptr %first_signed_integer, align 4
  %58 = load i32, ptr %second_signed_integer, align 4
  %59 = xor i32 %57, %58
  store i32 %59, ptr %bitwise_xor, align 4
  %60 = load i32, ptr %first_signed_integer, align 4
  %61 = load i32, ptr %second_signed_integer, align 4
  %62 = shl i32 %60, %61
  store i32 %62, ptr %bit_shift_left, align 4
  %63 = load i32, ptr %first_signed_integer, align 4
  %64 = load i32, ptr %second_signed_integer, align 4
  %65 = ashr i32 %63, %64
  store i32 %65, ptr %signed_bit_shift_right, align 4
  %66 = load i32, ptr %first_unsigned_integer, align 4
  %67 = load i32, ptr %second_unsigned_integer, align 4
  %68 = lshr i32 %66, %67
  store i32 %68, ptr %unsigned_bit_shift_right, align 4
  ret void
}

; Function Attrs: convergent
define void @Binary_expressions_boolean_operations(i1 noundef zeroext %"arguments[0].first_boolean", i1 noundef zeroext %"arguments[1].second_boolean") #0 {
entry:
  %first_boolean = alloca i8, align 1
  %second_boolean = alloca i8, align 1
  %equal = alloca i1, align 1
  %not_equal = alloca i1, align 1
  %logical_and = alloca i8, align 1
  %logical_or = alloca i8, align 1
  %0 = zext i1 %"arguments[0].first_boolean" to i8
  store i8 %0, ptr %first_boolean, align 1
  %1 = zext i1 %"arguments[1].second_boolean" to i8
  store i8 %1, ptr %second_boolean, align 1
  %2 = load i8, ptr %first_boolean, align 1
  %3 = load i8, ptr %second_boolean, align 1
  %4 = icmp eq i8 %2, %3
  store i1 %4, ptr %equal, align 1
  %5 = load i8, ptr %first_boolean, align 1
  %6 = load i8, ptr %second_boolean, align 1
  %7 = icmp ne i8 %5, %6
  store i1 %7, ptr %not_equal, align 1
  %8 = load i8, ptr %first_boolean, align 1
  %9 = load i8, ptr %second_boolean, align 1
  %10 = and i8 %8, %9
  store i8 %10, ptr %logical_and, align 1
  %11 = load i8, ptr %first_boolean, align 1
  %12 = load i8, ptr %second_boolean, align 1
  %13 = or i8 %11, %12
  store i8 %13, ptr %logical_or, align 1
  ret void
}

; Function Attrs: convergent
define void @Binary_expressions_float32_operations(float noundef %"arguments[0].first_float", float noundef %"arguments[1].second_float") #0 {
entry:
  %first_float = alloca float, align 4
  %second_float = alloca float, align 4
  %add = alloca float, align 4
  %subtract = alloca float, align 4
  %multiply = alloca float, align 4
  %divide = alloca float, align 4
  %modulus = alloca float, align 4
  %equal = alloca i1, align 1
  %not_equal = alloca i1, align 1
  %less_than = alloca i1, align 1
  %less_than_or_equal_to = alloca i1, align 1
  %greater_than = alloca i1, align 1
  %greater_than_or_equal_to = alloca i1, align 1
  store float %"arguments[0].first_float", ptr %first_float, align 4
  store float %"arguments[1].second_float", ptr %second_float, align 4
  %0 = load float, ptr %first_float, align 4
  %1 = load float, ptr %second_float, align 4
  %2 = fadd float %0, %1
  store float %2, ptr %add, align 4
  %3 = load float, ptr %first_float, align 4
  %4 = load float, ptr %second_float, align 4
  %5 = fsub float %3, %4
  store float %5, ptr %subtract, align 4
  %6 = load float, ptr %first_float, align 4
  %7 = load float, ptr %second_float, align 4
  %8 = fmul float %6, %7
  store float %8, ptr %multiply, align 4
  %9 = load float, ptr %first_float, align 4
  %10 = load float, ptr %second_float, align 4
  %11 = fdiv float %9, %10
  store float %11, ptr %divide, align 4
  %12 = load float, ptr %first_float, align 4
  %13 = load float, ptr %second_float, align 4
  %14 = frem float %12, %13
  store float %14, ptr %modulus, align 4
  %15 = load float, ptr %first_float, align 4
  %16 = load float, ptr %second_float, align 4
  %17 = fcmp oeq float %15, %16
  store i1 %17, ptr %equal, align 1
  %18 = load float, ptr %first_float, align 4
  %19 = load float, ptr %second_float, align 4
  %20 = fcmp one float %18, %19
  store i1 %20, ptr %not_equal, align 1
  %21 = load float, ptr %first_float, align 4
  %22 = load float, ptr %second_float, align 4
  %23 = fcmp olt float %21, %22
  store i1 %23, ptr %less_than, align 1
  %24 = load float, ptr %first_float, align 4
  %25 = load float, ptr %second_float, align 4
  %26 = fcmp ole float %24, %25
  store i1 %26, ptr %less_than_or_equal_to, align 1
  %27 = load float, ptr %first_float, align 4
  %28 = load float, ptr %second_float, align 4
  %29 = fcmp ogt float %27, %28
  store i1 %29, ptr %greater_than, align 1
  %30 = load float, ptr %first_float, align 4
  %31 = load float, ptr %second_float, align 4
  %32 = fcmp oge float %30, %31
  store i1 %32, ptr %greater_than_or_equal_to, align 1
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Binary Expressions Types", "[LLVM_IR]")
  {
    char const* const input_file = "binary_expressions_types.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define void @Binary_expression_types_run() #0 {
entry:
  %p0 = alloca ptr, align 8
  %v0 = alloca i1, align 1
  store ptr null, ptr %p0, align 8
  %0 = load ptr, ptr %p0, align 8
  %1 = icmp eq ptr %0, null
  store i1 %1, ptr %v0, align 1
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }


  TEST_CASE("Compile Bit Fields", "[LLVM_IR]")
  {
    char const* const input_file = "bit_fields.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
%struct.bit_fields_My_struct = type { i64 }

; Function Attrs: convergent
define private i64 @bit_fields_run(i64 noundef %"arguments[0].parameter") #0 {
entry:
  %0 = alloca %struct.bit_fields_My_struct, align 8
  %a = alloca i32, align 4
  %b = alloca i32, align 4
  %c = alloca i32, align 4
  %d = alloca i32, align 4
  %e = alloca i32, align 4
  %mutable_instance = alloca %struct.bit_fields_My_struct, align 8
  %instance = alloca %struct.bit_fields_My_struct, align 8
  %instance_2 = alloca %struct.bit_fields_My_struct, align 8
  %1 = getelementptr inbounds %struct.bit_fields_My_struct, ptr %0, i32 0, i32 0
  store i64 %"arguments[0].parameter", ptr %1, align 8
  %2 = getelementptr inbounds %struct.bit_fields_My_struct, ptr %0, i32 0, i32 0
  %3 = load i32, ptr %2, align 4
  %4 = lshr i32 %3, 0
  %5 = and i32 %4, 16777215
  store i32 %5, ptr %a, align 4
  %6 = getelementptr inbounds %struct.bit_fields_My_struct, ptr %0, i32 0, i32 0
  %7 = load i32, ptr %6, align 4
  %8 = lshr i32 %7, 24
  %9 = and i32 %8, 255
  store i32 %9, ptr %b, align 4
  %10 = getelementptr inbounds %struct.bit_fields_My_struct, ptr %0, i32 0, i32 0
  %11 = load i32, ptr %10, align 4
  %12 = lshr i32 %11, 32
  %13 = and i32 %12, 4095
  store i32 %13, ptr %c, align 4
  %14 = getelementptr inbounds %struct.bit_fields_My_struct, ptr %0, i32 0, i32 0
  %15 = load i32, ptr %14, align 4
  %16 = lshr i32 %15, 44
  %17 = and i32 %16, 255
  store i32 %17, ptr %d, align 4
  %18 = getelementptr inbounds %struct.bit_fields_My_struct, ptr %0, i32 0, i32 0
  %19 = load i32, ptr %18, align 4
  %20 = lshr i32 %19, 52
  %21 = and i32 %20, 4095
  store i32 %21, ptr %e, align 4
  %22 = getelementptr inbounds %struct.bit_fields_My_struct, ptr %mutable_instance, i32 0, i32 0
  %23 = load i32, ptr %22, align 4
  %24 = and i32 %23, -16777216
  %25 = or i32 %24, 0
  store i32 %25, ptr %22, align 4
  %26 = getelementptr inbounds %struct.bit_fields_My_struct, ptr %mutable_instance, i32 0, i32 0
  %27 = load i32, ptr %26, align 4
  %28 = and i32 %27, 16777215
  %29 = or i32 %28, 0
  store i32 %29, ptr %26, align 4
  %30 = getelementptr inbounds %struct.bit_fields_My_struct, ptr %mutable_instance, i32 0, i32 0
  %31 = load i32, ptr %30, align 4
  %32 = and i32 %31, -4096
  %33 = or i32 %32, poison
  store i32 %33, ptr %30, align 4
  %34 = getelementptr inbounds %struct.bit_fields_My_struct, ptr %mutable_instance, i32 0, i32 0
  %35 = load i32, ptr %34, align 4
  %36 = and i32 %35, -1044481
  %37 = or i32 %36, poison
  store i32 %37, ptr %34, align 4
  %38 = getelementptr inbounds %struct.bit_fields_My_struct, ptr %mutable_instance, i32 0, i32 0
  %39 = load i32, ptr %38, align 4
  %40 = and i32 %39, 1048575
  %41 = or i32 %40, poison
  store i32 %41, ptr %38, align 4
  %42 = getelementptr inbounds %struct.bit_fields_My_struct, ptr %mutable_instance, i32 0, i32 0
  %43 = load i32, ptr %42, align 4
  %44 = lshr i32 %43, 0
  %45 = and i32 %44, 16777215
  %46 = getelementptr inbounds %struct.bit_fields_My_struct, ptr %mutable_instance, i32 0, i32 0
  %47 = load i32, ptr %46, align 4
  %48 = and i32 %47, -16777216
  %49 = or i32 %48, 1
  store i32 %49, ptr %46, align 4
  %50 = getelementptr inbounds %struct.bit_fields_My_struct, ptr %mutable_instance, i32 0, i32 0
  %51 = load i32, ptr %50, align 4
  %52 = lshr i32 %51, 24
  %53 = and i32 %52, 255
  %54 = getelementptr inbounds %struct.bit_fields_My_struct, ptr %mutable_instance, i32 0, i32 0
  %55 = load i32, ptr %54, align 4
  %56 = and i32 %55, 16777215
  %57 = or i32 %56, 33554432
  store i32 %57, ptr %54, align 4
  %58 = getelementptr inbounds %struct.bit_fields_My_struct, ptr %mutable_instance, i32 0, i32 0
  %59 = load i32, ptr %58, align 4
  %60 = lshr i32 %59, 32
  %61 = and i32 %60, 4095
  %62 = getelementptr inbounds %struct.bit_fields_My_struct, ptr %mutable_instance, i32 0, i32 0
  %63 = load i32, ptr %62, align 4
  %64 = and i32 %63, -4096
  %65 = or i32 %64, poison
  store i32 %65, ptr %62, align 4
  %66 = getelementptr inbounds %struct.bit_fields_My_struct, ptr %instance, i32 0, i32 0
  %67 = load i32, ptr %66, align 4
  %68 = and i32 %67, -16777216
  %69 = or i32 %68, 1
  store i32 %69, ptr %66, align 4
  %70 = getelementptr inbounds %struct.bit_fields_My_struct, ptr %instance, i32 0, i32 0
  %71 = load i32, ptr %70, align 4
  %72 = and i32 %71, 16777215
  %73 = or i32 %72, 33554432
  store i32 %73, ptr %70, align 4
  %74 = getelementptr inbounds %struct.bit_fields_My_struct, ptr %instance, i32 0, i32 0
  %75 = load i32, ptr %74, align 4
  %76 = and i32 %75, -4096
  %77 = or i32 %76, poison
  store i32 %77, ptr %74, align 4
  %78 = getelementptr inbounds %struct.bit_fields_My_struct, ptr %instance, i32 0, i32 0
  %79 = load i32, ptr %78, align 4
  %80 = and i32 %79, -1044481
  %81 = or i32 %80, poison
  store i32 %81, ptr %78, align 4
  %82 = getelementptr inbounds %struct.bit_fields_My_struct, ptr %instance, i32 0, i32 0
  %83 = load i32, ptr %82, align 4
  %84 = and i32 %83, 1048575
  %85 = or i32 %84, poison
  store i32 %85, ptr %82, align 4
  %86 = getelementptr inbounds %struct.bit_fields_My_struct, ptr %instance_2, i32 0, i32 0
  %87 = load i32, ptr %86, align 4
  %88 = and i32 %87, -16777216
  %89 = or i32 %88, 1
  store i32 %89, ptr %86, align 4
  %90 = getelementptr inbounds %struct.bit_fields_My_struct, ptr %instance_2, i32 0, i32 0
  %91 = load i32, ptr %90, align 4
  %92 = and i32 %91, 16777215
  %93 = or i32 %92, 0
  store i32 %93, ptr %90, align 4
  %94 = getelementptr inbounds %struct.bit_fields_My_struct, ptr %instance_2, i32 0, i32 0
  %95 = load i32, ptr %94, align 4
  %96 = and i32 %95, -4096
  %97 = or i32 %96, poison
  store i32 %97, ptr %94, align 4
  %98 = getelementptr inbounds %struct.bit_fields_My_struct, ptr %instance_2, i32 0, i32 0
  %99 = load i32, ptr %98, align 4
  %100 = and i32 %99, -1044481
  %101 = or i32 %100, poison
  store i32 %101, ptr %98, align 4
  %102 = getelementptr inbounds %struct.bit_fields_My_struct, ptr %instance_2, i32 0, i32 0
  %103 = load i32, ptr %102, align 4
  %104 = and i32 %103, 1048575
  %105 = or i32 %104, poison
  store i32 %105, ptr %102, align 4
  %106 = getelementptr inbounds %struct.bit_fields_My_struct, ptr %instance, i32 0, i32 0
  %107 = load i64, ptr %106, align 8
  ret i64 %107
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }


  TEST_CASE("Compile Block Expressions", "[LLVM_IR]")
  {
    char const* const input_file = "block_expressions.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define void @Block_expressions_run_blocks() #0 {
entry:
  %a = alloca i32, align 4
  %b = alloca i32, align 4
  %b1 = alloca i32, align 4
  store i32 0, ptr %a, align 4
  %0 = load i32, ptr %a, align 4
  store i32 %0, ptr %b, align 4
  %1 = load i32, ptr %a, align 4
  store i32 %1, ptr %b1, align 4
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Booleans", "[LLVM_IR]")
  {
    char const* const input_file = "booleans.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define void @Booleans_foo() #0 {
entry:
  %my_true_boolean = alloca i8, align 1
  %my_false_boolean = alloca i8, align 1
  store i8 1, ptr %my_true_boolean, align 1
  store i8 0, ptr %my_false_boolean, align 1
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Break Expressions", "[LLVM_IR]")
  {
    char const* const input_file = "break_expressions.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
        { "c.stdio", import_c_header_and_get_file_path("c.stdio", "stdio.h") }
    };

    char const* const expected_llvm_ir = R"(
@global_0 = internal constant [3 x i8] c"%d\00"

; Function Attrs: convergent
declare i32 @printf(ptr noundef, ...) #0

; Function Attrs: convergent
define void @Break_expressions_run_breaks(i32 noundef %"arguments[0].size") #0 {
entry:
  %size = alloca i32, align 4
  %index = alloca i32, align 4
  %index1 = alloca i32, align 4
  %index_2 = alloca i32, align 4
  %index8 = alloca i32, align 4
  %index_213 = alloca i32, align 4
  store i32 %"arguments[0].size", ptr %size, align 4
  store i32 0, ptr %index, align 4
  br label %for_loop_condition

for_loop_condition:                               ; preds = %for_loop_update_index, %entry
  %0 = load i32, ptr %size, align 4
  %1 = load i32, ptr %index, align 4
  %2 = icmp slt i32 %1, %0
  br i1 %2, label %for_loop_then, label %for_loop_after

for_loop_then:                                    ; preds = %for_loop_condition
  %3 = load i32, ptr %index, align 4
  %4 = icmp sgt i32 %3, 4
  br i1 %4, label %if_s0_then, label %if_s1_after

for_loop_update_index:                            ; preds = %if_s1_after
  %5 = load i32, ptr %index, align 4
  %6 = add i32 %5, 1
  store i32 %6, ptr %index, align 4
  br label %for_loop_condition

for_loop_after:                                   ; preds = %if_s0_then, %for_loop_condition
  store i32 0, ptr %index1, align 4
  br label %for_loop_condition2

if_s0_then:                                       ; preds = %for_loop_then
  br label %for_loop_after

if_s1_after:                                      ; preds = %for_loop_then
  %7 = load i32, ptr %index, align 4
  call void @Break_expressions_print_integer(i32 noundef %7)
  br label %for_loop_update_index

for_loop_condition2:                              ; preds = %for_loop_update_index4, %for_loop_after
  %8 = load i32, ptr %size, align 4
  %9 = load i32, ptr %index1, align 4
  %10 = icmp slt i32 %9, %8
  br i1 %10, label %for_loop_then3, label %for_loop_after5

for_loop_then3:                                   ; preds = %for_loop_condition2
  store i32 0, ptr %index_2, align 4
  br label %while_loop_condition

for_loop_update_index4:                           ; preds = %while_loop_after
  %11 = load i32, ptr %index1, align 4
  %12 = add i32 %11, 1
  store i32 %12, ptr %index1, align 4
  br label %for_loop_condition2

for_loop_after5:                                  ; preds = %for_loop_condition2
  store i32 0, ptr %index8, align 4
  br label %for_loop_condition9

while_loop_condition:                             ; preds = %if_s1_after7, %for_loop_then3
  %13 = load i32, ptr %index_2, align 4
  %14 = load i32, ptr %size, align 4
  %15 = icmp slt i32 %13, %14
  br i1 %15, label %while_loop_then, label %while_loop_after

while_loop_then:                                  ; preds = %while_loop_condition
  %16 = load i32, ptr %index1, align 4
  %17 = icmp sgt i32 %16, 3
  br i1 %17, label %if_s0_then6, label %if_s1_after7

while_loop_after:                                 ; preds = %if_s0_then6, %while_loop_condition
  %18 = load i32, ptr %index1, align 4
  call void @Break_expressions_print_integer(i32 noundef %18)
  br label %for_loop_update_index4

if_s0_then6:                                      ; preds = %while_loop_then
  br label %while_loop_after

if_s1_after7:                                     ; preds = %while_loop_then
  %19 = load i32, ptr %index_2, align 4
  call void @Break_expressions_print_integer(i32 noundef %19)
  %20 = load i32, ptr %index1, align 4
  %21 = add i32 %20, 1
  store i32 %21, ptr %index1, align 4
  br label %while_loop_condition

for_loop_condition9:                              ; preds = %for_loop_update_index11, %for_loop_after5
  %22 = load i32, ptr %size, align 4
  %23 = load i32, ptr %index8, align 4
  %24 = icmp slt i32 %23, %22
  br i1 %24, label %for_loop_then10, label %for_loop_after12

for_loop_then10:                                  ; preds = %for_loop_condition9
  store i32 0, ptr %index_213, align 4
  br label %while_loop_condition14

for_loop_update_index11:                          ; preds = %while_loop_after16
  %25 = load i32, ptr %index8, align 4
  %26 = add i32 %25, 1
  store i32 %26, ptr %index8, align 4
  br label %for_loop_condition9

for_loop_after12:                                 ; preds = %if_s0_then17, %for_loop_condition9
  ret void

while_loop_condition14:                           ; preds = %if_s1_after18, %for_loop_then10
  %27 = load i32, ptr %index_213, align 4
  %28 = load i32, ptr %size, align 4
  %29 = icmp slt i32 %27, %28
  br i1 %29, label %while_loop_then15, label %while_loop_after16

while_loop_then15:                                ; preds = %while_loop_condition14
  %30 = load i32, ptr %index8, align 4
  %31 = icmp sgt i32 %30, 3
  br i1 %31, label %if_s0_then17, label %if_s1_after18

while_loop_after16:                               ; preds = %while_loop_condition14
  %32 = load i32, ptr %index8, align 4
  call void @Break_expressions_print_integer(i32 noundef %32)
  br label %for_loop_update_index11

if_s0_then17:                                     ; preds = %while_loop_then15
  br label %for_loop_after12

if_s1_after18:                                    ; preds = %while_loop_then15
  %33 = load i32, ptr %index_213, align 4
  call void @Break_expressions_print_integer(i32 noundef %33)
  %34 = load i32, ptr %index8, align 4
  %35 = add i32 %34, 1
  store i32 %35, ptr %index8, align 4
  br label %while_loop_condition14
}

; Function Attrs: convergent
define private void @Break_expressions_print_integer(i32 noundef %"arguments[0].value") #0 {
entry:
  %value = alloca i32, align 4
  store i32 %"arguments[0].value", ptr %value, align 4
  %0 = load i32, ptr %value, align 4
  %1 = call i32 (ptr, ...) @printf(ptr noundef @global_0, i32 noundef %0)
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Cast Expressions", "[LLVM_IR]")
  {
    char const* const input_file = "cast_expressions.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define void @Cast_expressions_run(i32 noundef %"arguments[0].first", i32 noundef %"arguments[1].second", i32 noundef %"arguments[2].third") #0 {
entry:
  %first = alloca i32, align 4
  %second = alloca i32, align 4
  %third = alloca i32, align 4
  %a = alloca i32, align 4
  %b = alloca i1, align 1
  %c = alloca i32, align 4
  %d = alloca i32, align 4
  %e = alloca i32, align 4
  store i32 %"arguments[0].first", ptr %first, align 4
  store i32 %"arguments[1].second", ptr %second, align 4
  store i32 %"arguments[2].third", ptr %third, align 4
  store i32 1, ptr %a, align 4
  %0 = load i32, ptr %a, align 4
  %1 = load i32, ptr %first, align 4
  %2 = icmp eq i32 %0, %1
  store i1 %2, ptr %b, align 1
  %3 = load i32, ptr %second, align 4
  store i32 %3, ptr %c, align 4
  %4 = load i32, ptr %a, align 4
  store i32 %4, ptr %d, align 4
  %5 = load i32, ptr %third, align 4
  store i32 %5, ptr %e, align 4
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Comment Expressions", "[LLVM_IR]")
  {
    char const* const input_file = "comment_expressions.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define void @Comment_expressions_comment_expressions() #0 {
entry:
  %index = alloca i32, align 4
  br i1 true, label %if_s0_then, label %if_s1_after

if_s0_then:                                       ; preds = %entry
  br label %if_s1_after

if_s1_after:                                      ; preds = %if_s0_then, %entry
  store i32 0, ptr %index, align 4
  br label %for_loop_condition

for_loop_condition:                               ; preds = %for_loop_update_index, %if_s1_after
  %0 = load i32, ptr %index, align 4
  %1 = icmp slt i32 %0, 3
  br i1 %1, label %for_loop_then, label %for_loop_after

for_loop_then:                                    ; preds = %for_loop_condition
  br label %for_loop_update_index

for_loop_update_index:                            ; preds = %for_loop_then
  %2 = load i32, ptr %index, align 4
  %3 = add i32 %2, 1
  store i32 %3, ptr %index, align 4
  br label %for_loop_condition

for_loop_after:                                   ; preds = %for_loop_condition
  br label %while_loop_condition

while_loop_condition:                             ; preds = %while_loop_then, %for_loop_after
  br i1 false, label %while_loop_then, label %while_loop_after

while_loop_then:                                  ; preds = %while_loop_condition
  br label %while_loop_condition

while_loop_after:                                 ; preds = %while_loop_condition
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Compile Time For", "[LLVM_IR]")
  {
    char const* const input_file = "compile_time_for.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define private void @compile_time_for_foo(i64 noundef %"arguments[0].index") #0 {
entry:
  %index = alloca i64, align 8
  store i64 %"arguments[0].index", ptr %index, align 8
  ret void
}

; Function Attrs: convergent
define private void @compile_time_for_run() #0 {
entry:
  call void @compile_time_for_foo(i64 noundef 0)
  call void @compile_time_for_foo(i64 noundef 1)
  call void @compile_time_for_foo(i64 noundef 2)
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Compile Time If", "[LLVM_IR]")
  {
    char const* const input_file = "compile_time_if.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
@compile_time_if_g_debug = constant i8 1

; Function Attrs: convergent
define i32 @compile_time_if_run_0() #0 {
entry:
  ret i32 0
}

; Function Attrs: convergent
define i32 @compile_time_if_run_1() #0 {
entry:
  ret i32 3
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Constant Expressions", "[LLVM_IR]")
  {
    char const* const input_file = "constant_expressions.iris";
    
    std::filesystem::path const root_directory_path = std::filesystem::temp_directory_path() / "constant_expressions";
    std::filesystem::create_directories(root_directory_path);

    std::string const header_content = R"(

typedef unsigned long long My_flags;
static const My_flags g_global_0 = 0x800000000ULL;
const My_flags g_global_1 = 0x000000001ULL;
My_flags g_global_2 = 0x000000002ULL;
My_flags g_global_unused = 0x000000003ULL;
)";

    std::filesystem::path const header_module_file_path = create_and_import_c_header(header_content, "my_header.h", "my_header.iris", "my_module", root_directory_path);

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
        { "my_module", header_module_file_path }
    };

    char const* const expected_llvm_ir = R"(
@g_global_1 = external constant i64
@g_global_2 = external global i64
@Constant_expressions_g_global_0 = constant i32 0
@Constant_expressions_g_global_1 = global i32 0

; Function Attrs: convergent
define void @Constant_expressions_run() #0 {
entry:
  %v0 = alloca i64, align 8
  %v1 = alloca i64, align 8
  %v2 = alloca i64, align 8
  store i64 34359738368, ptr %v0, align 8
  %0 = load i64, ptr @g_global_1, align 8
  store i64 %0, ptr %v1, align 8
  %1 = load i64, ptr @g_global_2, align 8
  store i64 %1, ptr %v2, align 8
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Array Bounds Checks 0", "[LLVM_IR]")
  {
    char const* const input_file = "bounds_check_0.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
%struct.iris_builtin_Generic_array_slice = type { ptr, i64 }

@function_contract_error_string = private unnamed_addr constant [67 x i8] c"Out-of-bounds array slice access in 'Bounds_check_0.access_slice'!\00"
@function_contract_error_string.1 = private unnamed_addr constant [76 x i8] c"Out-of-bounds constant array access in 'Bounds_check_0.access_fixed_array'!\00"

; Function Attrs: convergent
define i32 @Bounds_check_0_access_slice(ptr %"arguments[0].integers_0", i64 %"arguments[0].integers_1", i32 noundef %"arguments[1].index") #0 {
entry:
  %integers = alloca %struct.iris_builtin_Generic_array_slice, align 8
  %index = alloca i32, align 4
  %0 = getelementptr inbounds { ptr, i64 }, ptr %integers, i32 0, i32 0
  store ptr %"arguments[0].integers_0", ptr %0, align 8
  %1 = getelementptr inbounds { ptr, i64 }, ptr %integers, i32 0, i32 1
  store i64 %"arguments[0].integers_1", ptr %1, align 8
  store i32 %"arguments[1].index", ptr %index, align 4
  %2 = load i32, ptr %index, align 4
  %3 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %integers, i32 0, i32 1
  %4 = load i64, ptr %3, align 8
  %bounds_check_index = zext i32 %2 to i64
  %bounds_check_in_bounds = icmp ult i64 %bounds_check_index, %4
  br i1 %bounds_check_in_bounds, label %bounds_check_pass, label %bounds_check_fail

bounds_check_pass:                                ; preds = %entry
  %5 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %integers, i32 0, i32 0
  %6 = load ptr, ptr %5, align 8
  %array_slice_element_pointer = getelementptr i32, ptr %6, i32 %2
  %7 = load i32, ptr %array_slice_element_pointer, align 4
  ret i32 %7

bounds_check_fail:                                ; preds = %entry
  %8 = call i32 @puts(ptr @function_contract_error_string)
  call void @abort()
  unreachable
}

; Function Attrs: convergent
define i32 @Bounds_check_0_access_fixed_array(i32 noundef %"arguments[0].index") #0 {
entry:
  %index = alloca i32, align 4
  %array = alloca [4 x i32], i64 4, align 4
  %a = alloca [4 x i32], align 4
  store i32 %"arguments[0].index", ptr %index, align 4
  %array_element_pointer = getelementptr [4 x i32], ptr %array, i32 0, i32 0
  store i32 0, ptr %array_element_pointer, align 4
  %array_element_pointer1 = getelementptr [4 x i32], ptr %array, i32 0, i32 1
  store i32 1, ptr %array_element_pointer1, align 4
  %array_element_pointer2 = getelementptr [4 x i32], ptr %array, i32 0, i32 2
  store i32 2, ptr %array_element_pointer2, align 4
  %array_element_pointer3 = getelementptr [4 x i32], ptr %array, i32 0, i32 3
  store i32 3, ptr %array_element_pointer3, align 4
  %0 = load [4 x i32], ptr %array, align 4
  store [4 x i32] %0, ptr %a, align 4
  %1 = load i32, ptr %index, align 4
  %bounds_check_index = zext i32 %1 to i64
  %bounds_check_in_bounds = icmp ult i64 %bounds_check_index, 4
  br i1 %bounds_check_in_bounds, label %bounds_check_pass, label %bounds_check_fail

bounds_check_pass:                                ; preds = %entry
  %array_element_pointer4 = getelementptr [4 x i32], ptr %a, i32 0, i32 %1
  %2 = load i32, ptr %array_element_pointer4, align 4
  ret i32 %2

bounds_check_fail:                                ; preds = %entry
  %3 = call i32 @puts(ptr @function_contract_error_string.1)
  call void @abort()
  unreachable
}

declare i32 @puts(ptr)

declare void @abort()

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir, { .enable_bounds_checks = true });
  }

  TEST_CASE("Compile Array Bounds Checks 1", "[LLVM_IR]")
  {
    char const* const input_file = "bounds_check_1.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
%__hl_soa_array = type { ptr }
%struct.Bounds_check_1_Particle = type { float, float }

@function_contract_error_string = private unnamed_addr constant [77 x i8] c"Out-of-bounds SOA array access in 'Bounds_check_1.access_soa_array_element'!\00"
@function_contract_error_string.1 = private unnamed_addr constant [78 x i8] c"Out-of-bounds SOA array access in 'Bounds_check_1.access_soa_array_member_x'!\00"
@function_contract_error_string.2 = private unnamed_addr constant [78 x i8] c"Out-of-bounds SOA array access in 'Bounds_check_1.access_soa_array_member_y'!\00"

; Function Attrs: convergent
define <2 x float> @Bounds_check_1_access_soa_array_element(i32 noundef %"arguments[0].index") #0 {
entry:
  %index = alloca i32, align 4
  %particles = alloca %__hl_soa_array, align 8
  %soa_array_storage = alloca [32 x i8], align 4
  %soa_element = alloca %struct.Bounds_check_1_Particle, align 4
  store i32 %"arguments[0].index", ptr %index, align 4
  %soa_array_data = getelementptr [32 x i8], ptr %soa_array_storage, i32 0, i32 0
  %0 = getelementptr inbounds %__hl_soa_array, ptr %particles, i32 0, i32 0
  store ptr %soa_array_data, ptr %0, align 8
  %soa_member_base_pointer = getelementptr i8, ptr %soa_array_data, i64 0
  %soa_member_element_pointer = getelementptr float, ptr %soa_member_base_pointer, i64 0
  store float 0.000000e+00, ptr %soa_member_element_pointer, align 4
  %soa_member_base_pointer1 = getelementptr i8, ptr %soa_array_data, i64 16
  %soa_member_element_pointer2 = getelementptr float, ptr %soa_member_base_pointer1, i64 0
  store float 0.000000e+00, ptr %soa_member_element_pointer2, align 4
  %soa_member_base_pointer3 = getelementptr i8, ptr %soa_array_data, i64 0
  %soa_member_element_pointer4 = getelementptr float, ptr %soa_member_base_pointer3, i64 1
  store float 0.000000e+00, ptr %soa_member_element_pointer4, align 4
  %soa_member_base_pointer5 = getelementptr i8, ptr %soa_array_data, i64 16
  %soa_member_element_pointer6 = getelementptr float, ptr %soa_member_base_pointer5, i64 1
  store float 0.000000e+00, ptr %soa_member_element_pointer6, align 4
  %soa_member_base_pointer7 = getelementptr i8, ptr %soa_array_data, i64 0
  %soa_member_element_pointer8 = getelementptr float, ptr %soa_member_base_pointer7, i64 2
  store float 0.000000e+00, ptr %soa_member_element_pointer8, align 4
  %soa_member_base_pointer9 = getelementptr i8, ptr %soa_array_data, i64 16
  %soa_member_element_pointer10 = getelementptr float, ptr %soa_member_base_pointer9, i64 2
  store float 0.000000e+00, ptr %soa_member_element_pointer10, align 4
  %soa_member_base_pointer11 = getelementptr i8, ptr %soa_array_data, i64 0
  %soa_member_element_pointer12 = getelementptr float, ptr %soa_member_base_pointer11, i64 3
  store float 0.000000e+00, ptr %soa_member_element_pointer12, align 4
  %soa_member_base_pointer13 = getelementptr i8, ptr %soa_array_data, i64 16
  %soa_member_element_pointer14 = getelementptr float, ptr %soa_member_base_pointer13, i64 3
  store float 0.000000e+00, ptr %soa_member_element_pointer14, align 4
  %1 = load i32, ptr %index, align 4
  %bounds_check_index = zext i32 %1 to i64
  %bounds_check_in_bounds = icmp ult i64 %bounds_check_index, 4
  br i1 %bounds_check_in_bounds, label %bounds_check_pass, label %bounds_check_fail

bounds_check_pass:                                ; preds = %entry
  %2 = getelementptr inbounds %__hl_soa_array, ptr %particles, i32 0, i32 0
  %3 = load ptr, ptr %2, align 8
  %soa_member_base_pointer15 = getelementptr i8, ptr %3, i64 0
  %soa_member_element_pointer16 = getelementptr float, ptr %soa_member_base_pointer15, i32 %1
  %4 = load float, ptr %soa_member_element_pointer16, align 4
  %5 = getelementptr inbounds %struct.Bounds_check_1_Particle, ptr %soa_element, i32 0, i32 0
  store float %4, ptr %5, align 4
  %soa_member_base_pointer17 = getelementptr i8, ptr %3, i64 16
  %soa_member_element_pointer18 = getelementptr float, ptr %soa_member_base_pointer17, i32 %1
  %6 = load float, ptr %soa_member_element_pointer18, align 4
  %7 = getelementptr inbounds %struct.Bounds_check_1_Particle, ptr %soa_element, i32 0, i32 1
  store float %6, ptr %7, align 4
  %8 = getelementptr inbounds %struct.Bounds_check_1_Particle, ptr %soa_element, i32 0, i32 0
  %9 = load <2 x float>, ptr %8, align 4
  ret <2 x float> %9

bounds_check_fail:                                ; preds = %entry
  %10 = call i32 @puts(ptr @function_contract_error_string)
  call void @abort()
  unreachable
}

; Function Attrs: convergent
define float @Bounds_check_1_access_soa_array_member_x(i32 noundef %"arguments[0].index") #0 {
entry:
  %index = alloca i32, align 4
  %particles = alloca %__hl_soa_array, align 8
  %soa_array_storage = alloca [32 x i8], align 4
  store i32 %"arguments[0].index", ptr %index, align 4
  %soa_array_data = getelementptr [32 x i8], ptr %soa_array_storage, i32 0, i32 0
  %0 = getelementptr inbounds %__hl_soa_array, ptr %particles, i32 0, i32 0
  store ptr %soa_array_data, ptr %0, align 8
  %soa_member_base_pointer = getelementptr i8, ptr %soa_array_data, i64 0
  %soa_member_element_pointer = getelementptr float, ptr %soa_member_base_pointer, i64 0
  store float 0.000000e+00, ptr %soa_member_element_pointer, align 4
  %soa_member_base_pointer1 = getelementptr i8, ptr %soa_array_data, i64 16
  %soa_member_element_pointer2 = getelementptr float, ptr %soa_member_base_pointer1, i64 0
  store float 0.000000e+00, ptr %soa_member_element_pointer2, align 4
  %soa_member_base_pointer3 = getelementptr i8, ptr %soa_array_data, i64 0
  %soa_member_element_pointer4 = getelementptr float, ptr %soa_member_base_pointer3, i64 1
  store float 0.000000e+00, ptr %soa_member_element_pointer4, align 4
  %soa_member_base_pointer5 = getelementptr i8, ptr %soa_array_data, i64 16
  %soa_member_element_pointer6 = getelementptr float, ptr %soa_member_base_pointer5, i64 1
  store float 0.000000e+00, ptr %soa_member_element_pointer6, align 4
  %soa_member_base_pointer7 = getelementptr i8, ptr %soa_array_data, i64 0
  %soa_member_element_pointer8 = getelementptr float, ptr %soa_member_base_pointer7, i64 2
  store float 0.000000e+00, ptr %soa_member_element_pointer8, align 4
  %soa_member_base_pointer9 = getelementptr i8, ptr %soa_array_data, i64 16
  %soa_member_element_pointer10 = getelementptr float, ptr %soa_member_base_pointer9, i64 2
  store float 0.000000e+00, ptr %soa_member_element_pointer10, align 4
  %soa_member_base_pointer11 = getelementptr i8, ptr %soa_array_data, i64 0
  %soa_member_element_pointer12 = getelementptr float, ptr %soa_member_base_pointer11, i64 3
  store float 0.000000e+00, ptr %soa_member_element_pointer12, align 4
  %soa_member_base_pointer13 = getelementptr i8, ptr %soa_array_data, i64 16
  %soa_member_element_pointer14 = getelementptr float, ptr %soa_member_base_pointer13, i64 3
  store float 0.000000e+00, ptr %soa_member_element_pointer14, align 4
  %1 = load i32, ptr %index, align 4
  %2 = getelementptr inbounds %__hl_soa_array, ptr %particles, i32 0, i32 0
  %3 = load ptr, ptr %2, align 8
  %bounds_check_index = zext i32 %1 to i64
  %bounds_check_in_bounds = icmp ult i64 %bounds_check_index, 4
  br i1 %bounds_check_in_bounds, label %bounds_check_pass, label %bounds_check_fail

bounds_check_pass:                                ; preds = %entry
  %soa_member_base_pointer15 = getelementptr i8, ptr %3, i64 0
  %soa_member_element_pointer16 = getelementptr float, ptr %soa_member_base_pointer15, i32 %1
  %4 = load float, ptr %soa_member_element_pointer16, align 4
  ret float %4

bounds_check_fail:                                ; preds = %entry
  %5 = call i32 @puts(ptr @function_contract_error_string.1)
  call void @abort()
  unreachable
}

; Function Attrs: convergent
define float @Bounds_check_1_access_soa_array_member_y(i32 noundef %"arguments[0].index") #0 {
entry:
  %index = alloca i32, align 4
  %particles = alloca %__hl_soa_array, align 8
  %soa_array_storage = alloca [32 x i8], align 4
  store i32 %"arguments[0].index", ptr %index, align 4
  %soa_array_data = getelementptr [32 x i8], ptr %soa_array_storage, i32 0, i32 0
  %0 = getelementptr inbounds %__hl_soa_array, ptr %particles, i32 0, i32 0
  store ptr %soa_array_data, ptr %0, align 8
  %soa_member_base_pointer = getelementptr i8, ptr %soa_array_data, i64 0
  %soa_member_element_pointer = getelementptr float, ptr %soa_member_base_pointer, i64 0
  store float 0.000000e+00, ptr %soa_member_element_pointer, align 4
  %soa_member_base_pointer1 = getelementptr i8, ptr %soa_array_data, i64 16
  %soa_member_element_pointer2 = getelementptr float, ptr %soa_member_base_pointer1, i64 0
  store float 0.000000e+00, ptr %soa_member_element_pointer2, align 4
  %soa_member_base_pointer3 = getelementptr i8, ptr %soa_array_data, i64 0
  %soa_member_element_pointer4 = getelementptr float, ptr %soa_member_base_pointer3, i64 1
  store float 0.000000e+00, ptr %soa_member_element_pointer4, align 4
  %soa_member_base_pointer5 = getelementptr i8, ptr %soa_array_data, i64 16
  %soa_member_element_pointer6 = getelementptr float, ptr %soa_member_base_pointer5, i64 1
  store float 0.000000e+00, ptr %soa_member_element_pointer6, align 4
  %soa_member_base_pointer7 = getelementptr i8, ptr %soa_array_data, i64 0
  %soa_member_element_pointer8 = getelementptr float, ptr %soa_member_base_pointer7, i64 2
  store float 0.000000e+00, ptr %soa_member_element_pointer8, align 4
  %soa_member_base_pointer9 = getelementptr i8, ptr %soa_array_data, i64 16
  %soa_member_element_pointer10 = getelementptr float, ptr %soa_member_base_pointer9, i64 2
  store float 0.000000e+00, ptr %soa_member_element_pointer10, align 4
  %soa_member_base_pointer11 = getelementptr i8, ptr %soa_array_data, i64 0
  %soa_member_element_pointer12 = getelementptr float, ptr %soa_member_base_pointer11, i64 3
  store float 0.000000e+00, ptr %soa_member_element_pointer12, align 4
  %soa_member_base_pointer13 = getelementptr i8, ptr %soa_array_data, i64 16
  %soa_member_element_pointer14 = getelementptr float, ptr %soa_member_base_pointer13, i64 3
  store float 0.000000e+00, ptr %soa_member_element_pointer14, align 4
  %1 = load i32, ptr %index, align 4
  %2 = getelementptr inbounds %__hl_soa_array, ptr %particles, i32 0, i32 0
  %3 = load ptr, ptr %2, align 8
  %bounds_check_index = zext i32 %1 to i64
  %bounds_check_in_bounds = icmp ult i64 %bounds_check_index, 4
  br i1 %bounds_check_in_bounds, label %bounds_check_pass, label %bounds_check_fail

bounds_check_pass:                                ; preds = %entry
  %soa_member_base_pointer15 = getelementptr i8, ptr %3, i64 16
  %soa_member_element_pointer16 = getelementptr float, ptr %soa_member_base_pointer15, i32 %1
  %4 = load float, ptr %soa_member_element_pointer16, align 4
  ret float %4

bounds_check_fail:                                ; preds = %entry
  %5 = call i32 @puts(ptr @function_contract_error_string.2)
  call void @abort()
  unreachable
}

declare i32 @puts(ptr)

declare void @abort()

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir, { .enable_bounds_checks = true });
  }

  TEST_CASE("Compile Array Bounds Checks 2", "[LLVM_IR]")
  {
    char const* const input_file = "bounds_check_2.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
%__hl_soa_array_view = type { i64, i64, i64, ptr }
%struct.Bounds_check_2_Particle = type { float, float }

@function_contract_error_string = private unnamed_addr constant [87 x i8] c"Out-of-bounds SOA array view access in 'Bounds_check_2.access_soa_array_view_element'!\00"
@function_contract_error_string.1 = private unnamed_addr constant [88 x i8] c"Out-of-bounds SOA array view access in 'Bounds_check_2.access_soa_array_view_member_x'!\00"
@function_contract_error_string.2 = private unnamed_addr constant [88 x i8] c"Out-of-bounds SOA array view access in 'Bounds_check_2.access_soa_array_view_member_y'!\00"

; Function Attrs: convergent
define <2 x float> @Bounds_check_2_access_soa_array_view_element(ptr noundef byval(%__hl_soa_array_view) align 8 %"arguments[0].view", i32 noundef %"arguments[1].index") #0 {
entry:
  %index = alloca i32, align 4
  %soa_element = alloca %struct.Bounds_check_2_Particle, align 4
  store i32 %"arguments[1].index", ptr %index, align 4
  %0 = load i32, ptr %index, align 4
  %1 = getelementptr inbounds %__hl_soa_array_view, ptr %"arguments[0].view", i32 0, i32 3
  %2 = load ptr, ptr %1, align 8
  %3 = getelementptr inbounds %__hl_soa_array_view, ptr %"arguments[0].view", i32 0, i32 2
  %4 = load i64, ptr %3, align 8
  %bounds_check_index = zext i32 %0 to i64
  %bounds_check_in_bounds = icmp ult i64 %bounds_check_index, %4
  br i1 %bounds_check_in_bounds, label %bounds_check_pass, label %bounds_check_fail

bounds_check_pass:                                ; preds = %entry
  %5 = getelementptr inbounds %__hl_soa_array_view, ptr %"arguments[0].view", i32 0, i32 0
  %6 = load i64, ptr %5, align 8
  %soa_index_i64 = zext i32 %0 to i64
  %soa_adjusted_index = add i64 %6, %soa_index_i64
  %soa_member_base_pointer = getelementptr i8, ptr %2, i64 0
  %soa_member_element_pointer = getelementptr float, ptr %soa_member_base_pointer, i64 %soa_adjusted_index
  %7 = load float, ptr %soa_member_element_pointer, align 4
  %8 = getelementptr inbounds %struct.Bounds_check_2_Particle, ptr %soa_element, i32 0, i32 0
  store float %7, ptr %8, align 4
  %soa_member_block_size = mul i64 %4, 4
  %soa_member_block_offset = add i64 0, %soa_member_block_size
  %soa_offset_adjusted = add i64 %soa_member_block_offset, 3
  %soa_offset_aligned = and i64 %soa_offset_adjusted, -4
  %soa_member_base_pointer1 = getelementptr i8, ptr %2, i64 %soa_offset_aligned
  %soa_member_element_pointer2 = getelementptr float, ptr %soa_member_base_pointer1, i64 %soa_adjusted_index
  %9 = load float, ptr %soa_member_element_pointer2, align 4
  %10 = getelementptr inbounds %struct.Bounds_check_2_Particle, ptr %soa_element, i32 0, i32 1
  store float %9, ptr %10, align 4
  %11 = getelementptr inbounds %struct.Bounds_check_2_Particle, ptr %soa_element, i32 0, i32 0
  %12 = load <2 x float>, ptr %11, align 4
  ret <2 x float> %12

bounds_check_fail:                                ; preds = %entry
  %13 = call i32 @puts(ptr @function_contract_error_string)
  call void @abort()
  unreachable
}

; Function Attrs: convergent
define float @Bounds_check_2_access_soa_array_view_member_x(ptr noundef byval(%__hl_soa_array_view) align 8 %"arguments[0].view", i32 noundef %"arguments[1].index") #0 {
entry:
  %index = alloca i32, align 4
  store i32 %"arguments[1].index", ptr %index, align 4
  %0 = load i32, ptr %index, align 4
  %1 = getelementptr inbounds %__hl_soa_array_view, ptr %"arguments[0].view", i32 0, i32 3
  %2 = load ptr, ptr %1, align 8
  %3 = getelementptr inbounds %__hl_soa_array_view, ptr %"arguments[0].view", i32 0, i32 2
  %4 = load i64, ptr %3, align 8
  %bounds_check_index = zext i32 %0 to i64
  %bounds_check_in_bounds = icmp ult i64 %bounds_check_index, %4
  br i1 %bounds_check_in_bounds, label %bounds_check_pass, label %bounds_check_fail

bounds_check_pass:                                ; preds = %entry
  %5 = getelementptr inbounds %__hl_soa_array_view, ptr %"arguments[0].view", i32 0, i32 0
  %6 = load i64, ptr %5, align 8
  %soa_index_i64 = zext i32 %0 to i64
  %soa_adjusted_index = add i64 %6, %soa_index_i64
  %soa_member_base_pointer = getelementptr i8, ptr %2, i64 0
  %soa_member_element_pointer = getelementptr float, ptr %soa_member_base_pointer, i64 %soa_adjusted_index
  %7 = load float, ptr %soa_member_element_pointer, align 4
  ret float %7

bounds_check_fail:                                ; preds = %entry
  %8 = call i32 @puts(ptr @function_contract_error_string.1)
  call void @abort()
  unreachable
}

; Function Attrs: convergent
define float @Bounds_check_2_access_soa_array_view_member_y(ptr noundef byval(%__hl_soa_array_view) align 8 %"arguments[0].view", i32 noundef %"arguments[1].index") #0 {
entry:
  %index = alloca i32, align 4
  store i32 %"arguments[1].index", ptr %index, align 4
  %0 = load i32, ptr %index, align 4
  %1 = getelementptr inbounds %__hl_soa_array_view, ptr %"arguments[0].view", i32 0, i32 3
  %2 = load ptr, ptr %1, align 8
  %3 = getelementptr inbounds %__hl_soa_array_view, ptr %"arguments[0].view", i32 0, i32 2
  %4 = load i64, ptr %3, align 8
  %bounds_check_index = zext i32 %0 to i64
  %bounds_check_in_bounds = icmp ult i64 %bounds_check_index, %4
  br i1 %bounds_check_in_bounds, label %bounds_check_pass, label %bounds_check_fail

bounds_check_pass:                                ; preds = %entry
  %5 = getelementptr inbounds %__hl_soa_array_view, ptr %"arguments[0].view", i32 0, i32 0
  %6 = load i64, ptr %5, align 8
  %soa_index_i64 = zext i32 %0 to i64
  %soa_adjusted_index = add i64 %6, %soa_index_i64
  %soa_member_block_size = mul i64 %4, 4
  %soa_member_block_offset = add i64 0, %soa_member_block_size
  %soa_offset_adjusted = add i64 %soa_member_block_offset, 3
  %soa_offset_aligned = and i64 %soa_offset_adjusted, -4
  %soa_member_base_pointer = getelementptr i8, ptr %2, i64 %soa_offset_aligned
  %soa_member_element_pointer = getelementptr float, ptr %soa_member_base_pointer, i64 %soa_adjusted_index
  %7 = load float, ptr %soa_member_element_pointer, align 4
  ret float %7

bounds_check_fail:                                ; preds = %entry
  %8 = call i32 @puts(ptr @function_contract_error_string.2)
  call void @abort()
  unreachable
}

declare i32 @puts(ptr)

declare void @abort()

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir, { .enable_bounds_checks = true });
  }

  TEST_CASE("Compile Constant Arrays", "[LLVM_IR]")
  {
    char const* const input_file = "constant_array_expressions.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
%struct.Constant_array_expressions_My_struct = type { [4 x i32] }

; Function Attrs: convergent
define void @Constant_array_expressions_foo() #0 {
entry:
  %a = alloca [0 x i32], align 4
  %b = alloca [0 x i32], align 4
  %array = alloca [4 x i32], i64 4, align 4
  %c = alloca [4 x i32], align 4
  %d = alloca i32, align 4
  %instance = alloca %struct.Constant_array_expressions_My_struct, align 4
  %array7 = alloca [4 x i32], i64 4, align 4
  %e = alloca i32, align 4
  %array13 = alloca [8 x i32], i64 8, align 4
  %f = alloca [8 x i32], align 4
  %array_element_pointer = getelementptr [4 x i32], ptr %array, i32 0, i32 0
  store i32 0, ptr %array_element_pointer, align 4
  %array_element_pointer1 = getelementptr [4 x i32], ptr %array, i32 0, i32 1
  store i32 1, ptr %array_element_pointer1, align 4
  %array_element_pointer2 = getelementptr [4 x i32], ptr %array, i32 0, i32 2
  store i32 2, ptr %array_element_pointer2, align 4
  %array_element_pointer3 = getelementptr [4 x i32], ptr %array, i32 0, i32 3
  store i32 3, ptr %array_element_pointer3, align 4
  %0 = load [4 x i32], ptr %array, align 4
  store [4 x i32] %0, ptr %c, align 4
  %array_element_pointer4 = getelementptr [4 x i32], ptr %c, i32 0, i32 0
  store i32 0, ptr %array_element_pointer4, align 4
  %array_element_pointer5 = getelementptr [4 x i32], ptr %c, i32 0, i32 1
  store i32 1, ptr %array_element_pointer5, align 4
  %array_element_pointer6 = getelementptr [4 x i32], ptr %c, i32 0, i32 3
  %1 = load i32, ptr %array_element_pointer6, align 4
  store i32 %1, ptr %d, align 4
  %array_element_pointer8 = getelementptr [4 x i32], ptr %array7, i32 0, i32 0
  store i32 0, ptr %array_element_pointer8, align 4
  %array_element_pointer9 = getelementptr [4 x i32], ptr %array7, i32 0, i32 1
  store i32 2, ptr %array_element_pointer9, align 4
  %array_element_pointer10 = getelementptr [4 x i32], ptr %array7, i32 0, i32 2
  store i32 4, ptr %array_element_pointer10, align 4
  %array_element_pointer11 = getelementptr [4 x i32], ptr %array7, i32 0, i32 3
  store i32 6, ptr %array_element_pointer11, align 4
  %2 = load [4 x i32], ptr %array7, align 4
  %3 = getelementptr inbounds %struct.Constant_array_expressions_My_struct, ptr %instance, i32 0, i32 0
  store [4 x i32] %2, ptr %3, align 4
  %4 = getelementptr inbounds %struct.Constant_array_expressions_My_struct, ptr %instance, i32 0, i32 0
  %array_element_pointer12 = getelementptr [4 x i32], ptr %4, i32 0, i32 0
  %5 = load i32, ptr %array_element_pointer12, align 4
  store i32 %5, ptr %e, align 4
  call void @llvm.memset.p0.i64(ptr align 4 %array13, i8 0, i64 32, i1 false)
  %6 = load [8 x i32], ptr %array13, align 4
  store [8 x i32] %6, ptr %f, align 4
  ret void
}

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: write)
declare void @llvm.memset.p0.i64(ptr nocapture writeonly, i8, i64, i1 immarg) #1

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
attributes #1 = { nocallback nofree nounwind willreturn memory(argmem: write) }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Soa Array Type", "[LLVM_IR]")
  {
    char const* const input_file = "soa_array_type.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
%__hl_soa_array = type { ptr }
%struct.soa_array_type_Particle = type { float, float }

; Function Attrs: convergent
define private void @soa_array_type_run() #0 {
entry:
  %particles = alloca %__hl_soa_array, align 8
  %soa_array_storage = alloca [32 x i8], align 4
  %soa_element = alloca %struct.soa_array_type_Particle, align 4
  %p1 = alloca %struct.soa_array_type_Particle, align 4
  %soa_element19 = alloca %struct.soa_array_type_Particle, align 4
  %0 = alloca %struct.soa_array_type_Particle, align 4
  %soa_assignment_struct = alloca %struct.soa_array_type_Particle, align 4
  %x2 = alloca float, align 4
  %y3 = alloca float, align 4
  %length = alloca i64, align 8
  %data = alloca ptr, align 8
  %soa_array_data = getelementptr [32 x i8], ptr %soa_array_storage, i32 0, i32 0
  %1 = getelementptr inbounds %__hl_soa_array, ptr %particles, i32 0, i32 0
  store ptr %soa_array_data, ptr %1, align 8
  %soa_member_base_pointer = getelementptr i8, ptr %soa_array_data, i64 0
  %soa_member_element_pointer = getelementptr float, ptr %soa_member_base_pointer, i64 0
  store float 0.000000e+00, ptr %soa_member_element_pointer, align 4
  %soa_member_base_pointer1 = getelementptr i8, ptr %soa_array_data, i64 16
  %soa_member_element_pointer2 = getelementptr float, ptr %soa_member_base_pointer1, i64 0
  store float 0.000000e+00, ptr %soa_member_element_pointer2, align 4
  %soa_member_base_pointer3 = getelementptr i8, ptr %soa_array_data, i64 0
  %soa_member_element_pointer4 = getelementptr float, ptr %soa_member_base_pointer3, i64 1
  store float 0.000000e+00, ptr %soa_member_element_pointer4, align 4
  %soa_member_base_pointer5 = getelementptr i8, ptr %soa_array_data, i64 16
  %soa_member_element_pointer6 = getelementptr float, ptr %soa_member_base_pointer5, i64 1
  store float 0.000000e+00, ptr %soa_member_element_pointer6, align 4
  %soa_member_base_pointer7 = getelementptr i8, ptr %soa_array_data, i64 0
  %soa_member_element_pointer8 = getelementptr float, ptr %soa_member_base_pointer7, i64 2
  store float 0.000000e+00, ptr %soa_member_element_pointer8, align 4
  %soa_member_base_pointer9 = getelementptr i8, ptr %soa_array_data, i64 16
  %soa_member_element_pointer10 = getelementptr float, ptr %soa_member_base_pointer9, i64 2
  store float 0.000000e+00, ptr %soa_member_element_pointer10, align 4
  %soa_member_base_pointer11 = getelementptr i8, ptr %soa_array_data, i64 0
  %soa_member_element_pointer12 = getelementptr float, ptr %soa_member_base_pointer11, i64 3
  store float 0.000000e+00, ptr %soa_member_element_pointer12, align 4
  %soa_member_base_pointer13 = getelementptr i8, ptr %soa_array_data, i64 16
  %soa_member_element_pointer14 = getelementptr float, ptr %soa_member_base_pointer13, i64 3
  store float 0.000000e+00, ptr %soa_member_element_pointer14, align 4
  %2 = getelementptr inbounds %__hl_soa_array, ptr %particles, i32 0, i32 0
  %3 = load ptr, ptr %2, align 8
  %soa_member_base_pointer15 = getelementptr i8, ptr %3, i64 0
  %soa_member_element_pointer16 = getelementptr float, ptr %soa_member_base_pointer15, i32 1
  %4 = load float, ptr %soa_member_element_pointer16, align 4
  %5 = getelementptr inbounds %struct.soa_array_type_Particle, ptr %soa_element, i32 0, i32 0
  store float %4, ptr %5, align 4
  %soa_member_base_pointer17 = getelementptr i8, ptr %3, i64 16
  %soa_member_element_pointer18 = getelementptr float, ptr %soa_member_base_pointer17, i32 1
  %6 = load float, ptr %soa_member_element_pointer18, align 4
  %7 = getelementptr inbounds %struct.soa_array_type_Particle, ptr %soa_element, i32 0, i32 1
  store float %6, ptr %7, align 4
  %8 = load %struct.soa_array_type_Particle, ptr %soa_element, align 4
  store %struct.soa_array_type_Particle %8, ptr %p1, align 4
  %9 = getelementptr inbounds %__hl_soa_array, ptr %particles, i32 0, i32 0
  %10 = load ptr, ptr %9, align 8
  %soa_member_base_pointer20 = getelementptr i8, ptr %10, i64 0
  %soa_member_element_pointer21 = getelementptr float, ptr %soa_member_base_pointer20, i32 1
  %11 = load float, ptr %soa_member_element_pointer21, align 4
  %12 = getelementptr inbounds %struct.soa_array_type_Particle, ptr %soa_element19, i32 0, i32 0
  store float %11, ptr %12, align 4
  %soa_member_base_pointer22 = getelementptr i8, ptr %10, i64 16
  %soa_member_element_pointer23 = getelementptr float, ptr %soa_member_base_pointer22, i32 1
  %13 = load float, ptr %soa_member_element_pointer23, align 4
  %14 = getelementptr inbounds %struct.soa_array_type_Particle, ptr %soa_element19, i32 0, i32 1
  store float %13, ptr %14, align 4
  %15 = getelementptr inbounds %struct.soa_array_type_Particle, ptr %0, i32 0, i32 0
  store float 3.000000e+00, ptr %15, align 4
  %16 = getelementptr inbounds %struct.soa_array_type_Particle, ptr %0, i32 0, i32 1
  store float 4.000000e+00, ptr %16, align 4
  %17 = load %struct.soa_array_type_Particle, ptr %0, align 4
  %18 = getelementptr inbounds %__hl_soa_array, ptr %particles, i32 0, i32 0
  %19 = load ptr, ptr %18, align 8
  store %struct.soa_array_type_Particle %17, ptr %soa_assignment_struct, align 4
  %20 = getelementptr inbounds %struct.soa_array_type_Particle, ptr %soa_assignment_struct, i32 0, i32 0
  %soa_member_base_pointer24 = getelementptr i8, ptr %19, i64 0
  %soa_member_element_pointer25 = getelementptr float, ptr %soa_member_base_pointer24, i32 1
  %21 = load float, ptr %20, align 4
  store float %21, ptr %soa_member_element_pointer25, align 4
  %22 = getelementptr inbounds %struct.soa_array_type_Particle, ptr %soa_assignment_struct, i32 0, i32 1
  %soa_member_base_pointer26 = getelementptr i8, ptr %19, i64 16
  %soa_member_element_pointer27 = getelementptr float, ptr %soa_member_base_pointer26, i32 1
  %23 = load float, ptr %22, align 4
  store float %23, ptr %soa_member_element_pointer27, align 4
  %24 = getelementptr inbounds %__hl_soa_array, ptr %particles, i32 0, i32 0
  %25 = load ptr, ptr %24, align 8
  %soa_member_base_pointer28 = getelementptr i8, ptr %25, i64 0
  %soa_member_element_pointer29 = getelementptr float, ptr %soa_member_base_pointer28, i32 2
  %26 = load float, ptr %soa_member_element_pointer29, align 4
  store float %26, ptr %x2, align 4
  %27 = getelementptr inbounds %__hl_soa_array, ptr %particles, i32 0, i32 0
  %28 = load ptr, ptr %27, align 8
  %soa_member_base_pointer30 = getelementptr i8, ptr %28, i64 0
  %soa_member_element_pointer31 = getelementptr float, ptr %soa_member_base_pointer30, i32 2
  store float 1.000000e+00, ptr %soa_member_element_pointer31, align 4
  %29 = getelementptr inbounds %__hl_soa_array, ptr %particles, i32 0, i32 0
  %30 = load ptr, ptr %29, align 8
  %soa_member_base_pointer32 = getelementptr i8, ptr %30, i64 16
  %soa_member_element_pointer33 = getelementptr float, ptr %soa_member_base_pointer32, i32 3
  %31 = load float, ptr %soa_member_element_pointer33, align 4
  store float %31, ptr %y3, align 4
  %32 = getelementptr inbounds %__hl_soa_array, ptr %particles, i32 0, i32 0
  %33 = load ptr, ptr %32, align 8
  %soa_member_base_pointer34 = getelementptr i8, ptr %33, i64 16
  %soa_member_element_pointer35 = getelementptr float, ptr %soa_member_base_pointer34, i32 3
  store float 2.000000e+00, ptr %soa_member_element_pointer35, align 4
  store i64 4, ptr %length, align 8
  %34 = getelementptr inbounds %__hl_soa_array, ptr %particles, i32 0, i32 0
  %35 = load ptr, ptr %34, align 8
  store ptr %35, ptr %data, align 8
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Soa Array Type Debug Information", "[LLVM_IR]")
  {
    char const* const input_file = "soa_array_type_debug_information.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    std::string const expected_llvm_ir = std::format(R"(
%__hl_soa_array = type {{ ptr }}

; Function Attrs: convergent
define private void @soa_array_type_debug_information_run() #0 !dbg !3 {{
entry:
  %particles = alloca %__hl_soa_array, align 8, !dbg !7
  %soa_array_storage = alloca [32 x i8], align 4, !dbg !7
  %soa_array_data = getelementptr [32 x i8], ptr %soa_array_storage, i32 0, i32 0, !dbg !7
  %0 = getelementptr inbounds %__hl_soa_array, ptr %particles, i32 0, i32 0, !dbg !7
  store ptr %soa_array_data, ptr %0, align 8, !dbg !7
  %soa_member_base_pointer = getelementptr i8, ptr %soa_array_data, i64 0, !dbg !7
  %soa_member_element_pointer = getelementptr float, ptr %soa_member_base_pointer, i64 0, !dbg !7
  store float 0.000000e+00, ptr %soa_member_element_pointer, align 4, !dbg !7
  %soa_member_base_pointer1 = getelementptr i8, ptr %soa_array_data, i64 16, !dbg !7
  %soa_member_element_pointer2 = getelementptr float, ptr %soa_member_base_pointer1, i64 0, !dbg !7
  store float 0.000000e+00, ptr %soa_member_element_pointer2, align 4, !dbg !7
  %soa_member_base_pointer3 = getelementptr i8, ptr %soa_array_data, i64 0, !dbg !7
  %soa_member_element_pointer4 = getelementptr float, ptr %soa_member_base_pointer3, i64 1, !dbg !7
  store float 0.000000e+00, ptr %soa_member_element_pointer4, align 4, !dbg !7
  %soa_member_base_pointer5 = getelementptr i8, ptr %soa_array_data, i64 16, !dbg !7
  %soa_member_element_pointer6 = getelementptr float, ptr %soa_member_base_pointer5, i64 1, !dbg !7
  store float 0.000000e+00, ptr %soa_member_element_pointer6, align 4, !dbg !7
  %soa_member_base_pointer7 = getelementptr i8, ptr %soa_array_data, i64 0, !dbg !7
  %soa_member_element_pointer8 = getelementptr float, ptr %soa_member_base_pointer7, i64 2, !dbg !7
  store float 0.000000e+00, ptr %soa_member_element_pointer8, align 4, !dbg !7
  %soa_member_base_pointer9 = getelementptr i8, ptr %soa_array_data, i64 16, !dbg !7
  %soa_member_element_pointer10 = getelementptr float, ptr %soa_member_base_pointer9, i64 2, !dbg !7
  store float 0.000000e+00, ptr %soa_member_element_pointer10, align 4, !dbg !7
  %soa_member_base_pointer11 = getelementptr i8, ptr %soa_array_data, i64 0, !dbg !7
  %soa_member_element_pointer12 = getelementptr float, ptr %soa_member_base_pointer11, i64 3, !dbg !7
  store float 0.000000e+00, ptr %soa_member_element_pointer12, align 4, !dbg !7
  %soa_member_base_pointer13 = getelementptr i8, ptr %soa_array_data, i64 16, !dbg !7
  %soa_member_element_pointer14 = getelementptr float, ptr %soa_member_base_pointer13, i64 3, !dbg !7
  store float 0.000000e+00, ptr %soa_member_element_pointer14, align 4, !dbg !7
  call void @llvm.dbg.declare(metadata ptr %particles, metadata !8, metadata !DIExpression()), !dbg !7
  ret void, !dbg !7
}}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

attributes #0 = {{ convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }}
attributes #1 = {{ nocallback nofree nosync nounwind speculatable willreturn memory(none) }}

!llvm.module.flags = !{{!0}}
!llvm.dbg.cu = !{{!1}}

!0 = !{{i32 2, !"Debug Info Version", i32 3}}
!1 = distinct !DICompileUnit(language: DW_LANG_C, file: !2, producer: "Iris Compiler", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!2 = !DIFile(filename: "soa_array_type_debug_information.iris", directory: "{}")
!3 = distinct !DISubprogram(name: "run", linkageName: "soa_array_type_debug_information_run", scope: null, file: !2, line: 9, type: !4, scopeLine: 10, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !1, retainedNodes: !6)
!4 = !DISubroutineType(types: !5)
!5 = !{{null}}
!6 = !{{}}
!7 = !DILocation(line: 11, column: 5, scope: !3)
!8 = !DILocalVariable(name: "particles", scope: !3, file: !2, line: 11, type: !9)
!9 = !DICompositeType(tag: DW_TAG_structure_type, name: "Soa_array<2,soa_array_type_debug_information_Particle,4,x,float,y,float>", scope: !10, size: 64, align: 64, elements: !11)
!10 = !DINamespace(name: "h", scope: null)
!11 = !{{!12}}
!12 = !DIDerivedType(tag: DW_TAG_member, name: "data", scope: !10, baseType: !13, size: 64, align: 64)
!13 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !14, size: 64)
!14 = !DIBasicType(name: "uint8_t", size: 8, encoding: DW_ATE_unsigned_char)
)", g_test_source_files_path.generic_string());

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir, { .debug = true });
  }

  TEST_CASE("Compile Soa Array View Type", "[LLVM_IR]")
  {
    char const* const input_file = "soa_array_view_type.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
%__hl_soa_array = type { ptr }
%__hl_soa_array_view = type { i64, i64, i64, ptr }
%struct.soa_array_view_type_Particle = type { float, float }

; Function Attrs: convergent
define private void @soa_array_view_type_run() #0 {
entry:
  %soa_array = alloca %__hl_soa_array, align 8
  %soa_array_storage = alloca [32 x i8], align 4
  %particles = alloca %__hl_soa_array, align 8
  %soa_array_view = alloca %__hl_soa_array_view, align 8
  %view = alloca %__hl_soa_array_view, align 8
  %start_index = alloca i64, align 8
  %end_index = alloca i64, align 8
  %length = alloca i64, align 8
  %data = alloca ptr, align 8
  %soa_element = alloca %struct.soa_array_view_type_Particle, align 4
  %0 = alloca %struct.soa_array_view_type_Particle, align 4
  %soa_assignment_struct = alloca %struct.soa_array_view_type_Particle, align 4
  %soa_element39 = alloca %struct.soa_array_view_type_Particle, align 4
  %p_0 = alloca %struct.soa_array_view_type_Particle, align 4
  %x0 = alloca float, align 4
  %y1 = alloca float, align 4
  %soa_array_view58 = alloca %__hl_soa_array_view, align 8
  %full_view = alloca %__hl_soa_array_view, align 8
  %soa_array_data = getelementptr [32 x i8], ptr %soa_array_storage, i32 0, i32 0
  %1 = getelementptr inbounds %__hl_soa_array, ptr %soa_array, i32 0, i32 0
  store ptr %soa_array_data, ptr %1, align 8
  %soa_member_base_pointer = getelementptr i8, ptr %soa_array_data, i64 0
  %soa_member_element_pointer = getelementptr float, ptr %soa_member_base_pointer, i64 0
  store float 0.000000e+00, ptr %soa_member_element_pointer, align 4
  %soa_member_base_pointer1 = getelementptr i8, ptr %soa_array_data, i64 16
  %soa_member_element_pointer2 = getelementptr float, ptr %soa_member_base_pointer1, i64 0
  store float 0.000000e+00, ptr %soa_member_element_pointer2, align 4
  %soa_member_base_pointer3 = getelementptr i8, ptr %soa_array_data, i64 0
  %soa_member_element_pointer4 = getelementptr float, ptr %soa_member_base_pointer3, i64 1
  store float 0.000000e+00, ptr %soa_member_element_pointer4, align 4
  %soa_member_base_pointer5 = getelementptr i8, ptr %soa_array_data, i64 16
  %soa_member_element_pointer6 = getelementptr float, ptr %soa_member_base_pointer5, i64 1
  store float 0.000000e+00, ptr %soa_member_element_pointer6, align 4
  %soa_member_base_pointer7 = getelementptr i8, ptr %soa_array_data, i64 0
  %soa_member_element_pointer8 = getelementptr float, ptr %soa_member_base_pointer7, i64 2
  store float 0.000000e+00, ptr %soa_member_element_pointer8, align 4
  %soa_member_base_pointer9 = getelementptr i8, ptr %soa_array_data, i64 16
  %soa_member_element_pointer10 = getelementptr float, ptr %soa_member_base_pointer9, i64 2
  store float 0.000000e+00, ptr %soa_member_element_pointer10, align 4
  %soa_member_base_pointer11 = getelementptr i8, ptr %soa_array_data, i64 0
  %soa_member_element_pointer12 = getelementptr float, ptr %soa_member_base_pointer11, i64 3
  store float 0.000000e+00, ptr %soa_member_element_pointer12, align 4
  %soa_member_base_pointer13 = getelementptr i8, ptr %soa_array_data, i64 16
  %soa_member_element_pointer14 = getelementptr float, ptr %soa_member_base_pointer13, i64 3
  store float 0.000000e+00, ptr %soa_member_element_pointer14, align 4
  %2 = load %__hl_soa_array, ptr %soa_array, align 8
  store %__hl_soa_array %2, ptr %particles, align 8
  %3 = getelementptr inbounds %__hl_soa_array, ptr %particles, i32 0, i32 0
  %4 = load ptr, ptr %3, align 8
  %5 = getelementptr inbounds %__hl_soa_array_view, ptr %soa_array_view, i32 0, i32 0
  store i32 1, ptr %5, align 4
  %6 = getelementptr inbounds %__hl_soa_array_view, ptr %soa_array_view, i32 0, i32 1
  store i32 3, ptr %6, align 4
  %7 = getelementptr inbounds %__hl_soa_array_view, ptr %soa_array_view, i32 0, i32 2
  store i64 4, ptr %7, align 8
  %8 = getelementptr inbounds %__hl_soa_array_view, ptr %soa_array_view, i32 0, i32 3
  store ptr %4, ptr %8, align 8
  %9 = load %__hl_soa_array_view, ptr %soa_array_view, align 8
  store %__hl_soa_array_view %9, ptr %view, align 8
  %10 = getelementptr inbounds %__hl_soa_array_view, ptr %view, i32 0, i32 0
  %11 = load i64, ptr %10, align 8
  store i64 %11, ptr %start_index, align 8
  %12 = getelementptr inbounds %__hl_soa_array_view, ptr %view, i32 0, i32 1
  %13 = load i64, ptr %12, align 8
  store i64 %13, ptr %end_index, align 8
  %14 = getelementptr inbounds %__hl_soa_array_view, ptr %view, i32 0, i32 2
  %15 = load i64, ptr %14, align 8
  store i64 %15, ptr %length, align 8
  %16 = getelementptr inbounds %__hl_soa_array_view, ptr %view, i32 0, i32 3
  %17 = load ptr, ptr %16, align 8
  store ptr %17, ptr %data, align 8
  %18 = getelementptr inbounds %__hl_soa_array_view, ptr %view, i32 0, i32 3
  %19 = load ptr, ptr %18, align 8
  %20 = getelementptr inbounds %__hl_soa_array_view, ptr %view, i32 0, i32 2
  %21 = load i64, ptr %20, align 8
  %22 = getelementptr inbounds %__hl_soa_array_view, ptr %view, i32 0, i32 0
  %23 = load i64, ptr %22, align 8
  %soa_adjusted_index = add i64 %23, 0
  %soa_member_base_pointer15 = getelementptr i8, ptr %19, i64 0
  %soa_member_element_pointer16 = getelementptr float, ptr %soa_member_base_pointer15, i64 %soa_adjusted_index
  %24 = load float, ptr %soa_member_element_pointer16, align 4
  %25 = getelementptr inbounds %struct.soa_array_view_type_Particle, ptr %soa_element, i32 0, i32 0
  store float %24, ptr %25, align 4
  %soa_member_block_size = mul i64 %21, 4
  %soa_member_block_offset = add i64 0, %soa_member_block_size
  %soa_offset_adjusted = add i64 %soa_member_block_offset, 3
  %soa_offset_aligned = and i64 %soa_offset_adjusted, -4
  %soa_member_base_pointer17 = getelementptr i8, ptr %19, i64 %soa_offset_aligned
  %soa_member_element_pointer18 = getelementptr float, ptr %soa_member_base_pointer17, i64 %soa_adjusted_index
  %26 = load float, ptr %soa_member_element_pointer18, align 4
  %27 = getelementptr inbounds %struct.soa_array_view_type_Particle, ptr %soa_element, i32 0, i32 1
  store float %26, ptr %27, align 4
  %28 = getelementptr inbounds %struct.soa_array_view_type_Particle, ptr %0, i32 0, i32 0
  store float 3.000000e+00, ptr %28, align 4
  %29 = getelementptr inbounds %struct.soa_array_view_type_Particle, ptr %0, i32 0, i32 1
  store float 4.000000e+00, ptr %29, align 4
  %30 = load %struct.soa_array_view_type_Particle, ptr %0, align 4
  %31 = getelementptr inbounds %__hl_soa_array_view, ptr %view, i32 0, i32 3
  %32 = load ptr, ptr %31, align 8
  %33 = getelementptr inbounds %__hl_soa_array_view, ptr %view, i32 0, i32 2
  %34 = load i64, ptr %33, align 8
  %35 = getelementptr inbounds %__hl_soa_array_view, ptr %view, i32 0, i32 0
  %36 = load i64, ptr %35, align 8
  %soa_adjusted_index19 = add i64 %36, 0
  store %struct.soa_array_view_type_Particle %30, ptr %soa_assignment_struct, align 4
  %37 = getelementptr inbounds %struct.soa_array_view_type_Particle, ptr %soa_assignment_struct, i32 0, i32 0
  %soa_member_base_pointer20 = getelementptr i8, ptr %32, i64 0
  %soa_member_element_pointer21 = getelementptr float, ptr %soa_member_base_pointer20, i64 %soa_adjusted_index19
  %38 = load float, ptr %37, align 4
  store float %38, ptr %soa_member_element_pointer21, align 4
  %39 = getelementptr inbounds %struct.soa_array_view_type_Particle, ptr %soa_assignment_struct, i32 0, i32 1
  %soa_member_block_size22 = mul i64 %34, 4
  %soa_member_block_offset23 = add i64 0, %soa_member_block_size22
  %soa_offset_adjusted24 = add i64 %soa_member_block_offset23, 3
  %soa_offset_aligned25 = and i64 %soa_offset_adjusted24, -4
  %soa_member_base_pointer26 = getelementptr i8, ptr %32, i64 %soa_offset_aligned25
  %soa_member_element_pointer27 = getelementptr float, ptr %soa_member_base_pointer26, i64 %soa_adjusted_index19
  %40 = load float, ptr %39, align 4
  store float %40, ptr %soa_member_element_pointer27, align 4
  %41 = getelementptr inbounds %__hl_soa_array_view, ptr %view, i32 0, i32 3
  %42 = load ptr, ptr %41, align 8
  %43 = getelementptr inbounds %__hl_soa_array_view, ptr %view, i32 0, i32 2
  %44 = load i64, ptr %43, align 8
  %45 = getelementptr inbounds %__hl_soa_array_view, ptr %view, i32 0, i32 0
  %46 = load i64, ptr %45, align 8
  %soa_adjusted_index28 = add i64 %46, 0
  %soa_member_base_pointer29 = getelementptr i8, ptr %42, i64 0
  %soa_member_element_pointer30 = getelementptr float, ptr %soa_member_base_pointer29, i64 %soa_adjusted_index28
  store float 1.000000e+00, ptr %soa_member_element_pointer30, align 4
  %47 = getelementptr inbounds %__hl_soa_array_view, ptr %view, i32 0, i32 3
  %48 = load ptr, ptr %47, align 8
  %49 = getelementptr inbounds %__hl_soa_array_view, ptr %view, i32 0, i32 2
  %50 = load i64, ptr %49, align 8
  %51 = getelementptr inbounds %__hl_soa_array_view, ptr %view, i32 0, i32 0
  %52 = load i64, ptr %51, align 8
  %soa_adjusted_index31 = add i64 %52, 1
  %soa_member_block_size32 = mul i64 %50, 4
  %soa_member_block_offset33 = add i64 0, %soa_member_block_size32
  %soa_offset_adjusted34 = add i64 %soa_member_block_offset33, 3
  %soa_offset_aligned35 = and i64 %soa_offset_adjusted34, -4
  %soa_member_base_pointer36 = getelementptr i8, ptr %48, i64 %soa_offset_aligned35
  %soa_member_element_pointer37 = getelementptr float, ptr %soa_member_base_pointer36, i64 %soa_adjusted_index31
  store float 2.000000e+00, ptr %soa_member_element_pointer37, align 4
  %53 = getelementptr inbounds %__hl_soa_array_view, ptr %view, i32 0, i32 3
  %54 = load ptr, ptr %53, align 8
  %55 = getelementptr inbounds %__hl_soa_array_view, ptr %view, i32 0, i32 2
  %56 = load i64, ptr %55, align 8
  %57 = getelementptr inbounds %__hl_soa_array_view, ptr %view, i32 0, i32 0
  %58 = load i64, ptr %57, align 8
  %soa_adjusted_index38 = add i64 %58, 0
  %soa_member_base_pointer40 = getelementptr i8, ptr %54, i64 0
  %soa_member_element_pointer41 = getelementptr float, ptr %soa_member_base_pointer40, i64 %soa_adjusted_index38
  %59 = load float, ptr %soa_member_element_pointer41, align 4
  %60 = getelementptr inbounds %struct.soa_array_view_type_Particle, ptr %soa_element39, i32 0, i32 0
  store float %59, ptr %60, align 4
  %soa_member_block_size42 = mul i64 %56, 4
  %soa_member_block_offset43 = add i64 0, %soa_member_block_size42
  %soa_offset_adjusted44 = add i64 %soa_member_block_offset43, 3
  %soa_offset_aligned45 = and i64 %soa_offset_adjusted44, -4
  %soa_member_base_pointer46 = getelementptr i8, ptr %54, i64 %soa_offset_aligned45
  %soa_member_element_pointer47 = getelementptr float, ptr %soa_member_base_pointer46, i64 %soa_adjusted_index38
  %61 = load float, ptr %soa_member_element_pointer47, align 4
  %62 = getelementptr inbounds %struct.soa_array_view_type_Particle, ptr %soa_element39, i32 0, i32 1
  store float %61, ptr %62, align 4
  %63 = load %struct.soa_array_view_type_Particle, ptr %soa_element39, align 4
  store %struct.soa_array_view_type_Particle %63, ptr %p_0, align 4
  %64 = getelementptr inbounds %__hl_soa_array_view, ptr %view, i32 0, i32 3
  %65 = load ptr, ptr %64, align 8
  %66 = getelementptr inbounds %__hl_soa_array_view, ptr %view, i32 0, i32 2
  %67 = load i64, ptr %66, align 8
  %68 = getelementptr inbounds %__hl_soa_array_view, ptr %view, i32 0, i32 0
  %69 = load i64, ptr %68, align 8
  %soa_adjusted_index48 = add i64 %69, 0
  %soa_member_base_pointer49 = getelementptr i8, ptr %65, i64 0
  %soa_member_element_pointer50 = getelementptr float, ptr %soa_member_base_pointer49, i64 %soa_adjusted_index48
  %70 = load float, ptr %soa_member_element_pointer50, align 4
  store float %70, ptr %x0, align 4
  %71 = getelementptr inbounds %__hl_soa_array_view, ptr %view, i32 0, i32 3
  %72 = load ptr, ptr %71, align 8
  %73 = getelementptr inbounds %__hl_soa_array_view, ptr %view, i32 0, i32 2
  %74 = load i64, ptr %73, align 8
  %75 = getelementptr inbounds %__hl_soa_array_view, ptr %view, i32 0, i32 0
  %76 = load i64, ptr %75, align 8
  %soa_adjusted_index51 = add i64 %76, 1
  %soa_member_block_size52 = mul i64 %74, 4
  %soa_member_block_offset53 = add i64 0, %soa_member_block_size52
  %soa_offset_adjusted54 = add i64 %soa_member_block_offset53, 3
  %soa_offset_aligned55 = and i64 %soa_offset_adjusted54, -4
  %soa_member_base_pointer56 = getelementptr i8, ptr %72, i64 %soa_offset_aligned55
  %soa_member_element_pointer57 = getelementptr float, ptr %soa_member_base_pointer56, i64 %soa_adjusted_index51
  %77 = load float, ptr %soa_member_element_pointer57, align 4
  store float %77, ptr %y1, align 4
  %78 = getelementptr inbounds %__hl_soa_array, ptr %particles, i32 0, i32 0
  %79 = load ptr, ptr %78, align 8
  %80 = getelementptr inbounds %__hl_soa_array_view, ptr %soa_array_view58, i32 0, i32 0
  store i64 0, ptr %80, align 8
  %81 = getelementptr inbounds %__hl_soa_array_view, ptr %soa_array_view58, i32 0, i32 1
  store i64 4, ptr %81, align 8
  %82 = getelementptr inbounds %__hl_soa_array_view, ptr %soa_array_view58, i32 0, i32 2
  store i64 4, ptr %82, align 8
  %83 = getelementptr inbounds %__hl_soa_array_view, ptr %soa_array_view58, i32 0, i32 3
  store ptr %79, ptr %83, align 8
  %84 = load %__hl_soa_array_view, ptr %soa_array_view58, align 8
  store %__hl_soa_array_view %84, ptr %full_view, align 8
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Soa Array View Type Debug Information", "[LLVM_IR]")
  {
    char const* const input_file = "soa_array_view_type_debug_information.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    std::string const expected_llvm_ir = std::format(R"(
%__hl_soa_array_view = type {{ i64, i64, i64, ptr }}

; Function Attrs: convergent
define private void @soa_array_view_type_debug_information_run() #0 !dbg !3 {{
entry:
  %view = alloca %__hl_soa_array_view, align 8, !dbg !7
  call void @llvm.memset.p0.i64(ptr align 8 %view, i8 0, i64 32, i1 false), !dbg !7
  call void @llvm.dbg.declare(metadata ptr %view, metadata !8, metadata !DIExpression()), !dbg !7
  ret void, !dbg !7
}}

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: write)
declare void @llvm.memset.p0.i64(ptr nocapture writeonly, i8, i64, i1 immarg) #1

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare void @llvm.dbg.declare(metadata, metadata, metadata) #2

attributes #0 = {{ convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }}
attributes #1 = {{ nocallback nofree nounwind willreturn memory(argmem: write) }}
attributes #2 = {{ nocallback nofree nosync nounwind speculatable willreturn memory(none) }}

!llvm.module.flags = !{{!0}}
!llvm.dbg.cu = !{{!1}}

!0 = !{{i32 2, !"Debug Info Version", i32 3}}
!1 = distinct !DICompileUnit(language: DW_LANG_C, file: !2, producer: "Iris Compiler", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!2 = !DIFile(filename: "soa_array_view_type_debug_information.iris", directory: "{}")
!3 = distinct !DISubprogram(name: "run", linkageName: "soa_array_view_type_debug_information_run", scope: null, file: !2, line: 9, type: !4, scopeLine: 10, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !1, retainedNodes: !6)
!4 = !DISubroutineType(types: !5)
!5 = !{{null}}
!6 = !{{}}
!7 = !DILocation(line: 11, column: 5, scope: !3)
!8 = !DILocalVariable(name: "view", scope: !3, file: !2, line: 11, type: !9)
!9 = !DICompositeType(tag: DW_TAG_structure_type, name: "Soa_array_view<2,soa_array_view_type_debug_information_Particle,x,float,y,float>", scope: !10, size: 256, align: 64, elements: !11)
!10 = !DINamespace(name: "h", scope: null)
!11 = !{{!12, !14, !15, !16}}
!12 = !DIDerivedType(tag: DW_TAG_member, name: "start_index", scope: !10, baseType: !13, size: 64, align: 64)
!13 = !DIBasicType(name: "uint64_t", size: 64, encoding: DW_ATE_unsigned)
!14 = !DIDerivedType(tag: DW_TAG_member, name: "end_index", scope: !10, baseType: !13, size: 64, align: 64, offset: 64)
!15 = !DIDerivedType(tag: DW_TAG_member, name: "length", scope: !10, baseType: !13, size: 64, align: 64, offset: 128)
!16 = !DIDerivedType(tag: DW_TAG_member, name: "data", scope: !10, baseType: !17, size: 64, align: 64, offset: 192)
!17 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !18, size: 64)
!18 = !DIBasicType(name: "uint8_t", size: 8, encoding: DW_ATE_unsigned_char)
)", g_test_source_files_path.generic_string());

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir, { .debug = true });
  }

  TEST_CASE("Compile Debug Information C Headers", "[LLVM_IR]")
  {
    char const* const input_file = "debug_information_c_headers.iris";

    std::filesystem::path const root_directory_path = std::filesystem::temp_directory_path() / "debug_information_c_headers";
    std::filesystem::create_directories(root_directory_path);

    std::string const header_content = R"(
struct Vector2i
{
    int x;
    int y;
};

Vector2i add(Vector2i lhs, Vector2i rhs);
)";

    std::filesystem::path const header_module_file_path = create_and_import_c_header(header_content, "vector2i.h", "vector2i.iris", "c.vector2i", root_directory_path);

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
      { "c.vector2i", header_module_file_path }
    };

    std::string const expected_llvm_ir = std::format(R"(
%struct.Vector2i = type {{ i32, i32 }}

; Function Attrs: convergent
declare i64 @add(i64 noundef, i64 noundef) #0

; Function Attrs: convergent
define i32 @Debug_information_run() #0 !dbg !3 {{
entry:
  %a = alloca %struct.Vector2i, align 4, !dbg !8
  %b = alloca %struct.Vector2i, align 4, !dbg !9
  %0 = alloca %struct.Vector2i, align 4, !dbg !10
  %c = alloca %struct.Vector2i, align 4, !dbg !11
  %1 = getelementptr inbounds %struct.Vector2i, ptr %a, i32 0, i32 0, !dbg !8
  store i32 1, ptr %1, align 4, !dbg !8
  %2 = getelementptr inbounds %struct.Vector2i, ptr %a, i32 0, i32 1, !dbg !8
  store i32 -1, ptr %2, align 4, !dbg !8
  call void @llvm.dbg.declare(metadata ptr %a, metadata !12, metadata !DIExpression()), !dbg !8
  %3 = getelementptr inbounds %struct.Vector2i, ptr %b, i32 0, i32 0, !dbg !9
  store i32 2, ptr %3, align 4, !dbg !9
  %4 = getelementptr inbounds %struct.Vector2i, ptr %b, i32 0, i32 1, !dbg !9
  store i32 -2, ptr %4, align 4, !dbg !9
  call void @llvm.dbg.declare(metadata ptr %b, metadata !18, metadata !DIExpression()), !dbg !9
  %5 = getelementptr inbounds %struct.Vector2i, ptr %a, i32 0, i32 0, !dbg !10
  %6 = load i64, ptr %5, align 4, !dbg !10
  %7 = getelementptr inbounds %struct.Vector2i, ptr %b, i32 0, i32 0, !dbg !10
  %8 = load i64, ptr %7, align 4, !dbg !10
  %9 = call i64 @add(i64 noundef %6, i64 noundef %8), !dbg !10
  %10 = getelementptr inbounds %struct.Vector2i, ptr %0, i32 0, i32 0, !dbg !10
  store i64 %9, ptr %10, align 4, !dbg !10
  %11 = load %struct.Vector2i, ptr %0, align 4, !dbg !10
  call void @llvm.dbg.declare(metadata ptr %c, metadata !19, metadata !DIExpression()), !dbg !11
  store %struct.Vector2i %11, ptr %c, align 4, !dbg !11
  %12 = getelementptr inbounds %struct.Vector2i, ptr %c, i32 0, i32 0, !dbg !11
  %13 = load i32, ptr %12, align 4, !dbg !11
  %14 = getelementptr inbounds %struct.Vector2i, ptr %c, i32 0, i32 1, !dbg !11
  %15 = load i32, ptr %14, align 4, !dbg !11
  %16 = add i32 %13, %15, !dbg !11
  ret i32 %16, !dbg !20
}}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

attributes #0 = {{ convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }}
attributes #1 = {{ nocallback nofree nosync nounwind speculatable willreturn memory(none) }}

!llvm.module.flags = !{{!0}}
!llvm.dbg.cu = !{{!1}}

!0 = !{{i32 2, !"Debug Info Version", i32 3}}
!1 = distinct !DICompileUnit(language: DW_LANG_C, file: !2, producer: "Iris Compiler", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!2 = !DIFile(filename: "debug_information_c_headers.iris", directory: "{}")
!3 = distinct !DISubprogram(name: "run", linkageName: "Debug_information_run", scope: null, file: !2, line: 5, type: !4, scopeLine: 6, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !1, retainedNodes: !7)
!4 = !DISubroutineType(types: !5)
!5 = !{{!6}}
!6 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!7 = !{{}}
!8 = !DILocation(line: 7, column: 5, scope: !3)
!9 = !DILocation(line: 8, column: 5, scope: !3)
!10 = !DILocation(line: 9, column: 13, scope: !3)
!11 = !DILocation(line: 9, column: 5, scope: !3)
!12 = !DILocalVariable(name: "a", scope: !3, file: !2, line: 7, type: !13)
!13 = !DICompositeType(tag: DW_TAG_structure_type, name: "Vector2i", file: !14, line: 2, size: 64, align: 8, elements: !15)
!14 = !DIFile(filename: "vector2i.h", directory: "{}")
!15 = !{{!16, !17}}
!16 = !DIDerivedType(tag: DW_TAG_member, name: "x", file: !14, line: 4, baseType: !6, size: 32, align: 32)
!17 = !DIDerivedType(tag: DW_TAG_member, name: "y", file: !14, line: 5, baseType: !6, size: 32, align: 32, offset: 32)
!18 = !DILocalVariable(name: "b", scope: !3, file: !2, line: 8, type: !13)
!19 = !DILocalVariable(name: "c", scope: !3, file: !2, line: 9, type: !13)
!20 = !DILocation(line: 10, column: 5, scope: !3)
)", g_test_source_files_path.generic_string(), root_directory_path.generic_string());

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir, { .debug = true });
  }

  TEST_CASE("Compile Debug Information Array Slices", "[LLVM_IR]")
  {
    char const* const input_file = "debug_information_array_slices.iris";

    std::filesystem::path const root_directory_path = std::filesystem::temp_directory_path() / "debug_information_c_headers";
    std::filesystem::create_directories(root_directory_path);

    std::string const header_content = R"(
struct Vector2i
{
    int x;
    int y;
};

Vector2i add(Vector2i lhs, Vector2i rhs);
)";

    std::filesystem::path const header_module_file_path = create_and_import_c_header(header_content, "vector2i.h", "vector2i.iris", "c.vector2i", root_directory_path);

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
      { "c.vector2i", header_module_file_path }
    };

    std::string const expected_llvm_ir = std::format(R"(
%struct.iris_builtin_Generic_array_slice = type {{ ptr, i64 }}

; Function Attrs: convergent
define void @Debug_information_run(ptr noundef %"arguments[0].integers", ptr noundef %"arguments[1].vectors", i64 noundef %"arguments[2].length") #0 !dbg !3 {{
entry:
  %integers = alloca ptr, align 8
  %vectors = alloca ptr, align 8
  %length = alloca i64, align 8
  %0 = alloca %struct.iris_builtin_Generic_array_slice, align 8, !dbg !20
  %a = alloca %struct.iris_builtin_Generic_array_slice, align 8, !dbg !20
  %1 = alloca %struct.iris_builtin_Generic_array_slice, align 8, !dbg !21
  %b = alloca %struct.iris_builtin_Generic_array_slice, align 8, !dbg !21
  store ptr %"arguments[0].integers", ptr %integers, align 8
  call void @llvm.dbg.declare(metadata ptr %integers, metadata !17, metadata !DIExpression()), !dbg !22
  store ptr %"arguments[1].vectors", ptr %vectors, align 8
  call void @llvm.dbg.declare(metadata ptr %vectors, metadata !18, metadata !DIExpression()), !dbg !23
  store i64 %"arguments[2].length", ptr %length, align 8
  call void @llvm.dbg.declare(metadata ptr %length, metadata !19, metadata !DIExpression()), !dbg !24
  %2 = load ptr, ptr %integers, align 8, !dbg !20
  %3 = load i64, ptr %length, align 8, !dbg !20
  %4 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %0, i32 0, i32 0, !dbg !20
  store ptr %2, ptr %4, align 8, !dbg !20
  %5 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %0, i32 0, i32 1, !dbg !20
  store i64 %3, ptr %5, align 8, !dbg !20
  %6 = load %struct.iris_builtin_Generic_array_slice, ptr %0, align 8, !dbg !20
  call void @llvm.dbg.declare(metadata ptr %a, metadata !25, metadata !DIExpression()), !dbg !21
  store %struct.iris_builtin_Generic_array_slice %6, ptr %a, align 8, !dbg !21
  %7 = load ptr, ptr %vectors, align 8, !dbg !21
  %8 = load i64, ptr %length, align 8, !dbg !21
  %9 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %1, i32 0, i32 0, !dbg !21
  store ptr %7, ptr %9, align 8, !dbg !21
  %10 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %1, i32 0, i32 1, !dbg !21
  store i64 %8, ptr %10, align 8, !dbg !21
  %11 = load %struct.iris_builtin_Generic_array_slice, ptr %1, align 8, !dbg !21
  call void @llvm.dbg.declare(metadata ptr %b, metadata !30, metadata !DIExpression()), !dbg !34
  store %struct.iris_builtin_Generic_array_slice %11, ptr %b, align 8, !dbg !34
  ret void, !dbg !34
}}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

attributes #0 = {{ convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }}
attributes #1 = {{ nocallback nofree nosync nounwind speculatable willreturn memory(none) }}

!llvm.module.flags = !{{!0}}
!llvm.dbg.cu = !{{!1}}

!0 = !{{i32 2, !"Debug Info Version", i32 3}}
!1 = distinct !DICompileUnit(language: DW_LANG_C, file: !2, producer: "Iris Compiler", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!2 = !DIFile(filename: "debug_information_array_slices.iris", directory: "{}")
!3 = distinct !DISubprogram(name: "run", linkageName: "Debug_information_run", scope: null, file: !2, line: 5, type: !4, scopeLine: 6, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !1, retainedNodes: !16)
!4 = !DISubroutineType(types: !5)
!5 = !{{null, !6, !8, !15}}
!6 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !7, size: 64)
!7 = !DIBasicType(name: "int32_t", size: 32, encoding: DW_ATE_signed)
!8 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !9, size: 64)
!9 = !DICompositeType(tag: DW_TAG_structure_type, name: "Vector2i", file: !10, line: 2, size: 64, align: 8, elements: !11)
!10 = !DIFile(filename: "vector2i.h", directory: "{}")
!11 = !{{!12, !14}}
!12 = !DIDerivedType(tag: DW_TAG_member, name: "x", file: !10, line: 4, baseType: !13, size: 32, align: 32)
!13 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!14 = !DIDerivedType(tag: DW_TAG_member, name: "y", file: !10, line: 5, baseType: !13, size: 32, align: 32, offset: 32)
!15 = !DIBasicType(name: "uint64_t", size: 64, encoding: DW_ATE_unsigned)
!16 = !{{!17, !18, !19}}
!17 = !DILocalVariable(name: "integers", arg: 1, scope: !3, file: !2, line: 5, type: !6)
!18 = !DILocalVariable(name: "vectors", arg: 2, scope: !3, file: !2, line: 5, type: !8)
!19 = !DILocalVariable(name: "length", arg: 3, scope: !3, file: !2, line: 5, type: !15)
!20 = !DILocation(line: 6, column: 1, scope: !3)
!21 = !DILocation(line: 7, column: 5, scope: !3)
!22 = !DILocation(line: 5, column: 21, scope: !3)
!23 = !DILocation(line: 5, column: 39, scope: !3)
!24 = !DILocation(line: 5, column: 68, scope: !3)
!25 = !DILocalVariable(name: "a", scope: !3, file: !2, line: 7, type: !26)
!26 = !DICompositeType(tag: DW_TAG_structure_type, name: "Array_slice::<int32_t>", scope: !3, size: 128, align: 64, elements: !27)
!27 = !{{!28, !29}}
!28 = !DIDerivedType(tag: DW_TAG_member, name: "data", scope: !3, baseType: !6, size: 64, align: 64)
!29 = !DIDerivedType(tag: DW_TAG_member, name: "length", scope: !3, baseType: !15, size: 64, align: 64, offset: 64)
!30 = !DILocalVariable(name: "b", scope: !3, file: !2, line: 8, type: !31)
!31 = !DICompositeType(tag: DW_TAG_structure_type, name: "Array_slice::<Vector2i>", scope: !3, size: 128, align: 64, elements: !32)
!32 = !{{!33, !29}}
!33 = !DIDerivedType(tag: DW_TAG_member, name: "data", scope: !3, baseType: !8, size: 64, align: 64)
!34 = !DILocation(line: 8, column: 5, scope: !3)
)", g_test_source_files_path.generic_string(), root_directory_path.generic_string());

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir, { .debug = true });
  }

  TEST_CASE("Compile Debug Information Bit Fields", "[LLVM_IR]")
  {
    char const* const input_file = "debug_information_bit_fields.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    std::string const expected_llvm_ir = std::format(R"(
%struct.Debug_information_My_struct = type {{ i32, i32 }}

; Function Attrs: convergent
define private i64 @Debug_information_instantiate() #0 !dbg !3 {{
entry:
  %instance = alloca %struct.Debug_information_My_struct, align 4, !dbg !13
  %0 = getelementptr inbounds %struct.Debug_information_My_struct, ptr %instance, i32 0, i32 0, !dbg !13
  store i32 1, ptr %0, align 4, !dbg !13
  %1 = getelementptr inbounds %struct.Debug_information_My_struct, ptr %instance, i32 0, i32 1, !dbg !13
  %2 = load i32, ptr %1, align 4, !dbg !13
  %3 = and i32 %2, -16777216, !dbg !13
  %4 = or i32 %3, 1, !dbg !13
  store i32 %4, ptr %1, align 4, !dbg !13
  %5 = getelementptr inbounds %struct.Debug_information_My_struct, ptr %instance, i32 0, i32 1, !dbg !13
  %6 = load i32, ptr %5, align 4, !dbg !13
  %7 = and i32 %6, 16777215, !dbg !13
  %8 = or i32 %7, 33554432, !dbg !13
  store i32 %8, ptr %5, align 4, !dbg !13
  call void @llvm.dbg.declare(metadata ptr %instance, metadata !14, metadata !DIExpression()), !dbg !13
  %9 = getelementptr inbounds %struct.Debug_information_My_struct, ptr %instance, i32 0, i32 0, !dbg !15
  %10 = load i64, ptr %9, align 4, !dbg !15
  ret i64 %10, !dbg !15
}}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

attributes #0 = {{ convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }}
attributes #1 = {{ nocallback nofree nosync nounwind speculatable willreturn memory(none) }}

!llvm.module.flags = !{{!0}}
!llvm.dbg.cu = !{{!1}}

!0 = !{{i32 2, !"Debug Info Version", i32 3}}
!1 = distinct !DICompileUnit(language: DW_LANG_C, file: !2, producer: "Iris Compiler", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!2 = !DIFile(filename: "debug_information_bit_fields.iris", directory: "{}")
!3 = distinct !DISubprogram(name: "instantiate", linkageName: "Debug_information_instantiate", scope: null, file: !2, line: 10, type: !4, scopeLine: 11, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !1, retainedNodes: !12)
!4 = !DISubroutineType(types: !5)
!5 = !{{!6}}
!6 = !DICompositeType(tag: DW_TAG_structure_type, name: "Debug_information_My_struct", file: !2, line: 3, size: 64, align: 8, elements: !7)
!7 = !{{!8, !10, !11}}
!8 = !DIDerivedType(tag: DW_TAG_member, name: "a", file: !2, line: 5, baseType: !9, size: 32, align: 32)
!9 = !DIBasicType(name: "int32_t", size: 32, encoding: DW_ATE_signed)
!10 = !DIDerivedType(tag: DW_TAG_member, name: "b", file: !2, line: 6, baseType: !9, size: 24, align: 32, offset: 32, flags: DIFlagBitField)
!11 = !DIDerivedType(tag: DW_TAG_member, name: "c", file: !2, line: 7, baseType: !9, size: 8, align: 32, offset: 56, flags: DIFlagBitField)
!12 = !{{}}
!13 = !DILocation(line: 12, column: 5, scope: !3)
!14 = !DILocalVariable(name: "instance", scope: !3, file: !2, line: 12, type: !6)
!15 = !DILocation(line: 13, column: 5, scope: !3)
)", g_test_source_files_path.generic_string());

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir, { .debug = true });
  }

  TEST_CASE("Compile Debug Information For Loop", "[LLVM_IR]")
  {
    char const* const input_file = "debug_information_for_loop.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    std::string const expected_llvm_ir = std::format(R"(
; Function Attrs: convergent
define i32 @Debug_information_run() #0 !dbg !3 {{
entry:
  %value = alloca i32, align 4, !dbg !8
  call void @llvm.dbg.declare(metadata ptr %value, metadata !9, metadata !DIExpression()), !dbg !8
  %index = alloca i32, align 4, !dbg !10
  store i32 0, ptr %value, align 4, !dbg !8
  call void @llvm.dbg.declare(metadata ptr %index, metadata !12, metadata !DIExpression()), !dbg !10
  store i32 0, ptr %index, align 4, !dbg !10
  br label %for_loop_condition, !dbg !10

for_loop_condition:                               ; preds = %for_loop_update_index, %entry
  %0 = load i32, ptr %index, align 4, !dbg !10
  %1 = icmp slt i32 %0, 10, !dbg !10
  br i1 %1, label %for_loop_then, label %for_loop_after, !dbg !10

for_loop_then:                                    ; preds = %for_loop_condition
  %2 = load i32, ptr %value, align 4, !dbg !10
  %3 = load i32, ptr %index, align 4, !dbg !10
  %4 = add i32 %2, %3, !dbg !10
  store i32 %4, ptr %value, align 4, !dbg !13
  br label %for_loop_update_index, !dbg !13

for_loop_update_index:                            ; preds = %for_loop_then
  %5 = load i32, ptr %index, align 4, !dbg !10
  %6 = add i32 %5, 1, !dbg !10
  store i32 %6, ptr %index, align 4, !dbg !10
  br label %for_loop_condition, !dbg !10

for_loop_after:                                   ; preds = %for_loop_condition
  %7 = load i32, ptr %value, align 4, !dbg !14
  ret i32 %7, !dbg !14
}}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

attributes #0 = {{ convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }}
attributes #1 = {{ nocallback nofree nosync nounwind speculatable willreturn memory(none) }}

!llvm.module.flags = !{{!0}}
!llvm.dbg.cu = !{{!1}}

!0 = !{{i32 2, !"Debug Info Version", i32 3}}
!1 = distinct !DICompileUnit(language: DW_LANG_C, file: !2, producer: "Iris Compiler", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!2 = !DIFile(filename: "debug_information_for_loop.iris", directory: "{}")
!3 = distinct !DISubprogram(name: "run", linkageName: "Debug_information_run", scope: null, file: !2, line: 3, type: !4, scopeLine: 4, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !1, retainedNodes: !7)
!4 = !DISubroutineType(types: !5)
!5 = !{{!6}}
!6 = !DIBasicType(name: "int32_t", size: 32, encoding: DW_ATE_signed)
!7 = !{{}}
!8 = !DILocation(line: 5, column: 5, scope: !3)
!9 = !DILocalVariable(name: "value", scope: !3, file: !2, line: 5, type: !6)
!10 = !DILocation(line: 7, column: 5, scope: !11)
!11 = distinct !DILexicalBlock(scope: !3, file: !2, line: 7, column: 5)
!12 = !DILocalVariable(name: "index", scope: !11, file: !2, line: 7, type: !6)
!13 = !DILocation(line: 9, column: 9, scope: !11)
!14 = !DILocation(line: 12, column: 5, scope: !3)
)", g_test_source_files_path.generic_string());

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir, { .debug = true });
  }

  TEST_CASE("Compile Debug Information Function Call", "[LLVM_IR]")
  {
    char const* const input_file = "debug_information_function_call.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    std::string const expected_llvm_ir = std::format(R"(
; Function Attrs: convergent
define i32 @Debug_information_run() #0 !dbg !3 {{
entry:
  %value = alloca i32, align 4, !dbg !8
  %0 = call i32 @Debug_information_add(i32 noundef 1, i32 noundef 2), !dbg !9
  call void @llvm.dbg.declare(metadata ptr %value, metadata !10, metadata !DIExpression()), !dbg !8
  store i32 %0, ptr %value, align 4, !dbg !8
  %1 = load i32, ptr %value, align 4, !dbg !11
  ret i32 %1, !dbg !11
}}

; Function Attrs: convergent
define private i32 @Debug_information_add(i32 noundef %"arguments[0].lhs", i32 noundef %"arguments[1].rhs") #0 !dbg !12 {{
entry:
  %lhs = alloca i32, align 4
  %rhs = alloca i32, align 4
  store i32 %"arguments[0].lhs", ptr %lhs, align 4
  call void @llvm.dbg.declare(metadata ptr %lhs, metadata !16, metadata !DIExpression()), !dbg !18
  store i32 %"arguments[1].rhs", ptr %rhs, align 4
  call void @llvm.dbg.declare(metadata ptr %rhs, metadata !17, metadata !DIExpression()), !dbg !19
  %0 = load i32, ptr %lhs, align 4, !dbg !20
  %1 = load i32, ptr %rhs, align 4, !dbg !20
  %2 = add i32 %0, %1, !dbg !20
  ret i32 %2, !dbg !21
}}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

attributes #0 = {{ convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }}
attributes #1 = {{ nocallback nofree nosync nounwind speculatable willreturn memory(none) }}

!llvm.module.flags = !{{!0}}
!llvm.dbg.cu = !{{!1}}

!0 = !{{i32 2, !"Debug Info Version", i32 3}}
!1 = distinct !DICompileUnit(language: DW_LANG_C, file: !2, producer: "Iris Compiler", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!2 = !DIFile(filename: "debug_information_function_call.iris", directory: "{}")
!3 = distinct !DISubprogram(name: "run", linkageName: "Debug_information_run", scope: null, file: !2, line: 8, type: !4, scopeLine: 9, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !1, retainedNodes: !7)
!4 = !DISubroutineType(types: !5)
!5 = !{{!6}}
!6 = !DIBasicType(name: "int32_t", size: 32, encoding: DW_ATE_signed)
!7 = !{{}}
!8 = !DILocation(line: 10, column: 5, scope: !3)
!9 = !DILocation(line: 10, column: 17, scope: !3)
!10 = !DILocalVariable(name: "value", scope: !3, file: !2, line: 10, type: !6)
!11 = !DILocation(line: 11, column: 5, scope: !3)
!12 = distinct !DISubprogram(name: "add", linkageName: "Debug_information_add", scope: null, file: !2, line: 3, type: !13, scopeLine: 4, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !1, retainedNodes: !15)
!13 = !DISubroutineType(types: !14)
!14 = !{{!6, !6, !6}}
!15 = !{{!16, !17}}
!16 = !DILocalVariable(name: "lhs", arg: 1, scope: !12, file: !2, line: 3, type: !6)
!17 = !DILocalVariable(name: "rhs", arg: 2, scope: !12, file: !2, line: 3, type: !6)
!18 = !DILocation(line: 3, column: 14, scope: !12)
!19 = !DILocation(line: 3, column: 26, scope: !12)
!20 = !DILocation(line: 4, column: 1, scope: !12)
!21 = !DILocation(line: 5, column: 5, scope: !12)
)", g_test_source_files_path.generic_string());

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir, { .debug = true });
  }

  TEST_CASE("Compile Debug Information Imported Function Constructor", "[LLVM_IR]")
  {
    char const* const input_file = "debug_information_function_constructor_consumer.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
      { "Debug_information_function_constructor_provider", parse_and_get_file_path(g_test_source_files_path / "debug_information_function_constructor_provider.iris") },
    };

    std::string const expected_llvm_ir = std::format(R"(
; Function Attrs: convergent
define i32 @Debug_information_function_constructor_consumer_run() #0 !dbg !3 {{
entry:
  %value = alloca i32, align 4, !dbg !8
  %0 = call i32 @Debug_information_function_constructor_provider__at__add__at__489334907677298949(i32 noundef 1, i32 noundef 2), !dbg !9
  call void @llvm.dbg.declare(metadata ptr %value, metadata !10, metadata !DIExpression()), !dbg !8
  store i32 %0, ptr %value, align 4, !dbg !8
  %1 = load i32, ptr %value, align 4, !dbg !11
  ret i32 %1, !dbg !11
}}

; Function Attrs: convergent
define private i32 @Debug_information_function_constructor_provider__at__add__at__489334907677298949(i32 noundef %"arguments[0].lhs", i32 noundef %"arguments[1].rhs") #0 !dbg !12 {{
entry:
  %lhs = alloca i32, align 4
  %rhs = alloca i32, align 4
  store i32 %"arguments[0].lhs", ptr %lhs, align 4
  call void @llvm.dbg.declare(metadata ptr %lhs, metadata !17, metadata !DIExpression()), !dbg !19
  store i32 %"arguments[1].rhs", ptr %rhs, align 4
  call void @llvm.dbg.declare(metadata ptr %rhs, metadata !18, metadata !DIExpression()), !dbg !20
  %0 = load i32, ptr %lhs, align 4, !dbg !21
  %1 = load i32, ptr %rhs, align 4, !dbg !21
  %2 = add i32 %0, %1, !dbg !21
  ret i32 %2, !dbg !22
}}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

attributes #0 = {{ convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }}
attributes #1 = {{ nocallback nofree nosync nounwind speculatable willreturn memory(none) }}

!llvm.module.flags = !{{!0}}
!llvm.dbg.cu = !{{!1}}

!0 = !{{i32 2, !"Debug Info Version", i32 3}}
!1 = distinct !DICompileUnit(language: DW_LANG_C, file: !2, producer: "Iris Compiler", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!2 = !DIFile(filename: "debug_information_function_constructor_consumer.iris", directory: "{}")
!3 = distinct !DISubprogram(name: "run", linkageName: "Debug_information_function_constructor_consumer_run", scope: null, file: !2, line: 5, type: !4, scopeLine: 6, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !1, retainedNodes: !7)
!4 = !DISubroutineType(types: !5)
!5 = !{{!6}}
!6 = !DIBasicType(name: "int32_t", size: 32, encoding: DW_ATE_signed)
!7 = !{{}}
!8 = !DILocation(line: 7, column: 5, scope: !3)
!9 = !DILocation(line: 7, column: 17, scope: !3)
!10 = !DILocalVariable(name: "value", scope: !3, file: !2, line: 7, type: !6)
!11 = !DILocation(line: 8, column: 5, scope: !3)
!12 = distinct !DISubprogram(name: "Debug_information_function_constructor_provider__at__add__at__489334907677298949", linkageName: "Debug_information_function_constructor_provider__at__add__at__489334907677298949", scope: null, file: !13, line: 5, type: !14, scopeLine: 5, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !1, retainedNodes: !16)
!13 = !DIFile(filename: "debug_information_function_constructor_provider.iris", directory: "{}")
!14 = !DISubroutineType(types: !15)
!15 = !{{!6, !6, !6}}
!16 = !{{!17, !18}}
!17 = !DILocalVariable(name: "lhs", arg: 1, scope: !12, file: !13, line: 5, type: !6)
!18 = !DILocalVariable(name: "rhs", arg: 2, scope: !12, file: !13, line: 5, type: !6)
!19 = !DILocation(line: 5, column: 22, scope: !12)
!20 = !DILocation(line: 5, column: 39, scope: !12)
!21 = !DILocation(line: 5, column: 12, scope: !12)
!22 = !DILocation(line: 7, column: 9, scope: !12)
)", g_test_source_files_path.generic_string(), g_test_source_files_path.generic_string());

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir, { .debug = true });
  }

  TEST_CASE("Compile Debug Information If", "[LLVM_IR]")
  {
    char const* const input_file = "debug_information_if.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    std::string const expected_llvm_ir = std::format(R"(
; Function Attrs: convergent
define private i32 @Debug_information_run(i32 noundef %"arguments[0].value") #0 !dbg !3 {{
entry:
  %value = alloca i32, align 4
  store i32 %"arguments[0].value", ptr %value, align 4
  call void @llvm.dbg.declare(metadata ptr %value, metadata !8, metadata !DIExpression()), !dbg !9
  %0 = load i32, ptr %value, align 4, !dbg !10
  %1 = icmp eq i32 %0, 0, !dbg !10
  br i1 %1, label %if_s0_then, label %if_s1_else, !dbg !10

if_s0_then:                                       ; preds = %entry
  ret i32 1, !dbg !11

if_s1_else:                                       ; preds = %entry
  %2 = load i32, ptr %value, align 4, !dbg !13
  %3 = icmp eq i32 %2, 1, !dbg !13
  br i1 %3, label %if_s2_then, label %if_s3_else, !dbg !13

if_s2_then:                                       ; preds = %if_s1_else
  ret i32 2, !dbg !14

if_s3_else:                                       ; preds = %if_s1_else
  ret i32 3, !dbg !16
}}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

attributes #0 = {{ convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }}
attributes #1 = {{ nocallback nofree nosync nounwind speculatable willreturn memory(none) }}

!llvm.module.flags = !{{!0}}
!llvm.dbg.cu = !{{!1}}

!0 = !{{i32 2, !"Debug Info Version", i32 3}}
!1 = distinct !DICompileUnit(language: DW_LANG_C, file: !2, producer: "Iris Compiler", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!2 = !DIFile(filename: "debug_information_if.iris", directory: "{}")
!3 = distinct !DISubprogram(name: "run", linkageName: "Debug_information_run", scope: null, file: !2, line: 3, type: !4, scopeLine: 4, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !1, retainedNodes: !7)
!4 = !DISubroutineType(types: !5)
!5 = !{{!6, !6}}
!6 = !DIBasicType(name: "int32_t", size: 32, encoding: DW_ATE_signed)
!7 = !{{!8}}
!8 = !DILocalVariable(name: "value", arg: 1, scope: !3, file: !2, line: 3, type: !6)
!9 = !DILocation(line: 3, column: 14, scope: !3)
!10 = !DILocation(line: 5, column: 8, scope: !3)
!11 = !DILocation(line: 7, column: 9, scope: !12)
!12 = distinct !DILexicalBlock(scope: !3, file: !2, line: 6, column: 5)
!13 = !DILocation(line: 9, column: 13, scope: !3)
!14 = !DILocation(line: 11, column: 9, scope: !15)
!15 = distinct !DILexicalBlock(scope: !3, file: !2, line: 10, column: 5)
!16 = !DILocation(line: 15, column: 9, scope: !17)
!17 = distinct !DILexicalBlock(scope: !3, file: !2, line: 14, column: 5)
)", g_test_source_files_path.generic_string());

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir, { .debug = true });
  }

  TEST_CASE("Compile Debug Information Struct", "[LLVM_IR]")
  {
    char const* const input_file = "debug_information_structs.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    std::string const expected_llvm_ir = std::format(R"(
%struct.Debug_information_Vector2i = type {{ i32, i32 }}

; Function Attrs: convergent
define private i64 @Debug_information_instantiate() #0 !dbg !3 {{
entry:
  %instance = alloca %struct.Debug_information_Vector2i, align 4, !dbg !12
  %0 = getelementptr inbounds %struct.Debug_information_Vector2i, ptr %instance, i32 0, i32 0, !dbg !12
  store i32 1, ptr %0, align 4, !dbg !12
  %1 = getelementptr inbounds %struct.Debug_information_Vector2i, ptr %instance, i32 0, i32 1, !dbg !12
  store i32 2, ptr %1, align 4, !dbg !12
  call void @llvm.dbg.declare(metadata ptr %instance, metadata !13, metadata !DIExpression()), !dbg !12
  %2 = getelementptr inbounds %struct.Debug_information_Vector2i, ptr %instance, i32 0, i32 0, !dbg !14
  %3 = load i64, ptr %2, align 4, !dbg !14
  ret i64 %3, !dbg !14
}}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

attributes #0 = {{ convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }}
attributes #1 = {{ nocallback nofree nosync nounwind speculatable willreturn memory(none) }}

!llvm.module.flags = !{{!0}}
!llvm.dbg.cu = !{{!1}}

!0 = !{{i32 2, !"Debug Info Version", i32 3}}
!1 = distinct !DICompileUnit(language: DW_LANG_C, file: !2, producer: "Iris Compiler", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!2 = !DIFile(filename: "debug_information_structs.iris", directory: "{}")
!3 = distinct !DISubprogram(name: "instantiate", linkageName: "Debug_information_instantiate", scope: null, file: !2, line: 9, type: !4, scopeLine: 10, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !1, retainedNodes: !11)
!4 = !DISubroutineType(types: !5)
!5 = !{{!6}}
!6 = !DICompositeType(tag: DW_TAG_structure_type, name: "Debug_information_Vector2i", file: !2, line: 3, size: 64, align: 8, elements: !7)
!7 = !{{!8, !10}}
!8 = !DIDerivedType(tag: DW_TAG_member, name: "x", file: !2, line: 5, baseType: !9, size: 32, align: 32)
!9 = !DIBasicType(name: "int32_t", size: 32, encoding: DW_ATE_signed)
!10 = !DIDerivedType(tag: DW_TAG_member, name: "y", file: !2, line: 6, baseType: !9, size: 32, align: 32, offset: 32)
!11 = !{{}}
!12 = !DILocation(line: 11, column: 5, scope: !3)
!13 = !DILocalVariable(name: "instance", scope: !3, file: !2, line: 11, type: !6)
!14 = !DILocation(line: 12, column: 5, scope: !3)
)", g_test_source_files_path.generic_string());

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir, { .debug = true });
  }

  TEST_CASE("Compile Debug Information Switch", "[LLVM_IR]")
  {
    char const* const input_file = "debug_information_switch.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    std::string const expected_llvm_ir = std::format(R"(
; Function Attrs: convergent
define private i32 @Debug_information_run(i32 noundef %"arguments[0].value") #0 !dbg !3 {{
entry:
  %value = alloca i32, align 4
  store i32 %"arguments[0].value", ptr %value, align 4
  call void @llvm.dbg.declare(metadata ptr %value, metadata !8, metadata !DIExpression()), !dbg !9
  %0 = load i32, ptr %value, align 4, !dbg !10
  switch i32 %0, label %switch_case_default [
    i32 0, label %switch_case_i0_
    i32 1, label %switch_case_i1_
  ], !dbg !10

switch_after:                                     ; preds = %switch_case_default, %switch_case_i1_
  %1 = load i32, ptr %value, align 4, !dbg !11
  ret i32 %1, !dbg !11

switch_case_i0_:                                  ; preds = %entry
  ret i32 10, !dbg !12

switch_case_i1_:                                  ; preds = %entry
  br label %switch_after, !dbg !13

switch_case_default:                              ; preds = %entry
  br label %switch_after, !dbg !14
}}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

attributes #0 = {{ convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }}
attributes #1 = {{ nocallback nofree nosync nounwind speculatable willreturn memory(none) }}

!llvm.module.flags = !{{!0}}
!llvm.dbg.cu = !{{!1}}

!0 = !{{i32 2, !"Debug Info Version", i32 3}}
!1 = distinct !DICompileUnit(language: DW_LANG_C, file: !2, producer: "Iris Compiler", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!2 = !DIFile(filename: "debug_information_switch.iris", directory: "{}")
!3 = distinct !DISubprogram(name: "run", linkageName: "Debug_information_run", scope: null, file: !2, line: 3, type: !4, scopeLine: 4, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !1, retainedNodes: !7)
!4 = !DISubroutineType(types: !5)
!5 = !{{!6, !6}}
!6 = !DIBasicType(name: "int32_t", size: 32, encoding: DW_ATE_signed)
!7 = !{{!8}}
!8 = !DILocalVariable(name: "value", arg: 1, scope: !3, file: !2, line: 3, type: !6)
!9 = !DILocation(line: 3, column: 14, scope: !3)
!10 = !DILocation(line: 5, column: 5, scope: !3)
!11 = !DILocation(line: 15, column: 5, scope: !3)
!12 = !DILocation(line: 8, column: 9, scope: !3)
!13 = !DILocation(line: 10, column: 9, scope: !3)
!14 = !DILocation(line: 12, column: 9, scope: !3)
)", g_test_source_files_path.generic_string());

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir, { .debug = true });
  }

  TEST_CASE("Compile Debug Information Temporary Replacement", "[LLVM_IR]")
  {
    char const* const input_file = "debug_information_temporary_replacement.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    std::string const expected_llvm_ir = std::format(R"(
%struct.Debug_information_temporary_replacement_My_struct = type {{ ptr, ptr }}

; Function Attrs: convergent
define private void @Debug_information_temporary_replacement_run() #0 !dbg !3 {{
entry:
  %instance = alloca %struct.Debug_information_temporary_replacement_My_struct, align 8, !dbg !7
  %0 = getelementptr inbounds %struct.Debug_information_temporary_replacement_My_struct, ptr %instance, i32 0, i32 0, !dbg !7
  store ptr null, ptr %0, align 8, !dbg !7
  %1 = getelementptr inbounds %struct.Debug_information_temporary_replacement_My_struct, ptr %instance, i32 0, i32 1, !dbg !7
  store ptr null, ptr %1, align 8, !dbg !7
  call void @llvm.dbg.declare(metadata ptr %instance, metadata !8, metadata !DIExpression()), !dbg !7
  ret void, !dbg !7
}}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

attributes #0 = {{ convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }}
attributes #1 = {{ nocallback nofree nosync nounwind speculatable willreturn memory(none) }}

!llvm.module.flags = !{{!0}}
!llvm.dbg.cu = !{{!1}}

!0 = !{{i32 2, !"Debug Info Version", i32 3}}
!1 = distinct !DICompileUnit(language: DW_LANG_C, file: !2, producer: "Iris Compiler", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!2 = !DIFile(filename: "debug_information_temporary_replacement.iris", directory: "{}")
!3 = distinct !DISubprogram(name: "run", linkageName: "Debug_information_temporary_replacement_run", scope: null, file: !2, line: 10, type: !4, scopeLine: 11, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !1, retainedNodes: !6)
!4 = !DISubroutineType(types: !5)
!5 = !{{null}}
!6 = !{{}}
!7 = !DILocation(line: 12, column: 5, scope: !3)
!8 = !DILocalVariable(name: "instance", scope: !3, file: !2, line: 12, type: !9)
!9 = !DICompositeType(tag: DW_TAG_structure_type, name: "Debug_information_temporary_replacement_My_struct", file: !2, line: 3, size: 128, align: 8, elements: !10)
!10 = !{{!11, !13}}
!11 = !DIDerivedType(tag: DW_TAG_member, name: "a", file: !2, line: 4, baseType: !12, size: 64, align: 64)
!12 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !9, size: 64)
!13 = !DIDerivedType(tag: DW_TAG_member, name: "b", file: !2, line: 5, baseType: !14, size: 64, align: 64, offset: 64)
!14 = !DIDerivedType(tag: DW_TAG_typedef, name: "Debug_information_temporary_replacement_My_alias", file: !2, line: 8, baseType: !12)
)", g_test_source_files_path.generic_string());

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir, { .debug = true });
  }

  TEST_CASE("Compile Debug Information Union", "[LLVM_IR]")
  {
    char const* const input_file = "debug_information_unions.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    std::string const expected_llvm_ir = std::format(R"(
%union.Debug_information_My_int = type {{ i32 }}

; Function Attrs: convergent
define private i32 @Debug_information_instantiate() #0 !dbg !3 {{
entry:
  %instance = alloca %union.Debug_information_My_int, align 4, !dbg !13
  store i32 0, ptr %instance, align 4, !dbg !13
  call void @llvm.dbg.declare(metadata ptr %instance, metadata !14, metadata !DIExpression()), !dbg !13
  %0 = getelementptr inbounds %union.Debug_information_My_int, ptr %instance, i32 0, i32 0, !dbg !15
  %1 = load i32, ptr %0, align 4, !dbg !15
  ret i32 %1, !dbg !15
}}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

attributes #0 = {{ convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }}
attributes #1 = {{ nocallback nofree nosync nounwind speculatable willreturn memory(none) }}

!llvm.module.flags = !{{!0}}
!llvm.dbg.cu = !{{!1}}

!0 = !{{i32 2, !"Debug Info Version", i32 3}}
!1 = distinct !DICompileUnit(language: DW_LANG_C, file: !2, producer: "Iris Compiler", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!2 = !DIFile(filename: "debug_information_unions.iris", directory: "{}")
!3 = distinct !DISubprogram(name: "instantiate", linkageName: "Debug_information_instantiate", scope: null, file: !2, line: 9, type: !4, scopeLine: 10, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !1, retainedNodes: !12)
!4 = !DISubroutineType(types: !5)
!5 = !{{!6}}
!6 = !DICompositeType(tag: DW_TAG_union_type, name: "Debug_information_My_int", file: !2, line: 3, size: 32, align: 8, elements: !7)
!7 = !{{!8, !10}}
!8 = !DIDerivedType(tag: DW_TAG_member, name: "x", file: !2, line: 5, baseType: !9, size: 32, align: 8)
!9 = !DIBasicType(name: "int32_t", size: 32, encoding: DW_ATE_signed)
!10 = !DIDerivedType(tag: DW_TAG_member, name: "y", file: !2, line: 6, baseType: !11, size: 32, align: 8)
!11 = !DIBasicType(name: "float", size: 32, encoding: DW_ATE_float)
!12 = !{{}}
!13 = !DILocation(line: 11, column: 5, scope: !3)
!14 = !DILocalVariable(name: "instance", scope: !3, file: !2, line: 11, type: !6)
!15 = !DILocation(line: 12, column: 5, scope: !3)
)", g_test_source_files_path.generic_string());

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir, { .debug = true });
  }

  TEST_CASE("Compile Debug Information Variables", "[LLVM_IR]")
  {
    char const* const input_file = "debug_information_variables.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    std::string const expected_llvm_ir = std::format(R"(
; Function Attrs: convergent
define i32 @Debug_information_run() #0 !dbg !3 {{
entry:
  %i = alloca i32, align 4, !dbg !8
  call void @llvm.dbg.declare(metadata ptr %i, metadata !9, metadata !DIExpression()), !dbg !8
  store i32 0, ptr %i, align 4, !dbg !8
  store i32 2, ptr %i, align 4, !dbg !10
  %0 = load i32, ptr %i, align 4, !dbg !11
  ret i32 %0, !dbg !11
}}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

attributes #0 = {{ convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }}
attributes #1 = {{ nocallback nofree nosync nounwind speculatable willreturn memory(none) }}

!llvm.module.flags = !{{!0}}
!llvm.dbg.cu = !{{!1}}

!0 = !{{i32 2, !"Debug Info Version", i32 3}}
!1 = distinct !DICompileUnit(language: DW_LANG_C, file: !2, producer: "Iris Compiler", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!2 = !DIFile(filename: "debug_information_variables.iris", directory: "{}")
!3 = distinct !DISubprogram(name: "run", linkageName: "Debug_information_run", scope: null, file: !2, line: 3, type: !4, scopeLine: 4, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !1, retainedNodes: !7)
!4 = !DISubroutineType(types: !5)
!5 = !{{!6}}
!6 = !DIBasicType(name: "int32_t", size: 32, encoding: DW_ATE_signed)
!7 = !{{}}
!8 = !DILocation(line: 5, column: 5, scope: !3)
!9 = !DILocalVariable(name: "i", scope: !3, file: !2, line: 5, type: !6)
!10 = !DILocation(line: 6, column: 5, scope: !3)
!11 = !DILocation(line: 7, column: 5, scope: !3)
)", g_test_source_files_path.generic_string());

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir, { .debug = true });
  }

  TEST_CASE("Compile Debug Information While Loop", "[LLVM_IR]")
  {
    char const* const input_file = "debug_information_while_loop.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    std::string const expected_llvm_ir = std::format(R"(
; Function Attrs: convergent
define i32 @Debug_information_run() #0 !dbg !3 {{
entry:
  %value = alloca i32, align 4, !dbg !8
  call void @llvm.dbg.declare(metadata ptr %value, metadata !9, metadata !DIExpression()), !dbg !8
  %index = alloca i32, align 4, !dbg !10
  store i32 0, ptr %value, align 4, !dbg !8
  call void @llvm.dbg.declare(metadata ptr %index, metadata !11, metadata !DIExpression()), !dbg !10
  store i32 0, ptr %index, align 4, !dbg !10
  br label %while_loop_condition, !dbg !10

while_loop_condition:                             ; preds = %while_loop_then, %entry
  %0 = load i32, ptr %index, align 4, !dbg !10
  %1 = icmp slt i32 %0, 10, !dbg !10
  br i1 %1, label %while_loop_then, label %while_loop_after, !dbg !10

while_loop_then:                                  ; preds = %while_loop_condition
  %2 = load i32, ptr %value, align 4, !dbg !10
  %3 = load i32, ptr %index, align 4, !dbg !10
  %4 = add i32 %2, %3, !dbg !10
  store i32 %4, ptr %value, align 4, !dbg !12
  %5 = load i32, ptr %index, align 4, !dbg !12
  %6 = add i32 %5, 1, !dbg !12
  store i32 %6, ptr %index, align 4, !dbg !14
  br label %while_loop_condition, !dbg !15

while_loop_after:                                 ; preds = %while_loop_condition
  br label %while_loop_then1, !dbg !16

while_loop_then1:                                 ; preds = %if_s1_after, %while_loop_after
  %7 = load i32, ptr %index, align 4, !dbg !16
  %8 = add i32 %7, 1, !dbg !16
  store i32 %8, ptr %index, align 4, !dbg !17
  %9 = load i32, ptr %index, align 4, !dbg !19
  %10 = icmp sge i32 %9, 20, !dbg !19
  br i1 %10, label %if_s0_then, label %if_s1_after, !dbg !19

while_loop_after2:                                ; preds = %if_s0_then
  %11 = load i32, ptr %value, align 4, !dbg !20
  ret i32 %11, !dbg !20

if_s0_then:                                       ; preds = %while_loop_then1
  br label %while_loop_after2, !dbg !21

if_s1_after:                                      ; preds = %while_loop_then1
  br label %while_loop_then1, !dbg !23
}}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

attributes #0 = {{ convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }}
attributes #1 = {{ nocallback nofree nosync nounwind speculatable willreturn memory(none) }}

!llvm.module.flags = !{{!0}}
!llvm.dbg.cu = !{{!1}}

!0 = !{{i32 2, !"Debug Info Version", i32 3}}
!1 = distinct !DICompileUnit(language: DW_LANG_C, file: !2, producer: "Iris Compiler", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!2 = !DIFile(filename: "debug_information_while_loop.iris", directory: "{}")
!3 = distinct !DISubprogram(name: "run", linkageName: "Debug_information_run", scope: null, file: !2, line: 3, type: !4, scopeLine: 4, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !1, retainedNodes: !7)
!4 = !DISubroutineType(types: !5)
!5 = !{{!6}}
!6 = !DIBasicType(name: "int32_t", size: 32, encoding: DW_ATE_signed)
!7 = !{{}}
!8 = !DILocation(line: 5, column: 5, scope: !3)
!9 = !DILocalVariable(name: "value", scope: !3, file: !2, line: 5, type: !6)
!10 = !DILocation(line: 7, column: 5, scope: !3)
!11 = !DILocalVariable(name: "index", scope: !3, file: !2, line: 7, type: !6)
!12 = !DILocation(line: 10, column: 9, scope: !13)
!13 = distinct !DILexicalBlock(scope: !3, file: !2, line: 8, column: 5)
!14 = !DILocation(line: 11, column: 9, scope: !13)
!15 = !DILocation(line: 8, column: 11, scope: !13)
!16 = !DILocation(line: 14, column: 11, scope: !3)
!17 = !DILocation(line: 16, column: 9, scope: !18)
!18 = distinct !DILexicalBlock(scope: !3, file: !2, line: 14, column: 5)
!19 = !DILocation(line: 17, column: 12, scope: !18)
!20 = !DILocation(line: 23, column: 5, scope: !3)
!21 = !DILocation(line: 19, column: 13, scope: !22)
!22 = distinct !DILexicalBlock(scope: !18, file: !2, line: 18, column: 9)
!23 = !DILocation(line: 14, column: 11, scope: !18)
)", g_test_source_files_path.generic_string());

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir, { .debug = true });
  }

  TEST_CASE("Compile Decimal Expressions", "[LLVM_IR]")
  {
    char const* const input_file = "decimal_expressions.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define void @Decimal_expressions_decimal_constants() #0 {
entry:
  %a = alloca i32, align 4
  %b = alloca i32, align 4
  %c = alloca i64, align 8
  %d = alloca i32, align 4
  store i32 15, ptr %a, align 4
  store i32 15700, ptr %b, align 4
  store i64 1234567000, ptr %c, align 8
  store i32 10000, ptr %d, align 4
  ret void
}

; Function Attrs: convergent
define void @Decimal_expressions_decimal_32_arithmetic(i32 noundef %"arguments[0].x", i32 noundef %"arguments[1].y") #0 {
entry:
  %x = alloca i32, align 4
  %y = alloca i32, align 4
  %sum = alloca i32, align 4
  %diff = alloca i32, align 4
  %product = alloca i32, align 4
  %quotient = alloca i32, align 4
  store i32 %"arguments[0].x", ptr %x, align 4
  store i32 %"arguments[1].y", ptr %y, align 4
  %0 = load i32, ptr %x, align 4
  %1 = load i32, ptr %y, align 4
  %2 = add i32 %0, %1
  store i32 %2, ptr %sum, align 4
  %3 = load i32, ptr %x, align 4
  %4 = load i32, ptr %y, align 4
  %5 = sub i32 %3, %4
  store i32 %5, ptr %diff, align 4
  %6 = load i32, ptr %x, align 4
  %7 = load i32, ptr %y, align 4
  %8 = sext i32 %6 to i64
  %9 = sext i32 %7 to i64
  %10 = mul i64 %8, %9
  %11 = sdiv i64 %10, 10000
  %12 = trunc i64 %11 to i32
  store i32 %12, ptr %product, align 4
  %13 = load i32, ptr %x, align 4
  %14 = load i32, ptr %y, align 4
  %15 = sext i32 %13 to i64
  %16 = sext i32 %14 to i64
  %17 = mul i64 %15, 10000
  %18 = sdiv i64 %17, %16
  %19 = trunc i64 %18 to i32
  store i32 %19, ptr %quotient, align 4
  ret void
}

; Function Attrs: convergent
define void @Decimal_expressions_decimal_64_arithmetic(i64 noundef %"arguments[0].x", i64 noundef %"arguments[1].y") #0 {
entry:
  %x = alloca i64, align 8
  %y = alloca i64, align 8
  %sum = alloca i64, align 8
  %diff = alloca i64, align 8
  %product = alloca i64, align 8
  %quotient = alloca i64, align 8
  store i64 %"arguments[0].x", ptr %x, align 8
  store i64 %"arguments[1].y", ptr %y, align 8
  %0 = load i64, ptr %x, align 8
  %1 = load i64, ptr %y, align 8
  %2 = add i64 %0, %1
  store i64 %2, ptr %sum, align 8
  %3 = load i64, ptr %x, align 8
  %4 = load i64, ptr %y, align 8
  %5 = sub i64 %3, %4
  store i64 %5, ptr %diff, align 8
  %6 = load i64, ptr %x, align 8
  %7 = load i64, ptr %y, align 8
  %8 = sext i64 %6 to i128
  %9 = sext i64 %7 to i128
  %10 = mul i128 %8, %9
  %11 = sdiv i128 %10, 10000000
  %12 = trunc i128 %11 to i64
  store i64 %12, ptr %product, align 8
  %13 = load i64, ptr %x, align 8
  %14 = load i64, ptr %y, align 8
  %15 = sext i64 %13 to i128
  %16 = sext i64 %14 to i128
  %17 = mul i128 %15, 10000000
  %18 = sdiv i128 %17, %16
  %19 = trunc i128 %18 to i64
  store i64 %19, ptr %quotient, align 8
  ret void
}

; Function Attrs: convergent
define void @Decimal_expressions_decimal_casts(i32 noundef %"arguments[0].i32_value", i64 noundef %"arguments[1].i64_value", float noundef %"arguments[2].f32_value", double noundef %"arguments[3].f64_value", i32 noundef %"arguments[4].d4_value", i64 noundef %"arguments[5].d7_value") #0 {
entry:
  %i32_value = alloca i32, align 4
  %i64_value = alloca i64, align 8
  %f32_value = alloca float, align 4
  %f64_value = alloca double, align 8
  %d4_value = alloca i32, align 4
  %d7_value = alloca i64, align 8
  %i32_to_d4 = alloca i32, align 4
  %i32_to_d7 = alloca i64, align 8
  %i64_to_d4 = alloca i32, align 4
  %i64_to_d7 = alloca i64, align 8
  %d4_to_i32 = alloca i32, align 4
  %d4_to_i64 = alloca i64, align 8
  %d7_to_i32 = alloca i32, align 4
  %d7_to_i64 = alloca i64, align 8
  %f32_to_d4 = alloca i32, align 4
  %f32_to_d7 = alloca i64, align 8
  %f64_to_d4 = alloca i32, align 4
  %f64_to_d7 = alloca i64, align 8
  %d4_to_f32 = alloca float, align 4
  %d4_to_f64 = alloca double, align 8
  %d7_to_f32 = alloca float, align 4
  %d7_to_f64 = alloca double, align 8
  %d4_to_d7 = alloca i64, align 8
  %d7_to_d4 = alloca i32, align 4
  %rounded_positive_i32 = alloca i32, align 4
  %rounded_negative_i32 = alloca i32, align 4
  %rounded_positive_i64 = alloca i64, align 8
  %rounded_negative_i64 = alloca i64, align 8
  %rounded_f32_positive_d4 = alloca i32, align 4
  %rounded_f32_negative_d4 = alloca i32, align 4
  %rounded_f64_positive_d7 = alloca i64, align 8
  %rounded_f64_negative_d7 = alloca i64, align 8
  store i32 %"arguments[0].i32_value", ptr %i32_value, align 4
  store i64 %"arguments[1].i64_value", ptr %i64_value, align 8
  store float %"arguments[2].f32_value", ptr %f32_value, align 4
  store double %"arguments[3].f64_value", ptr %f64_value, align 8
  store i32 %"arguments[4].d4_value", ptr %d4_value, align 4
  store i64 %"arguments[5].d7_value", ptr %d7_value, align 8
  %0 = load i32, ptr %i32_value, align 4
  %1 = mul i32 %0, 10000
  store i32 %1, ptr %i32_to_d4, align 4
  %2 = load i32, ptr %i32_value, align 4
  %3 = sext i32 %2 to i64
  %4 = mul i64 %3, 10000000
  store i64 %4, ptr %i32_to_d7, align 8
  %5 = load i64, ptr %i64_value, align 8
  %6 = trunc i64 %5 to i32
  %7 = mul i32 %6, 10000
  store i32 %7, ptr %i64_to_d4, align 4
  %8 = load i64, ptr %i64_value, align 8
  %9 = mul i64 %8, 10000000
  store i64 %9, ptr %i64_to_d7, align 8
  %10 = load i32, ptr %d4_value, align 4
  %11 = sext i32 %10 to i64
  %12 = icmp sge i64 %11, 0
  %13 = select i1 %12, i64 5000, i64 -5000
  %14 = add i64 %11, %13
  %15 = sdiv i64 %14, 10000
  %16 = trunc i64 %15 to i32
  store i32 %16, ptr %d4_to_i32, align 4
  %17 = load i32, ptr %d4_value, align 4
  %18 = sext i32 %17 to i64
  %19 = icmp sge i64 %18, 0
  %20 = select i1 %19, i64 5000, i64 -5000
  %21 = add i64 %18, %20
  %22 = sdiv i64 %21, 10000
  store i64 %22, ptr %d4_to_i64, align 8
  %23 = load i64, ptr %d7_value, align 8
  %24 = sext i64 %23 to i128
  %25 = icmp sge i128 %24, 0
  %26 = select i1 %25, i128 5000000, i128 -5000000
  %27 = add i128 %24, %26
  %28 = sdiv i128 %27, 10000000
  %29 = trunc i128 %28 to i32
  store i32 %29, ptr %d7_to_i32, align 4
  %30 = load i64, ptr %d7_value, align 8
  %31 = sext i64 %30 to i128
  %32 = icmp sge i128 %31, 0
  %33 = select i1 %32, i128 5000000, i128 -5000000
  %34 = add i128 %31, %33
  %35 = sdiv i128 %34, 10000000
  %36 = trunc i128 %35 to i64
  store i64 %36, ptr %d7_to_i64, align 8
  %37 = load float, ptr %f32_value, align 4
  %38 = fmul float %37, 1.000000e+04
  %39 = fcmp oge float %38, 0.000000e+00
  %40 = select i1 %39, float 5.000000e-01, float -5.000000e-01
  %41 = fadd float %38, %40
  %42 = fptosi float %41 to i32
  store i32 %42, ptr %f32_to_d4, align 4
  %43 = load float, ptr %f32_value, align 4
  %44 = fmul float %43, 1.000000e+07
  %45 = fcmp oge float %44, 0.000000e+00
  %46 = select i1 %45, float 5.000000e-01, float -5.000000e-01
  %47 = fadd float %44, %46
  %48 = fptosi float %47 to i64
  store i64 %48, ptr %f32_to_d7, align 8
  %49 = load double, ptr %f64_value, align 8
  %50 = fmul double %49, 1.000000e+04
  %51 = fcmp oge double %50, 0.000000e+00
  %52 = select i1 %51, double 5.000000e-01, double -5.000000e-01
  %53 = fadd double %50, %52
  %54 = fptosi double %53 to i32
  store i32 %54, ptr %f64_to_d4, align 4
  %55 = load double, ptr %f64_value, align 8
  %56 = fmul double %55, 1.000000e+07
  %57 = fcmp oge double %56, 0.000000e+00
  %58 = select i1 %57, double 5.000000e-01, double -5.000000e-01
  %59 = fadd double %56, %58
  %60 = fptosi double %59 to i64
  store i64 %60, ptr %f64_to_d7, align 8
  %61 = load i32, ptr %d4_value, align 4
  %62 = sitofp i32 %61 to float
  %63 = fdiv float %62, 1.000000e+04
  store float %63, ptr %d4_to_f32, align 4
  %64 = load i32, ptr %d4_value, align 4
  %65 = sitofp i32 %64 to double
  %66 = fdiv double %65, 1.000000e+04
  store double %66, ptr %d4_to_f64, align 8
  %67 = load i64, ptr %d7_value, align 8
  %68 = sitofp i64 %67 to float
  %69 = fdiv float %68, 1.000000e+07
  store float %69, ptr %d7_to_f32, align 4
  %70 = load i64, ptr %d7_value, align 8
  %71 = sitofp i64 %70 to double
  %72 = fdiv double %71, 1.000000e+07
  store double %72, ptr %d7_to_f64, align 8
  %73 = load i32, ptr %d4_value, align 4
  %74 = sext i32 %73 to i128
  %75 = mul i128 %74, 1000
  %76 = trunc i128 %75 to i64
  store i64 %76, ptr %d4_to_d7, align 8
  %77 = load i64, ptr %d7_value, align 8
  %78 = sext i64 %77 to i128
  %79 = sdiv i128 %78, 1000
  %80 = trunc i128 %79 to i32
  store i32 %80, ptr %d7_to_d4, align 4
  store i32 1, ptr %rounded_positive_i32, align 4
  store i32 -1, ptr %rounded_negative_i32, align 4
  store i64 1, ptr %rounded_positive_i64, align 8
  store i64 -1, ptr %rounded_negative_i64, align 8
  store i32 12346, ptr %rounded_f32_positive_d4, align 4
  store i32 -12346, ptr %rounded_f32_negative_d4, align 4
  store i64 25000001, ptr %rounded_f64_positive_d7, align 8
  store i64 -25000001, ptr %rounded_f64_negative_d7, align 8
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Debug Information Decimal Types", "[LLVM_IR]")
  {
    char const* const input_file = "debug_information_decimal_expressions.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    std::string const expected_llvm_ir = std::format(R"(
; Function Attrs: convergent
define void @Debug_information_decimal_expressions_decimal_constants() #0 !dbg !3 {{
entry:
  %a = alloca i32, align 4, !dbg !7
  call void @llvm.dbg.declare(metadata ptr %a, metadata !8, metadata !DIExpression()), !dbg !14
  %b = alloca i32, align 4, !dbg !14
  %c = alloca i64, align 8, !dbg !15
  %d = alloca i32, align 4, !dbg !16
  store i32 15, ptr %a, align 4, !dbg !14
  call void @llvm.dbg.declare(metadata ptr %b, metadata !17, metadata !DIExpression()), !dbg !15
  store i32 15700, ptr %b, align 4, !dbg !15
  call void @llvm.dbg.declare(metadata ptr %c, metadata !22, metadata !DIExpression()), !dbg !16
  store i64 1234567000, ptr %c, align 8, !dbg !16
  call void @llvm.dbg.declare(metadata ptr %d, metadata !27, metadata !DIExpression()), !dbg !28
  store i32 10000, ptr %d, align 4, !dbg !28
  ret void, !dbg !28
}}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

attributes #0 = {{ convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }}
attributes #1 = {{ nocallback nofree nosync nounwind speculatable willreturn memory(none) }}

!llvm.module.flags = !{{!0}}
!llvm.dbg.cu = !{{!1}}

!0 = !{{i32 2, !"Debug Info Version", i32 3}}
!1 = distinct !DICompileUnit(language: DW_LANG_C, file: !2, producer: "Iris Compiler", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!2 = !DIFile(filename: "debug_information_decimal_expressions.iris", directory: "{}")
!3 = distinct !DISubprogram(name: "decimal_constants", linkageName: "Debug_information_decimal_expressions_decimal_constants", scope: null, file: !2, line: 3, type: !4, scopeLine: 4, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !1, retainedNodes: !6)
!4 = !DISubroutineType(types: !5)
!5 = !{{null}}
!6 = !{{}}
!7 = !DILocation(line: 4, column: 1, scope: !3)
!8 = !DILocalVariable(name: "a", scope: !3, file: !2, line: 5, type: !9)
!9 = !DICompositeType(tag: DW_TAG_structure_type, name: "iris::Decimal2", scope: !10, size: 32, align: 32, elements: !11)
!10 = !DINamespace(name: "h", scope: null)
!11 = !{{!12}}
!12 = !DIDerivedType(tag: DW_TAG_member, name: "raw", scope: !10, baseType: !13, size: 32, align: 32)
!13 = !DIBasicType(name: "iris::Decimal2_storage", size: 32, encoding: DW_ATE_signed)
!14 = !DILocation(line: 5, column: 5, scope: !3)
!15 = !DILocation(line: 6, column: 5, scope: !3)
!16 = !DILocation(line: 7, column: 5, scope: !3)
!17 = !DILocalVariable(name: "b", scope: !3, file: !2, line: 6, type: !18)
!18 = !DICompositeType(tag: DW_TAG_structure_type, name: "iris::Decimal4", scope: !10, size: 32, align: 32, elements: !19)
!19 = !{{!20}}
!20 = !DIDerivedType(tag: DW_TAG_member, name: "raw", scope: !10, baseType: !21, size: 32, align: 32)
!21 = !DIBasicType(name: "iris::Decimal4_storage", size: 32, encoding: DW_ATE_signed)
!22 = !DILocalVariable(name: "c", scope: !3, file: !2, line: 7, type: !23)
!23 = !DICompositeType(tag: DW_TAG_structure_type, name: "iris::Decimal7", scope: !10, size: 64, align: 64, elements: !24)
!24 = !{{!25}}
!25 = !DIDerivedType(tag: DW_TAG_member, name: "raw", scope: !10, baseType: !26, size: 64, align: 64)
!26 = !DIBasicType(name: "iris::Decimal7_storage", size: 64, encoding: DW_ATE_signed)
!27 = !DILocalVariable(name: "d", scope: !3, file: !2, line: 8, type: !9)
!28 = !DILocation(line: 8, column: 5, scope: !3)
)", g_test_source_files_path.generic_string());

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir, { .debug = true });
  }

  TEST_CASE("Compile Defer Expressions", "[LLVM_IR]")
  {
    char const* const input_file = "defer_expressions.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define private void @Defer_expressions_do_defer(i32 noundef %"arguments[0].value") #0 {
entry:
  %value = alloca i32, align 4
  store i32 %"arguments[0].value", ptr %value, align 4
  ret void
}

; Function Attrs: convergent
define private void @Defer_expressions_run(i1 noundef zeroext %"arguments[0].condition", i32 noundef %"arguments[1].value") #0 {
entry:
  %condition = alloca i8, align 1
  %value = alloca i32, align 4
  %v2 = alloca i32, align 4
  %v3 = alloca i32, align 4
  %v4 = alloca i32, align 4
  %v5 = alloca i32, align 4
  %v6 = alloca i32, align 4
  %v7 = alloca i32, align 4
  %index = alloca i32, align 4
  %v8 = alloca i32, align 4
  %v9 = alloca i32, align 4
  %v10 = alloca i32, align 4
  %i = alloca i32, align 4
  %j = alloca i32, align 4
  %0 = zext i1 %"arguments[0].condition" to i8
  store i8 %0, ptr %condition, align 1
  store i32 %"arguments[1].value", ptr %value, align 4
  %1 = load i8, ptr %condition, align 1
  %2 = trunc i8 %1 to i1
  br i1 %2, label %if_s0_then, label %if_s1_else

if_s0_then:                                       ; preds = %entry
  store i32 2, ptr %v2, align 4
  %3 = load i32, ptr %v2, align 4
  call void @Defer_expressions_do_defer(i32 noundef %3)
  call void @Defer_expressions_do_defer(i32 noundef 2)
  br label %if_s4_after

if_s1_else:                                       ; preds = %entry
  %4 = load i32, ptr %value, align 4
  %5 = icmp eq i32 %4, 0
  br i1 %5, label %if_s2_then, label %if_s3_else

if_s2_then:                                       ; preds = %if_s1_else
  store i32 3, ptr %v3, align 4
  %6 = load i32, ptr %v3, align 4
  call void @Defer_expressions_do_defer(i32 noundef %6)
  call void @Defer_expressions_do_defer(i32 noundef 3)
  br label %if_s4_after

if_s3_else:                                       ; preds = %if_s1_else
  store i32 4, ptr %v4, align 4
  %7 = load i32, ptr %v4, align 4
  call void @Defer_expressions_do_defer(i32 noundef %7)
  call void @Defer_expressions_do_defer(i32 noundef 4)
  br label %if_s4_after

if_s4_after:                                      ; preds = %if_s3_else, %if_s2_then, %if_s0_then
  %8 = load i8, ptr %condition, align 1
  %9 = trunc i8 %8 to i1
  br i1 %9, label %if_s0_then1, label %if_s1_after

if_s0_then1:                                      ; preds = %if_s4_after
  call void @Defer_expressions_do_defer(i32 noundef 1)
  call void @Defer_expressions_do_defer(i32 noundef 0)
  ret void

if_s1_after:                                      ; preds = %if_s4_after
  br label %while_loop_condition

while_loop_condition:                             ; preds = %while_loop_then, %if_s1_after
  %10 = load i8, ptr %condition, align 1
  %11 = trunc i8 %10 to i1
  br i1 %11, label %while_loop_then, label %while_loop_after

while_loop_then:                                  ; preds = %while_loop_condition
  store i32 5, ptr %v5, align 4
  %12 = load i32, ptr %v5, align 4
  call void @Defer_expressions_do_defer(i32 noundef %12)
  call void @Defer_expressions_do_defer(i32 noundef 5)
  br label %while_loop_condition

while_loop_after:                                 ; preds = %while_loop_condition
  br label %while_loop_condition2

while_loop_condition2:                            ; preds = %while_loop_then3, %while_loop_after
  %13 = load i8, ptr %condition, align 1
  %14 = trunc i8 %13 to i1
  br i1 %14, label %while_loop_then3, label %while_loop_after4

while_loop_then3:                                 ; preds = %while_loop_condition2
  store i32 6, ptr %v6, align 4
  call void @Defer_expressions_do_defer(i32 noundef 6)
  br label %while_loop_condition2

while_loop_after4:                                ; preds = %while_loop_condition2
  br label %while_loop_condition5

while_loop_condition5:                            ; preds = %while_loop_after4
  %15 = load i8, ptr %condition, align 1
  %16 = trunc i8 %15 to i1
  br i1 %16, label %while_loop_then6, label %while_loop_after7

while_loop_then6:                                 ; preds = %while_loop_condition5
  store i32 7, ptr %v7, align 4
  call void @Defer_expressions_do_defer(i32 noundef 7)
  br label %while_loop_after7

while_loop_after7:                                ; preds = %while_loop_then6, %while_loop_condition5
  store i32 0, ptr %index, align 4
  br label %for_loop_condition

for_loop_condition:                               ; preds = %for_loop_update_index, %while_loop_after7
  %17 = load i32, ptr %index, align 4
  %18 = icmp slt i32 %17, 10
  br i1 %18, label %for_loop_then, label %for_loop_after

for_loop_then:                                    ; preds = %for_loop_condition
  store i32 8, ptr %v8, align 4
  %19 = load i32, ptr %v8, align 4
  call void @Defer_expressions_do_defer(i32 noundef %19)
  call void @Defer_expressions_do_defer(i32 noundef 8)
  br label %for_loop_update_index

for_loop_update_index:                            ; preds = %for_loop_then
  %20 = load i32, ptr %index, align 4
  %21 = add i32 %20, 1
  store i32 %21, ptr %index, align 4
  br label %for_loop_condition

for_loop_after:                                   ; preds = %for_loop_condition
  %22 = load i32, ptr %value, align 4
  switch i32 %22, label %switch_after [
    i32 0, label %switch_case_i0_
  ]

switch_after:                                     ; preds = %switch_case_i0_, %for_loop_after
  store i32 10, ptr %v10, align 4
  %23 = load i32, ptr %v10, align 4
  call void @Defer_expressions_do_defer(i32 noundef %23)
  call void @Defer_expressions_do_defer(i32 noundef 10)
  store i32 0, ptr %i, align 4
  br label %for_loop_condition8

switch_case_i0_:                                  ; preds = %for_loop_after
  store i32 9, ptr %v9, align 4
  %24 = load i32, ptr %v9, align 4
  call void @Defer_expressions_do_defer(i32 noundef %24)
  call void @Defer_expressions_do_defer(i32 noundef 9)
  br label %switch_after

for_loop_condition8:                              ; preds = %for_loop_update_index10, %switch_after
  %25 = load i32, ptr %i, align 4
  %26 = icmp slt i32 %25, 10
  br i1 %26, label %for_loop_then9, label %for_loop_after11

for_loop_then9:                                   ; preds = %for_loop_condition8
  %27 = load i32, ptr %i, align 4
  %28 = srem i32 %27, 2
  %29 = icmp eq i32 %28, 0
  br i1 %29, label %if_s0_then12, label %if_s1_after13

for_loop_update_index10:                          ; preds = %if_s1_after13
  %30 = load i32, ptr %i, align 4
  %31 = add i32 %30, 1
  store i32 %31, ptr %i, align 4
  br label %for_loop_condition8

for_loop_after11:                                 ; preds = %if_s0_then18, %for_loop_condition8
  call void @Defer_expressions_do_defer(i32 noundef 1)
  call void @Defer_expressions_do_defer(i32 noundef 0)
  ret void

if_s0_then12:                                     ; preds = %for_loop_then9
  store i32 0, ptr %j, align 4
  br label %for_loop_condition14

if_s1_after13:                                    ; preds = %for_loop_after17, %for_loop_then9
  call void @Defer_expressions_do_defer(i32 noundef 11)
  br label %for_loop_update_index10

for_loop_condition14:                             ; preds = %for_loop_update_index16, %if_s0_then12
  %32 = load i32, ptr %j, align 4
  %33 = icmp slt i32 %32, 10
  br i1 %33, label %for_loop_then15, label %for_loop_after17

for_loop_then15:                                  ; preds = %for_loop_condition14
  %34 = load i32, ptr %j, align 4
  %35 = srem i32 %34, 2
  %36 = icmp eq i32 %35, 0
  br i1 %36, label %if_s0_then18, label %if_s1_after19

for_loop_update_index16:                          ; preds = %if_s1_after19
  %37 = load i32, ptr %j, align 4
  %38 = add i32 %37, 1
  store i32 %38, ptr %j, align 4
  br label %for_loop_condition14

for_loop_after17:                                 ; preds = %for_loop_condition14
  call void @Defer_expressions_do_defer(i32 noundef 12)
  br label %if_s1_after13

if_s0_then18:                                     ; preds = %for_loop_then15
  call void @Defer_expressions_do_defer(i32 noundef 12)
  call void @Defer_expressions_do_defer(i32 noundef 11)
  br label %for_loop_after11

if_s1_after19:                                    ; preds = %for_loop_then15
  br label %for_loop_update_index16
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Dereference and Access Expressions", "[LLVM_IR]")
  {
    char const* const input_file = "dereference_and_access_expressions.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
%struct.Dereference_and_access_My_struct = type { i32 }

; Function Attrs: convergent
define void @Dereference_and_access_run() #0 {
entry:
  %instance = alloca %struct.Dereference_and_access_My_struct, align 4
  %pointer = alloca ptr, align 8
  %a = alloca i32, align 4
  %0 = getelementptr inbounds %struct.Dereference_and_access_My_struct, ptr %instance, i32 0, i32 0
  store i32 0, ptr %0, align 4
  store ptr %instance, ptr %pointer, align 8
  %1 = load ptr, ptr %pointer, align 8
  %2 = getelementptr inbounds %struct.Dereference_and_access_My_struct, ptr %1, i32 0, i32 0
  %3 = load i32, ptr %2, align 4
  store i32 %3, ptr %a, align 4
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Defer Expressions with Debug Information", "[LLVM_IR]")
  {
    char const* const input_file = "defer_expressions_with_debug_information.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    std::string const expected_llvm_ir = std::format(R"(
; Function Attrs: convergent
define private void @Defer_expressions_with_debug_information_do_defer(i32 noundef %"arguments[0].value") #0 !dbg !3 {{
entry:
  %value = alloca i32, align 4
  store i32 %"arguments[0].value", ptr %value, align 4
  call void @llvm.dbg.declare(metadata ptr %value, metadata !8, metadata !DIExpression()), !dbg !9
  ret void, !dbg !10
}}

; Function Attrs: convergent
define private void @Defer_expressions_with_debug_information_run(i1 noundef zeroext %"arguments[0].condition", i32 noundef %"arguments[1].value") #0 !dbg !11 {{
entry:
  %condition = alloca i8, align 1
  %value = alloca i32, align 4
  %value_2 = alloca i32, align 4, !dbg !18
  %0 = zext i1 %"arguments[0].condition" to i8
  store i8 %0, ptr %condition, align 1
  call void @llvm.dbg.declare(metadata ptr %condition, metadata !16, metadata !DIExpression()), !dbg !19
  store i32 %"arguments[1].value", ptr %value, align 4
  call void @llvm.dbg.declare(metadata ptr %value, metadata !17, metadata !DIExpression()), !dbg !20
  call void @llvm.dbg.declare(metadata ptr %value_2, metadata !21, metadata !DIExpression()), !dbg !18
  store i32 0, ptr %value_2, align 4, !dbg !18
  call void @Defer_expressions_with_debug_information_do_defer(i32 noundef 1), !dbg !22
  call void @Defer_expressions_with_debug_information_do_defer(i32 noundef 0), !dbg !23
  ret void, !dbg !24
}}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

attributes #0 = {{ convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }}
attributes #1 = {{ nocallback nofree nosync nounwind speculatable willreturn memory(none) }}

!llvm.module.flags = !{{!0}}
!llvm.dbg.cu = !{{!1}}

!0 = !{{i32 2, !"Debug Info Version", i32 3}}
!1 = distinct !DICompileUnit(language: DW_LANG_C, file: !2, producer: "Iris Compiler", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!2 = !DIFile(filename: "defer_expressions_with_debug_information.iris", directory: "{}")
!3 = distinct !DISubprogram(name: "do_defer", linkageName: "Defer_expressions_with_debug_information_do_defer", scope: null, file: !2, line: 3, type: !4, scopeLine: 4, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !1, retainedNodes: !7)
!4 = !DISubroutineType(types: !5)
!5 = !{{null, !6}}
!6 = !DIBasicType(name: "int32_t", size: 32, encoding: DW_ATE_signed)
!7 = !{{!8}}
!8 = !DILocalVariable(name: "value", arg: 1, scope: !3, file: !2, line: 3, type: !6)
!9 = !DILocation(line: 3, column: 19, scope: !3)
!10 = !DILocation(line: 4, column: 1, scope: !3)
!11 = distinct !DISubprogram(name: "run", linkageName: "Defer_expressions_with_debug_information_run", scope: null, file: !2, line: 7, type: !12, scopeLine: 8, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !1, retainedNodes: !15)
!12 = !DISubroutineType(types: !13)
!13 = !{{null, !14, !6}}
!14 = !DIBasicType(name: "bool", size: 1, encoding: DW_ATE_boolean)
!15 = !{{!16, !17}}
!16 = !DILocalVariable(name: "condition", arg: 1, scope: !11, file: !2, line: 7, type: !14)
!17 = !DILocalVariable(name: "value", arg: 2, scope: !11, file: !2, line: 7, type: !6)
!18 = !DILocation(line: 10, column: 5, scope: !11)
!19 = !DILocation(line: 7, column: 14, scope: !11)
!20 = !DILocation(line: 7, column: 31, scope: !11)
!21 = !DILocalVariable(name: "value_2", scope: !11, file: !2, line: 10, type: !6)
!22 = !DILocation(line: 11, column: 11, scope: !11)
!23 = !DILocation(line: 9, column: 11, scope: !11)
!24 = !DILocation(line: 12, column: 5, scope: !11)
)", g_test_source_files_path.generic_string());

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir, { .debug = true });
  }

  TEST_CASE("Compile Dynamic Array", "[LLVM_IR]")
  {
    char const* const input_file = "dynamic_array.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"()";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Dynamic Array Usage", "[LLVM_IR]")
  {
    char const* const input_file = "dynamic_array_usage.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
      { "dynamic_array", parse_and_get_file_path(g_test_source_files_path / "dynamic_array.iris") },
    };

    char const* const expected_llvm_ir = R"(
%struct.dynamic_array_Allocator = type { ptr, ptr }
%struct.dynamic_array__at__Dynamic_array__at__10870525800499546629 = type { ptr, i64, i64, %struct.dynamic_array_Allocator }

@function_contract_error_string = private unnamed_addr constant [135 x i8] c"In function 'dynamic_array_usage.dynamic_array__at__create__at__9190698639914732028' precondition 'allocator.allocate != null' failed!\00"
@function_contract_error_string.1 = private unnamed_addr constant [137 x i8] c"In function 'dynamic_array_usage.dynamic_array__at__create__at__9190698639914732028' precondition 'allocator.deallocate != null' failed!\00"
@function_contract_error_string.2 = private unnamed_addr constant [129 x i8] c"In function 'dynamic_array_usage.dynamic_array__at__push_back__at__15363871578545837817' precondition 'instance != null' failed!\00"
@function_contract_error_string.3 = private unnamed_addr constant [130 x i8] c"In function 'dynamic_array_usage.dynamic_array__at__push_back__at__15363871578545837817' assert 'Allocation did not fail' failed!\00"
@function_contract_error_string.4 = private unnamed_addr constant [123 x i8] c"In function 'dynamic_array_usage.dynamic_array__at__get__at__11326571526526506051' precondition 'instance != null' failed!\00"
@function_contract_error_string.5 = private unnamed_addr constant [131 x i8] c"In function 'dynamic_array_usage.dynamic_array__at__get__at__11326571526526506051' precondition 'index < instance->length' failed!\00"

; Function Attrs: convergent
define private void @dynamic_array_usage_run() #0 {
entry:
  %allocator = alloca %struct.dynamic_array_Allocator, align 8
  %0 = alloca %struct.dynamic_array__at__Dynamic_array__at__10870525800499546629, align 8
  %instance = alloca %struct.dynamic_array__at__Dynamic_array__at__10870525800499546629, align 8
  %element = alloca i32, align 4
  %1 = getelementptr inbounds %struct.dynamic_array_Allocator, ptr %allocator, i32 0, i32 0
  store ptr null, ptr %1, align 8
  %2 = getelementptr inbounds %struct.dynamic_array_Allocator, ptr %allocator, i32 0, i32 1
  store ptr null, ptr %2, align 8
  %3 = getelementptr inbounds { ptr, ptr }, ptr %allocator, i32 0, i32 0
  %4 = load ptr, ptr %3, align 8
  %5 = getelementptr inbounds { ptr, ptr }, ptr %allocator, i32 0, i32 1
  %6 = load ptr, ptr %5, align 8
  call void @dynamic_array__at__create__at__9190698639914732028(ptr dead_on_unwind noalias writable sret(%struct.dynamic_array__at__Dynamic_array__at__10870525800499546629) align 8 %0, ptr %4, ptr %6)
  %7 = load %struct.dynamic_array__at__Dynamic_array__at__10870525800499546629, ptr %0, align 8
  store %struct.dynamic_array__at__Dynamic_array__at__10870525800499546629 %7, ptr %instance, align 8
  call void @dynamic_array__at__push_back__at__15363871578545837817(ptr noundef %instance, i32 noundef 1)
  %8 = call i32 @dynamic_array__at__get__at__11326571526526506051(ptr noundef %instance, i64 noundef 0)
  store i32 %8, ptr %element, align 4
  ret void
}

; Function Attrs: convergent
define private void @dynamic_array__at__create__at__9190698639914732028(ptr dead_on_unwind noalias writable sret(%struct.dynamic_array__at__Dynamic_array__at__10870525800499546629) align 8 %return.instance, ptr %"arguments[0].allocator_0", ptr %"arguments[0].allocator_1") #0 {
entry:
  %allocator = alloca %struct.dynamic_array_Allocator, align 8
  %0 = alloca %struct.dynamic_array__at__Dynamic_array__at__10870525800499546629, align 8
  %1 = getelementptr inbounds { ptr, ptr }, ptr %allocator, i32 0, i32 0
  store ptr %"arguments[0].allocator_0", ptr %1, align 8
  %2 = getelementptr inbounds { ptr, ptr }, ptr %allocator, i32 0, i32 1
  store ptr %"arguments[0].allocator_1", ptr %2, align 8
  %3 = getelementptr inbounds %struct.dynamic_array_Allocator, ptr %allocator, i32 0, i32 0
  %4 = load ptr, ptr %3, align 8
  %5 = icmp ne ptr %4, null
  br i1 %5, label %condition_success, label %condition_fail

condition_success:                                ; preds = %entry
  %6 = getelementptr inbounds %struct.dynamic_array_Allocator, ptr %allocator, i32 0, i32 1
  %7 = load ptr, ptr %6, align 8
  %8 = icmp ne ptr %7, null
  br i1 %8, label %condition_success1, label %condition_fail2

condition_fail:                                   ; preds = %entry
  %9 = call i32 @puts(ptr @function_contract_error_string)
  call void @abort()
  unreachable

condition_success1:                               ; preds = %condition_success
  %10 = getelementptr inbounds %struct.dynamic_array__at__Dynamic_array__at__10870525800499546629, ptr %0, i32 0, i32 0
  store ptr null, ptr %10, align 8
  %11 = getelementptr inbounds %struct.dynamic_array__at__Dynamic_array__at__10870525800499546629, ptr %0, i32 0, i32 1
  store i64 0, ptr %11, align 8
  %12 = getelementptr inbounds %struct.dynamic_array__at__Dynamic_array__at__10870525800499546629, ptr %0, i32 0, i32 2
  store i64 0, ptr %12, align 8
  %13 = load %struct.dynamic_array_Allocator, ptr %allocator, align 8
  %14 = getelementptr inbounds %struct.dynamic_array__at__Dynamic_array__at__10870525800499546629, ptr %0, i32 0, i32 3
  store %struct.dynamic_array_Allocator %13, ptr %14, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr align 8 %return.instance, ptr align 8 %0, i64 40, i1 false)
  ret void

condition_fail2:                                  ; preds = %condition_success
  %15 = call i32 @puts(ptr @function_contract_error_string.1)
  call void @abort()
  unreachable
}

; Function Attrs: convergent
define private void @dynamic_array__at__push_back__at__15363871578545837817(ptr noundef %"arguments[0].instance", i32 noundef %"arguments[1].element") #0 {
entry:
  %instance = alloca ptr, align 8
  %element = alloca i32, align 4
  %new_capacity = alloca i64, align 8
  %allocation_size_in_bytes = alloca i64, align 8
  %allocation = alloca ptr, align 8
  %index = alloca i64, align 8
  store ptr %"arguments[0].instance", ptr %instance, align 8
  store i32 %"arguments[1].element", ptr %element, align 4
  %0 = load ptr, ptr %instance, align 8
  %1 = icmp ne ptr %0, null
  br i1 %1, label %condition_success, label %condition_fail

condition_success:                                ; preds = %entry
  %2 = load ptr, ptr %instance, align 8
  %3 = getelementptr inbounds %struct.dynamic_array__at__Dynamic_array__at__10870525800499546629, ptr %2, i32 0, i32 1
  %4 = load i64, ptr %3, align 8
  %5 = load ptr, ptr %instance, align 8
  %6 = getelementptr inbounds %struct.dynamic_array__at__Dynamic_array__at__10870525800499546629, ptr %5, i32 0, i32 2
  %7 = load i64, ptr %6, align 8
  %8 = icmp eq i64 %4, %7
  br i1 %8, label %if_s0_then, label %if_s1_after

condition_fail:                                   ; preds = %entry
  %9 = call i32 @puts(ptr @function_contract_error_string.2)
  call void @abort()
  unreachable

if_s0_then:                                       ; preds = %condition_success
  %10 = load ptr, ptr %instance, align 8
  %11 = getelementptr inbounds %struct.dynamic_array__at__Dynamic_array__at__10870525800499546629, ptr %10, i32 0, i32 2
  %12 = load i64, ptr %11, align 8
  %13 = add i64 %12, 1
  %14 = mul i64 2, %13
  store i64 %14, ptr %new_capacity, align 8
  %15 = load i64, ptr %new_capacity, align 8
  %16 = mul i64 %15, 4
  store i64 %16, ptr %allocation_size_in_bytes, align 8
  %17 = load ptr, ptr %instance, align 8
  %18 = getelementptr inbounds %struct.dynamic_array__at__Dynamic_array__at__10870525800499546629, ptr %17, i32 0, i32 3
  %19 = getelementptr inbounds %struct.dynamic_array_Allocator, ptr %18, i32 0, i32 0
  %20 = load ptr, ptr %19, align 8
  %21 = load i64, ptr %allocation_size_in_bytes, align 8
  %22 = call ptr %20(i64 noundef %21, i64 noundef 4)
  store ptr %22, ptr %allocation, align 8
  %23 = load ptr, ptr %allocation, align 8
  %24 = icmp ne ptr %23, null
  br i1 %24, label %condition_success1, label %condition_fail2

if_s1_after:                                      ; preds = %condition_success1, %condition_success
  %25 = load ptr, ptr %instance, align 8
  %26 = getelementptr inbounds %struct.dynamic_array__at__Dynamic_array__at__10870525800499546629, ptr %25, i32 0, i32 1
  %27 = load i64, ptr %26, align 8
  store i64 %27, ptr %index, align 8
  %28 = load i64, ptr %index, align 8
  %29 = load ptr, ptr %instance, align 8
  %30 = getelementptr inbounds %struct.dynamic_array__at__Dynamic_array__at__10870525800499546629, ptr %29, i32 0, i32 0
  %31 = load ptr, ptr %30, align 8
  %array_element_pointer = getelementptr i32, ptr %31, i64 %28
  %32 = load i32, ptr %element, align 4
  store i32 %32, ptr %array_element_pointer, align 4
  %33 = load ptr, ptr %instance, align 8
  %34 = getelementptr inbounds %struct.dynamic_array__at__Dynamic_array__at__10870525800499546629, ptr %33, i32 0, i32 1
  %35 = load ptr, ptr %instance, align 8
  %36 = getelementptr inbounds %struct.dynamic_array__at__Dynamic_array__at__10870525800499546629, ptr %35, i32 0, i32 1
  %37 = load i64, ptr %36, align 8
  %38 = add i64 %37, 1
  store i64 %38, ptr %34, align 8
  ret void

condition_success1:                               ; preds = %if_s0_then
  %39 = load ptr, ptr %instance, align 8
  %40 = getelementptr inbounds %struct.dynamic_array__at__Dynamic_array__at__10870525800499546629, ptr %39, i32 0, i32 0
  %41 = load ptr, ptr %allocation, align 8
  store ptr %41, ptr %40, align 8
  %42 = load ptr, ptr %instance, align 8
  %43 = getelementptr inbounds %struct.dynamic_array__at__Dynamic_array__at__10870525800499546629, ptr %42, i32 0, i32 2
  %44 = load i64, ptr %new_capacity, align 8
  store i64 %44, ptr %43, align 8
  br label %if_s1_after

condition_fail2:                                  ; preds = %if_s0_then
  %45 = call i32 @puts(ptr @function_contract_error_string.3)
  call void @abort()
  unreachable
}

; Function Attrs: convergent
define private i32 @dynamic_array__at__get__at__11326571526526506051(ptr noundef %"arguments[0].instance", i64 noundef %"arguments[1].index") #0 {
entry:
  %instance = alloca ptr, align 8
  %index = alloca i64, align 8
  store ptr %"arguments[0].instance", ptr %instance, align 8
  store i64 %"arguments[1].index", ptr %index, align 8
  %0 = load ptr, ptr %instance, align 8
  %1 = icmp ne ptr %0, null
  br i1 %1, label %condition_success, label %condition_fail

condition_success:                                ; preds = %entry
  %2 = load i64, ptr %index, align 8
  %3 = load ptr, ptr %instance, align 8
  %4 = getelementptr inbounds %struct.dynamic_array__at__Dynamic_array__at__10870525800499546629, ptr %3, i32 0, i32 1
  %5 = load i64, ptr %4, align 8
  %6 = icmp ult i64 %2, %5
  br i1 %6, label %condition_success1, label %condition_fail2

condition_fail:                                   ; preds = %entry
  %7 = call i32 @puts(ptr @function_contract_error_string.4)
  call void @abort()
  unreachable

condition_success1:                               ; preds = %condition_success
  %8 = load i64, ptr %index, align 8
  %9 = load ptr, ptr %instance, align 8
  %10 = getelementptr inbounds %struct.dynamic_array__at__Dynamic_array__at__10870525800499546629, ptr %9, i32 0, i32 0
  %11 = load ptr, ptr %10, align 8
  %array_element_pointer = getelementptr i32, ptr %11, i64 %8
  %12 = load i32, ptr %array_element_pointer, align 4
  ret i32 %12

condition_fail2:                                  ; preds = %condition_success
  %13 = call i32 @puts(ptr @function_contract_error_string.5)
  call void @abort()
  unreachable
}

declare i32 @puts(ptr)

declare void @abort()

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: readwrite)
declare void @llvm.memcpy.p0.p0.i64(ptr noalias nocapture writeonly, ptr noalias nocapture readonly, i64, i1 immarg) #1

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
attributes #1 = { nocallback nofree nounwind willreturn memory(argmem: readwrite) }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Empty Return Expression", "[LLVM_IR]")
  {
    char const* const input_file = "empty_return_expression.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define private void @Empty_return_expression_run() #0 {
entry:
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile For Loop Expressions", "[LLVM_IR]")
  {
    char const* const input_file = "for_loop_expressions.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
      { "c.stdio", import_c_header_and_get_file_path("c.stdio", "stdio.h") }
    };

    char const* const expected_llvm_ir = R"(
@global_0 = internal constant [3 x i8] c"%d\00"

; Function Attrs: convergent
declare i32 @printf(ptr noundef, ...) #0

; Function Attrs: convergent
define void @For_loop_expressions_run_for_loops() #0 {
entry:
  %index = alloca i32, align 4
  %index1 = alloca i32, align 4
  %index6 = alloca i32, align 4
  %index11 = alloca i32, align 4
  store i32 0, ptr %index, align 4
  br label %for_loop_condition

for_loop_condition:                               ; preds = %for_loop_update_index, %entry
  %0 = load i32, ptr %index, align 4
  %1 = icmp slt i32 %0, 3
  br i1 %1, label %for_loop_then, label %for_loop_after

for_loop_then:                                    ; preds = %for_loop_condition
  %2 = load i32, ptr %index, align 4
  call void @For_loop_expressions_print_integer(i32 noundef %2)
  br label %for_loop_update_index

for_loop_update_index:                            ; preds = %for_loop_then
  %3 = load i32, ptr %index, align 4
  %4 = add i32 %3, 1
  store i32 %4, ptr %index, align 4
  br label %for_loop_condition

for_loop_after:                                   ; preds = %for_loop_condition
  store i32 0, ptr %index1, align 4
  br label %for_loop_condition2

for_loop_condition2:                              ; preds = %for_loop_update_index4, %for_loop_after
  %5 = load i32, ptr %index1, align 4
  %6 = icmp slt i32 %5, 4
  br i1 %6, label %for_loop_then3, label %for_loop_after5

for_loop_then3:                                   ; preds = %for_loop_condition2
  %7 = load i32, ptr %index1, align 4
  call void @For_loop_expressions_print_integer(i32 noundef %7)
  br label %for_loop_update_index4

for_loop_update_index4:                           ; preds = %for_loop_then3
  %8 = load i32, ptr %index1, align 4
  %9 = add i32 %8, 1
  store i32 %9, ptr %index1, align 4
  br label %for_loop_condition2

for_loop_after5:                                  ; preds = %for_loop_condition2
  store i32 4, ptr %index6, align 4
  br label %for_loop_condition7

for_loop_condition7:                              ; preds = %for_loop_update_index9, %for_loop_after5
  %10 = load i32, ptr %index6, align 4
  %11 = icmp sgt i32 %10, 0
  br i1 %11, label %for_loop_then8, label %for_loop_after10

for_loop_then8:                                   ; preds = %for_loop_condition7
  %12 = load i32, ptr %index6, align 4
  call void @For_loop_expressions_print_integer(i32 noundef %12)
  br label %for_loop_update_index9

for_loop_update_index9:                           ; preds = %for_loop_then8
  %13 = load i32, ptr %index6, align 4
  %14 = add i32 %13, -1
  store i32 %14, ptr %index6, align 4
  br label %for_loop_condition7

for_loop_after10:                                 ; preds = %for_loop_condition7
  store i32 4, ptr %index11, align 4
  br label %for_loop_condition12

for_loop_condition12:                             ; preds = %for_loop_update_index14, %for_loop_after10
  %15 = load i32, ptr %index11, align 4
  %16 = icmp sgt i32 %15, 0
  br i1 %16, label %for_loop_then13, label %for_loop_after15

for_loop_then13:                                  ; preds = %for_loop_condition12
  %17 = load i32, ptr %index11, align 4
  call void @For_loop_expressions_print_integer(i32 noundef %17)
  br label %for_loop_update_index14

for_loop_update_index14:                          ; preds = %for_loop_then13
  %18 = load i32, ptr %index11, align 4
  %19 = add i32 %18, -1
  store i32 %19, ptr %index11, align 4
  br label %for_loop_condition12

for_loop_after15:                                 ; preds = %for_loop_condition12
  ret void
}

; Function Attrs: convergent
define private void @For_loop_expressions_print_integer(i32 noundef %"arguments[0].value") #0 {
entry:
  %value = alloca i32, align 4
  store i32 %"arguments[0].value", ptr %value, align 4
  %0 = load i32, ptr %value, align 4
  %1 = call i32 (ptr, ...) @printf(ptr noundef @global_0, i32 noundef %0)
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Function Pointers", "[LLVM_IR]")
  {
    char const* const input_file = "function_pointers.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
%struct.Function_pointers_My_struct = type { ptr, ptr }

; Function Attrs: convergent
define void @Function_pointers_run() #0 {
entry:
  %a = alloca ptr, align 8
  %r0 = alloca i32, align 4
  %b = alloca %struct.Function_pointers_My_struct, align 8
  %r1 = alloca i32, align 4
  store ptr @Function_pointers_add, ptr %a, align 8
  %0 = load ptr, ptr %a, align 8
  %1 = call i32 %0(i32 noundef 1, i32 noundef 2)
  store i32 %1, ptr %r0, align 4
  %2 = getelementptr inbounds %struct.Function_pointers_My_struct, ptr %b, i32 0, i32 0
  store ptr @Function_pointers_add, ptr %2, align 8
  %3 = getelementptr inbounds %struct.Function_pointers_My_struct, ptr %b, i32 0, i32 1
  store ptr null, ptr %3, align 8
  %4 = getelementptr inbounds %struct.Function_pointers_My_struct, ptr %b, i32 0, i32 0
  %5 = load ptr, ptr %4, align 8
  %6 = call i32 %5(i32 noundef 3, i32 noundef 4)
  store i32 %6, ptr %r1, align 4
  ret void
}

; Function Attrs: convergent
define private i32 @Function_pointers_add(i32 noundef %"arguments[0].lhs", i32 noundef %"arguments[1].rhs") #0 {
entry:
  %lhs = alloca i32, align 4
  %rhs = alloca i32, align 4
  store i32 %"arguments[0].lhs", ptr %lhs, align 4
  store i32 %"arguments[1].rhs", ptr %rhs, align 4
  %0 = load i32, ptr %lhs, align 4
  %1 = load i32, ptr %rhs, align 4
  %2 = add i32 %0, %1
  ret i32 %2
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile hello world!", "[LLVM_IR]")
  {
    char const* const input_file = "hello_world.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
        { "c.stdio", import_c_header_and_get_file_path("c.stdio", "stdio.h") }
    };

    char const* const expected_llvm_ir = R"(
@global_0 = internal constant [13 x i8] c"Hello world!\00"

; Function Attrs: convergent
declare i32 @puts(ptr noundef) #0

; Function Attrs: convergent
define i32 @hello_world_main() #0 {
entry:
  %0 = call i32 @puts(ptr noundef @global_0)
  ret i32 0
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile If Expressions", "[LLVM_IR]")
  {
    char const* const input_file = "if_expressions.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
        { "c.stdio", import_c_header_and_get_file_path("c.stdio", "stdio.h") }
    };

    char const* const expected_llvm_ir = R"(
@global_0 = internal constant [4 x i8] c"%s\0A\00"
@global_1 = internal constant [5 x i8] c"zero\00"
@global_2 = internal constant [9 x i8] c"negative\00"
@global_3 = internal constant [13 x i8] c"non-negative\00"
@global_4 = internal constant [9 x i8] c"negative\00"
@global_5 = internal constant [9 x i8] c"positive\00"
@global_6 = internal constant [9 x i8] c"negative\00"
@global_7 = internal constant [9 x i8] c"positive\00"
@global_8 = internal constant [5 x i8] c"zero\00"
@global_9 = internal constant [5 x i8] c"true\00"
@global_10 = internal constant [6 x i8] c"false\00"

; Function Attrs: convergent
declare i32 @printf(ptr noundef, ...) #0

; Function Attrs: convergent
define void @If_expressions_run_ifs(i32 noundef %"arguments[0].value") #0 {
entry:
  %value = alloca i32, align 4
  %c_boolean = alloca i8, align 1
  store i32 %"arguments[0].value", ptr %value, align 4
  %0 = load i32, ptr %value, align 4
  %1 = icmp eq i32 %0, 0
  br i1 %1, label %if_s0_then, label %if_s1_after

if_s0_then:                                       ; preds = %entry
  call void @If_expressions_print_message(ptr noundef @global_1)
  br label %if_s1_after

if_s1_after:                                      ; preds = %if_s0_then, %entry
  %2 = load i32, ptr %value, align 4
  %3 = icmp slt i32 %2, 0
  br i1 %3, label %if_s0_then1, label %if_s1_else

if_s0_then1:                                      ; preds = %if_s1_after
  call void @If_expressions_print_message(ptr noundef @global_2)
  br label %if_s2_after

if_s1_else:                                       ; preds = %if_s1_after
  call void @If_expressions_print_message(ptr noundef @global_3)
  br label %if_s2_after

if_s2_after:                                      ; preds = %if_s1_else, %if_s0_then1
  %4 = load i32, ptr %value, align 4
  %5 = icmp slt i32 %4, 0
  br i1 %5, label %if_s0_then2, label %if_s1_else3

if_s0_then2:                                      ; preds = %if_s2_after
  call void @If_expressions_print_message(ptr noundef @global_4)
  br label %if_s3_after

if_s1_else3:                                      ; preds = %if_s2_after
  %6 = load i32, ptr %value, align 4
  %7 = icmp sgt i32 %6, 0
  br i1 %7, label %if_s2_then, label %if_s3_after

if_s2_then:                                       ; preds = %if_s1_else3
  call void @If_expressions_print_message(ptr noundef @global_5)
  br label %if_s3_after

if_s3_after:                                      ; preds = %if_s2_then, %if_s1_else3, %if_s0_then2
  %8 = load i32, ptr %value, align 4
  %9 = icmp slt i32 %8, 0
  br i1 %9, label %if_s0_then4, label %if_s1_else5

if_s0_then4:                                      ; preds = %if_s3_after
  call void @If_expressions_print_message(ptr noundef @global_6)
  br label %if_s4_after

if_s1_else5:                                      ; preds = %if_s3_after
  %10 = load i32, ptr %value, align 4
  %11 = icmp sgt i32 %10, 0
  br i1 %11, label %if_s2_then6, label %if_s3_else

if_s2_then6:                                      ; preds = %if_s1_else5
  call void @If_expressions_print_message(ptr noundef @global_7)
  br label %if_s4_after

if_s3_else:                                       ; preds = %if_s1_else5
  call void @If_expressions_print_message(ptr noundef @global_8)
  br label %if_s4_after

if_s4_after:                                      ; preds = %if_s3_else, %if_s2_then6, %if_s0_then4
  store i8 1, ptr %c_boolean, align 1
  %12 = load i8, ptr %c_boolean, align 1
  %13 = trunc i8 %12 to i1
  br i1 %13, label %if_s0_then7, label %if_s1_after8

if_s0_then7:                                      ; preds = %if_s4_after
  call void @If_expressions_print_message(ptr noundef @global_9)
  br label %if_s1_after8

if_s1_after8:                                     ; preds = %if_s0_then7, %if_s4_after
  %14 = load i8, ptr %c_boolean, align 1
  %15 = xor i8 %14, -1
  %16 = trunc i8 %15 to i1
  br i1 %16, label %if_s0_then9, label %if_s1_after10

if_s0_then9:                                      ; preds = %if_s1_after8
  call void @If_expressions_print_message(ptr noundef @global_10)
  br label %if_s1_after10

if_s1_after10:                                    ; preds = %if_s0_then9, %if_s1_after8
  ret void
}

; Function Attrs: convergent
define private void @If_expressions_print_message(ptr noundef %"arguments[0].message") #0 {
entry:
  %message = alloca ptr, align 8
  store ptr %"arguments[0].message", ptr %message, align 8
  %0 = load ptr, ptr %message, align 8
  %1 = call i32 (ptr, ...) @printf(ptr noundef @global_0, ptr noundef %0)
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile If Return Expressions", "[LLVM_IR]")
  {
    char const* const input_file = "if_return_expressions.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define private i32 @If_return_expressions_run(i32 noundef %"arguments[0].value") #0 {
entry:
  %value = alloca i32, align 4
  store i32 %"arguments[0].value", ptr %value, align 4
  %0 = load i32, ptr %value, align 4
  %1 = icmp eq i32 %0, 0
  br i1 %1, label %if_s0_then, label %if_s1_else

if_s0_then:                                       ; preds = %entry
  ret i32 1

if_s1_else:                                       ; preds = %entry
  ret i32 2
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Input Output Macros", "[LLVM_IR]")
  {
    char const* const input_file = "input_output_macros.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
      { "c.stdio", import_c_header_and_get_file_path("c.stdio", "stdio.h") }
    };

    char const* const expected_llvm_ir = R"(
@global_0 = internal constant [4 x i8] c"%s\0A\00"
@global_1 = internal constant [13 x i8] c"Hello world!\00"
@global_2 = internal constant [4 x i8] c"%s\0A\00"
@global_3 = internal constant [13 x i8] c"Hello error!\00"

; Function Attrs: convergent
declare i32 @fprintf(ptr noundef, ptr noundef, ...) #0

; Function Attrs: convergent
define private void @Input_output_macros_run() #0 {
entry:
  %0 = call ptr @__acrt_iob_func(i32 noundef 1)
  %1 = call i32 (ptr, ptr, ...) @fprintf(ptr noundef %0, ptr noundef @global_0, ptr noundef @global_1)
  %2 = call ptr @__acrt_iob_func(i32 noundef 2)
  %3 = call i32 (ptr, ptr, ...) @fprintf(ptr noundef %2, ptr noundef @global_2, ptr noundef @global_3)
  ret void
}

; Function Attrs: convergent
declare ptr @__acrt_iob_func(i32 noundef) #0

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Implicit Arguments", "[LLVM_IR]")
  {
    char const* const input_file = "implicit_arguments.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
%struct.Implicit_arguments_My_struct = type { i32, i32 }

; Function Attrs: convergent
define i32 @Implicit_arguments_get_v0(ptr noundef %"arguments[0].instance") #0 {
entry:
  %instance = alloca ptr, align 8
  store ptr %"arguments[0].instance", ptr %instance, align 8
  %0 = load ptr, ptr %instance, align 8
  %1 = getelementptr inbounds %struct.Implicit_arguments_My_struct, ptr %0, i32 0, i32 0
  %2 = load i32, ptr %1, align 4
  ret i32 %2
}

; Function Attrs: convergent
define private void @Implicit_arguments_run() #0 {
entry:
  %instance = alloca %struct.Implicit_arguments_My_struct, align 4
  %a = alloca i32, align 4
  %instance_pointer = alloca ptr, align 8
  %b = alloca i32, align 4
  %0 = getelementptr inbounds %struct.Implicit_arguments_My_struct, ptr %instance, i32 0, i32 0
  store i32 1, ptr %0, align 4
  %1 = getelementptr inbounds %struct.Implicit_arguments_My_struct, ptr %instance, i32 0, i32 1
  store i32 2, ptr %1, align 4
  %2 = call i32 @Implicit_arguments_get_v0(ptr noundef %instance)
  store i32 %2, ptr %a, align 4
  store ptr %instance, ptr %instance_pointer, align 8
  %3 = load ptr, ptr %instance_pointer, align 8
  %4 = call i32 @Implicit_arguments_get_v0(ptr noundef %3)
  store i32 %4, ptr %b, align 4
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Implicit Arguments External", "[LLVM_IR]")
  {
    char const* const input_file = "implicit_arguments_external.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
      { "Implicit_arguments", parse_and_get_file_path(g_test_source_files_path / "implicit_arguments.iris") },
    };

    char const* const expected_llvm_ir = R"(
%struct.Implicit_arguments_My_struct = type { i32, i32 }

; Function Attrs: convergent
declare i32 @Implicit_arguments_get_v0(ptr noundef) #0

; Function Attrs: convergent
define private void @Implicit_arguments_external_run() #0 {
entry:
  %instance = alloca %struct.Implicit_arguments_My_struct, align 4
  %a = alloca i32, align 4
  %instance_pointer = alloca ptr, align 8
  %b = alloca i32, align 4
  %0 = getelementptr inbounds %struct.Implicit_arguments_My_struct, ptr %instance, i32 0, i32 0
  store i32 1, ptr %0, align 4
  %1 = getelementptr inbounds %struct.Implicit_arguments_My_struct, ptr %instance, i32 0, i32 1
  store i32 2, ptr %1, align 4
  %2 = call i32 @Implicit_arguments_get_v0(ptr noundef %instance)
  store i32 %2, ptr %a, align 4
  store ptr %instance, ptr %instance_pointer, align 8
  %3 = load ptr, ptr %instance_pointer, align 8
  %4 = call i32 @Implicit_arguments_get_v0(ptr noundef %3)
  store i32 %4, ptr %b, align 4
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Instantiate Struct with Enum from Module", "[LLVM_IR]")
  {
    char const* const input_file = "instantiate_struct_with_enum_from_module.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
      { "Instantiate_struct_with_enum", parse_and_get_file_path(g_test_source_files_path / "instantiate_struct_with_enum.iris") },
    };

    char const* const expected_llvm_ir = R"(
%struct.Instantiate_struct_with_enum_My_struct = type { i32 }

; Function Attrs: convergent
define private void @Instantiate_struct_with_enum_from_module_run() #0 {
entry:
  %instance = alloca %struct.Instantiate_struct_with_enum_My_struct, align 4
  %0 = getelementptr inbounds %struct.Instantiate_struct_with_enum_My_struct, ptr %instance, i32 0, i32 0
  store i32 0, ptr %0, align 4
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

   TEST_CASE("Compile Instantiate Uninitialized", "[LLVM_IR]")
  {
    char const* const input_file = "instantiate_uninitialized.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
%struct.Instantiate_uninitialized_My_struct = type { i32, i32 }

; Function Attrs: convergent
define private void @Instantiate_uninitialized_run() #0 {
entry:
  %instance = alloca %struct.Instantiate_uninitialized_My_struct, align 4
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Instantiate Zero Initialized", "[LLVM_IR]")
  {
    char const* const input_file = "instantiate_zero_initialized.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
%struct.Instantiate_zero_initialized_My_struct = type { i32, i32 }

; Function Attrs: convergent
define private void @Instantiate_zero_initialized_run() #0 {
entry:
  %instance = alloca %struct.Instantiate_zero_initialized_My_struct, align 4
  call void @llvm.memset.p0.i64(ptr align 4 %instance, i8 0, i64 8, i1 false)
  ret void
}

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: write)
declare void @llvm.memset.p0.i64(ptr nocapture writeonly, i8, i64, i1 immarg) #1

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
attributes #1 = { nocallback nofree nounwind willreturn memory(argmem: write) }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }


  TEST_CASE("Compile Primitive Instantiate", "[LLVM_IR]")
  {
    char const* const input_file = "primitive_instantiate.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define private void @Primitive_instantiate_run() #0 {
entry:
  %a = alloca i32, align 4
  %b = alloca float, align 4
  %c = alloca i8, align 1
  %d = alloca ptr, align 8
  %e = alloca i32, align 4
  %f = alloca [4 x i32], align 4
  store i32 0, ptr %a, align 4
  store float 0.000000e+00, ptr %b, align 4
  store i8 0, ptr %c, align 1
  store ptr null, ptr %d, align 8
  store i32 0, ptr %e, align 4
  store [4 x i32] zeroinitializer, ptr %f, align 4
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Load Pointers", "[LLVM_IR]")
  {
    char const* const input_file = "load_pointers.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
%struct.Load_pointers_My_struct = type { ptr }

; Function Attrs: convergent
define private void @Load_pointers_run(ptr noundef %"arguments[0].instance") #0 {
entry:
  %instance = alloca ptr, align 8
  %p0 = alloca ptr, align 8
  %v0 = alloca %struct.Load_pointers_My_struct, align 8
  store ptr %"arguments[0].instance", ptr %instance, align 8
  %0 = load ptr, ptr %instance, align 8
  %1 = getelementptr inbounds %struct.Load_pointers_My_struct, ptr %0, i32 0, i32 0
  %2 = load ptr, ptr %1, align 8
  %3 = icmp ne ptr %2, null
  br i1 %3, label %if_s0_then, label %if_s1_after

if_s0_then:                                       ; preds = %entry
  br label %if_s1_after

if_s1_after:                                      ; preds = %if_s0_then, %entry
  %4 = load ptr, ptr %instance, align 8
  %array_element_pointer = getelementptr %struct.Load_pointers_My_struct, ptr %4, i32 0
  store ptr %array_element_pointer, ptr %p0, align 8
  %5 = load ptr, ptr %instance, align 8
  %array_element_pointer1 = getelementptr %struct.Load_pointers_My_struct, ptr %5, i32 0
  %6 = load %struct.Load_pointers_My_struct, ptr %array_element_pointer1, align 8
  store %struct.Load_pointers_My_struct %6, ptr %v0, align 8
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }


  TEST_CASE("Compile Merge Functions", "[LLVM_IR]")
  {
    char const* const input_file = "merge_functions.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define private void @Merge_functions_run(ptr noundef %"arguments[0].external_pointer") #0 {
entry:
  %external_pointer = alloca ptr, align 8
  %p0 = alloca ptr, align 8
  %p1 = alloca ptr, align 8
  store ptr %"arguments[0].external_pointer", ptr %external_pointer, align 8
  %0 = load ptr, ptr %external_pointer, align 8
  %1 = call ptr @Merge_functions__at__cast__at__10621281525101525598(ptr noundef %0)
  store ptr %1, ptr %p0, align 8
  %2 = load ptr, ptr %external_pointer, align 8
  %3 = call ptr @Merge_functions__at__cast__at__10621281525101525598(ptr noundef %2)
  store ptr %3, ptr %p1, align 8
  ret void
}

; Function Attrs: convergent
define private ptr @Merge_functions__at__cast__at__10621281525101525598(ptr noundef %"arguments[0].value") #0 {
entry:
  %value = alloca ptr, align 8
  store ptr %"arguments[0].value", ptr %value, align 8
  %0 = load ptr, ptr %value, align 8
  ret ptr %0
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }


  TEST_CASE("Compile Module with Dots", "[LLVM_IR]")
  {
    char const* const input_file = "module_with_dots.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define void @name_with_dots_function_name(i32 noundef %"arguments[0].a", i32 noundef %"arguments[1].b") #0 {
entry:
  %a = alloca i32, align 4
  %b = alloca i32, align 4
  store i32 %"arguments[0].a", ptr %a, align 4
  store i32 %"arguments[1].b", ptr %b, align 4
  call void @name_with_dots_other_function_name()
  ret void
}

; Function Attrs: convergent
define void @name_with_dots_other_function_name() #0 {
entry:
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }


  TEST_CASE("Compile Multiple Modules", "[LLVM_IR]")
  {
    char const* const input_file = "multiple_modules_a.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
      { "MB", parse_and_get_file_path(g_test_source_files_path / "multiple_modules_b.iris") },
      { "MC", parse_and_get_file_path(g_test_source_files_path / "multiple_modules_c.iris") },
    };

    char const* const expected_llvm_ir = R"(
%struct.MC_Struct_c = type { %struct.MC_Private_struct_c }
%struct.MC_Private_struct_c = type { i32 }
%struct.MA_Struct_a = type { %struct.MB_Struct_b }
%struct.MB_Struct_b = type { %struct.MC_Struct_c }
%struct.MA_Private_struct_a = type { %struct.MB_Private_struct_b }
%struct.MB_Private_struct_b = type { %struct.MC_Private_struct_c }

; Function Attrs: convergent
define void @MA_run(i32 noundef %"arguments[0].a", i32 noundef %"arguments[1].b", i32 noundef %"arguments[2].c", i32 noundef %"arguments[3].d") #0 {
entry:
  %0 = alloca %struct.MC_Struct_c, align 4
  %1 = alloca %struct.MC_Private_struct_c, align 4
  %2 = alloca %struct.MA_Struct_a, align 4
  %3 = alloca %struct.MA_Private_struct_a, align 4
  %4 = getelementptr inbounds %struct.MC_Struct_c, ptr %0, i32 0, i32 0
  store i32 %"arguments[0].a", ptr %4, align 4
  %5 = getelementptr inbounds %struct.MC_Private_struct_c, ptr %1, i32 0, i32 0
  store i32 %"arguments[1].b", ptr %5, align 4
  %6 = getelementptr inbounds %struct.MA_Struct_a, ptr %2, i32 0, i32 0
  store i32 %"arguments[2].c", ptr %6, align 4
  %7 = getelementptr inbounds %struct.MA_Private_struct_a, ptr %3, i32 0, i32 0
  store i32 %"arguments[3].d", ptr %7, align 4
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Null Pointers", "[LLVM_IR]")
  {
    char const* const input_file = "null_pointers.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define i32 @Null_pointers_pointers(ptr noundef %"arguments[0].parameter") #0 {
entry:
  %parameter = alloca ptr, align 8
  store ptr %"arguments[0].parameter", ptr %parameter, align 8
  %0 = load ptr, ptr %parameter, align 8
  %1 = icmp eq ptr %0, null
  br i1 %1, label %if_s0_then, label %if_s1_after

if_s0_then:                                       ; preds = %entry
  ret i32 -1

if_s1_after:                                      ; preds = %entry
  %2 = load ptr, ptr %parameter, align 8
  %3 = icmp ne ptr %2, null
  br i1 %3, label %if_s0_then1, label %if_s1_after2

if_s0_then1:                                      ; preds = %if_s1_after
  ret i32 1

if_s1_after2:                                     ; preds = %if_s1_after
  ret i32 0
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Numbers", "[LLVM_IR]")
  {
    char const* const input_file = "numbers.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define i32 @Numbers_main() #0 {
entry:
  %my_int8 = alloca i8, align 1
  %my_int16 = alloca i16, align 2
  %my_int32 = alloca i32, align 4
  %my_int64 = alloca i64, align 8
  %my_uint8 = alloca i8, align 1
  %my_uint16 = alloca i16, align 2
  %my_uint32 = alloca i32, align 4
  %my_uint64 = alloca i64, align 8
  %my_float16 = alloca half, align 2
  %my_float32 = alloca float, align 4
  %my_float64 = alloca double, align 8
  %my_c_char = alloca i8, align 1
  %my_c_short = alloca i16, align 2
  %my_c_int = alloca i32, align 4
  %my_c_long = alloca i32, align 4
  %my_c_long_long = alloca i64, align 8
  %my_c_unsigned_char = alloca i8, align 1
  %my_c_unsigned_short = alloca i16, align 2
  %my_c_unsigned_int = alloca i32, align 4
  %my_c_unsigned_long = alloca i32, align 4
  %my_c_unsigned_long_long = alloca i64, align 8
  store i8 1, ptr %my_int8, align 1
  store i16 1, ptr %my_int16, align 2
  store i32 1, ptr %my_int32, align 4
  store i64 1, ptr %my_int64, align 8
  store i8 1, ptr %my_uint8, align 1
  store i16 1, ptr %my_uint16, align 2
  store i32 1, ptr %my_uint32, align 4
  store i64 1, ptr %my_uint64, align 8
  store half 0xH3C00, ptr %my_float16, align 2
  store float 1.000000e+00, ptr %my_float32, align 4
  store double 1.000000e+00, ptr %my_float64, align 8
  store i8 97, ptr %my_c_char, align 1
  store i16 1, ptr %my_c_short, align 2
  store i32 1, ptr %my_c_int, align 4
  store i32 1, ptr %my_c_long, align 4
  store i64 1, ptr %my_c_long_long, align 8
  store i8 1, ptr %my_c_unsigned_char, align 1
  store i16 1, ptr %my_c_unsigned_short, align 2
  store i32 1, ptr %my_c_unsigned_int, align 4
  store i32 1, ptr %my_c_unsigned_long, align 4
  store i64 1, ptr %my_c_unsigned_long_long, align 8
  ret i32 0
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Numeric_casts", "[LLVM_IR]")
  {
    char const* const input_file = "numeric_casts.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define i32 @Numeric_casts_do_casts(i32 noundef %"arguments[0].uint32_argument", i64 noundef %"arguments[1].uint64_argument", i32 noundef %"arguments[2].int32_argument", i64 noundef %"arguments[3].int64_argument", half noundef %"arguments[4].float16_argument", float noundef %"arguments[5].float32_argument", double noundef %"arguments[6].float64_argument") #0 {
entry:
  %uint32_argument = alloca i32, align 4
  %uint64_argument = alloca i64, align 8
  %int32_argument = alloca i32, align 4
  %int64_argument = alloca i64, align 8
  %float16_argument = alloca half, align 2
  %float32_argument = alloca float, align 4
  %float64_argument = alloca double, align 8
  %u64_to_u32 = alloca i32, align 4
  %u64_to_i32 = alloca i32, align 4
  %i64_to_u32 = alloca i32, align 4
  %i64_to_i32 = alloca i32, align 4
  %u32_to_u64 = alloca i64, align 8
  %u32_to_i64 = alloca i64, align 8
  %i32_to_u64 = alloca i64, align 8
  %i32_to_i64 = alloca i64, align 8
  %u32_to_f32 = alloca float, align 4
  %i32_to_f32 = alloca float, align 4
  %f32_to_u32 = alloca i32, align 4
  %f32_to_i32 = alloca i32, align 4
  %f16_to_f32 = alloca float, align 4
  %f32_to_f64 = alloca double, align 8
  %f64_to_f32 = alloca float, align 4
  %f32_to_f16 = alloca half, align 2
  store i32 %"arguments[0].uint32_argument", ptr %uint32_argument, align 4
  store i64 %"arguments[1].uint64_argument", ptr %uint64_argument, align 8
  store i32 %"arguments[2].int32_argument", ptr %int32_argument, align 4
  store i64 %"arguments[3].int64_argument", ptr %int64_argument, align 8
  store half %"arguments[4].float16_argument", ptr %float16_argument, align 2
  store float %"arguments[5].float32_argument", ptr %float32_argument, align 4
  store double %"arguments[6].float64_argument", ptr %float64_argument, align 8
  %0 = load i64, ptr %uint64_argument, align 8
  %1 = trunc i64 %0 to i32
  store i32 %1, ptr %u64_to_u32, align 4
  %2 = load i64, ptr %uint64_argument, align 8
  %3 = trunc i64 %2 to i32
  store i32 %3, ptr %u64_to_i32, align 4
  %4 = load i64, ptr %int64_argument, align 8
  %5 = trunc i64 %4 to i32
  store i32 %5, ptr %i64_to_u32, align 4
  %6 = load i64, ptr %int64_argument, align 8
  %7 = trunc i64 %6 to i32
  store i32 %7, ptr %i64_to_i32, align 4
  %8 = load i32, ptr %uint32_argument, align 4
  %9 = zext i32 %8 to i64
  store i64 %9, ptr %u32_to_u64, align 8
  %10 = load i32, ptr %uint32_argument, align 4
  %11 = zext i32 %10 to i64
  store i64 %11, ptr %u32_to_i64, align 8
  %12 = load i32, ptr %int32_argument, align 4
  %13 = zext i32 %12 to i64
  store i64 %13, ptr %i32_to_u64, align 8
  %14 = load i32, ptr %int32_argument, align 4
  %15 = sext i32 %14 to i64
  store i64 %15, ptr %i32_to_i64, align 8
  %16 = load i32, ptr %uint32_argument, align 4
  %17 = uitofp i32 %16 to float
  store float %17, ptr %u32_to_f32, align 4
  %18 = load i32, ptr %int32_argument, align 4
  %19 = sitofp i32 %18 to float
  store float %19, ptr %i32_to_f32, align 4
  %20 = load float, ptr %float32_argument, align 4
  %21 = fptoui float %20 to i32
  store i32 %21, ptr %f32_to_u32, align 4
  %22 = load float, ptr %float32_argument, align 4
  %23 = fptosi float %22 to i32
  store i32 %23, ptr %f32_to_i32, align 4
  %24 = load half, ptr %float16_argument, align 2
  %25 = fpext half %24 to float
  store float %25, ptr %f16_to_f32, align 4
  %26 = load float, ptr %float32_argument, align 4
  %27 = fpext float %26 to double
  store double %27, ptr %f32_to_f64, align 8
  %28 = load double, ptr %float64_argument, align 8
  %29 = fptrunc double %28 to float
  store float %29, ptr %f64_to_f32, align 4
  %30 = load float, ptr %float32_argument, align 4
  %31 = fptrunc float %30 to half
  store half %31, ptr %f32_to_f16, align 2
  ret i32 0
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Offset Pointer", "[LLVM_IR]")
  {
    char const* const input_file = "offset_pointer.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define ptr @Offset_pointer_pointers(ptr noundef %"arguments[0].external_pointer") #0 {
entry:
  %external_pointer = alloca ptr, align 8
  %p0 = alloca ptr, align 8
  %p1 = alloca ptr, align 8
  %p2 = alloca ptr, align 8
  store ptr %"arguments[0].external_pointer", ptr %external_pointer, align 8
  %0 = load ptr, ptr %external_pointer, align 8
  %1 = getelementptr i8, ptr %0, i64 8
  store ptr %1, ptr %p0, align 8
  %2 = load ptr, ptr %p0, align 8
  store i32 0, ptr %2, align 4
  %3 = load ptr, ptr %external_pointer, align 8
  %4 = getelementptr i8, ptr %3, i64 16
  call void @Offset_pointer_take(ptr noundef %4)
  store ptr null, ptr %p1, align 8
  %5 = load ptr, ptr %p1, align 8
  %6 = getelementptr i8, ptr %5, i64 10
  store ptr %6, ptr %p2, align 8
  %7 = load ptr, ptr %external_pointer, align 8
  %8 = getelementptr i8, ptr %7, i64 24
  ret ptr %8
}

; Function Attrs: convergent
define private void @Offset_pointer_take(ptr noundef %"arguments[0].external_pointer") #0 {
entry:
  %external_pointer = alloca ptr, align 8
  store ptr %"arguments[0].external_pointer", ptr %external_pointer, align 8
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Passing Pointers to Functions", "[LLVM_IR]")
  {
    char const* const input_file = "passing_pointers_to_functions.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
%struct.Passing_pointers_to_functions_My_struct = type { i32, ptr }

; Function Attrs: convergent
define void @Passing_pointers_to_functions_run() #0 {
entry:
  %v0_null = alloca ptr, align 8
  %v1 = alloca i32, align 4
  %v2 = alloca ptr, align 8
  %instance = alloca %struct.Passing_pointers_to_functions_My_struct, align 8
  call void @Passing_pointers_to_functions_take(ptr noundef null)
  store ptr null, ptr %v0_null, align 8
  %0 = load ptr, ptr %v0_null, align 8
  call void @Passing_pointers_to_functions_take(ptr noundef %0)
  store i32 1, ptr %v1, align 4
  call void @Passing_pointers_to_functions_take(ptr noundef %v1)
  store ptr null, ptr %v2, align 8
  call void @Passing_pointers_to_functions_take(ptr noundef %v2)
  %1 = getelementptr inbounds %struct.Passing_pointers_to_functions_My_struct, ptr %instance, i32 0, i32 0
  store i32 0, ptr %1, align 4
  %2 = getelementptr inbounds %struct.Passing_pointers_to_functions_My_struct, ptr %instance, i32 0, i32 1
  store ptr null, ptr %2, align 8
  %3 = getelementptr inbounds %struct.Passing_pointers_to_functions_My_struct, ptr %instance, i32 0, i32 0
  call void @Passing_pointers_to_functions_take(ptr noundef %3)
  %4 = getelementptr inbounds %struct.Passing_pointers_to_functions_My_struct, ptr %instance, i32 0, i32 1
  %5 = load ptr, ptr %4, align 8
  call void @Passing_pointers_to_functions_take(ptr noundef %5)
  %6 = getelementptr inbounds %struct.Passing_pointers_to_functions_My_struct, ptr %instance, i32 0, i32 1
  call void @Passing_pointers_to_functions_take(ptr noundef %6)
  ret void
}

; Function Attrs: convergent
define private void @Passing_pointers_to_functions_take(ptr noundef %"arguments[0].v0") #0 {
entry:
  %v0 = alloca ptr, align 8
  store ptr %"arguments[0].v0", ptr %v0, align 8
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Pointers", "[LLVM_IR]")
  {
    char const* const input_file = "pointers.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define void @Pointers_pointers(ptr noundef %"arguments[0].external_pointer") #0 {
entry:
  %external_pointer = alloca ptr, align 8
  %a = alloca i32, align 4
  %pointer_a = alloca ptr, align 8
  %dereferenced_a = alloca i32, align 4
  %p0 = alloca ptr, align 8
  store ptr %"arguments[0].external_pointer", ptr %external_pointer, align 8
  store i32 1, ptr %a, align 4
  store ptr %a, ptr %pointer_a, align 8
  %0 = load ptr, ptr %pointer_a, align 8
  %1 = load i32, ptr %0, align 4
  store i32 %1, ptr %dereferenced_a, align 4
  %2 = load ptr, ptr %external_pointer, align 8
  %3 = getelementptr i8, ptr %2, i64 8
  store ptr %3, ptr %p0, align 8
  %4 = load ptr, ptr %p0, align 8
  store i32 0, ptr %4, align 4
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Reinterpret_as", "[LLVM_IR]")
  {
    char const* const input_file = "reinterpret_as.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define private void @Reinterpret_as_run(ptr noundef %"arguments[0].data") #0 {
entry:
  %data = alloca ptr, align 8
  %converted = alloca ptr, align 8
  store ptr %"arguments[0].data", ptr %data, align 8
  %0 = load ptr, ptr %data, align 8
  store ptr %0, ptr %converted, align 8
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Size_of", "[LLVM_IR]")
  {
    char const* const input_file = "size_of.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define private void @Size_of_run() #0 {
entry:
  %size = alloca i64, align 8
  %alignment = alloca i64, align 8
  store i64 8, ptr %size, align 8
  store i64 4, ptr %alignment, align 8
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Stack Array Entry", "[LLVM_IR]")
  {
    char const* const input_file = "stack_array_entry.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
%struct.iris_builtin_Generic_array_slice = type { ptr, i64 }

; Function Attrs: convergent
define void @Stack_array_entry_foo(i64 noundef %"arguments[0].length") #0 {
entry:
  %length = alloca i64, align 8
  %0 = alloca %struct.iris_builtin_Generic_array_slice, align 8
  %array_0 = alloca %struct.iris_builtin_Generic_array_slice, align 8
  %1 = alloca %struct.iris_builtin_Generic_array_slice, align 8
  %array_1 = alloca %struct.iris_builtin_Generic_array_slice, align 8
  store i64 %"arguments[0].length", ptr %length, align 8
  %2 = load i64, ptr %length, align 8
  %stack_save_pointer = call ptr @llvm.stacksave.p0()
  %stack_array = alloca i32, i64 %2, align 16
  %3 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %0, i32 0, i32 0
  store ptr %stack_array, ptr %3, align 8
  %4 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %0, i32 0, i32 1
  store i64 %2, ptr %4, align 8
  %5 = load %struct.iris_builtin_Generic_array_slice, ptr %0, align 8
  store %struct.iris_builtin_Generic_array_slice %5, ptr %array_0, align 8
  %6 = load i64, ptr %length, align 8
  %stack_array1 = alloca i32, i64 %6, align 16
  %7 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %1, i32 0, i32 0
  store ptr %stack_array1, ptr %7, align 8
  %8 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %1, i32 0, i32 1
  store i64 %6, ptr %8, align 8
  %9 = load %struct.iris_builtin_Generic_array_slice, ptr %1, align 8
  store %struct.iris_builtin_Generic_array_slice %9, ptr %array_1, align 8
  call void @llvm.stackrestore.p0(ptr %stack_save_pointer)
  ret void
}

; Function Attrs: nocallback nofree nosync nounwind willreturn
declare ptr @llvm.stacksave.p0() #1

; Function Attrs: nocallback nofree nosync nounwind willreturn
declare void @llvm.stackrestore.p0(ptr) #1

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
attributes #1 = { nocallback nofree nosync nounwind willreturn }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Stack Array Loop", "[LLVM_IR]")
  {
    char const* const input_file = "stack_array_loop.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
%struct.iris_builtin_Generic_array_slice = type { ptr, i64 }

; Function Attrs: convergent
define void @Stack_array_loop_foo(i64 noundef %"arguments[0].length") #0 {
entry:
  %length = alloca i64, align 8
  %0 = alloca %struct.iris_builtin_Generic_array_slice, align 8
  %array_0 = alloca %struct.iris_builtin_Generic_array_slice, align 8
  %index = alloca i64, align 8
  %1 = alloca %struct.iris_builtin_Generic_array_slice, align 8
  %array_1 = alloca %struct.iris_builtin_Generic_array_slice, align 8
  store i64 %"arguments[0].length", ptr %length, align 8
  %2 = load i64, ptr %length, align 8
  %stack_save_pointer = call ptr @llvm.stacksave.p0()
  %stack_array = alloca i32, i64 %2, align 16
  %3 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %0, i32 0, i32 0
  store ptr %stack_array, ptr %3, align 8
  %4 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %0, i32 0, i32 1
  store i64 %2, ptr %4, align 8
  %5 = load %struct.iris_builtin_Generic_array_slice, ptr %0, align 8
  store %struct.iris_builtin_Generic_array_slice %5, ptr %array_0, align 8
  store i64 1, ptr %index, align 8
  br label %for_loop_condition

for_loop_condition:                               ; preds = %for_loop_update_index, %entry
  %6 = load i64, ptr %length, align 8
  %7 = load i64, ptr %index, align 8
  %8 = icmp ult i64 %7, %6
  br i1 %8, label %for_loop_then, label %for_loop_after

for_loop_then:                                    ; preds = %for_loop_condition
  %9 = load i64, ptr %index, align 8
  %stack_save_pointer1 = call ptr @llvm.stacksave.p0()
  %stack_array2 = alloca i32, i64 %9, align 16
  %10 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %1, i32 0, i32 0
  store ptr %stack_array2, ptr %10, align 8
  %11 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %1, i32 0, i32 1
  store i64 %9, ptr %11, align 8
  %12 = load %struct.iris_builtin_Generic_array_slice, ptr %1, align 8
  store %struct.iris_builtin_Generic_array_slice %12, ptr %array_1, align 8
  %13 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %array_1, i32 0, i32 0
  %14 = load ptr, ptr %13, align 8
  %array_slice_element_pointer = getelementptr i32, ptr %14, i32 0
  %15 = load i64, ptr %index, align 8
  %16 = trunc i64 %15 to i32
  store i32 %16, ptr %array_slice_element_pointer, align 4
  call void @llvm.stackrestore.p0(ptr %stack_save_pointer1)
  br label %for_loop_update_index

for_loop_update_index:                            ; preds = %for_loop_then
  %17 = load i64, ptr %index, align 8
  %18 = add i64 %17, 1
  store i64 %18, ptr %index, align 8
  br label %for_loop_condition

for_loop_after:                                   ; preds = %for_loop_condition
  call void @llvm.stackrestore.p0(ptr %stack_save_pointer)
  ret void
}

; Function Attrs: nocallback nofree nosync nounwind willreturn
declare ptr @llvm.stacksave.p0() #1

; Function Attrs: nocallback nofree nosync nounwind willreturn
declare void @llvm.stackrestore.p0(ptr) #1

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
attributes #1 = { nocallback nofree nosync nounwind willreturn }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Switch Expressions", "[LLVM_IR]")
  {
    char const* const input_file = "switch_expressions.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define i32 @Switch_expressions_run_switch(i32 noundef %"arguments[0].value") #0 {
entry:
  %value = alloca i32, align 4
  %return_value = alloca i32, align 4
  store i32 %"arguments[0].value", ptr %value, align 4
  %0 = load i32, ptr %value, align 4
  switch i32 %0, label %switch_after [
    i32 0, label %switch_case_i0_
  ]

switch_after:                                     ; preds = %entry
  %1 = load i32, ptr %value, align 4
  switch i32 %1, label %switch_case_default [
    i32 1, label %switch_case_i0_2
    i32 2, label %switch_case_i1_
    i32 3, label %switch_case_i2_
    i32 4, label %switch_case_i3_
    i32 5, label %switch_case_i4_
  ]

switch_case_i0_:                                  ; preds = %entry
  store i32 0, ptr %return_value, align 4
  %2 = load i32, ptr %return_value, align 4
  ret i32 %2

switch_after1:                                    ; preds = %switch_case_i3_
  %3 = load i32, ptr %value, align 4
  switch i32 %3, label %switch_case_default4 [
    i32 6, label %switch_case_i1_5
  ]

switch_case_i0_2:                                 ; preds = %switch_after
  ret i32 1

switch_case_i1_:                                  ; preds = %switch_after
  br label %switch_case_i2_

switch_case_i2_:                                  ; preds = %switch_case_i1_, %switch_after
  ret i32 2

switch_case_i3_:                                  ; preds = %switch_after
  br label %switch_after1

switch_case_i4_:                                  ; preds = %switch_after
  br label %switch_case_default

switch_case_default:                              ; preds = %switch_case_i4_, %switch_after
  ret i32 3

switch_after3:                                    ; No predecessors!
  ret i32 5

switch_case_default4:                             ; preds = %switch_after1
  br label %switch_case_i1_5

switch_case_i1_5:                                 ; preds = %switch_case_default4, %switch_after1
  ret i32 4
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Ternary Condition Expressions", "[LLVM_IR]")
  {
    char const* const input_file = "ternary_condition_expressions.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define void @Ternary_condition_expressions_run_ternary_conditions(i1 noundef zeroext %"arguments[0].first_boolean", i1 noundef zeroext %"arguments[1].second_boolean") #0 {
entry:
  %first_boolean = alloca i8, align 1
  %second_boolean = alloca i8, align 1
  %a = alloca i32, align 4
  %b = alloca i32, align 4
  %c = alloca i32, align 4
  %d = alloca i32, align 4
  %e = alloca i32, align 4
  %first = alloca i32, align 4
  %second = alloca i32, align 4
  %f = alloca i32, align 4
  %c_boolean = alloca i8, align 1
  %g = alloca i32, align 4
  %0 = zext i1 %"arguments[0].first_boolean" to i8
  store i8 %0, ptr %first_boolean, align 1
  %1 = zext i1 %"arguments[1].second_boolean" to i8
  store i8 %1, ptr %second_boolean, align 1
  %2 = load i8, ptr %first_boolean, align 1
  %3 = trunc i8 %2 to i1
  br i1 %3, label %ternary_condition_then, label %ternary_condition_else

ternary_condition_then:                           ; preds = %entry
  br label %ternary_condition_end

ternary_condition_else:                           ; preds = %entry
  br label %ternary_condition_end

ternary_condition_end:                            ; preds = %ternary_condition_else, %ternary_condition_then
  %4 = phi i32 [ 1, %ternary_condition_then ], [ 0, %ternary_condition_else ]
  store i32 %4, ptr %a, align 4
  %5 = load i8, ptr %first_boolean, align 1
  %6 = icmp eq i8 %5, 0
  br i1 %6, label %ternary_condition_then1, label %ternary_condition_else2

ternary_condition_then1:                          ; preds = %ternary_condition_end
  br label %ternary_condition_end3

ternary_condition_else2:                          ; preds = %ternary_condition_end
  br label %ternary_condition_end3

ternary_condition_end3:                           ; preds = %ternary_condition_else2, %ternary_condition_then1
  %7 = phi i32 [ 1, %ternary_condition_then1 ], [ 0, %ternary_condition_else2 ]
  store i32 %7, ptr %b, align 4
  %8 = load i8, ptr %first_boolean, align 1
  %9 = xor i8 %8, -1
  %10 = trunc i8 %9 to i1
  br i1 %10, label %ternary_condition_then4, label %ternary_condition_else5

ternary_condition_then4:                          ; preds = %ternary_condition_end3
  br label %ternary_condition_end6

ternary_condition_else5:                          ; preds = %ternary_condition_end3
  br label %ternary_condition_end6

ternary_condition_end6:                           ; preds = %ternary_condition_else5, %ternary_condition_then4
  %11 = phi i32 [ 1, %ternary_condition_then4 ], [ 0, %ternary_condition_else5 ]
  store i32 %11, ptr %c, align 4
  %12 = load i8, ptr %first_boolean, align 1
  %13 = trunc i8 %12 to i1
  br i1 %13, label %ternary_condition_then7, label %ternary_condition_else8

ternary_condition_then7:                          ; preds = %ternary_condition_end6
  %14 = load i8, ptr %second_boolean, align 1
  %15 = trunc i8 %14 to i1
  br i1 %15, label %ternary_condition_then10, label %ternary_condition_else11

ternary_condition_else8:                          ; preds = %ternary_condition_end6
  br label %ternary_condition_end9

ternary_condition_end9:                           ; preds = %ternary_condition_else8, %ternary_condition_end12
  %16 = phi i32 [ %19, %ternary_condition_end12 ], [ 0, %ternary_condition_else8 ]
  store i32 %16, ptr %d, align 4
  %17 = load i8, ptr %first_boolean, align 1
  %18 = trunc i8 %17 to i1
  br i1 %18, label %ternary_condition_then13, label %ternary_condition_else14

ternary_condition_then10:                         ; preds = %ternary_condition_then7
  br label %ternary_condition_end12

ternary_condition_else11:                         ; preds = %ternary_condition_then7
  br label %ternary_condition_end12

ternary_condition_end12:                          ; preds = %ternary_condition_else11, %ternary_condition_then10
  %19 = phi i32 [ 2, %ternary_condition_then10 ], [ 1, %ternary_condition_else11 ]
  br label %ternary_condition_end9

ternary_condition_then13:                         ; preds = %ternary_condition_end9
  br label %ternary_condition_end15

ternary_condition_else14:                         ; preds = %ternary_condition_end9
  %20 = load i8, ptr %second_boolean, align 1
  %21 = trunc i8 %20 to i1
  br i1 %21, label %ternary_condition_then16, label %ternary_condition_else17

ternary_condition_end15:                          ; preds = %ternary_condition_end18, %ternary_condition_then13
  %22 = phi i32 [ 2, %ternary_condition_then13 ], [ %25, %ternary_condition_end18 ]
  store i32 %22, ptr %e, align 4
  store i32 0, ptr %first, align 4
  store i32 1, ptr %second, align 4
  %23 = load i8, ptr %first_boolean, align 1
  %24 = trunc i8 %23 to i1
  br i1 %24, label %ternary_condition_then19, label %ternary_condition_else20

ternary_condition_then16:                         ; preds = %ternary_condition_else14
  br label %ternary_condition_end18

ternary_condition_else17:                         ; preds = %ternary_condition_else14
  br label %ternary_condition_end18

ternary_condition_end18:                          ; preds = %ternary_condition_else17, %ternary_condition_then16
  %25 = phi i32 [ 1, %ternary_condition_then16 ], [ 0, %ternary_condition_else17 ]
  br label %ternary_condition_end15

ternary_condition_then19:                         ; preds = %ternary_condition_end15
  %26 = load i32, ptr %first, align 4
  br label %ternary_condition_end21

ternary_condition_else20:                         ; preds = %ternary_condition_end15
  %27 = load i32, ptr %second, align 4
  br label %ternary_condition_end21

ternary_condition_end21:                          ; preds = %ternary_condition_else20, %ternary_condition_then19
  %28 = phi i32 [ %26, %ternary_condition_then19 ], [ %27, %ternary_condition_else20 ]
  store i32 %28, ptr %f, align 4
  store i8 1, ptr %c_boolean, align 1
  %29 = load i8, ptr %c_boolean, align 1
  %30 = trunc i8 %29 to i1
  br i1 %30, label %ternary_condition_then22, label %ternary_condition_else23

ternary_condition_then22:                         ; preds = %ternary_condition_end21
  br label %ternary_condition_end24

ternary_condition_else23:                         ; preds = %ternary_condition_end21
  br label %ternary_condition_end24

ternary_condition_end24:                          ; preds = %ternary_condition_else23, %ternary_condition_then22
  %31 = phi i32 [ 1, %ternary_condition_then22 ], [ 0, %ternary_condition_else23 ]
  store i32 %31, ptr %g, align 4
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Test Framework Non Test Mode", "[LLVM_IR]")
  {
    char const* const input_file = "test_framework.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define private i32 @Test_framework_add(i32 noundef %"arguments[0].a", i32 noundef %"arguments[1].b") #0 {
entry:
  %a = alloca i32, align 4
  %b = alloca i32, align 4
  store i32 %"arguments[0].a", ptr %a, align 4
  store i32 %"arguments[1].b", ptr %b, align 4
  %0 = load i32, ptr %a, align 4
  %1 = load i32, ptr %b, align 4
  %2 = add i32 %0, %1
  ret i32 %2
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Test Framework Test Mode", "[LLVM_IR]")
  {
    char const* const input_file = "test_framework.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    std::string const test_source_file_path = (g_test_source_files_path / input_file).generic_string();

    std::string const expected_llvm_ir = std::format(R"(
%struct.iris_builtin_Generic_array_slice = type {{ ptr, i64 }}
%struct.iris_json_Write_stream = type {{ ptr }}

@iris_test_source_file_path = internal constant [68 x i8] c"C:/Users/JPMMa/Desktop/source/iris/Examples/txt/test_framework.iris\00"
@global_1 = internal constant [4 x i8] c"hhi\00"
@global_2 = internal constant [3 x i8] c"hi\00"
@global_3 = internal constant [2 x i8] c"i\00"
@global_4 = internal constant [4 x i8] c"lli\00"
@global_5 = internal constant [4 x i8] c"hhu\00"
@global_6 = internal constant [3 x i8] c"hu\00"
@global_7 = internal constant [2 x i8] c"u\00"
@global_8 = internal constant [4 x i8] c"llu\00"
@global_9 = internal constant [2 x i8] c"f\00"
@global_10 = internal constant [4 x i8] c"lld\00"
@global_11 = internal constant [5 x i8] c"null\00"
@global_12 = internal constant [3 x i8] c"%s\00"
@global_13 = internal constant [61 x i8] c"Expected vs Actual (Right-hand side vs Left-hand side)\0A    '\00"
@global_14 = internal constant [7 x i8] c"' vs '\00"
@global_15 = internal constant [3 x i8] c"'\0A\00"

; Function Attrs: convergent
declare i32 @fflush(ptr noundef) #0

; Function Attrs: convergent
declare i32 @fprintf(ptr noundef, ptr noundef, ...) #0

; Function Attrs: convergent
declare i32 @snprintf(ptr noundef, i64 noundef, ptr noundef, ...) #0

; Function Attrs: convergent
declare i64 @strlen(ptr noundef) #0

; Function Attrs: convergent
define private i32 @Test_framework_add(i32 noundef %"arguments[0].a", i32 noundef %"arguments[1].b") #0 {{
entry:
  %a = alloca i32, align 4
  %b = alloca i32, align 4
  store i32 %"arguments[0].a", ptr %a, align 4
  store i32 %"arguments[1].b", ptr %b, align 4
  %0 = load i32, ptr %a, align 4
  %1 = load i32, ptr %b, align 4
  %2 = add i32 %0, %1
  ret i32 %2
}}

; Function Attrs: convergent
define void @Test_framework_test_addition() #0 {{
entry:
  %__lhs = alloca i32, align 4
  %__rhs = alloca i32, align 4
  %__condition = alloca i1, align 1
  %__lhs1 = alloca i32, align 4
  %__rhs2 = alloca i32, align 4
  %__condition3 = alloca i1, align 1
  %0 = call i32 @Test_framework_add(i32 noundef 1, i32 noundef 2)
  store i32 %0, ptr %__lhs, align 4
  store i32 3, ptr %__rhs, align 4
  %1 = load i32, ptr %__lhs, align 4
  %2 = load i32, ptr %__rhs, align 4
  %3 = icmp eq i32 %1, %2
  store i1 %3, ptr %__condition, align 1
  %4 = load i8, ptr %__condition, align 1
  %5 = trunc i8 %4 to i1
  call void @iris_test_check(i1 noundef zeroext %5, ptr noundef @iris_test_source_file_path, i64 noundef 11)
  %6 = load i8, ptr %__condition, align 1
  %7 = xor i8 %6, -1
  %8 = trunc i8 %7 to i1
  br i1 %8, label %if_s0_then, label %if_s1_after

if_s0_then:                                       ; preds = %entry
  call void @iris.json__at__print_json_difference__at__16983553210230134252(ptr noundef %__lhs, ptr noundef %__rhs)
  br label %if_s1_after

if_s1_after:                                      ; preds = %if_s0_then, %entry
  %9 = call i32 @Test_framework_add(i32 noundef 2, i32 noundef 3)
  store i32 %9, ptr %__lhs1, align 4
  store i32 5, ptr %__rhs2, align 4
  %10 = load i32, ptr %__lhs1, align 4
  %11 = load i32, ptr %__rhs2, align 4
  %12 = icmp eq i32 %10, %11
  store i1 %12, ptr %__condition3, align 1
  %13 = load i8, ptr %__condition3, align 1
  %14 = trunc i8 %13 to i1
  call void @iris_test_check(i1 noundef zeroext %14, ptr noundef @iris_test_source_file_path, i64 noundef 12)
  %15 = load i8, ptr %__condition3, align 1
  %16 = xor i8 %15, -1
  %17 = trunc i8 %16 to i1
  br i1 %17, label %if_s0_then4, label %if_s1_after5

if_s0_then4:                                      ; preds = %if_s1_after
  call void @iris.json__at__print_json_difference__at__16983553210230134252(ptr noundef %__lhs1, ptr noundef %__rhs2)
  br label %if_s1_after5

if_s1_after5:                                     ; preds = %if_s0_then4, %if_s1_after
  ret void
}}

; Function Attrs: convergent
define private ptr @iris.json.create_format_specifier(ptr %"arguments[0].buffer_0", i64 %"arguments[0].buffer_1", i32 noundef %"arguments[1].kind", i64 noundef %"arguments[2].size_in_bytes") #0 {{
entry:
  %buffer = alloca %struct.iris_builtin_Generic_array_slice, align 8
  %kind = alloca i32, align 4
  %size_in_bytes = alloca i64, align 8
  %specifier = alloca ptr, align 8
  %0 = getelementptr inbounds {{ ptr, i64 }}, ptr %buffer, i32 0, i32 0
  store ptr %"arguments[0].buffer_0", ptr %0, align 8
  %1 = getelementptr inbounds {{ ptr, i64 }}, ptr %buffer, i32 0, i32 1
  store i64 %"arguments[0].buffer_1", ptr %1, align 8
  store i32 %"arguments[1].kind", ptr %kind, align 4
  store i64 %"arguments[2].size_in_bytes", ptr %size_in_bytes, align 8
  %2 = load i32, ptr %kind, align 4
  %3 = load i64, ptr %size_in_bytes, align 8
  %4 = call ptr @iris.json.get_format_specifier(i32 noundef %2, i64 noundef %3)
  store ptr %4, ptr %specifier, align 8
  %5 = getelementptr inbounds {{ ptr, i64 }}, ptr %buffer, i32 0, i32 0
  %6 = load ptr, ptr %5, align 8
  %7 = getelementptr inbounds {{ ptr, i64 }}, ptr %buffer, i32 0, i32 1
  %8 = load i64, ptr %7, align 8
  %9 = load ptr, ptr %specifier, align 8
  call void @iris.json.write_format_specifier(ptr %6, i64 %8, ptr noundef %9)
  %10 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %buffer, i32 0, i32 0
  %11 = load ptr, ptr %10, align 8
  ret ptr %11
}}

; Function Attrs: convergent
define private ptr @iris.json.get_format_specifier(i32 noundef %"arguments[0].kind", i64 noundef %"arguments[1].size_in_bytes") #0 {{
entry:
  %kind = alloca i32, align 4
  %size_in_bytes = alloca i64, align 8
  store i32 %"arguments[0].kind", ptr %kind, align 4
  store i64 %"arguments[1].size_in_bytes", ptr %size_in_bytes, align 8
  %0 = load i32, ptr %kind, align 4
  %1 = icmp eq i32 %0, 0
  br i1 %1, label %if_s0_then, label %if_s1_else

if_s0_then:                                       ; preds = %entry
  %2 = load i64, ptr %size_in_bytes, align 8
  switch i64 %2, label %switch_case_default [
    i64 1, label %switch_case_i0_
    i64 2, label %switch_case_i1_
    i64 4, label %switch_case_i2_
  ]

if_s1_else:                                       ; preds = %entry
  %3 = load i32, ptr %kind, align 4
  %4 = icmp eq i32 %3, 1
  br i1 %4, label %if_s2_then, label %if_s3_else

if_s2_then:                                       ; preds = %if_s1_else
  %5 = load i64, ptr %size_in_bytes, align 8
  switch i64 %5, label %switch_case_default5 [
    i64 1, label %switch_case_i0_2
    i64 2, label %switch_case_i1_3
    i64 4, label %switch_case_i2_4
  ]

if_s3_else:                                       ; preds = %if_s1_else
  %6 = load i32, ptr %kind, align 4
  %7 = icmp eq i32 %6, 2
  br i1 %7, label %if_s4_then, label %if_s5_after

if_s4_then:                                       ; preds = %if_s3_else
  ret ptr @global_9

if_s5_after:                                      ; preds = %if_s3_else, %switch_after1, %switch_after
  ret ptr @global_10

switch_after:                                     ; No predecessors!
  br label %if_s5_after

switch_case_i0_:                                  ; preds = %if_s0_then
  ret ptr @global_1

switch_case_i1_:                                  ; preds = %if_s0_then
  ret ptr @global_2

switch_case_i2_:                                  ; preds = %if_s0_then
  ret ptr @global_3

switch_case_default:                              ; preds = %if_s0_then
  ret ptr @global_4

switch_after1:                                    ; No predecessors!
  br label %if_s5_after

switch_case_i0_2:                                 ; preds = %if_s2_then
  ret ptr @global_5

switch_case_i1_3:                                 ; preds = %if_s2_then
  ret ptr @global_6

switch_case_i2_4:                                 ; preds = %if_s2_then
  ret ptr @global_7

switch_case_default5:                             ; preds = %if_s2_then
  ret ptr @global_8
}}

; Function Attrs: convergent
define private void @iris.json.write_format_specifier(ptr %"arguments[0].buffer_0", i64 %"arguments[0].buffer_1", ptr noundef %"arguments[1].specifier") #0 {{
entry:
  %buffer = alloca %struct.iris_builtin_Generic_array_slice, align 8
  %specifier = alloca ptr, align 8
  %length = alloca i64, align 8
  %index = alloca i64, align 8
  %0 = getelementptr inbounds {{ ptr, i64 }}, ptr %buffer, i32 0, i32 0
  store ptr %"arguments[0].buffer_0", ptr %0, align 8
  %1 = getelementptr inbounds {{ ptr, i64 }}, ptr %buffer, i32 0, i32 1
  store i64 %"arguments[0].buffer_1", ptr %1, align 8
  store ptr %"arguments[1].specifier", ptr %specifier, align 8
  %2 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %buffer, i32 0, i32 0
  %3 = load ptr, ptr %2, align 8
  %array_slice_element_pointer = getelementptr i8, ptr %3, i32 0
  store i8 37, ptr %array_slice_element_pointer, align 1
  %4 = load ptr, ptr %specifier, align 8
  %5 = call i64 @strlen(ptr noundef %4)
  store i64 %5, ptr %length, align 8
  store i64 0, ptr %index, align 8
  br label %for_loop_condition

for_loop_condition:                               ; preds = %for_loop_update_index, %entry
  %6 = load i64, ptr %length, align 8
  %7 = load i64, ptr %index, align 8
  %8 = icmp ult i64 %7, %6
  br i1 %8, label %for_loop_then, label %for_loop_after

for_loop_then:                                    ; preds = %for_loop_condition
  %9 = load i64, ptr %index, align 8
  %10 = add i64 1, %9
  %11 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %buffer, i32 0, i32 0
  %12 = load ptr, ptr %11, align 8
  %array_slice_element_pointer1 = getelementptr i8, ptr %12, i64 %10
  %13 = load i64, ptr %index, align 8
  %14 = load ptr, ptr %specifier, align 8
  %array_element_pointer = getelementptr i8, ptr %14, i64 %13
  %15 = load i8, ptr %array_element_pointer, align 1
  store i8 %15, ptr %array_slice_element_pointer1, align 1
  br label %for_loop_update_index

for_loop_update_index:                            ; preds = %for_loop_then
  %16 = load i64, ptr %index, align 8
  %17 = add i64 %16, 1
  store i64 %17, ptr %index, align 8
  br label %for_loop_condition

for_loop_after:                                   ; preds = %for_loop_condition
  %18 = load i64, ptr %length, align 8
  %19 = add i64 1, %18
  %20 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %buffer, i32 0, i32 0
  %21 = load ptr, ptr %20, align 8
  %array_slice_element_pointer2 = getelementptr i8, ptr %21, i64 %19
  store i8 0, ptr %array_slice_element_pointer2, align 1
  ret void
}}

; Function Attrs: convergent
define private void @iris.json__at__to_json__at__3489948734076117284(ptr noundef %"arguments[0].stream", ptr noundef %"arguments[1].value") #0 {{
entry:
  %0 = alloca %struct.iris_json_Write_stream, align 8
  %value = alloca ptr, align 8
  %array = alloca [16 x i8], i64 16, align 1
  %format_specifier_buffer = alloca [16 x i8], align 1
  %1 = alloca %struct.iris_builtin_Generic_array_slice, align 8
  %format_specifier = alloca ptr, align 8
  %array1 = alloca [64 x i8], i64 64, align 1
  %format_buffer = alloca [64 x i8], align 1
  %2 = getelementptr inbounds %struct.iris_json_Write_stream, ptr %0, i32 0, i32 0
  store ptr %"arguments[0].stream", ptr %2, align 8
  store ptr %"arguments[1].value", ptr %value, align 8
  %3 = load ptr, ptr %value, align 8
  %4 = icmp eq ptr %3, null
  br i1 %4, label %if_s0_then, label %if_s1_after

if_s0_then:                                       ; preds = %entry
  %5 = getelementptr inbounds %struct.iris_json_Write_stream, ptr %0, i32 0, i32 0
  %6 = load ptr, ptr %5, align 8
  call void %6(ptr noundef @global_11)
  ret void

if_s1_after:                                      ; preds = %entry
  call void @llvm.memset.p0.i64(ptr align 1 %array, i8 0, i64 16, i1 false)
  %7 = load [16 x i8], ptr %array, align 1
  store [16 x i8] %7, ptr %format_specifier_buffer, align 1
  %data_pointer = getelementptr [16 x i8], ptr %format_specifier_buffer, i32 0, i32 0
  %8 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %1, i32 0, i32 0
  store ptr %data_pointer, ptr %8, align 8
  %9 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %1, i32 0, i32 1
  store i64 16, ptr %9, align 8
  %10 = getelementptr inbounds {{ ptr, i64 }}, ptr %1, i32 0, i32 0
  %11 = load ptr, ptr %10, align 8
  %12 = getelementptr inbounds {{ ptr, i64 }}, ptr %1, i32 0, i32 1
  %13 = load i64, ptr %12, align 8
  %14 = call ptr @iris.json.create_format_specifier(ptr %11, i64 %13, i32 noundef 0, i64 noundef 4)
  store ptr %14, ptr %format_specifier, align 8
  call void @llvm.memset.p0.i64(ptr align 1 %array1, i8 0, i64 64, i1 false)
  %15 = load [64 x i8], ptr %array1, align 1
  store [64 x i8] %15, ptr %format_buffer, align 1
  %array_element_pointer = getelementptr [64 x i8], ptr %format_buffer, i32 0, i32 0
  %16 = load ptr, ptr %value, align 8
  %17 = load ptr, ptr %format_specifier, align 8
  %18 = load i32, ptr %16, align 4
  %19 = call i32 (ptr, i64, ptr, ...) @snprintf(ptr noundef %array_element_pointer, i64 noundef 64, ptr noundef %17, i32 noundef %18)
  %20 = getelementptr inbounds %struct.iris_json_Write_stream, ptr %0, i32 0, i32 0
  %21 = load ptr, ptr %20, align 8
  %array_element_pointer2 = getelementptr [64 x i8], ptr %format_buffer, i32 0, i32 0
  call void %21(ptr noundef %array_element_pointer2)
  ret void
}}

; Function Attrs: convergent
define private void @iris.json.print_to_stderr(ptr noundef %"arguments[0].value") #0 {{
entry:
  %value = alloca ptr, align 8
  store ptr %"arguments[0].value", ptr %value, align 8
  %0 = call ptr @__acrt_iob_func(i32 noundef 2)
  %1 = load ptr, ptr %value, align 8
  %2 = call i32 (ptr, ptr, ...) @fprintf(ptr noundef %0, ptr noundef @global_12, ptr noundef %1)
  ret void
}}

; Function Attrs: convergent
define private void @iris.json__at__print_json__at__9753731967319569499(ptr noundef %"arguments[0].value") #0 {{
entry:
  %value = alloca ptr, align 8
  %stream = alloca %struct.iris_json_Write_stream, align 8
  store ptr %"arguments[0].value", ptr %value, align 8
  %0 = getelementptr inbounds %struct.iris_json_Write_stream, ptr %stream, i32 0, i32 0
  store ptr @iris.json.print_to_stderr, ptr %0, align 8
  %1 = getelementptr inbounds %struct.iris_json_Write_stream, ptr %stream, i32 0, i32 0
  %2 = load ptr, ptr %1, align 8
  %3 = load ptr, ptr %value, align 8
  call void @iris.json__at__to_json__at__3489948734076117284(ptr noundef %2, ptr noundef %3)
  ret void
}}

; Function Attrs: convergent
define private void @iris.json__at__print_json_difference__at__16983553210230134252(ptr noundef %"arguments[0].lhs", ptr noundef %"arguments[1].rhs") #0 {{
entry:
  %lhs = alloca ptr, align 8
  %rhs = alloca ptr, align 8
  store ptr %"arguments[0].lhs", ptr %lhs, align 8
  store ptr %"arguments[1].rhs", ptr %rhs, align 8
  %0 = call ptr @__acrt_iob_func(i32 noundef 2)
  %1 = call i32 (ptr, ptr, ...) @fprintf(ptr noundef %0, ptr noundef @global_13)
  %2 = load ptr, ptr %rhs, align 8
  call void @iris.json__at__print_json__at__9753731967319569499(ptr noundef %2)
  %3 = call ptr @__acrt_iob_func(i32 noundef 2)
  %4 = call i32 (ptr, ptr, ...) @fprintf(ptr noundef %3, ptr noundef @global_14)
  %5 = load ptr, ptr %lhs, align 8
  call void @iris.json__at__print_json__at__9753731967319569499(ptr noundef %5)
  %6 = call ptr @__acrt_iob_func(i32 noundef 2)
  %7 = call i32 (ptr, ptr, ...) @fprintf(ptr noundef %6, ptr noundef @global_15)
  %8 = call ptr @__acrt_iob_func(i32 noundef 2)
  %9 = call i32 @fflush(ptr noundef %8)
  ret void
}}

; Function Attrs: convergent
declare void @iris_test_check(i1 noundef zeroext, ptr noundef, i64 noundef) #0

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: write)
declare void @llvm.memset.p0.i64(ptr nocapture writeonly, i8, i64, i1 immarg) #1

; Function Attrs: convergent
declare ptr @__acrt_iob_func(i32 noundef) #0

attributes #0 = {{ convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }}
attributes #1 = {{ nocallback nofree nounwind willreturn memory(argmem: write) }}
)", test_source_file_path.size() + 1, test_source_file_path);

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir, {.is_test_mode = true});
  }

  TEST_CASE("Compile Type Kind", "[LLVM_IR]")
  {
    char const* const input_file = "type_kind.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define private void @test_type_kind_run(i32 noundef %"arguments[0].parameter") #0 {
entry:
  %parameter = alloca i32, align 4
  %a = alloca i32, align 4
  store i32 %"arguments[0].parameter", ptr %parameter, align 4
  store i32 4, ptr %a, align 4
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Unary Expressions", "[LLVM_IR]")
  {
    char const* const input_file = "unary_expressions.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
%struct.Unary_expressions_My_struct = type { i32 }

; Function Attrs: convergent
define void @Unary_expressions_unary_operations(i32 noundef %"arguments[0].my_integer", i1 noundef zeroext %"arguments[1].my_boolean", i1 noundef zeroext %"arguments[2].my_c_boolean", ptr noundef %"arguments[3].my_struct") #0 {
entry:
  %my_integer = alloca i32, align 4
  %my_boolean = alloca i8, align 1
  %my_c_boolean = alloca i8, align 1
  %my_struct = alloca ptr, align 8
  %not_variable = alloca i8, align 1
  %bitwise_not_variable = alloca i32, align 4
  %minus_variable = alloca i32, align 4
  %my_mutable_integer = alloca i32, align 4
  %address_of_variable = alloca ptr, align 8
  %indirection_variable = alloca i32, align 4
  %not_c_variable = alloca i8, align 1
  %address_of_member = alloca ptr, align 8
  %minus_variable_2 = alloca float, align 4
  store i32 %"arguments[0].my_integer", ptr %my_integer, align 4
  %0 = zext i1 %"arguments[1].my_boolean" to i8
  store i8 %0, ptr %my_boolean, align 1
  %1 = zext i1 %"arguments[2].my_c_boolean" to i8
  store i8 %1, ptr %my_c_boolean, align 1
  store ptr %"arguments[3].my_struct", ptr %my_struct, align 8
  %2 = load i8, ptr %my_boolean, align 1
  %3 = xor i8 %2, -1
  store i8 %3, ptr %not_variable, align 1
  %4 = load i32, ptr %my_integer, align 4
  %5 = xor i32 %4, -1
  store i32 %5, ptr %bitwise_not_variable, align 4
  %6 = load i32, ptr %my_integer, align 4
  %7 = sub i32 0, %6
  store i32 %7, ptr %minus_variable, align 4
  store i32 1, ptr %my_mutable_integer, align 4
  store ptr %my_mutable_integer, ptr %address_of_variable, align 8
  %8 = load ptr, ptr %address_of_variable, align 8
  %9 = load i32, ptr %8, align 4
  store i32 %9, ptr %indirection_variable, align 4
  %10 = load i8, ptr %my_c_boolean, align 1
  %11 = xor i8 %10, -1
  store i8 %11, ptr %not_c_variable, align 1
  %12 = load ptr, ptr %my_struct, align 8
  %13 = getelementptr inbounds %struct.Unary_expressions_My_struct, ptr %12, i32 0, i32 0
  store ptr %13, ptr %address_of_member, align 8
  %14 = load i32, ptr %my_integer, align 4
  %15 = sitofp i32 %14 to float
  %16 = fneg float %15
  store float %16, ptr %minus_variable_2, align 4
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Use Printf", "[LLVM_IR]")
  {
    char const* const input_file = "use_printf.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
      { "c.stdio", import_c_header_and_get_file_path("c.stdio", "stdio.h") }
    };

    char const* const expected_llvm_ir = R"(
@global_0 = internal constant [24 x i8] c"Value: %d, pointer: %p\0A\00"

; Function Attrs: convergent
declare i32 @printf(ptr noundef, ...) #0

; Function Attrs: convergent
define void @Use_printf_run() #0 {
entry:
  %a = alloca i32, align 4
  store i32 1, ptr %a, align 4
  %0 = load i32, ptr %a, align 4
  %1 = call i32 (ptr, ...) @printf(ptr noundef @global_0, i32 noundef %0, ptr noundef %a)
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Use Printf Variadic Promotions", "[LLVM_IR]")
  {
    char const* const input_file = "use_printf_variadic_promotions.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
      { "c.stdio", import_c_header_and_get_file_path("c.stdio", "stdio.h") }
    };

    char const* const expected_llvm_ir = R"(
@global_0 = internal constant [13 x i8] c"%f %d %u %d\0A\00"

; Function Attrs: convergent
declare i32 @printf(ptr noundef, ...) #0

; Function Attrs: convergent
define void @Use_printf_variadic_promotions_run() #0 {
entry:
  %f = alloca float, align 4
  %s = alloca i8, align 1
  %u = alloca i8, align 1
  %sh = alloca i16, align 2
  store float 1.500000e+00, ptr %f, align 4
  store i8 -2, ptr %s, align 1
  store i8 3, ptr %u, align 1
  store i16 4, ptr %sh, align 2
  %0 = load float, ptr %f, align 4
  %1 = fpext float %0 to double
  %2 = load i8, ptr %s, align 1
  %3 = sext i8 %2 to i32
  %4 = load i8, ptr %u, align 1
  %5 = zext i8 %4 to i32
  %6 = load i16, ptr %sh, align 2
  %7 = sext i16 %6 to i32
  %8 = call i32 (ptr, ...) @printf(ptr noundef @global_0, double noundef %1, i32 noundef %3, i32 noundef %5, i32 noundef %7)
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Use Printf Variadic Promotions Indirection", "[LLVM_IR]")
  {
    char const* const input_file = "use_printf_variadic_promotions_indirection.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
      { "c.stdio", import_c_header_and_get_file_path("c.stdio", "stdio.h") }
    };

    char const* const expected_llvm_ir = R"(
@global_0 = internal constant [4 x i8] c"%f\0A\00"

; Function Attrs: convergent
declare i32 @printf(ptr noundef, ...) #0

; Function Attrs: convergent
define void @Use_printf_variadic_promotions_indirection_run() #0 {
entry:
  %x = alloca float, align 4
  %y = alloca ptr, align 8
  store float 3.000000e+00, ptr %x, align 4
  store ptr %x, ptr %y, align 8
  %0 = load ptr, ptr %y, align 8
  %1 = load float, ptr %0, align 4
  %2 = fpext float %1 to double
  %3 = call i32 (ptr, ...) @printf(ptr noundef @global_0, double noundef %2)
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Unique Name", "[LLVM_IR]")
  {
    char const* const input_file = "unique_name.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
@my_unique_global = global i32 0

; Function Attrs: convergent
define void @my_unique_function() #0 {
entry:
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Using Alias From Modules", "[LLVM_IR]")
  {
    char const* const input_file = "using_alias_from_modules.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
        { "Alias", parse_and_get_file_path(g_test_source_files_path / "using_alias.iris") },
    };

    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define void @Alias_from_modules_use_alias(i32 noundef %"arguments[0].my_enum") #0 {
entry:
  %my_enum = alloca i32, align 4
  %a = alloca i32, align 4
  store i32 %"arguments[0].my_enum", ptr %my_enum, align 4
  store i32 10, ptr %a, align 4
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Using Alias", "[LLVM_IR]")
  {
    char const* const input_file = "using_alias.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define void @Alias_use_alias(i64 noundef %"arguments[0].size", i32 noundef %"arguments[1].my_enum") #0 {
entry:
  %size = alloca i64, align 8
  %my_enum = alloca i32, align 4
  %a = alloca i32, align 4
  store i64 %"arguments[0].size", ptr %size, align 8
  store i32 %"arguments[1].my_enum", ptr %my_enum, align 4
  store i32 10, ptr %a, align 4
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Using Enum Flags", "[LLVM_IR]")
  {
    char const* const input_file = "using_enum_flags.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define i32 @Enum_flags_use_enums(i32 noundef %"arguments[0].enum_argument") #0 {
entry:
  %enum_argument = alloca i32, align 4
  %a = alloca i32, align 4
  %b = alloca i32, align 4
  %c = alloca i32, align 4
  store i32 %"arguments[0].enum_argument", ptr %enum_argument, align 4
  store i32 3, ptr %a, align 4
  %0 = load i32, ptr %enum_argument, align 4
  %1 = and i32 %0, 1
  store i32 %1, ptr %b, align 4
  %2 = load i32, ptr %enum_argument, align 4
  %3 = xor i32 %2, 1
  store i32 %3, ptr %c, align 4
  %4 = load i32, ptr %a, align 4
  %5 = load i32, ptr %enum_argument, align 4
  %6 = icmp eq i32 %4, %5
  br i1 %6, label %if_s0_then, label %if_s1_after

if_s0_then:                                       ; preds = %entry
  ret i32 0

if_s1_after:                                      ; preds = %entry
  %7 = load i32, ptr %b, align 4
  %8 = load i32, ptr %enum_argument, align 4
  %9 = icmp ne i32 %7, %8
  br i1 %9, label %if_s0_then1, label %if_s1_after2

if_s0_then1:                                      ; preds = %if_s1_after
  ret i32 1

if_s1_after2:                                     ; preds = %if_s1_after
  %10 = load i32, ptr %enum_argument, align 4
  %11 = and i32 %10, 1
  %12 = icmp ugt i32 %11, 0
  br i1 %12, label %if_s0_then3, label %if_s1_after4

if_s0_then3:                                      ; preds = %if_s1_after2
  ret i32 2

if_s1_after4:                                     ; preds = %if_s1_after2
  %13 = load i32, ptr %enum_argument, align 4
  %14 = and i32 %13, 2
  %15 = icmp ugt i32 %14, 0
  br i1 %15, label %if_s0_then5, label %if_s1_after6

if_s0_then5:                                      ; preds = %if_s1_after4
  ret i32 3

if_s1_after6:                                     ; preds = %if_s1_after4
  %16 = load i32, ptr %enum_argument, align 4
  %17 = and i32 %16, 4
  %18 = icmp ugt i32 %17, 0
  br i1 %18, label %if_s0_then7, label %if_s1_after8

if_s0_then7:                                      ; preds = %if_s1_after6
  ret i32 4

if_s1_after8:                                     ; preds = %if_s1_after6
  ret i32 5
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Using Enum Default Values", "[LLVM_IR]")
  {
    char const* const input_file = "using_enums_default_values.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define void @Enums_default_values_use_enums() #0 {
entry:
  %v0 = alloca i32, align 4
  %v1 = alloca i32, align 4
  %v2 = alloca i32, align 4
  store i32 0, ptr %v0, align 4
  store i32 1, ptr %v1, align 4
  store i32 2, ptr %v2, align 4
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Using Enums Duplicate", "[LLVM_IR]")
  {
    char const* const input_file = "using_enums_duplicate.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
        { "Enums", parse_and_get_file_path(g_test_source_files_path / "using_enums.iris") },
    };

    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define void @Enums_duplicate_use_enums() #0 {
entry:
  %v0 = alloca i32, align 4
  %v1 = alloca i32, align 4
  store i32 10, ptr %v0, align 4
  store i32 1, ptr %v1, align 4
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Using Enums From Modules", "[LLVM_IR]")
  {
    char const* const input_file = "using_enums_from_modules.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
        { "Enums", parse_and_get_file_path(g_test_source_files_path / "using_enums.iris") },
    };

    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define void @Enums_from_modules_use_enums() #0 {
entry:
  %my_value = alloca i32, align 4
  store i32 1, ptr %my_value, align 4
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Using Enums", "[LLVM_IR]")
  {
    char const* const input_file = "using_enums.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define i32 @Enums_use_enums(i32 noundef %"arguments[0].enum_argument") #0 {
entry:
  %enum_argument = alloca i32, align 4
  %my_value = alloca i32, align 4
  store i32 %"arguments[0].enum_argument", ptr %enum_argument, align 4
  store i32 1, ptr %my_value, align 4
  %0 = load i32, ptr %enum_argument, align 4
  switch i32 %0, label %switch_after [
    i32 0, label %switch_case_i0_
    i32 1, label %switch_case_i1_
    i32 4, label %switch_case_i2_
    i32 8, label %switch_case_i3_
    i32 10, label %switch_case_i4_
    i32 11, label %switch_case_i5_
  ]

switch_after:                                     ; preds = %entry
  ret i32 2

switch_case_i0_:                                  ; preds = %entry
  br label %switch_case_i1_

switch_case_i1_:                                  ; preds = %switch_case_i0_, %entry
  br label %switch_case_i2_

switch_case_i2_:                                  ; preds = %switch_case_i1_, %entry
  br label %switch_case_i3_

switch_case_i3_:                                  ; preds = %switch_case_i2_, %entry
  ret i32 0

switch_case_i4_:                                  ; preds = %entry
  br label %switch_case_i5_

switch_case_i5_:                                  ; preds = %switch_case_i4_, %entry
  ret i32 1
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Using Function Constructors", "[LLVM_IR]")
  {
    char const* const input_file = "using_function_constructors.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define private void @Function_constructor_run() #0 {
entry:
  %a = alloca i32, align 4
  %b = alloca float, align 4
  %c = alloca i32, align 4
  %0 = call i32 @Function_constructor__at__add__at__10481941949038830817(i32 noundef 1, i32 noundef 2)
  store i32 %0, ptr %a, align 4
  %1 = call float @Function_constructor__at__add__at__4195550094456234142(float noundef 3.000000e+00, float noundef 4.000000e+00)
  store float %1, ptr %b, align 4
  %2 = call i32 @Function_constructor__at__add__at__10481941949038830817(i32 noundef 1, i32 noundef 2)
  store i32 %2, ptr %c, align 4
  ret void
}

; Function Attrs: convergent
define private i32 @Function_constructor__at__add__at__10481941949038830817(i32 noundef %"arguments[0].first", i32 noundef %"arguments[1].second") #0 {
entry:
  %first = alloca i32, align 4
  %second = alloca i32, align 4
  store i32 %"arguments[0].first", ptr %first, align 4
  store i32 %"arguments[1].second", ptr %second, align 4
  %0 = load i32, ptr %first, align 4
  %1 = load i32, ptr %second, align 4
  %2 = add i32 %0, %1
  ret i32 %2
}

; Function Attrs: convergent
define private float @Function_constructor__at__add__at__4195550094456234142(float noundef %"arguments[0].first", float noundef %"arguments[1].second") #0 {
entry:
  %first = alloca float, align 4
  %second = alloca float, align 4
  store float %"arguments[0].first", ptr %first, align 4
  store float %"arguments[1].second", ptr %second, align 4
  %0 = load float, ptr %first, align 4
  %1 = load float, ptr %second, align 4
  %2 = fadd float %0, %1
  ret float %2
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Using Global Variables", "[LLVM_IR]")
  {
    char const* const input_file = "using_global_variables.iris";

    std::filesystem::path const root_directory_path = std::filesystem::temp_directory_path() / "using_global_variables";
    std::filesystem::create_directories(root_directory_path);

    std::string const header_content = R"(
#define MY_DEFINE 2.0f
float my_global = 0.0f;
)";

    std::filesystem::path const header_module_file_path = create_and_import_c_header(header_content, "my_header.h", "my_header.iris", "my_module", root_directory_path);

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
        { "my_module", header_module_file_path }
    };

    char const* const expected_llvm_ir = R"(
@my_global = external global float
@Global_variables_my_global_constant_0 = constant float 1.000000e+00
@Global_variables_my_global_constant_1 = constant float 1.000000e+00
@Global_variables_my_global_variable_0 = global float 1.000000e+00
@Global_variables_my_global_array = constant [3 x i32] [i32 1, i32 2, i32 3]

; Function Attrs: convergent
define void @Global_variables_use_global_variables(float noundef %"arguments[0].parameter") #0 {
entry:
  %parameter = alloca float, align 4
  %a = alloca float, align 4
  %b = alloca ptr, align 8
  %c = alloca float, align 4
  %d = alloca float, align 4
  %e = alloca float, align 4
  %f = alloca i32, align 4
  store float %"arguments[0].parameter", ptr %parameter, align 4
  %0 = load float, ptr @Global_variables_my_global_constant_0, align 4
  %1 = load float, ptr @Global_variables_my_global_constant_1, align 4
  %2 = fadd float %0, %1
  %3 = load float, ptr @Global_variables_my_global_variable_0, align 4
  %4 = fadd float %2, %3
  %5 = load float, ptr %parameter, align 4
  %6 = fadd float %4, %5
  store float %6, ptr %a, align 4
  store ptr @Global_variables_my_global_variable_0, ptr %b, align 8
  store float 2.000000e+00, ptr %c, align 4
  %7 = load float, ptr @my_global, align 4
  store float %7, ptr %d, align 4
  store float 1.000000e+00, ptr %e, align 4
  %8 = load i32, ptr getelementptr inbounds ([3 x i32], ptr @Global_variables_my_global_array, i32 0, i32 1), align 4
  store i32 %8, ptr %f, align 4
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Using Global Variables 2", "[LLVM_IR]")
  {
    char const* const input_file = "using_global_variables_2.iris";

    std::filesystem::path const root_directory_path = std::filesystem::temp_directory_path() / "using_global_variables";
    std::filesystem::create_directories(root_directory_path);

    std::string const header_content = R"(
void foo();
)";

    std::filesystem::path const header_module_file_path = create_and_import_c_header(header_content, "my_header.h", "my_header.iris", "my_module", root_directory_path);

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
        { "my_module", header_module_file_path }
    };

    char const* const expected_llvm_ir = R"(
@Global_variables_2_my_global_array = constant [1 x ptr] [ptr @foo]
@Global_variables_2_my_global_array_2 = constant i32 1

; Function Attrs: convergent
declare void @foo() #0

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Using Structs", "[LLVM_IR]")
  {
    char const* const input_file = "using_structs.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
%struct.Structs_My_struct = type { i32, i32 }
%struct.Structs_My_struct_2 = type { %struct.Structs_My_struct, %struct.Structs_My_struct, %struct.Structs_My_struct }
%struct.Structs_My_struct_3 = type { i32, %union.Structs_My_Union }
%union.Structs_My_Union = type { %struct.Structs_My_struct_2 }

@Structs_g_v0 = constant %struct.Structs_My_struct { i32 0, i32 1 }

; Function Attrs: convergent
define void @Structs_use_structs(i64 noundef %"arguments[0].my_struct") #0 {
entry:
  %0 = alloca %struct.Structs_My_struct, align 4
  %a = alloca i32, align 4
  %instance_0 = alloca %struct.Structs_My_struct, align 4
  %instance_1 = alloca %struct.Structs_My_struct, align 4
  %instance_2 = alloca %struct.Structs_My_struct_2, align 4
  %1 = alloca %struct.Structs_My_struct, align 4
  %2 = alloca %struct.Structs_My_struct, align 4
  %3 = alloca %struct.Structs_My_struct, align 4
  %instance_3 = alloca %struct.Structs_My_struct_2, align 4
  %4 = alloca %struct.Structs_My_struct, align 4
  %5 = alloca %struct.Structs_My_struct, align 4
  %6 = alloca %struct.Structs_My_struct, align 4
  %nested_b_a = alloca i32, align 4
  %instance_4 = alloca %struct.Structs_My_struct, align 4
  %7 = alloca %struct.Structs_My_struct, align 4
  %8 = alloca %struct.Structs_My_struct, align 4
  %9 = alloca %struct.Structs_My_struct, align 4
  %instance_5 = alloca %struct.Structs_My_struct, align 4
  %instance_6 = alloca %struct.Structs_My_struct_3, align 4
  %10 = alloca %struct.Structs_My_struct, align 4
  %11 = alloca %union.Structs_My_Union, align 4
  %12 = getelementptr inbounds %struct.Structs_My_struct, ptr %0, i32 0, i32 0
  store i64 %"arguments[0].my_struct", ptr %12, align 4
  %13 = getelementptr inbounds %struct.Structs_My_struct, ptr %0, i32 0, i32 0
  %14 = load i32, ptr %13, align 4
  store i32 %14, ptr %a, align 4
  %15 = getelementptr inbounds %struct.Structs_My_struct, ptr %instance_0, i32 0, i32 0
  store i32 1, ptr %15, align 4
  %16 = getelementptr inbounds %struct.Structs_My_struct, ptr %instance_0, i32 0, i32 1
  store i32 2, ptr %16, align 4
  %17 = getelementptr inbounds %struct.Structs_My_struct, ptr %instance_1, i32 0, i32 0
  store i32 1, ptr %17, align 4
  %18 = getelementptr inbounds %struct.Structs_My_struct, ptr %instance_1, i32 0, i32 1
  store i32 3, ptr %18, align 4
  %19 = getelementptr inbounds %struct.Structs_My_struct, ptr %1, i32 0, i32 0
  store i32 1, ptr %19, align 4
  %20 = getelementptr inbounds %struct.Structs_My_struct, ptr %1, i32 0, i32 1
  store i32 2, ptr %20, align 4
  %21 = load %struct.Structs_My_struct, ptr %1, align 4
  %22 = getelementptr inbounds %struct.Structs_My_struct_2, ptr %instance_2, i32 0, i32 0
  store %struct.Structs_My_struct %21, ptr %22, align 4
  %23 = getelementptr inbounds %struct.Structs_My_struct, ptr %2, i32 0, i32 0
  store i32 2, ptr %23, align 4
  %24 = getelementptr inbounds %struct.Structs_My_struct, ptr %2, i32 0, i32 1
  store i32 2, ptr %24, align 4
  %25 = load %struct.Structs_My_struct, ptr %2, align 4
  %26 = getelementptr inbounds %struct.Structs_My_struct_2, ptr %instance_2, i32 0, i32 1
  store %struct.Structs_My_struct %25, ptr %26, align 4
  %27 = getelementptr inbounds %struct.Structs_My_struct, ptr %3, i32 0, i32 0
  store i32 3, ptr %27, align 4
  %28 = getelementptr inbounds %struct.Structs_My_struct, ptr %3, i32 0, i32 1
  store i32 4, ptr %28, align 4
  %29 = load %struct.Structs_My_struct, ptr %3, align 4
  %30 = getelementptr inbounds %struct.Structs_My_struct_2, ptr %instance_2, i32 0, i32 2
  store %struct.Structs_My_struct %29, ptr %30, align 4
  %31 = getelementptr inbounds %struct.Structs_My_struct, ptr %4, i32 0, i32 0
  store i32 1, ptr %31, align 4
  %32 = getelementptr inbounds %struct.Structs_My_struct, ptr %4, i32 0, i32 1
  store i32 2, ptr %32, align 4
  %33 = load %struct.Structs_My_struct, ptr %4, align 4
  %34 = getelementptr inbounds %struct.Structs_My_struct_2, ptr %instance_3, i32 0, i32 0
  store %struct.Structs_My_struct %33, ptr %34, align 4
  %35 = getelementptr inbounds %struct.Structs_My_struct, ptr %5, i32 0, i32 0
  store i32 1, ptr %35, align 4
  %36 = getelementptr inbounds %struct.Structs_My_struct, ptr %5, i32 0, i32 1
  store i32 2, ptr %36, align 4
  %37 = load %struct.Structs_My_struct, ptr %5, align 4
  %38 = getelementptr inbounds %struct.Structs_My_struct_2, ptr %instance_3, i32 0, i32 1
  store %struct.Structs_My_struct %37, ptr %38, align 4
  %39 = getelementptr inbounds %struct.Structs_My_struct, ptr %6, i32 0, i32 0
  store i32 0, ptr %39, align 4
  %40 = getelementptr inbounds %struct.Structs_My_struct, ptr %6, i32 0, i32 1
  store i32 1, ptr %40, align 4
  %41 = load %struct.Structs_My_struct, ptr %6, align 4
  %42 = getelementptr inbounds %struct.Structs_My_struct_2, ptr %instance_3, i32 0, i32 2
  store %struct.Structs_My_struct %41, ptr %42, align 4
  %43 = getelementptr inbounds %struct.Structs_My_struct_2, ptr %instance_3, i32 0, i32 1
  %44 = getelementptr inbounds %struct.Structs_My_struct, ptr %43, i32 0, i32 0
  %45 = load i32, ptr %44, align 4
  store i32 %45, ptr %nested_b_a, align 4
  %46 = getelementptr inbounds %struct.Structs_My_struct, ptr %instance_4, i32 0, i32 0
  store i32 1, ptr %46, align 4
  %47 = getelementptr inbounds %struct.Structs_My_struct, ptr %instance_4, i32 0, i32 1
  store i32 2, ptr %47, align 4
  %48 = getelementptr inbounds %struct.Structs_My_struct, ptr %7, i32 0, i32 0
  store i32 10, ptr %48, align 4
  %49 = getelementptr inbounds %struct.Structs_My_struct, ptr %7, i32 0, i32 1
  store i32 11, ptr %49, align 4
  %50 = load %struct.Structs_My_struct, ptr %7, align 4
  store %struct.Structs_My_struct %50, ptr %instance_4, align 4
  %51 = getelementptr inbounds %struct.Structs_My_struct, ptr %instance_4, i32 0, i32 0
  %52 = getelementptr inbounds %struct.Structs_My_struct, ptr %instance_4, i32 0, i32 0
  store i32 0, ptr %52, align 4
  %53 = getelementptr inbounds %struct.Structs_My_struct, ptr %8, i32 0, i32 0
  store i32 1, ptr %53, align 4
  %54 = getelementptr inbounds %struct.Structs_My_struct, ptr %8, i32 0, i32 1
  store i32 2, ptr %54, align 4
  %55 = getelementptr inbounds %struct.Structs_My_struct, ptr %8, i32 0, i32 0
  %56 = load i64, ptr %55, align 4
  call void @Structs_pass_struct(i64 noundef %56)
  %57 = call i64 @Structs_return_struct()
  %58 = getelementptr inbounds %struct.Structs_My_struct, ptr %9, i32 0, i32 0
  store i64 %57, ptr %58, align 4
  %59 = load %struct.Structs_My_struct, ptr %9, align 4
  store %struct.Structs_My_struct %59, ptr %instance_5, align 4
  %60 = getelementptr inbounds %struct.Structs_My_struct_3, ptr %instance_6, i32 0, i32 0
  store i32 4, ptr %60, align 4
  %61 = getelementptr inbounds %struct.Structs_My_struct, ptr %10, i32 0, i32 0
  store i32 1, ptr %61, align 4
  %62 = getelementptr inbounds %struct.Structs_My_struct, ptr %10, i32 0, i32 1
  store i32 2, ptr %62, align 4
  %63 = load %struct.Structs_My_struct, ptr %10, align 4
  store %struct.Structs_My_struct %63, ptr %11, align 4
  %64 = load %union.Structs_My_Union, ptr %11, align 4
  %65 = getelementptr inbounds %struct.Structs_My_struct_3, ptr %instance_6, i32 0, i32 1
  store %union.Structs_My_Union %64, ptr %65, align 4
  ret void
}

; Function Attrs: convergent
define private void @Structs_pass_struct(i64 noundef %"arguments[0].my_struct") #0 {
entry:
  %0 = alloca %struct.Structs_My_struct, align 4
  %1 = getelementptr inbounds %struct.Structs_My_struct, ptr %0, i32 0, i32 0
  store i64 %"arguments[0].my_struct", ptr %1, align 4
  ret void
}

; Function Attrs: convergent
define private i64 @Structs_return_struct() #0 {
entry:
  %0 = alloca %struct.Structs_My_struct, align 4
  %1 = getelementptr inbounds %struct.Structs_My_struct, ptr %0, i32 0, i32 0
  store i32 1, ptr %1, align 4
  %2 = getelementptr inbounds %struct.Structs_My_struct, ptr %0, i32 0, i32 1
  store i32 2, ptr %2, align 4
  %3 = getelementptr inbounds %struct.Structs_My_struct, ptr %0, i32 0, i32 0
  %4 = load i64, ptr %3, align 4
  ret i64 %4
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Using Type Constructors", "[LLVM_IR]")
  {
    char const* const input_file = "using_type_constructors.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
%struct.Type_constructor__at__Dynamic_array__at__9266664480299747837 = type { ptr, i64 }
%struct.Type_constructor__at__Dynamic_array__at__12246575587352456780 = type { ptr, i64 }
%struct.Type_constructor_My_struct = type { %struct.Type_constructor__at__Dynamic_array__at__13825035046261308487 }
%struct.Type_constructor__at__Dynamic_array__at__13825035046261308487 = type { ptr, i64 }
%struct.Type_constructor__at__Dynamic_array__at__11091932333614297595 = type { ptr, i64 }

; Function Attrs: convergent
define private void @Type_constructor_run(ptr %"arguments[0].instance_0_0", i64 %"arguments[0].instance_0_1") #0 {
entry:
  %instance_0 = alloca %struct.Type_constructor__at__Dynamic_array__at__9266664480299747837, align 8
  %instance_1 = alloca %struct.Type_constructor__at__Dynamic_array__at__12246575587352456780, align 8
  %instance_2 = alloca %struct.Type_constructor_My_struct, align 8
  %0 = alloca %struct.Type_constructor__at__Dynamic_array__at__13825035046261308487, align 8
  %instance_3 = alloca %struct.Type_constructor__at__Dynamic_array__at__11091932333614297595, align 8
  %1 = getelementptr inbounds { ptr, i64 }, ptr %instance_0, i32 0, i32 0
  store ptr %"arguments[0].instance_0_0", ptr %1, align 8
  %2 = getelementptr inbounds { ptr, i64 }, ptr %instance_0, i32 0, i32 1
  store i64 %"arguments[0].instance_0_1", ptr %2, align 8
  %3 = getelementptr inbounds %struct.Type_constructor__at__Dynamic_array__at__12246575587352456780, ptr %instance_1, i32 0, i32 0
  store ptr null, ptr %3, align 8
  %4 = getelementptr inbounds %struct.Type_constructor__at__Dynamic_array__at__12246575587352456780, ptr %instance_1, i32 0, i32 1
  store i64 0, ptr %4, align 8
  %5 = getelementptr inbounds %struct.Type_constructor__at__Dynamic_array__at__13825035046261308487, ptr %0, i32 0, i32 0
  store ptr null, ptr %5, align 8
  %6 = getelementptr inbounds %struct.Type_constructor__at__Dynamic_array__at__13825035046261308487, ptr %0, i32 0, i32 1
  store i64 0, ptr %6, align 8
  %7 = load %struct.Type_constructor__at__Dynamic_array__at__13825035046261308487, ptr %0, align 8
  %8 = getelementptr inbounds %struct.Type_constructor_My_struct, ptr %instance_2, i32 0, i32 0
  store %struct.Type_constructor__at__Dynamic_array__at__13825035046261308487 %7, ptr %8, align 8
  %9 = getelementptr inbounds %struct.Type_constructor__at__Dynamic_array__at__11091932333614297595, ptr %instance_3, i32 0, i32 0
  store ptr null, ptr %9, align 8
  %10 = getelementptr inbounds %struct.Type_constructor__at__Dynamic_array__at__11091932333614297595, ptr %instance_3, i32 0, i32 1
  store i64 0, ptr %10, align 8
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Using Type Constructors 2", "[LLVM_IR]")
  {
    char const* const input_file = "using_type_constructors_2.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
%struct.Using_type_constructors_2__at__Vector3__at__5571078378519863159 = type { float, float, float }

; Function Attrs: convergent
define { <2 x float>, float } @Using_type_constructors_2_get_one() #0 {
entry:
  %0 = alloca %struct.Using_type_constructors_2__at__Vector3__at__5571078378519863159, align 4
  %1 = getelementptr inbounds %struct.Using_type_constructors_2__at__Vector3__at__5571078378519863159, ptr %0, i32 0, i32 0
  store float 1.000000e+00, ptr %1, align 4
  %2 = getelementptr inbounds %struct.Using_type_constructors_2__at__Vector3__at__5571078378519863159, ptr %0, i32 0, i32 1
  store float 1.000000e+00, ptr %2, align 4
  %3 = getelementptr inbounds %struct.Using_type_constructors_2__at__Vector3__at__5571078378519863159, ptr %0, i32 0, i32 2
  store float 1.000000e+00, ptr %3, align 4
  %4 = load { <2 x float>, float }, ptr %0, align 4
  ret { <2 x float>, float } %4
}

; Function Attrs: convergent
define void @Using_type_constructors_2_use(<2 x float> %"arguments[0].value_0", float %"arguments[0].value_1") #0 {
entry:
  %value = alloca %struct.Using_type_constructors_2__at__Vector3__at__5571078378519863159, align 4
  %x = alloca float, align 4
  %0 = getelementptr inbounds { <2 x float>, float }, ptr %value, i32 0, i32 0
  store <2 x float> %"arguments[0].value_0", ptr %0, align 4
  %1 = getelementptr inbounds { <2 x float>, float }, ptr %value, i32 0, i32 1
  store float %"arguments[0].value_1", ptr %1, align 4
  %2 = getelementptr inbounds %struct.Using_type_constructors_2__at__Vector3__at__5571078378519863159, ptr %value, i32 0, i32 0
  %3 = load float, ptr %2, align 4
  store float %3, ptr %x, align 4
  ret void
}

; Function Attrs: convergent
define void @Using_type_constructors_2_use_pointer(ptr noundef %"arguments[0].value") #0 {
entry:
  %value = alloca ptr, align 8
  %x = alloca float, align 4
  store ptr %"arguments[0].value", ptr %value, align 8
  %0 = load ptr, ptr %value, align 8
  %1 = getelementptr inbounds %struct.Using_type_constructors_2__at__Vector3__at__5571078378519863159, ptr %0, i32 0, i32 0
  %2 = load float, ptr %1, align 4
  store float %2, ptr %x, align 4
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Using Type Constructors 3", "[LLVM_IR]")
  {
    char const* const input_file = "using_type_constructors_3.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
      { "Using_type_constructors_2", parse_and_get_file_path(g_test_source_files_path / "using_type_constructors_2.iris") },
    };

    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define void @Using_type_constructors_3_run() #0 {
entry:
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Using Unions", "[LLVM_IR]")
  {
    char const* const input_file = "using_unions.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    // TODO currently Unions_g_v5 is zero initialized (which it shouldn't)
    char const* const expected_llvm_ir = R"(
%union.Unions_My_union = type { i32 }
%union.Unions_My_union_2 = type { i64 }
%union.Unions_My_union_3 = type { i64 }
%union.Unions_My_union_4 = type { %struct.Unions_Big_struct }
%struct.Unions_Big_struct = type { i64, i64, i64, i64 }
%struct.Unions_My_struct = type { i32 }

@Unions_g_v0 = constant %union.Unions_My_union zeroinitializer
@Unions_g_v1 = constant %union.Unions_My_union_2 { i64 1 }
@Unions_g_v2 = constant %union.Unions_My_union_2 { i64 2 }
@Unions_g_v3 = constant %union.Unions_My_union_3 { i64 3 }
@Unions_g_v4 = constant %union.Unions_My_union_3 { i64 4 }
@Unions_g_v5 = constant %union.Unions_My_union_4 zeroinitializer
@Unions_g_v6 = constant %union.Unions_My_union_4 { %struct.Unions_Big_struct { i64 6, i64 7, i64 8, i64 9 } }

; Function Attrs: convergent
define void @Unions_use_unions(i32 noundef %"arguments[0].my_union", i32 noundef %"arguments[1].my_union_tag") #0 {
entry:
  %0 = alloca %union.Unions_My_union, align 4
  %my_union_tag = alloca i32, align 4
  %a = alloca i32, align 4
  %b = alloca float, align 4
  %instance_0 = alloca %union.Unions_My_union, align 4
  %instance_1 = alloca %union.Unions_My_union, align 4
  %instance_2 = alloca %union.Unions_My_union_2, align 8
  %instance_3 = alloca %union.Unions_My_union_2, align 8
  %instance_4 = alloca %union.Unions_My_union_3, align 8
  %1 = alloca %struct.Unions_My_struct, align 4
  %instance_5 = alloca %union.Unions_My_union_3, align 8
  %2 = alloca %struct.Unions_My_struct, align 4
  %instance_6 = alloca %union.Unions_My_union_3, align 8
  %nested_b_a = alloca i32, align 4
  %instance_7 = alloca %union.Unions_My_union, align 4
  %3 = alloca %union.Unions_My_union, align 4
  %4 = alloca %union.Unions_My_union, align 4
  %5 = alloca %union.Unions_My_union, align 4
  %instance_8 = alloca %union.Unions_My_union, align 4
  %instance_9 = alloca %union.Unions_My_union, align 4
  %6 = getelementptr inbounds %union.Unions_My_union, ptr %0, i32 0, i32 0
  store i32 %"arguments[0].my_union", ptr %6, align 4
  store i32 %"arguments[1].my_union_tag", ptr %my_union_tag, align 4
  %7 = load i32, ptr %my_union_tag, align 4
  %8 = icmp eq i32 %7, 0
  br i1 %8, label %if_s0_then, label %if_s1_else

if_s0_then:                                       ; preds = %entry
  %9 = getelementptr inbounds %union.Unions_My_union, ptr %0, i32 0, i32 0
  %10 = load i32, ptr %9, align 4
  store i32 %10, ptr %a, align 4
  br label %if_s3_after

if_s1_else:                                       ; preds = %entry
  %11 = load i32, ptr %my_union_tag, align 4
  %12 = icmp eq i32 %11, 1
  br i1 %12, label %if_s2_then, label %if_s3_after

if_s2_then:                                       ; preds = %if_s1_else
  %13 = getelementptr inbounds %union.Unions_My_union, ptr %0, i32 0, i32 0
  %14 = load float, ptr %13, align 4
  store float %14, ptr %b, align 4
  br label %if_s3_after

if_s3_after:                                      ; preds = %if_s2_then, %if_s1_else, %if_s0_then
  store i32 2, ptr %instance_0, align 4
  store float 3.000000e+00, ptr %instance_1, align 4
  store i32 2, ptr %instance_2, align 4
  store i64 3, ptr %instance_3, align 8
  store i64 3, ptr %instance_4, align 8
  %15 = getelementptr inbounds %struct.Unions_My_struct, ptr %1, i32 0, i32 0
  store i32 1, ptr %15, align 4
  %16 = load %struct.Unions_My_struct, ptr %1, align 4
  store %struct.Unions_My_struct %16, ptr %instance_5, align 4
  %17 = getelementptr inbounds %struct.Unions_My_struct, ptr %2, i32 0, i32 0
  store i32 2, ptr %17, align 4
  %18 = load %struct.Unions_My_struct, ptr %2, align 4
  store %struct.Unions_My_struct %18, ptr %instance_6, align 4
  %19 = getelementptr inbounds %union.Unions_My_union_3, ptr %instance_6, i32 0, i32 0
  %20 = getelementptr inbounds %struct.Unions_My_struct, ptr %19, i32 0, i32 0
  %21 = load i32, ptr %20, align 4
  store i32 %21, ptr %nested_b_a, align 4
  store i32 1, ptr %instance_7, align 4
  store i32 2, ptr %3, align 4
  %22 = load %union.Unions_My_union, ptr %3, align 4
  store %union.Unions_My_union %22, ptr %instance_7, align 4
  store i32 4, ptr %4, align 4
  %23 = getelementptr inbounds %union.Unions_My_union, ptr %4, i32 0, i32 0
  %24 = load i32, ptr %23, align 4
  call void @Unions_pass_union(i32 noundef %24)
  %25 = call i32 @Unions_return_union()
  %26 = getelementptr inbounds %union.Unions_My_union, ptr %5, i32 0, i32 0
  store i32 %25, ptr %26, align 4
  %27 = load %union.Unions_My_union, ptr %5, align 4
  store %union.Unions_My_union %27, ptr %instance_8, align 4
  call void @llvm.memset.p0.i64(ptr align 4 %instance_9, i8 0, i64 4, i1 false)
  ret void
}

; Function Attrs: convergent
define private void @Unions_pass_union(i32 noundef %"arguments[0].my_union") #0 {
entry:
  %0 = alloca %union.Unions_My_union, align 4
  %1 = getelementptr inbounds %union.Unions_My_union, ptr %0, i32 0, i32 0
  store i32 %"arguments[0].my_union", ptr %1, align 4
  ret void
}

; Function Attrs: convergent
define private i32 @Unions_return_union() #0 {
entry:
  %0 = alloca %union.Unions_My_union, align 4
  store float 1.000000e+01, ptr %0, align 4
  %1 = getelementptr inbounds %union.Unions_My_union, ptr %0, i32 0, i32 0
  %2 = load i32, ptr %1, align 4
  ret i32 %2
}

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: write)
declare void @llvm.memset.p0.i64(ptr nocapture writeonly, i8, i64, i1 immarg) #1

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
attributes #1 = { nocallback nofree nounwind willreturn memory(argmem: write) }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Variables", "[LLVM_IR]")
  {
    char const* const input_file = "variables.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define i32 @Variables_main() #0 {
entry:
  %my_constant_variable = alloca i32, align 4
  %my_mutable_variable = alloca i32, align 4
  store i32 1, ptr %my_constant_variable, align 4
  store i32 2, ptr %my_mutable_variable, align 4
  store i32 3, ptr %my_mutable_variable, align 4
  ret i32 0
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile While Loop Expressions", "[LLVM_IR]")
  {
    char const* const input_file = "while_loop_expressions.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
        { "c.stdio", import_c_header_and_get_file_path("c.stdio", "stdio.h") }
    };

    char const* const expected_llvm_ir = R"(
@global_0 = internal constant [3 x i8] c"%d\00"

; Function Attrs: convergent
declare i32 @printf(ptr noundef, ...) #0

; Function Attrs: convergent
define void @While_loop_expressions_run_while_loops(i32 noundef %"arguments[0].size") #0 {
entry:
  %size = alloca i32, align 4
  %index = alloca i32, align 4
  %index1 = alloca i32, align 4
  %c_boolean = alloca i8, align 1
  store i32 %"arguments[0].size", ptr %size, align 4
  store i32 0, ptr %index, align 4
  br label %while_loop_condition

while_loop_condition:                             ; preds = %while_loop_then, %entry
  %0 = load i32, ptr %index, align 4
  %1 = load i32, ptr %size, align 4
  %2 = icmp slt i32 %0, %1
  br i1 %2, label %while_loop_then, label %while_loop_after

while_loop_then:                                  ; preds = %while_loop_condition
  %3 = load i32, ptr %index, align 4
  call void @While_loop_expressions_print_integer(i32 noundef %3)
  %4 = load i32, ptr %index, align 4
  %5 = add i32 %4, 1
  store i32 %5, ptr %index, align 4
  br label %while_loop_condition

while_loop_after:                                 ; preds = %while_loop_condition
  store i32 0, ptr %index1, align 4
  br label %while_loop_condition2

while_loop_condition2:                            ; preds = %if_s1_after6, %if_s0_then, %while_loop_after
  %6 = load i32, ptr %index1, align 4
  %7 = load i32, ptr %size, align 4
  %8 = icmp slt i32 %6, %7
  br i1 %8, label %while_loop_then3, label %while_loop_after4

while_loop_then3:                                 ; preds = %while_loop_condition2
  %9 = load i32, ptr %index1, align 4
  %10 = srem i32 %9, 2
  %11 = icmp eq i32 %10, 0
  br i1 %11, label %if_s0_then, label %if_s1_after

while_loop_after4:                                ; preds = %if_s0_then5, %while_loop_condition2
  store i8 1, ptr %c_boolean, align 1
  br label %while_loop_condition7

if_s0_then:                                       ; preds = %while_loop_then3
  br label %while_loop_condition2

if_s1_after:                                      ; preds = %while_loop_then3
  %12 = load i32, ptr %index1, align 4
  %13 = icmp sgt i32 %12, 5
  br i1 %13, label %if_s0_then5, label %if_s1_after6

if_s0_then5:                                      ; preds = %if_s1_after
  br label %while_loop_after4

if_s1_after6:                                     ; preds = %if_s1_after
  %14 = load i32, ptr %index1, align 4
  call void @While_loop_expressions_print_integer(i32 noundef %14)
  %15 = load i32, ptr %index1, align 4
  %16 = add i32 %15, 1
  store i32 %16, ptr %index1, align 4
  br label %while_loop_condition2

while_loop_condition7:                            ; preds = %while_loop_then8, %while_loop_after4
  %17 = load i8, ptr %c_boolean, align 1
  %18 = trunc i8 %17 to i1
  br i1 %18, label %while_loop_then8, label %while_loop_after9

while_loop_then8:                                 ; preds = %while_loop_condition7
  store i8 0, ptr %c_boolean, align 1
  br label %while_loop_condition7

while_loop_after9:                                ; preds = %while_loop_condition7
  ret void
}

; Function Attrs: convergent
define private void @While_loop_expressions_print_integer(i32 noundef %"arguments[0].value") #0 {
entry:
  %value = alloca i32, align 4
  store i32 %"arguments[0].value", ptr %value, align 4
  %0 = load i32, ptr %value, align 4
  %1 = call i32 (ptr, ...) @printf(ptr noundef @global_0, i32 noundef %0)
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile Function Contracts", "[LLVM_IR]")
  {
    char const* const input_file = "function_contracts.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
@function_contract_error_string = private unnamed_addr constant [67 x i8] c"In function 'Function_contracts.run' precondition 'x >= 0' failed!\00"
@function_contract_error_string.1 = private unnamed_addr constant [67 x i8] c"In function 'Function_contracts.run' precondition 'x <= 8' failed!\00"
@function_contract_error_string.2 = private unnamed_addr constant [73 x i8] c"In function 'Function_contracts.run' postcondition 'result >= 0' failed!\00"
@function_contract_error_string.3 = private unnamed_addr constant [74 x i8] c"In function 'Function_contracts.run' postcondition 'result <= 64' failed!\00"
@function_contract_error_string.4 = private unnamed_addr constant [73 x i8] c"In function 'Function_contracts.run' postcondition 'result >= 0' failed!\00"
@function_contract_error_string.5 = private unnamed_addr constant [74 x i8] c"In function 'Function_contracts.run' postcondition 'result <= 64' failed!\00"

; Function Attrs: convergent
define i32 @Function_contracts_run(i32 noundef %"arguments[0].x") #0 {
entry:
  %x = alloca i32, align 4
  store i32 %"arguments[0].x", ptr %x, align 4
  %0 = load i32, ptr %x, align 4
  %1 = icmp sge i32 %0, 0
  br i1 %1, label %condition_success, label %condition_fail

condition_success:                                ; preds = %entry
  %2 = load i32, ptr %x, align 4
  %3 = icmp sle i32 %2, 8
  br i1 %3, label %condition_success1, label %condition_fail2

condition_fail:                                   ; preds = %entry
  %4 = call i32 @puts(ptr @function_contract_error_string)
  call void @abort()
  unreachable

condition_success1:                               ; preds = %condition_success
  %5 = load i32, ptr %x, align 4
  %6 = icmp eq i32 %5, 8
  br i1 %6, label %if_s0_then, label %if_s1_after

condition_fail2:                                  ; preds = %condition_success
  %7 = call i32 @puts(ptr @function_contract_error_string.1)
  call void @abort()
  unreachable

if_s0_then:                                       ; preds = %condition_success1
  br i1 true, label %condition_success3, label %condition_fail4

if_s1_after:                                      ; preds = %condition_success1
  %8 = load i32, ptr %x, align 4
  %9 = load i32, ptr %x, align 4
  %10 = mul i32 %8, %9
  %11 = icmp sge i32 %10, 0
  br i1 %11, label %condition_success7, label %condition_fail8

condition_success3:                               ; preds = %if_s0_then
  br i1 true, label %condition_success5, label %condition_fail6

condition_fail4:                                  ; preds = %if_s0_then
  %12 = call i32 @puts(ptr @function_contract_error_string.2)
  call void @abort()
  unreachable

condition_success5:                               ; preds = %condition_success3
  ret i32 64

condition_fail6:                                  ; preds = %condition_success3
  %13 = call i32 @puts(ptr @function_contract_error_string.3)
  call void @abort()
  unreachable

condition_success7:                               ; preds = %if_s1_after
  %14 = icmp sle i32 %10, 64
  br i1 %14, label %condition_success9, label %condition_fail10

condition_fail8:                                  ; preds = %if_s1_after
  %15 = call i32 @puts(ptr @function_contract_error_string.4)
  call void @abort()
  unreachable

condition_success9:                               ; preds = %condition_success7
  ret i32 %10

condition_fail10:                                 ; preds = %condition_success7
  %16 = call i32 @puts(ptr @function_contract_error_string.5)
  call void @abort()
  unreachable
}

declare i32 @puts(ptr)

declare void @abort()

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

    TEST_CASE("Compile Function Contracts with disable contracts", "[LLVM_IR]")
  {
    char const* const input_file = "function_contracts.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
    };

    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define i32 @Function_contracts_run(i32 noundef %"arguments[0].x") #0 {
entry:
  %x = alloca i32, align 4
  store i32 %"arguments[0].x", ptr %x, align 4
  %0 = load i32, ptr %x, align 4
  %1 = icmp eq i32 %0, 8
  br i1 %1, label %if_s0_then, label %if_s1_after

if_s0_then:                                       ; preds = %entry
  ret i32 64

if_s1_after:                                      ; preds = %entry
  %2 = load i32, ptr %x, align 4
  %3 = load i32, ptr %x, align 4
  %4 = mul i32 %2, %3
  ret i32 %4
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir, {.contract_options = iris::compiler::Contract_options::Disabled});
  }

  TEST_CASE("Struct layout of imported C header matches 0", "[LLVM_IR]")
  {
    std::filesystem::path const root_directory_path = std::filesystem::temp_directory_path() / "struct_layout_0";
    std::filesystem::create_directories(root_directory_path);

    std::string const header_content = R"(
#include "stdint.h"

struct My_struct
{
    uint8_t v0;  // Offset: 0, Size: 1, Alignment: 1
    uint16_t v1; // Offset: 2, Size: 2, Alignment: 2
    uint8_t v2;  // Offset: 4, Size: 1, Alignment: 1
    uint32_t v3; // Offset: 8, Size: 4, Alignment: 4
    uint8_t v4;  // Offset: 12, Size: 1, Alignment: 1
    uint64_t v5; // Offset: 16, Size: 8, Alignment: 8
};
)";

std::filesystem::path const header_module_file_path = create_and_import_c_header(header_content, "my_struct.h", "my_struct.iris", "my_module", root_directory_path);

    std::filesystem::path const header_file_path = root_directory_path / "my_struct.h";
    std::optional<iris::Struct_layout> const expected_struct_layout = iris::c::calculate_struct_layout(header_file_path, "My_struct", {});
    REQUIRE(expected_struct_layout.has_value());

    std::optional<iris::Module> core_module = iris::compiler::read_core_module(header_module_file_path);
    REQUIRE(core_module.has_value());

    iris::compiler::LLVM_data llvm_data = iris::compiler::initialize_llvm({});

    iris::Declaration_database declaration_database = iris::create_declaration_database();
    iris::add_declarations(declaration_database, *core_module);

    std::pmr::vector<iris::Module const*> core_modules{ &core_module.value() };
    iris::compiler::Clang_module_data_pointer clang_module_data = iris::compiler::create_clang_module_data(
        *llvm_data.context,
        *llvm_data.clang_data,
        "Iris_clang_module",
        core_modules,
        declaration_database
    );

    iris::compiler::Type_database type_database = iris::compiler::create_type_database(*llvm_data.context);
    iris::compiler::add_module_types(type_database, *llvm_data.context, llvm_data.data_layout, *clang_module_data, *core_module);

    iris::Struct_layout const actual_struct_layout = iris::compiler::calculate_struct_layout(llvm_data.data_layout, type_database, "my_module", "My_struct");

    CHECK(actual_struct_layout == expected_struct_layout.value());
  }

  void test_c_interoperability_call_function_with_struct_argument(
    std::string_view const target_triple,
    std::string_view const expected_llvm_ir
  )
  {
    char const* const input_file = "c_interoperability_call_function_with_struct.iris";

    std::filesystem::path const root_directory_path = std::filesystem::temp_directory_path() / "c_interoperability_call_function_with_struct";
    std::filesystem::create_directories(root_directory_path);

    std::string const header_content = R"(
typedef struct My_struct
{
    int v0;
    int v1;
    int v2;
    int v3;
} My_struct;

void foo(My_struct argument);
)";

    std::filesystem::path const header_module_file_path = create_and_import_c_header(header_content, "my_header.h", "my_header.iris", "my_module", root_directory_path);

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
        { "my_module", header_module_file_path }
    };

    Test_options const test_options
    {
      .target_triple = target_triple,
    };

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir, test_options);
  }

  TEST_CASE("C Interoperability - Call function with struct argument x86_64-pc-linux-gnu", "[LLVM_IR]")
  {
    char const* const expected_llvm_ir = R"(
%struct.My_struct = type { i32, i32, i32, i32 }

; Function Attrs: convergent
declare void @foo(i64, i64) #0

; Function Attrs: convergent
define private void @c_interoperability_run() #0 {
entry:
  %instance = alloca %struct.My_struct, align 4
  %0 = getelementptr inbounds %struct.My_struct, ptr %instance, i32 0, i32 0
  store i32 0, ptr %0, align 4
  %1 = getelementptr inbounds %struct.My_struct, ptr %instance, i32 0, i32 1
  store i32 0, ptr %1, align 4
  %2 = getelementptr inbounds %struct.My_struct, ptr %instance, i32 0, i32 2
  store i32 0, ptr %2, align 4
  %3 = getelementptr inbounds %struct.My_struct, ptr %instance, i32 0, i32 3
  store i32 0, ptr %3, align 4
  %4 = getelementptr inbounds { i64, i64 }, ptr %instance, i32 0, i32 0
  %5 = load i64, ptr %4, align 4
  %6 = getelementptr inbounds { i64, i64 }, ptr %instance, i32 0, i32 1
  %7 = load i64, ptr %6, align 4
  call void @foo(i64 %5, i64 %7)
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_c_interoperability_call_function_with_struct_argument("x86_64-pc-linux-gnu", expected_llvm_ir);
  }

    TEST_CASE("C Interoperability - Call function with struct argument x86_64-pc-windows-msvc", "[LLVM_IR]")
  {
    char const* const expected_llvm_ir = R"(
%struct.My_struct = type { i32, i32, i32, i32 }

; Function Attrs: convergent
declare void @foo(ptr noundef) #0

; Function Attrs: convergent
define private void @c_interoperability_run() #0 {
entry:
  %instance = alloca %struct.My_struct, align 4
  %0 = alloca %struct.My_struct, align 4
  %1 = getelementptr inbounds %struct.My_struct, ptr %instance, i32 0, i32 0
  store i32 0, ptr %1, align 4
  %2 = getelementptr inbounds %struct.My_struct, ptr %instance, i32 0, i32 1
  store i32 0, ptr %2, align 4
  %3 = getelementptr inbounds %struct.My_struct, ptr %instance, i32 0, i32 2
  store i32 0, ptr %3, align 4
  %4 = getelementptr inbounds %struct.My_struct, ptr %instance, i32 0, i32 3
  store i32 0, ptr %4, align 4
  call void @llvm.memcpy.p0.p0.i64(ptr align 4 %0, ptr align 4 %instance, i64 16, i1 false)
  call void @foo(ptr noundef %0)
  ret void
}

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: readwrite)
declare void @llvm.memcpy.p0.p0.i64(ptr noalias nocapture writeonly, ptr noalias nocapture readonly, i64, i1 immarg) #1

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
attributes #1 = { nocallback nofree nounwind willreturn memory(argmem: readwrite) }
)";

    test_c_interoperability_call_function_with_struct_argument("x86_64-pc-windows-msvc", expected_llvm_ir);
  }

  void test_c_interoperability_definition_of_function_with_struct_argument(
    std::string_view const target_triple,
    std::string_view const expected_llvm_ir
  )
  {
    char const* const input_file = "c_interoperability_define_function_with_struct.iris";

    std::filesystem::path const root_directory_path = std::filesystem::temp_directory_path() / "c_interoperability_define_function_with_struct";
    std::filesystem::create_directories(root_directory_path);

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map{};

    Test_options const test_options
    {
      .target_triple = target_triple,
    };

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir, test_options);
  }

  TEST_CASE("C Interoperability - Definition of function with struct argument x86_64-pc-linux-gnu", "[LLVM_IR]")
  {
    char const* const expected_llvm_ir = R"(
%struct.c_interoperability_My_struct = type { i32, i32, i32, i32 }

; Function Attrs: convergent
define private i32 @c_interoperability_add_all(i64 %"arguments[0].instance_0", i64 %"arguments[0].instance_1") #0 {
entry:
  %instance = alloca %struct.c_interoperability_My_struct, align 4
  %0 = getelementptr inbounds { i64, i64 }, ptr %instance, i32 0, i32 0
  store i64 %"arguments[0].instance_0", ptr %0, align 4
  %1 = getelementptr inbounds { i64, i64 }, ptr %instance, i32 0, i32 1
  store i64 %"arguments[0].instance_1", ptr %1, align 4
  %2 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %instance, i32 0, i32 0
  %3 = load i32, ptr %2, align 4
  %4 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %instance, i32 0, i32 1
  %5 = load i32, ptr %4, align 4
  %6 = add i32 %3, %5
  %7 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %instance, i32 0, i32 2
  %8 = load i32, ptr %7, align 4
  %9 = add i32 %6, %8
  %10 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %instance, i32 0, i32 3
  %11 = load i32, ptr %10, align 4
  %12 = add i32 %9, %11
  ret i32 %12
}

; Function Attrs: convergent
define private i32 @c_interoperability_run() #0 {
entry:
  %instance = alloca %struct.c_interoperability_My_struct, align 4
  %0 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %instance, i32 0, i32 0
  store i32 0, ptr %0, align 4
  %1 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %instance, i32 0, i32 1
  store i32 0, ptr %1, align 4
  %2 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %instance, i32 0, i32 2
  store i32 0, ptr %2, align 4
  %3 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %instance, i32 0, i32 3
  store i32 0, ptr %3, align 4
  %4 = getelementptr inbounds { i64, i64 }, ptr %instance, i32 0, i32 0
  %5 = load i64, ptr %4, align 4
  %6 = getelementptr inbounds { i64, i64 }, ptr %instance, i32 0, i32 1
  %7 = load i64, ptr %6, align 4
  %8 = call i32 @c_interoperability_add_all(i64 %5, i64 %7)
  ret i32 %8
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_c_interoperability_definition_of_function_with_struct_argument("x86_64-pc-linux-gnu", expected_llvm_ir);
  }

  TEST_CASE("C Interoperability - Definition of function with struct argument x86_64-pc-windows-msvc", "[LLVM_IR]")
  {
    char const* const expected_llvm_ir = R"(
%struct.c_interoperability_My_struct = type { i32, i32, i32, i32 }

; Function Attrs: convergent
define private i32 @c_interoperability_add_all(ptr noundef %"arguments[0].instance") #0 {
entry:
  %0 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %"arguments[0].instance", i32 0, i32 0
  %1 = load i32, ptr %0, align 4
  %2 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %"arguments[0].instance", i32 0, i32 1
  %3 = load i32, ptr %2, align 4
  %4 = add i32 %1, %3
  %5 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %"arguments[0].instance", i32 0, i32 2
  %6 = load i32, ptr %5, align 4
  %7 = add i32 %4, %6
  %8 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %"arguments[0].instance", i32 0, i32 3
  %9 = load i32, ptr %8, align 4
  %10 = add i32 %7, %9
  ret i32 %10
}

; Function Attrs: convergent
define private i32 @c_interoperability_run() #0 {
entry:
  %instance = alloca %struct.c_interoperability_My_struct, align 4
  %0 = alloca %struct.c_interoperability_My_struct, align 4
  %1 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %instance, i32 0, i32 0
  store i32 0, ptr %1, align 4
  %2 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %instance, i32 0, i32 1
  store i32 0, ptr %2, align 4
  %3 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %instance, i32 0, i32 2
  store i32 0, ptr %3, align 4
  %4 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %instance, i32 0, i32 3
  store i32 0, ptr %4, align 4
  call void @llvm.memcpy.p0.p0.i64(ptr align 4 %0, ptr align 4 %instance, i64 16, i1 false)
  %5 = call i32 @c_interoperability_add_all(ptr noundef %0)
  ret i32 %5
}

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: readwrite)
declare void @llvm.memcpy.p0.p0.i64(ptr noalias nocapture writeonly, ptr noalias nocapture readonly, i64, i1 immarg) #1

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
attributes #1 = { nocallback nofree nounwind willreturn memory(argmem: readwrite) }
)";

    test_c_interoperability_definition_of_function_with_struct_argument("x86_64-pc-windows-msvc", expected_llvm_ir);
  }

  void test_c_interoperability_common(
    std::string_view const input_file,
    std::string_view const target_triple,
    std::string_view const expected_llvm_ir
  )
  {
    std::string_view const directory_name = input_file.substr(0, input_file.find_last_of('.'));
    std::filesystem::path const root_directory_path = std::filesystem::temp_directory_path() / directory_name;
    std::filesystem::create_directories(root_directory_path);

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map{};

    Test_options const test_options
    {
      .target_triple = target_triple,
    };

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir, test_options);
  }

  TEST_CASE("C Interoperability - Call function that returns c bool x86_64-pc-linux-gnu", "[LLVM_IR]")
  {
    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define private i1 @c_interoperability_initialize(i1 noundef zeroext %"arguments[0].first", i1 noundef zeroext %"arguments[1].second") #0 {
entry:
  %first = alloca i8, align 1
  %second = alloca i8, align 1
  %0 = zext i1 %"arguments[0].first" to i8
  store i8 %0, ptr %first, align 1
  %1 = zext i1 %"arguments[1].second" to i8
  store i8 %1, ptr %second, align 1
  ret i1 true
}

; Function Attrs: convergent
define private void @c_interoperability_run(i1 noundef zeroext %"arguments[0].parameter") #0 {
entry:
  %parameter = alloca i8, align 1
  %first = alloca i8, align 1
  %result = alloca i8, align 1
  %0 = zext i1 %"arguments[0].parameter" to i8
  store i8 %0, ptr %parameter, align 1
  store i8 1, ptr %first, align 1
  %1 = load i8, ptr %first, align 1
  %2 = trunc i8 %1 to i1
  %3 = load i8, ptr %parameter, align 1
  %4 = trunc i8 %3 to i1
  %5 = call i1 @c_interoperability_initialize(i1 noundef zeroext %2, i1 noundef zeroext %4)
  %6 = zext i1 %5 to i8
  store i8 %6, ptr %result, align 1
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_c_interoperability_common("c_interoperability_call_function_that_returns_bool.iris", "x86_64-pc-linux-gnu", expected_llvm_ir);
  }

  TEST_CASE("C Interoperability - Call function that returns c bool x86_64-pc-windows-msvc", "[LLVM_IR]")
  {
    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define private i1 @c_interoperability_initialize(i1 noundef zeroext %"arguments[0].first", i1 noundef zeroext %"arguments[1].second") #0 {
entry:
  %first = alloca i8, align 1
  %second = alloca i8, align 1
  %0 = zext i1 %"arguments[0].first" to i8
  store i8 %0, ptr %first, align 1
  %1 = zext i1 %"arguments[1].second" to i8
  store i8 %1, ptr %second, align 1
  ret i1 true
}

; Function Attrs: convergent
define private void @c_interoperability_run(i1 noundef zeroext %"arguments[0].parameter") #0 {
entry:
  %parameter = alloca i8, align 1
  %first = alloca i8, align 1
  %result = alloca i8, align 1
  %0 = zext i1 %"arguments[0].parameter" to i8
  store i8 %0, ptr %parameter, align 1
  store i8 1, ptr %first, align 1
  %1 = load i8, ptr %first, align 1
  %2 = trunc i8 %1 to i1
  %3 = load i8, ptr %parameter, align 1
  %4 = trunc i8 %3 to i1
  %5 = call i1 @c_interoperability_initialize(i1 noundef zeroext %2, i1 noundef zeroext %4)
  %6 = zext i1 %5 to i8
  store i8 %6, ptr %result, align 1
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_c_interoperability_common("c_interoperability_call_function_that_returns_bool.iris", "x86_64-pc-windows-msvc", expected_llvm_ir);
  }

  TEST_CASE("C Interoperability - function_with_big_struct_and_return x86_64-pc-linux-gnu", "[LLVM_IR]")
  {
    char const* const expected_llvm_ir = R"(
%struct.c_interoperability_My_struct = type { i32, i32, i32, i32, i32 }

; Function Attrs: convergent
define private void @c_interoperability_foo(ptr dead_on_unwind noalias writable sret(%struct.c_interoperability_My_struct) align 4 %return.result, ptr noundef byval(%struct.c_interoperability_My_struct) align 8 %"arguments[0].instance") #0 {
entry:
  %value = alloca %struct.c_interoperability_My_struct, align 4
  call void @llvm.memset.p0.i64(ptr align 4 %value, i8 0, i64 20, i1 false)
  call void @llvm.memcpy.p0.p0.i64(ptr align 4 %return.result, ptr align 4 %value, i64 20, i1 false)
  ret void
}

; Function Attrs: convergent
define private void @c_interoperability_run() #0 {
entry:
  %instance = alloca %struct.c_interoperability_My_struct, align 4
  %0 = alloca %struct.c_interoperability_My_struct, align 4
  %result = alloca %struct.c_interoperability_My_struct, align 4
  call void @llvm.memset.p0.i64(ptr align 4 %instance, i8 0, i64 20, i1 false)
  call void @c_interoperability_foo(ptr dead_on_unwind noalias writable sret(%struct.c_interoperability_My_struct) align 4 %0, ptr noundef byval(%struct.c_interoperability_My_struct) align 8 %instance)
  %1 = load %struct.c_interoperability_My_struct, ptr %0, align 4
  store %struct.c_interoperability_My_struct %1, ptr %result, align 4
  ret void
}

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: write)
declare void @llvm.memset.p0.i64(ptr nocapture writeonly, i8, i64, i1 immarg) #1

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: readwrite)
declare void @llvm.memcpy.p0.p0.i64(ptr noalias nocapture writeonly, ptr noalias nocapture readonly, i64, i1 immarg) #2

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
attributes #1 = { nocallback nofree nounwind willreturn memory(argmem: write) }
attributes #2 = { nocallback nofree nounwind willreturn memory(argmem: readwrite) }
)";

    test_c_interoperability_common("c_interoperability_function_with_big_struct_and_return.iris", "x86_64-pc-linux-gnu", expected_llvm_ir);
  }

  TEST_CASE("C Interoperability - function_with_big_struct_and_return x86_64-pc-windows-msvc", "[LLVM_IR]")
  {
    char const* const expected_llvm_ir = R"(
%struct.c_interoperability_My_struct = type { i32, i32, i32, i32, i32 }

; Function Attrs: convergent
define private void @c_interoperability_foo(ptr dead_on_unwind noalias writable align 4 %return.result, ptr noundef %"arguments[0].instance") #0 {
entry:
  %value = alloca %struct.c_interoperability_My_struct, align 4
  call void @llvm.memset.p0.i64(ptr align 4 %value, i8 0, i64 20, i1 false)
  call void @llvm.memcpy.p0.p0.i64(ptr align 4 %return.result, ptr align 4 %value, i64 20, i1 false)
  ret void
}

; Function Attrs: convergent
define private void @c_interoperability_run() #0 {
entry:
  %instance = alloca %struct.c_interoperability_My_struct, align 4
  %0 = alloca %struct.c_interoperability_My_struct, align 4
  %1 = alloca %struct.c_interoperability_My_struct, align 4
  %result = alloca %struct.c_interoperability_My_struct, align 4
  call void @llvm.memset.p0.i64(ptr align 4 %instance, i8 0, i64 20, i1 false)
  call void @llvm.memcpy.p0.p0.i64(ptr align 4 %1, ptr align 4 %instance, i64 20, i1 false)
  call void @c_interoperability_foo(ptr dead_on_unwind noalias writable align 4 %0, ptr noundef %1)
  %2 = load %struct.c_interoperability_My_struct, ptr %0, align 4
  store %struct.c_interoperability_My_struct %2, ptr %result, align 4
  ret void
}

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: write)
declare void @llvm.memset.p0.i64(ptr nocapture writeonly, i8, i64, i1 immarg) #1

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: readwrite)
declare void @llvm.memcpy.p0.p0.i64(ptr noalias nocapture writeonly, ptr noalias nocapture readonly, i64, i1 immarg) #2

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
attributes #1 = { nocallback nofree nounwind willreturn memory(argmem: write) }
attributes #2 = { nocallback nofree nounwind willreturn memory(argmem: readwrite) }
)";

    test_c_interoperability_common("c_interoperability_function_with_big_struct_and_return.iris", "x86_64-pc-windows-msvc", expected_llvm_ir);
  }
  
  TEST_CASE("C Interoperability - function_return_big_struct x86_64-pc-linux-gnu", "[LLVM_IR]")
  {
    char const* const expected_llvm_ir = R"(
%struct.c_interoperability_My_struct = type { i32, i32, i32, i32, i32 }

; Function Attrs: convergent
define private void @c_interoperability_foo(ptr dead_on_unwind noalias writable sret(%struct.c_interoperability_My_struct) align 4 %return.result) #0 {
entry:
  %0 = alloca %struct.c_interoperability_My_struct, align 4
  %1 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %0, i32 0, i32 0
  store i32 0, ptr %1, align 4
  %2 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %0, i32 0, i32 1
  store i32 0, ptr %2, align 4
  %3 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %0, i32 0, i32 2
  store i32 0, ptr %3, align 4
  %4 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %0, i32 0, i32 3
  store i32 0, ptr %4, align 4
  %5 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %0, i32 0, i32 4
  store i32 0, ptr %5, align 4
  call void @llvm.memcpy.p0.p0.i64(ptr align 4 %return.result, ptr align 4 %0, i64 20, i1 false)
  ret void
}

; Function Attrs: convergent
define private void @c_interoperability_run() #0 {
entry:
  %0 = alloca %struct.c_interoperability_My_struct, align 4
  %instance = alloca %struct.c_interoperability_My_struct, align 4
  call void @c_interoperability_foo(ptr dead_on_unwind noalias writable sret(%struct.c_interoperability_My_struct) align 4 %0)
  %1 = load %struct.c_interoperability_My_struct, ptr %0, align 4
  store %struct.c_interoperability_My_struct %1, ptr %instance, align 4
  ret void
}

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: readwrite)
declare void @llvm.memcpy.p0.p0.i64(ptr noalias nocapture writeonly, ptr noalias nocapture readonly, i64, i1 immarg) #1

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
attributes #1 = { nocallback nofree nounwind willreturn memory(argmem: readwrite) }
)";

    test_c_interoperability_common("c_interoperability_function_return_big_struct.iris", "x86_64-pc-linux-gnu", expected_llvm_ir);
  }

  TEST_CASE("C Interoperability - function_return_big_struct x86_64-pc-windows-msvc", "[LLVM_IR]")
  {
    char const* const expected_llvm_ir = R"(
%struct.c_interoperability_My_struct = type { i32, i32, i32, i32, i32 }

; Function Attrs: convergent
define private void @c_interoperability_foo(ptr dead_on_unwind noalias writable align 4 %return.result) #0 {
entry:
  %0 = alloca %struct.c_interoperability_My_struct, align 4
  %1 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %0, i32 0, i32 0
  store i32 0, ptr %1, align 4
  %2 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %0, i32 0, i32 1
  store i32 0, ptr %2, align 4
  %3 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %0, i32 0, i32 2
  store i32 0, ptr %3, align 4
  %4 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %0, i32 0, i32 3
  store i32 0, ptr %4, align 4
  %5 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %0, i32 0, i32 4
  store i32 0, ptr %5, align 4
  call void @llvm.memcpy.p0.p0.i64(ptr align 4 %return.result, ptr align 4 %0, i64 20, i1 false)
  ret void
}

; Function Attrs: convergent
define private void @c_interoperability_run() #0 {
entry:
  %0 = alloca %struct.c_interoperability_My_struct, align 4
  %instance = alloca %struct.c_interoperability_My_struct, align 4
  call void @c_interoperability_foo(ptr dead_on_unwind noalias writable align 4 %0)
  %1 = load %struct.c_interoperability_My_struct, ptr %0, align 4
  store %struct.c_interoperability_My_struct %1, ptr %instance, align 4
  ret void
}

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: readwrite)
declare void @llvm.memcpy.p0.p0.i64(ptr noalias nocapture writeonly, ptr noalias nocapture readonly, i64, i1 immarg) #1

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
attributes #1 = { nocallback nofree nounwind willreturn memory(argmem: readwrite) }
)";

    test_c_interoperability_common("c_interoperability_function_return_big_struct.iris", "x86_64-pc-windows-msvc", expected_llvm_ir);
  }

  TEST_CASE("C Interoperability - function_bool x86_64-pc-linux-gnu", "[LLVM_IR]")
  {
    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define private i1 @c_interoperability_return_bool() #0 {
entry:
  ret i1 true
}

; Function Attrs: convergent
define private void @c_interoperability_take_bool(i1 noundef zeroext %"arguments[0].value") #0 {
entry:
  %value = alloca i8, align 1
  %0 = zext i1 %"arguments[0].value" to i8
  store i8 %0, ptr %value, align 1
  ret void
}

; Function Attrs: convergent
define private i1 @c_interoperability_take_bool_pointer(ptr noundef %"arguments[0].value") #0 {
entry:
  %value = alloca ptr, align 8
  store ptr %"arguments[0].value", ptr %value, align 8
  %0 = load ptr, ptr %value, align 8
  %array_element_pointer = getelementptr i8, ptr %0, i32 1
  %1 = load i8, ptr %array_element_pointer, align 1
  %2 = trunc i8 %1 to i1
  ret i1 %2
}

; Function Attrs: convergent
define private void @c_interoperability_run() #0 {
entry:
  %value = alloca i8, align 1
  %0 = call i1 @c_interoperability_return_bool()
  %1 = zext i1 %0 to i8
  store i8 %1, ptr %value, align 1
  %2 = load i8, ptr %value, align 1
  %3 = trunc i8 %2 to i1
  call void @c_interoperability_take_bool(i1 noundef zeroext %3)
  %4 = call i1 @c_interoperability_take_bool_pointer(ptr noundef %value)
  %5 = zext i1 %4 to i8
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_c_interoperability_common("c_interoperability_function_bool.iris", "x86_64-pc-linux-gnu", expected_llvm_ir);
  }

  TEST_CASE("C Interoperability - function_bool x86_64-pc-windows-msvc", "[LLVM_IR]")
  {
    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define private i1 @c_interoperability_return_bool() #0 {
entry:
  ret i1 true
}

; Function Attrs: convergent
define private void @c_interoperability_take_bool(i1 noundef zeroext %"arguments[0].value") #0 {
entry:
  %value = alloca i8, align 1
  %0 = zext i1 %"arguments[0].value" to i8
  store i8 %0, ptr %value, align 1
  ret void
}

; Function Attrs: convergent
define private i1 @c_interoperability_take_bool_pointer(ptr noundef %"arguments[0].value") #0 {
entry:
  %value = alloca ptr, align 8
  store ptr %"arguments[0].value", ptr %value, align 8
  %0 = load ptr, ptr %value, align 8
  %array_element_pointer = getelementptr i8, ptr %0, i32 1
  %1 = load i8, ptr %array_element_pointer, align 1
  %2 = trunc i8 %1 to i1
  ret i1 %2
}

; Function Attrs: convergent
define private void @c_interoperability_run() #0 {
entry:
  %value = alloca i8, align 1
  %0 = call i1 @c_interoperability_return_bool()
  %1 = zext i1 %0 to i8
  store i8 %1, ptr %value, align 1
  %2 = load i8, ptr %value, align 1
  %3 = trunc i8 %2 to i1
  call void @c_interoperability_take_bool(i1 noundef zeroext %3)
  %4 = call i1 @c_interoperability_take_bool_pointer(ptr noundef %value)
  %5 = zext i1 %4 to i8
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_c_interoperability_common("c_interoperability_function_bool.iris", "x86_64-pc-windows-msvc", expected_llvm_ir);
  }
  
  TEST_CASE("C Interoperability - function_return_empty_struct x86_64-pc-linux-gnu", "[LLVM_IR]")
  {
    char const* const expected_llvm_ir = R"(
%struct.c_interoperability_My_struct = type {}

; Function Attrs: convergent
define private void @c_interoperability_foo() #0 {
entry:
  %0 = alloca %struct.c_interoperability_My_struct, align 1
  ret void
}

; Function Attrs: convergent
define private void @c_interoperability_run() #0 {
entry:
  call void @c_interoperability_foo()
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_c_interoperability_common("c_interoperability_function_return_empty_struct.iris", "x86_64-pc-linux-gnu", expected_llvm_ir);
  }

  TEST_CASE("C Interoperability - function_return_empty_struct x86_64-pc-windows-msvc", "[LLVM_IR]")
  {
    char const* const expected_llvm_ir = R"(
%struct.c_interoperability_My_struct = type { [4 x i8] }

; Function Attrs: convergent
define private i32 @c_interoperability_foo() #0 {
entry:
  %0 = alloca %struct.c_interoperability_My_struct, align 1
  %1 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %0, i32 0, i32 0
  %2 = load i32, ptr %1, align 1
  ret i32 %2
}

; Function Attrs: convergent
define private void @c_interoperability_run() #0 {
entry:
  %0 = alloca %struct.c_interoperability_My_struct, align 1
  %instance = alloca %struct.c_interoperability_My_struct, align 1
  %1 = call i32 @c_interoperability_foo()
  %2 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %0, i32 0, i32 0
  store i32 %1, ptr %2, align 1
  %3 = load %struct.c_interoperability_My_struct, ptr %0, align 1
  store %struct.c_interoperability_My_struct %3, ptr %instance, align 1
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_c_interoperability_common("c_interoperability_function_return_empty_struct.iris", "x86_64-pc-windows-msvc", expected_llvm_ir);
  }
  
  TEST_CASE("C Interoperability - function_return_int x86_64-pc-linux-gnu", "[LLVM_IR]")
  {
    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define private i32 @c_interoperability_foo() #0 {
entry:
  ret i32 0
}

; Function Attrs: convergent
define private void @c_interoperability_run() #0 {
entry:
  %value = alloca i32, align 4
  %0 = call i32 @c_interoperability_foo()
  store i32 %0, ptr %value, align 4
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_c_interoperability_common("c_interoperability_function_return_int.iris", "x86_64-pc-linux-gnu", expected_llvm_ir);
  }

  TEST_CASE("C Interoperability - function_return_int x86_64-pc-windows-msvc", "[LLVM_IR]")
  {
    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define private i32 @c_interoperability_foo() #0 {
entry:
  ret i32 0
}

; Function Attrs: convergent
define private void @c_interoperability_run() #0 {
entry:
  %value = alloca i32, align 4
  %0 = call i32 @c_interoperability_foo()
  store i32 %0, ptr %value, align 4
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_c_interoperability_common("c_interoperability_function_return_int.iris", "x86_64-pc-windows-msvc", expected_llvm_ir);
  }
  
  TEST_CASE("C Interoperability - function_return_pointer x86_64-pc-linux-gnu", "[LLVM_IR]")
  {
    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define private ptr @c_interoperability_foo() #0 {
entry:
  ret ptr null
}

; Function Attrs: convergent
define private void @c_interoperability_run() #0 {
entry:
  %value = alloca ptr, align 8
  %0 = call ptr @c_interoperability_foo()
  store ptr %0, ptr %value, align 8
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_c_interoperability_common("c_interoperability_function_return_pointer.iris", "x86_64-pc-linux-gnu", expected_llvm_ir);
  }

  TEST_CASE("C Interoperability - function_return_pointer x86_64-pc-windows-msvc", "[LLVM_IR]")
  {
    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define private ptr @c_interoperability_foo() #0 {
entry:
  ret ptr null
}

; Function Attrs: convergent
define private void @c_interoperability_run() #0 {
entry:
  %value = alloca ptr, align 8
  %0 = call ptr @c_interoperability_foo()
  store ptr %0, ptr %value, align 8
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_c_interoperability_common("c_interoperability_function_return_pointer.iris", "x86_64-pc-windows-msvc", expected_llvm_ir);
  }
  
  TEST_CASE("C Interoperability - function_return_small_struct x86_64-pc-linux-gnu", "[LLVM_IR]")
  {
    char const* const expected_llvm_ir = R"(
%struct.c_interoperability_My_struct = type { i32, i32, i32, i32 }

; Function Attrs: convergent
define private { i64, i64 } @c_interoperability_foo() #0 {
entry:
  %0 = alloca %struct.c_interoperability_My_struct, align 4
  %1 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %0, i32 0, i32 0
  store i32 0, ptr %1, align 4
  %2 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %0, i32 0, i32 1
  store i32 0, ptr %2, align 4
  %3 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %0, i32 0, i32 2
  store i32 0, ptr %3, align 4
  %4 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %0, i32 0, i32 3
  store i32 0, ptr %4, align 4
  %5 = load { i64, i64 }, ptr %0, align 4
  ret { i64, i64 } %5
}

; Function Attrs: convergent
define private void @c_interoperability_run() #0 {
entry:
  %0 = alloca %struct.c_interoperability_My_struct, align 4
  %instance = alloca %struct.c_interoperability_My_struct, align 4
  %1 = call { i64, i64 } @c_interoperability_foo()
  %2 = getelementptr inbounds { i64, i64 }, ptr %0, i32 0, i32 0
  %3 = extractvalue { i64, i64 } %1, 0
  store i64 %3, ptr %2, align 4
  %4 = getelementptr inbounds { i64, i64 }, ptr %0, i32 0, i32 1
  %5 = extractvalue { i64, i64 } %1, 1
  store i64 %5, ptr %4, align 4
  %6 = load %struct.c_interoperability_My_struct, ptr %0, align 4
  store %struct.c_interoperability_My_struct %6, ptr %instance, align 4
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_c_interoperability_common("c_interoperability_function_return_small_struct.iris", "x86_64-pc-linux-gnu", expected_llvm_ir);
  }

  TEST_CASE("C Interoperability - function_return_small_struct x86_64-pc-windows-msvc", "[LLVM_IR]")
  {
    char const* const expected_llvm_ir = R"(
%struct.c_interoperability_My_struct = type { i32, i32, i32, i32 }

; Function Attrs: convergent
define private void @c_interoperability_foo(ptr dead_on_unwind noalias writable align 4 %return.result) #0 {
entry:
  %0 = alloca %struct.c_interoperability_My_struct, align 4
  %1 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %0, i32 0, i32 0
  store i32 0, ptr %1, align 4
  %2 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %0, i32 0, i32 1
  store i32 0, ptr %2, align 4
  %3 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %0, i32 0, i32 2
  store i32 0, ptr %3, align 4
  %4 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %0, i32 0, i32 3
  store i32 0, ptr %4, align 4
  call void @llvm.memcpy.p0.p0.i64(ptr align 4 %return.result, ptr align 4 %0, i64 16, i1 false)
  ret void
}

; Function Attrs: convergent
define private void @c_interoperability_run() #0 {
entry:
  %0 = alloca %struct.c_interoperability_My_struct, align 4
  %instance = alloca %struct.c_interoperability_My_struct, align 4
  call void @c_interoperability_foo(ptr dead_on_unwind noalias writable align 4 %0)
  %1 = load %struct.c_interoperability_My_struct, ptr %0, align 4
  store %struct.c_interoperability_My_struct %1, ptr %instance, align 4
  ret void
}

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: readwrite)
declare void @llvm.memcpy.p0.p0.i64(ptr noalias nocapture writeonly, ptr noalias nocapture readonly, i64, i1 immarg) #1

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
attributes #1 = { nocallback nofree nounwind willreturn memory(argmem: readwrite) }
)";

    test_c_interoperability_common("c_interoperability_function_return_small_struct.iris", "x86_64-pc-windows-msvc", expected_llvm_ir);
  }

  TEST_CASE("C Interoperability - function_with_array_slice x86_64-pc-linux-gnu", "[LLVM_IR]")
  {
    char const* const expected_llvm_ir = R"(
%struct.iris_builtin_Generic_array_slice = type { ptr, i64 }

; Function Attrs: convergent
define private void @c_interoperability_take(ptr %"arguments[0].a0_0", i64 %"arguments[0].a0_1") #0 {
entry:
  %a0 = alloca %struct.iris_builtin_Generic_array_slice, align 8
  %0 = getelementptr inbounds { ptr, i64 }, ptr %a0, i32 0, i32 0
  store ptr %"arguments[0].a0_0", ptr %0, align 8
  %1 = getelementptr inbounds { ptr, i64 }, ptr %a0, i32 0, i32 1
  store i64 %"arguments[0].a0_1", ptr %1, align 8
  ret void
}

; Function Attrs: convergent
define private void @c_interoperability_run() #0 {
entry:
  %v0 = alloca i32, align 4
  %array = alloca [1 x ptr], i64 1, align 8
  %array1 = alloca [1 x ptr], align 8
  %0 = alloca %struct.iris_builtin_Generic_array_slice, align 8
  store i32 0, ptr %v0, align 4
  %array_element_pointer = getelementptr [1 x ptr], ptr %array, i32 0, i32 0
  store ptr %v0, ptr %array_element_pointer, align 8
  %1 = load [1 x ptr], ptr %array, align 8
  store [1 x ptr] %1, ptr %array1, align 8
  %data_pointer = getelementptr [1 x ptr], ptr %array1, i32 0, i32 0
  %2 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %0, i32 0, i32 0
  store ptr %data_pointer, ptr %2, align 8
  %3 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %0, i32 0, i32 1
  store i64 1, ptr %3, align 8
  %4 = getelementptr inbounds { ptr, i64 }, ptr %0, i32 0, i32 0
  %5 = load ptr, ptr %4, align 8
  %6 = getelementptr inbounds { ptr, i64 }, ptr %0, i32 0, i32 1
  %7 = load i64, ptr %6, align 8
  call void @c_interoperability_take(ptr %5, i64 %7)
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_c_interoperability_common("c_interoperability_function_with_array_slice.iris", "x86_64-pc-linux-gnu", expected_llvm_ir);
  }

  TEST_CASE("C Interoperability - function_with_array_slice x86_64-pc-windows-msvc", "[LLVM_IR]")
  {
    char const* const expected_llvm_ir = R"(
%struct.iris_builtin_Generic_array_slice = type { ptr, i64 }

; Function Attrs: convergent
define private void @c_interoperability_take(ptr noundef %"arguments[0].a0") #0 {
entry:
  ret void
}

; Function Attrs: convergent
define private void @c_interoperability_run() #0 {
entry:
  %v0 = alloca i32, align 4
  %array = alloca [1 x ptr], i64 1, align 8
  %array1 = alloca [1 x ptr], align 8
  %0 = alloca %struct.iris_builtin_Generic_array_slice, align 8
  %1 = alloca %struct.iris_builtin_Generic_array_slice, align 8
  store i32 0, ptr %v0, align 4
  %array_element_pointer = getelementptr [1 x ptr], ptr %array, i32 0, i32 0
  store ptr %v0, ptr %array_element_pointer, align 8
  %2 = load [1 x ptr], ptr %array, align 8
  store [1 x ptr] %2, ptr %array1, align 8
  %data_pointer = getelementptr [1 x ptr], ptr %array1, i32 0, i32 0
  %3 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %0, i32 0, i32 0
  store ptr %data_pointer, ptr %3, align 8
  %4 = getelementptr inbounds %struct.iris_builtin_Generic_array_slice, ptr %0, i32 0, i32 1
  store i64 1, ptr %4, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr align 8 %1, ptr align 8 %0, i64 16, i1 false)
  call void @c_interoperability_take(ptr noundef %1)
  ret void
}

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: readwrite)
declare void @llvm.memcpy.p0.p0.i64(ptr noalias nocapture writeonly, ptr noalias nocapture readonly, i64, i1 immarg) #1

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
attributes #1 = { nocallback nofree nounwind willreturn memory(argmem: readwrite) }
)";

    test_c_interoperability_common("c_interoperability_function_with_array_slice.iris", "x86_64-pc-windows-msvc", expected_llvm_ir);
  }
  
  TEST_CASE("C Interoperability - function_with_big_struct x86_64-pc-linux-gnu", "[LLVM_IR]")
  {
    char const* const expected_llvm_ir = R"(
%struct.c_interoperability_My_struct = type { i32, i32, i32, i32, i32 }

; Function Attrs: convergent
define private void @c_interoperability_foo(ptr noundef byval(%struct.c_interoperability_My_struct) align 8 %"arguments[0].instance") #0 {
entry:
  ret void
}

; Function Attrs: convergent
define private void @c_interoperability_run() #0 {
entry:
  %instance = alloca %struct.c_interoperability_My_struct, align 4
  %0 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %instance, i32 0, i32 0
  store i32 0, ptr %0, align 4
  %1 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %instance, i32 0, i32 1
  store i32 0, ptr %1, align 4
  %2 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %instance, i32 0, i32 2
  store i32 0, ptr %2, align 4
  %3 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %instance, i32 0, i32 3
  store i32 0, ptr %3, align 4
  %4 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %instance, i32 0, i32 4
  store i32 0, ptr %4, align 4
  call void @c_interoperability_foo(ptr noundef byval(%struct.c_interoperability_My_struct) align 8 %instance)
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_c_interoperability_common("c_interoperability_function_with_big_struct.iris", "x86_64-pc-linux-gnu", expected_llvm_ir);
  }

  TEST_CASE("C Interoperability - function_with_big_struct x86_64-pc-windows-msvc", "[LLVM_IR]")
  {
    char const* const expected_llvm_ir = R"(
%struct.c_interoperability_My_struct = type { i32, i32, i32, i32, i32 }

; Function Attrs: convergent
define private void @c_interoperability_foo(ptr noundef %"arguments[0].instance") #0 {
entry:
  ret void
}

; Function Attrs: convergent
define private void @c_interoperability_run() #0 {
entry:
  %instance = alloca %struct.c_interoperability_My_struct, align 4
  %0 = alloca %struct.c_interoperability_My_struct, align 4
  %1 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %instance, i32 0, i32 0
  store i32 0, ptr %1, align 4
  %2 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %instance, i32 0, i32 1
  store i32 0, ptr %2, align 4
  %3 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %instance, i32 0, i32 2
  store i32 0, ptr %3, align 4
  %4 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %instance, i32 0, i32 3
  store i32 0, ptr %4, align 4
  %5 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %instance, i32 0, i32 4
  store i32 0, ptr %5, align 4
  call void @llvm.memcpy.p0.p0.i64(ptr align 4 %0, ptr align 4 %instance, i64 20, i1 false)
  call void @c_interoperability_foo(ptr noundef %0)
  ret void
}

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: readwrite)
declare void @llvm.memcpy.p0.p0.i64(ptr noalias nocapture writeonly, ptr noalias nocapture readonly, i64, i1 immarg) #1

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
attributes #1 = { nocallback nofree nounwind willreturn memory(argmem: readwrite) }
)";

    test_c_interoperability_common("c_interoperability_function_with_big_struct.iris", "x86_64-pc-windows-msvc", expected_llvm_ir);
  }
  
  TEST_CASE("C Interoperability - function_with_empty_struct x86_64-pc-linux-gnu", "[LLVM_IR]")
  {
    char const* const expected_llvm_ir = R"(
%struct.c_interoperability_My_struct = type {}

; Function Attrs: convergent
define private void @c_interoperability_foo() #0 {
entry:
  ret void
}

; Function Attrs: convergent
define private void @c_interoperability_run() #0 {
entry:
  %instance = alloca %struct.c_interoperability_My_struct, align 1
  call void @c_interoperability_foo()
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_c_interoperability_common("c_interoperability_function_with_empty_struct.iris", "x86_64-pc-linux-gnu", expected_llvm_ir);
  }

  TEST_CASE("C Interoperability - function_with_empty_struct x86_64-pc-windows-msvc", "[LLVM_IR]")
  {
    char const* const expected_llvm_ir = R"(
%struct.c_interoperability_My_struct = type { [4 x i8] }

; Function Attrs: convergent
define private void @c_interoperability_foo(i32 noundef %"arguments[0].instance") #0 {
entry:
  %0 = alloca %struct.c_interoperability_My_struct, align 1
  %1 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %0, i32 0, i32 0
  store i32 %"arguments[0].instance", ptr %1, align 1
  ret void
}

; Function Attrs: convergent
define private void @c_interoperability_run() #0 {
entry:
  %instance = alloca %struct.c_interoperability_My_struct, align 1
  %0 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %instance, i32 0, i32 0
  %1 = load i32, ptr %0, align 1
  call void @c_interoperability_foo(i32 noundef %1)
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_c_interoperability_common("c_interoperability_function_with_empty_struct.iris", "x86_64-pc-windows-msvc", expected_llvm_ir);
  }
  
  TEST_CASE("C Interoperability - function_with_int_arguments x86_64-pc-linux-gnu", "[LLVM_IR]")
  {
    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define private void @c_interoperability_foo(i32 noundef %"arguments[0].a", i32 noundef %"arguments[1].b") #0 {
entry:
  %a = alloca i32, align 4
  %b = alloca i32, align 4
  store i32 %"arguments[0].a", ptr %a, align 4
  store i32 %"arguments[1].b", ptr %b, align 4
  ret void
}

; Function Attrs: convergent
define private void @c_interoperability_run() #0 {
entry:
  call void @c_interoperability_foo(i32 noundef 0, i32 noundef 0)
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_c_interoperability_common("c_interoperability_function_with_int_arguments.iris", "x86_64-pc-linux-gnu", expected_llvm_ir);
  }

  TEST_CASE("C Interoperability - function_with_int_arguments x86_64-pc-windows-msvc", "[LLVM_IR]")
  {
    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define private void @c_interoperability_foo(i32 noundef %"arguments[0].a", i32 noundef %"arguments[1].b") #0 {
entry:
  %a = alloca i32, align 4
  %b = alloca i32, align 4
  store i32 %"arguments[0].a", ptr %a, align 4
  store i32 %"arguments[1].b", ptr %b, align 4
  ret void
}

; Function Attrs: convergent
define private void @c_interoperability_run() #0 {
entry:
  call void @c_interoperability_foo(i32 noundef 0, i32 noundef 0)
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_c_interoperability_common("c_interoperability_function_with_int_arguments.iris", "x86_64-pc-windows-msvc", expected_llvm_ir);
  }
  
  TEST_CASE("C Interoperability - function_with_pointer x86_64-pc-linux-gnu", "[LLVM_IR]")
  {
    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define private void @c_interoperability_foo(ptr noundef %"arguments[0].value") #0 {
entry:
  %value = alloca ptr, align 8
  store ptr %"arguments[0].value", ptr %value, align 8
  ret void
}

; Function Attrs: convergent
define private void @c_interoperability_run() #0 {
entry:
  call void @c_interoperability_foo(ptr noundef null)
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_c_interoperability_common("c_interoperability_function_with_pointer.iris", "x86_64-pc-linux-gnu", expected_llvm_ir);
  }

  TEST_CASE("C Interoperability - function_with_pointer x86_64-pc-windows-msvc", "[LLVM_IR]")
  {
    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define private void @c_interoperability_foo(ptr noundef %"arguments[0].value") #0 {
entry:
  %value = alloca ptr, align 8
  store ptr %"arguments[0].value", ptr %value, align 8
  ret void
}

; Function Attrs: convergent
define private void @c_interoperability_run() #0 {
entry:
  call void @c_interoperability_foo(ptr noundef null)
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_c_interoperability_common("c_interoperability_function_with_pointer.iris", "x86_64-pc-windows-msvc", expected_llvm_ir);
  }
  
  TEST_CASE("C Interoperability - function_with_small_struct x86_64-pc-linux-gnu", "[LLVM_IR]")
  {
    char const* const expected_llvm_ir = R"(
%struct.c_interoperability_My_struct = type { i32, i32, i32, i32 }

; Function Attrs: convergent
define private void @c_interoperability_foo(i64 %"arguments[0].instance_0", i64 %"arguments[0].instance_1") #0 {
entry:
  %instance = alloca %struct.c_interoperability_My_struct, align 4
  %0 = getelementptr inbounds { i64, i64 }, ptr %instance, i32 0, i32 0
  store i64 %"arguments[0].instance_0", ptr %0, align 4
  %1 = getelementptr inbounds { i64, i64 }, ptr %instance, i32 0, i32 1
  store i64 %"arguments[0].instance_1", ptr %1, align 4
  ret void
}

; Function Attrs: convergent
define private void @c_interoperability_run() #0 {
entry:
  %instance = alloca %struct.c_interoperability_My_struct, align 4
  %0 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %instance, i32 0, i32 0
  store i32 0, ptr %0, align 4
  %1 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %instance, i32 0, i32 1
  store i32 0, ptr %1, align 4
  %2 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %instance, i32 0, i32 2
  store i32 0, ptr %2, align 4
  %3 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %instance, i32 0, i32 3
  store i32 0, ptr %3, align 4
  %4 = getelementptr inbounds { i64, i64 }, ptr %instance, i32 0, i32 0
  %5 = load i64, ptr %4, align 4
  %6 = getelementptr inbounds { i64, i64 }, ptr %instance, i32 0, i32 1
  %7 = load i64, ptr %6, align 4
  call void @c_interoperability_foo(i64 %5, i64 %7)
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_c_interoperability_common("c_interoperability_function_with_small_struct.iris", "x86_64-pc-linux-gnu", expected_llvm_ir);
  }

  TEST_CASE("C Interoperability - function_with_small_struct x86_64-pc-windows-msvc", "[LLVM_IR]")
  {
    char const* const expected_llvm_ir = R"(
%struct.c_interoperability_My_struct = type { i32, i32, i32, i32 }

; Function Attrs: convergent
define private void @c_interoperability_foo(ptr noundef %"arguments[0].instance") #0 {
entry:
  ret void
}

; Function Attrs: convergent
define private void @c_interoperability_run() #0 {
entry:
  %instance = alloca %struct.c_interoperability_My_struct, align 4
  %0 = alloca %struct.c_interoperability_My_struct, align 4
  %1 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %instance, i32 0, i32 0
  store i32 0, ptr %1, align 4
  %2 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %instance, i32 0, i32 1
  store i32 0, ptr %2, align 4
  %3 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %instance, i32 0, i32 2
  store i32 0, ptr %3, align 4
  %4 = getelementptr inbounds %struct.c_interoperability_My_struct, ptr %instance, i32 0, i32 3
  store i32 0, ptr %4, align 4
  call void @llvm.memcpy.p0.p0.i64(ptr align 4 %0, ptr align 4 %instance, i64 16, i1 false)
  call void @c_interoperability_foo(ptr noundef %0)
  ret void
}

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: readwrite)
declare void @llvm.memcpy.p0.p0.i64(ptr noalias nocapture writeonly, ptr noalias nocapture readonly, i64, i1 immarg) #1

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
attributes #1 = { nocallback nofree nounwind willreturn memory(argmem: readwrite) }
)";

    test_c_interoperability_common("c_interoperability_function_with_small_struct.iris", "x86_64-pc-windows-msvc", expected_llvm_ir);
  }

  TEST_CASE("Compile Function Constructor Assigned to Global Variable Across Module Boundary", "[LLVM_IR]")
  {
    char const* const input_file = "function_constructor_global_consumer.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
      { "Function_constructor_global_provider", parse_and_get_file_path(g_test_source_files_path / "function_constructor_global_provider.iris") },
    };

    char const* const expected_llvm_ir = R"(
@Function_constructor_global_consumer_bar = constant ptr @Function_constructor_global_provider__at__to_json__at__7179855141281402803

; Function Attrs: convergent
define private void @Function_constructor_global_provider__at__to_json__at__7179855141281402803(ptr noundef %"arguments[0].value") #0 {
entry:
  %value = alloca ptr, align 8
  store ptr %"arguments[0].value", ptr %value, align 8
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile using imported global variable assigned to function constructor instance", "[LLVM_IR]")
  {
    char const* const input_file = "function_constructor_global_consumer_2.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map
    {
      { "Function_constructor_global_provider_2", parse_and_get_file_path(g_test_source_files_path / "function_constructor_global_provider_2.iris") },
    };

    char const* const expected_llvm_ir = R"(
@Function_constructor_global_provider_2_to_json_int32 = external constant ptr

; Function Attrs: convergent
define private void @Function_constructor_global_consumer_2_run() #0 {
entry:
  %a = alloca i32, align 4
  store i32 0, ptr %a, align 4
  %0 = load ptr, ptr @Function_constructor_global_provider_2_to_json_int32, align 8
  call void %0(ptr noundef %a)
  ret void
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile create_soa_array_view_from_pointer", "[LLVM_IR]")
  {
    char const* const input_file = "soa_array_view_from_pointer.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map{};

    char const* const expected_llvm_ir = R"(
%__hl_soa_array_view = type { i64, i64, i64, ptr }

; Function Attrs: convergent
define private void @soa_array_view_from_pointer_create_mutable_view(ptr dead_on_unwind noalias writable sret(%__hl_soa_array_view) align 8 %return.result, ptr noundef %"arguments[0].data", i64 noundef %"arguments[1].length") #0 {
entry:
  %data = alloca ptr, align 8
  %length = alloca i64, align 8
  %soa_array_view = alloca %__hl_soa_array_view, align 8
  store ptr %"arguments[0].data", ptr %data, align 8
  store i64 %"arguments[1].length", ptr %length, align 8
  %0 = load ptr, ptr %data, align 8
  %1 = load i64, ptr %length, align 8
  %2 = getelementptr inbounds %__hl_soa_array_view, ptr %soa_array_view, i32 0, i32 0
  store i64 0, ptr %2, align 8
  %3 = getelementptr inbounds %__hl_soa_array_view, ptr %soa_array_view, i32 0, i32 1
  store i64 %1, ptr %3, align 8
  %4 = getelementptr inbounds %__hl_soa_array_view, ptr %soa_array_view, i32 0, i32 2
  store i64 %1, ptr %4, align 8
  %5 = getelementptr inbounds %__hl_soa_array_view, ptr %soa_array_view, i32 0, i32 3
  store ptr %0, ptr %5, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr align 8 %return.result, ptr align 8 %soa_array_view, i64 32, i1 false)
  ret void
}

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: readwrite)
declare void @llvm.memcpy.p0.p0.i64(ptr noalias nocapture writeonly, ptr noalias nocapture readonly, i64, i1 immarg) #1

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
attributes #1 = { nocallback nofree nounwind willreturn memory(argmem: readwrite) }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }

  TEST_CASE("Compile calculate_soa_array_size_bytes", "[LLVM_IR]")
  {
    char const* const input_file = "calculate_soa_array_size_bytes.iris";

    std::pmr::unordered_map<std::pmr::string, std::filesystem::path> const module_name_to_file_path_map{};

    char const* const expected_llvm_ir = R"(
; Function Attrs: convergent
define private i64 @calculate_soa_array_size_bytes_get_size(i64 noundef %"arguments[0].capacity") #0 {
entry:
  %capacity = alloca i64, align 8
  store i64 %"arguments[0].capacity", ptr %capacity, align 8
  %0 = load i64, ptr %capacity, align 8
  %soa_member_block_size = mul i64 %0, 4
  %soa_member_block_offset = add i64 0, %soa_member_block_size
  %soa_offset_adjusted = add i64 %soa_member_block_offset, 3
  %soa_offset_aligned = and i64 %soa_offset_adjusted, -4
  %soa_member_block_size1 = mul i64 %0, 4
  %soa_member_block_offset2 = add i64 %soa_offset_aligned, %soa_member_block_size1
  ret i64 %soa_member_block_offset2
}

attributes #0 = { convergent "no-trapping-math"="true" "stack-protector-buffer-size"="0" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
)";

    test_create_llvm_module(input_file, module_name_to_file_path_map, expected_llvm_ir);
  }
}



