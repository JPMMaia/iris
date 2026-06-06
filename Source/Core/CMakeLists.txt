add_library(Iris_core SHARED)
add_library(Iris::Core ALIAS Iris_core)

target_link_libraries(Iris_core PUBLIC std_module)
target_compile_features(Iris_core PUBLIC cxx_std_23)

target_link_libraries(Iris_core PRIVATE Iris::Common)

target_sources(Iris_core
   PUBLIC FILE_SET modules TYPE CXX_MODULES
      FILES
         "Core.cppm"
         "Declarations.cppm"
         "Expressions_visitor.cppm"
         "Expressions.cppm"
         "Formatter.cppm"
         "Hash.cppm"
         "String_hash.cppm"
         "Struct_layout.cppm"
         "Types.cppm"
         "Execution_engine/Execution_engine.cppm"
   PRIVATE
      "Core.cpp"
      "Declarations.cpp"
      "Expressions.cpp"
      "Formatter.cpp"
      "Hash.cpp"
      "Types.cpp"
      "Execution_engine/Execution_engine.cpp"
)

if(WIN32)
   find_package(xxHash CONFIG REQUIRED)
   target_link_libraries(Iris_core PRIVATE xxHash::xxhash)
   install(IMPORTED_RUNTIME_ARTIFACTS xxHash::xxhash)
else()
   find_package(PkgConfig REQUIRED)
   pkg_check_modules(XXHASH REQUIRED IMPORTED_TARGET libxxhash)

   target_include_directories(Iris_core PRIVATE ${XXHASIris_INCLUDE_DIRS})
   target_link_libraries(Iris_core PRIVATE PkgConfig::XXHASH)
endif()

add_dependencies(Iris_core Generate_h_core_expressions_visitor)

target_generate_module_exports(Iris_core)
install(TARGETS Iris_core)
