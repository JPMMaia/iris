add_library(H_compiler)
add_library(H::Compiler ALIAS H_compiler)

target_compile_features(H_compiler PRIVATE cxx_std_23)

target_link_libraries(H_compiler PRIVATE H::Binary_serializer H::Core H::Common H::C_header_exporter H::C_header_importer H::JSON_serializer H::Parser)

# TODO find a way to not hardcode this when installing
set(BUILTIN_SOURCE_FILE_PATH "${CMAKE_CURRENT_LIST_DIR}/Builtin/Builtin.hltxt")
target_compile_definitions(H_compiler PRIVATE BUILTIN_SOURCE_FILE_PATH="${BUILTIN_SOURCE_FILE_PATH}")

## LLVM ##

find_package(LLVM CONFIG REQUIRED)

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(HandleLLVMOptions)
add_definitions(${LLVM_DEFINITIONS})
target_compile_definitions(H_compiler PUBLIC ${LLVM_DEFINITIONS} LLVM_DISABLE_ABI_BREAKING_CHECKS_ENFORCING)

target_include_directories(H_compiler PUBLIC ${LLVM_INCLUDE_DIRS})

if (WIN32)
   llvm_map_components_to_libnames(
      llvm_libs
      BinaryFormat
      Core
      LibDriver
      LTO
      MC
      nativecodegen
      OrcDebugging
      OrcJIT
      Object
      Option
      Passes
      ProfileData
      Support
      TargetParser
      WindowsDriver
      WindowsManifest
      X86
      X86asmparser
   )
   
   target_link_libraries(H_compiler PUBLIC ${llvm_libs})

   find_package(LLD REQUIRED CONFIG)
   target_link_libraries(H_compiler PUBLIC lldCOFF)
endif()

find_package(LLD REQUIRED CONFIG)
target_link_libraries(H_compiler PUBLIC lldCommon)

if(WIN32)
   find_package(ZLIB REQUIRED)
   get_target_property(ZLIB_IMPORTED_LOCATION_DEBUG ZLIB::ZLIB IMPORTED_LOCATION_DEBUG)
   
   cmake_path(GET ZLIB_IMPORTED_LOCATION_DEBUG PARENT_PATH ZLIB_IMPORTED_LOCATION_DEBUG_DIRECTORY)
   cmake_path(GET ZLIB_IMPORTED_LOCATION_DEBUG_DIRECTORY PARENT_PATH ZLIB_IMPORTED_LOCATION_DEBUG_PARENT_DIRECTORY)
   find_program(ZLIB_DLL_DEBUG "zlibd1.dll" PATHS "${ZLIB_IMPORTED_LOCATION_DEBUG_PARENT_DIRECTORY}")
   install(FILES ${ZLIB_DLL_DEBUG} TYPE BIN CONFIGURATIONS Debug)
   
   cmake_path(GET ZLIB_IMPORTED_LOCATION_DEBUG_PARENT_DIRECTORY PARENT_PATH ZLIB_IMPORTED_LOCATION_RELEASE_PARENT_DIRECTORY)
   find_program(ZLIB_DLL_RELEASE "zlib1.dll" PATHS "${ZLIB_IMPORTED_LOCATION_RELEASE_PARENT_DIRECTORY}" REQUIRED)
   install(FILES ${ZLIB_DLL_RELEASE} TYPE BIN CONFIGURATIONS Debug Release)
endif()

find_package(Clang CONFIG REQUIRED)
target_include_directories(H_compiler PRIVATE ${CLANG_INCLUDE_DIRS})

if(WIN32)
   target_link_libraries(
      H_compiler
      PRIVATE
      clangBasic
      clangCodeGen
      libclang
   )
else()

   find_library(clang_library clang REQUIRED)
   target_link_libraries(H_compiler PRIVATE ${clang_library})

   find_library(clang_cpp_library clang-cpp REQUIRED)
   target_link_libraries(H_compiler PRIVATE ${clang_cpp_library})

   find_library(llvm_library LLVM REQUIRED)
   target_link_libraries(H_compiler PUBLIC ${llvm_library})

endif()

if (WIN32)
   find_program(LIBCLANG_DLL_LOCATION "libclang.dll" REQUIRED)
   install(FILES ${LIBCLANG_DLL_LOCATION} TYPE BIN CONFIGURATIONS Debug Release)
endif ()

if (WIN32)
   find_program(PROGRAM_CLANG_CL "clang-cl.exe" REQUIRED)
   install(FILES ${PROGRAM_CLANG_CL} TYPE BIN CONFIGURATIONS Debug Release)

   add_custom_command(
      TARGET H_compiler
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PROGRAM_CLANG_CL} ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/clang-cl.exe
   )
else()
   find_program(PROGRAM_CLANG "clang++.exe" REQUIRED)
   install(FILES ${PROGRAM_CLANG} TYPE BIN CONFIGURATIONS Debug Release)

   add_custom_command(
      TARGET H_compiler
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PROGRAM_CLANG} ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/clang++
   )
endif ()

## End LLVM ##

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

   find_package(Catch2 CONFIG REQUIRED)
   target_link_libraries(H_compiler_tests PRIVATE Catch2::Catch2 Catch2::Catch2WithMain)

   target_compile_definitions(
      H_compiler_tests
      PRIVATE
         EXAMPLES_DIRECTORY="${CMAKE_SOURCE_DIR}/Examples"
         TEST_SOURCE_FILES_PATH="${CMAKE_SOURCE_DIR}/Examples/txt"
         TESTS_OUTPUT_DIRECTORY_PATH="${CMAKE_BINARY_DIR}/Temp/Tests/Compiler"
         C_STANDARD_LIBRARY_PATH="${CMAKE_BINARY_DIR}/C_standard_library"
         STANDARD_REPOSITORY_FILE_PATH="${CMAKE_SOURCE_DIR}/share/hlang/libraries/hlang_repository.json"
   )

   target_sources(H_compiler_tests PRIVATE
      "Builder.tests.cpp"
      "Compiler.tests.cpp"
      "Recompilation.tests.cpp"
      "Validation.tests.cpp"
      "JIT/JIT_runner.tests.cpp"
   )

   # add_dependencies(H_compiler_tests C_standard_library)

   include(Catch)
   catch_discover_tests(H_compiler_tests)

endif()
