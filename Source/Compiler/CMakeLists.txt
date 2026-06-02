add_library(Iris_compiler SHARED)
add_library(Iris::Compiler ALIAS Iris_compiler)

target_compile_features(Iris_compiler PRIVATE cxx_std_23)

target_link_libraries(Iris_compiler PRIVATE Iris::Binary_serializer Iris::Core Iris::Common Iris::C_header_exporter Iris::C_header_importer Iris::JSON_serializer Iris::Parser)
target_link_libraries(Iris_compiler PUBLIC clang_target llvm_target)

target_link_libraries(Iris_compiler PUBLIC "wtr.hdr_watcher")

if(WIN32)
   find_package(xxHash CONFIG REQUIRED)
   target_link_libraries(Iris_compiler PRIVATE xxHash::xxhash)
   install(IMPORTED_RUNTIME_ARTIFACTS xxHash::xxhash)
else()
   find_package(PkgConfig REQUIRED)
   pkg_check_modules(XXHASH REQUIRED IMPORTED_TARGET libxxhash)

   target_include_directories(Iris_compiler PRIVATE ${XXHASIris_INCLUDE_DIRS})
   target_link_libraries(Iris_compiler PRIVATE PkgConfig::XXHASH)
endif()

target_sources(Iris_compiler
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
         "Test_framework.cppm"
         "Types.cppm"
         "Validation.cppm"
         "Passes/All_passes.cppm"
         "Passes/Compile_time_pass.cppm"
         "Passes/Implicit_function_pass.cppm"
         "Passes/Instantiate_pass.cppm"
         "Project/Artifact.cppm"
         "Project/Presets.cppm"
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
      "Test_framework.cpp"
      "Types.cpp"
      "Validation.cpp"
      "Passes/All_passes.cpp"
      "Passes/Compile_time_pass.cpp"
      "Passes/Implicit_function_pass.cpp"
      "Passes/Instantiate_pass.cpp"
      "Project/Artifact.cpp"
      "Project/Presets.cpp"
      "Project/Repository.cpp"
)

if(WIN32)
   target_sources(Iris_compiler PRIVATE
      "Linker_coff.cpp"
      "Project/Target_windows.cpp"
   )
else()
   target_sources(Iris_compiler PRIVATE
      "Linker_elf.cpp"
      "Project/Target_linux.cpp"
   )
endif()

target_generate_module_exports(Iris_compiler)
install(TARGETS Iris_compiler)

if(BUILD_TESTING)
   find_package(Catch2 CONFIG REQUIRED)
   include(Catch)

   set(iris_test_link_libraries
      Iris::C_header_importer
      Iris::Compiler
      Iris::JSON_serializer
      Iris::Parser
      Catch2::Catch2
      Catch2::Catch2WithMain
   )

   set(iris_test_compile_definitions
      EXAMPLES_DIRECTORY="${CMAKE_SOURCE_DIR}/Examples"
      TEST_SOURCE_FILES_PATH="${CMAKE_SOURCE_DIR}/Examples/txt"
      TESTS_OUTPUT_DIRECTORY_PATH="${CMAKE_BINARY_DIR}/Temp/Tests/Compiler"
      C_STANDARD_LIBRARY_PATH="${CMAKE_BINARY_DIR}/C_standard_library"
      STANDARD_REPOSITORY_FILE_PATH="${CMAKE_SOURCE_DIR}/share/iris/libraries/iris_repository.json"
   )

   # Builder tests
   add_executable(Iris_builder_tests)
   target_link_libraries(Iris_builder_tests PRIVATE ${iris_test_link_libraries})
   target_compile_definitions(Iris_builder_tests PRIVATE ${iris_test_compile_definitions})
   target_sources(Iris_builder_tests PRIVATE
      "main.tests.cpp"
      "Builder.tests.cpp"
   )
   catch_discover_tests(Iris_builder_tests)

   # Passes tests
   add_executable(Iris_passes_tests)
   target_link_libraries(Iris_passes_tests PRIVATE ${iris_test_link_libraries})
   target_compile_definitions(Iris_passes_tests PRIVATE ${iris_test_compile_definitions})
   target_sources(Iris_passes_tests
      PUBLIC FILE_SET modules TYPE CXX_MODULES
         FILES
            "Passes/Pass_test_helpers.cppm"
      PRIVATE
         "main.tests.cpp"
         "Passes/All_passes.tests.cpp"
         "Passes/Compile_time_pass.tests.cpp"
         "Passes/Instantiate_pass.tests.cpp"
         "Passes/Implicit_function_pass.tests.cpp"
   )
   catch_discover_tests(Iris_passes_tests)

   # Project tests
   add_executable(Iris_project_tests)
   target_link_libraries(Iris_project_tests PRIVATE ${iris_test_link_libraries})
   target_compile_definitions(Iris_project_tests PRIVATE ${iris_test_compile_definitions})
   target_sources(Iris_project_tests PRIVATE
      "main.tests.cpp"
      "Project/Artifacts.tests.cpp"
      "Project/Presets.tests.cpp"
      "Project/Repository.tests.cpp"
   )
   catch_discover_tests(Iris_project_tests)

   # Validation tests
   add_executable(Iris_validation_tests)
   target_link_libraries(Iris_validation_tests PRIVATE ${iris_test_link_libraries})
   target_compile_definitions(Iris_validation_tests PRIVATE ${iris_test_compile_definitions})
   target_sources(Iris_validation_tests PRIVATE
      "main.tests.cpp"
      "Validation.tests.cpp"
   )
   catch_discover_tests(Iris_validation_tests)

   # Compiler tests
   add_executable(Iris_compiler_tests)
   target_link_libraries(Iris_compiler_tests PRIVATE ${iris_test_link_libraries})
   target_compile_definitions(Iris_compiler_tests PRIVATE ${iris_test_compile_definitions})
   target_sources(Iris_compiler_tests PRIVATE
      "main.tests.cpp"
      "Compiler.tests.cpp"
   )
   catch_discover_tests(Iris_compiler_tests)

   # Core Hash tests
   add_executable(Iris_hash_tests)
   target_link_libraries(Iris_hash_tests PRIVATE ${iris_test_link_libraries})
   target_compile_definitions(Iris_hash_tests PRIVATE ${iris_test_compile_definitions})
   target_sources(Iris_hash_tests PRIVATE
      "main.tests.cpp"
      "Hash.tests.cpp"
   )
   catch_discover_tests(Iris_hash_tests)

endif()
