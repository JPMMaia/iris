add_library(H_compiler)
add_library(H::Compiler ALIAS H_compiler)

target_compile_features(H_compiler PRIVATE cxx_std_23)

target_link_libraries(H_compiler PRIVATE H::Binary_serializer H::Core H::Common H::C_header_exporter H::C_header_importer H::JSON_serializer H::Parser)
target_link_libraries(H_compiler PUBLIC clang_target llvm_target)

target_link_libraries(H_compiler PUBLIC "wtr.hdr_watcher")

if(WIN32)
   find_package(xxHash CONFIG REQUIRED)
   target_link_libraries(H_compiler PRIVATE xxHash::xxhash)
   install(IMPORTED_RUNTIME_ARTIFACTS xxHash::xxhash)
else()
   find_package(PkgConfig REQUIRED)
   pkg_check_modules(XXHASH REQUIRED IMPORTED_TARGET libxxhash)

   target_include_directories(H_compiler PRIVATE ${XXHASH_INCLUDE_DIRS})
   target_link_libraries(H_compiler PRIVATE PkgConfig::XXHASH)
endif()

target_sources(H_compiler
   PUBLIC FILE_SET modules TYPE CXX_MODULES
      FILES
         "Analysis.cppm"
         "Builder.cppm"
         "Clang_code_generation.cppm"
         "Clang_compiler.cppm"
         "Clang_data.cppm"
         "Common.cppm"
         "Compile_commands_generator.cppm"
         "Compiler.cppm"
         "Debug_info.cppm"
         "Debug_info_formatter.cppm"
         "Diagnostic.cppm"
         "Expressions.cppm"
         "Instructions.cppm"
         "Linker.cppm"
         "Profiler.cppm"
         "Recompilation.cppm"
         "Test_framework.cppm"
         "Types.cppm"
         "Validation.cppm"
         "JIT/Core_module_layer.cppm"
         "JIT/File_watcher.cppm"
         "JIT/JIT_compiler.cppm"
         "JIT/JIT_runner.cppm"
         "JIT/Recompile_module_layer.cppm"
         "Passes/All_passes.cppm"
         "Passes/Compile_time_pass.cppm"
         "Passes/Implicit_function_pass.cppm"
         "Passes/Instantiate_pass.cppm"
         "Project/Artifact.cppm"
         "Project/Repository.cppm"
         "Project/Target.cppm"
   PRIVATE
      "Analysis.cpp"
      "Builder.cpp"
      "Clang_code_generation.cpp"
      "Clang_compiler.cpp"
      "Common.cpp"
      "Compiler.cpp"
      "Debug_info.cpp"
      "Debug_info_formatter.cpp"
      "Diagnostic.cpp"
      "Expressions.cpp"
      "Instructions.cpp"
      "Recompilation.cpp"
      "Test_framework.cpp"
      "Types.cpp"
      "Validation.cpp"
      "JIT/Core_module_layer.cpp"
      "JIT/File_watcher.cpp"
      "JIT/JIT_compiler.cpp"
      "JIT/JIT_runner.cpp"
      "JIT/Recompile_module_layer.cpp"
      "Passes/All_passes.cpp"
      "Passes/Compile_time_pass.cpp"
      "Passes/Implicit_function_pass.cpp"
      "Passes/Instantiate_pass.cpp"
      "Project/Artifact.cpp"
      "Project/Repository.cpp"
)

if(WIN32)
   target_sources(H_compiler PRIVATE
      "Linker_coff.cpp"
      "Project/Target_windows.cpp"
   )
else()
   target_sources(H_compiler PRIVATE
      "Linker_elf.cpp"
      "Project/Target_linux.cpp"
   )
endif()

if(BUILD_TESTING)
   add_executable(H_compiler_tests)
   target_link_libraries(H_compiler_tests PRIVATE H::C_header_importer H::Compiler H::JSON_serializer H::Parser)

   target_sources(H_compiler_tests
      PUBLIC FILE_SET modules TYPE CXX_MODULES
         FILES
            "passes/Pass_test_helpers.cppm"
   )

   find_package(Catch2 CONFIG REQUIRED)
   target_link_libraries(H_compiler_tests PRIVATE Catch2::Catch2 Catch2::Catch2WithMain)

   target_compile_definitions(
      H_compiler_tests
      PRIVATE
         EXAMPLES_DIRECTORY="${CMAKE_SOURCE_DIR}/Examples"
         TEST_SOURCE_FILES_PATH="${CMAKE_SOURCE_DIR}/Examples/txt"
         TESTS_OUTPUT_DIRECTORY_PATH="${CMAKE_BINARY_DIR}/Temp/Tests/Compiler"
         C_STANDARD_LIBRARY_PATH="${CMAKE_BINARY_DIR}/C_standard_library"
         STANDARD_REPOSITORY_FILE_PATH="${CMAKE_SOURCE_DIR}/share/hlang/libraries/iris_repository.json"
   )

   target_sources(H_compiler_tests PRIVATE
      "main.tests.cpp"
      "Builder.tests.cpp"
      "Compiler.tests.cpp"
      "passes/All_passes.tests.cpp"
      "passes/Compile_time_pass.tests.cpp"
      "passes/Instantiate_pass.tests.cpp"
      "passes/Implicit_function_pass.tests.cpp"
      "Recompilation.tests.cpp"
      "Validation.tests.cpp"
      "JIT/JIT_runner.tests.cpp"
   )

   # add_dependencies(H_compiler_tests C_standard_library)

   include(Catch)
   catch_discover_tests(H_compiler_tests)

endif()
