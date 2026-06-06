add_subdirectory("tree-sitter-iris")

add_library(Iris_parser SHARED)
add_library(Iris::Parser ALIAS Iris_parser)

set_target_properties(Iris_parser PROPERTIES OUTPUT_NAME "iris_parser_lib")

target_compile_features(Iris_parser PRIVATE cxx_std_23)

target_link_libraries(Iris_parser PUBLIC Iris::Common Iris::Core)

target_link_libraries(Iris_parser PRIVATE tree-sitter-iris)

if (WIN32)
   find_package(unofficial-tree-sitter CONFIG REQUIRED)
   target_link_libraries(Iris_parser PRIVATE unofficial::tree-sitter::tree-sitter)
else ()
   find_library(TREE_SITTER_LIBRARY tree-sitter REQUIRED)
   target_link_libraries(Iris_parser PRIVATE ${TREE_SITTER_LIBRARY})
endif()

target_sources(Iris_parser
   PUBLIC FILE_SET modules TYPE CXX_MODULES
      FILES
         "Convertor.cppm"
         "Parse_tree.cppm"
         "Parser.cppm"
         "Type_name_parser.cppm"
   PRIVATE
      "Convertor.cpp"
      "Parse_tree.cpp"
      "Parser.cpp"
      "Type_name_parser.cpp"
)

target_generate_module_exports(Iris_parser)
install(TARGETS Iris_parser)

add_executable(Iris_parser_executable)
add_executable(Iris::Parser_executable ALIAS Iris_parser_executable)

set_target_properties(Iris_parser_executable PROPERTIES OUTPUT_NAME "iris_parser")

target_compile_features(Iris_parser_executable PRIVATE cxx_std_23)

target_link_libraries(Iris_parser_executable PRIVATE Iris::Common Iris::Parser Iris::JSON_serializer)

find_package(argparse CONFIG REQUIRED)
target_link_libraries(Iris_parser_executable PRIVATE argparse::argparse)

target_sources(Iris_parser_executable PRIVATE
   "main.cpp"
)

install(TARGETS Iris_parser_executable)


if(BUILD_TESTING)
   add_executable(Iris_parser_tests)
   target_link_libraries(Iris_parser_tests PRIVATE Iris_parser)

   find_package(Catch2 CONFIG REQUIRED)
   target_link_libraries(Iris_parser_tests PRIVATE Catch2::Catch2 Catch2::Catch2WithMain)

   target_compile_definitions(
      Iris_parser_tests
      PRIVATE
         TEST_SOURCE_FILES_PATH="${CMAKE_SOURCE_DIR}/Examples/txt"
   )

   target_sources(Iris_parser_tests
      PRIVATE
         "Convertor.tests.cpp"
         "Parser.tests.cpp"
   )

   include(Catch)
   catch_discover_tests(Iris_parser_tests)
endif()
