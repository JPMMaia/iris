add_executable(Iris_builder_executable)
add_executable(Iris::Builder_executable ALIAS Iris_builder_executable)

set_target_properties(Iris_builder_executable PROPERTIES OUTPUT_NAME "iris")

target_compile_features(Iris_builder_executable PRIVATE cxx_std_23)
target_link_libraries(Iris_builder_executable PUBLIC std_module)

target_link_libraries(Iris_builder_executable PRIVATE Iris::Compiler)


find_package(argparse CONFIG REQUIRED)
target_link_libraries(Iris_builder_executable PRIVATE argparse::argparse)

target_sources(Iris_builder_executable PRIVATE
   "main.cpp"
)

install(
   TARGETS Iris_builder_executable
   RUNTIME_DEPENDENCIES
      PRE_EXCLUDE_REGEXES "api-ms-" "ext-ms-"
      POST_EXCLUDE_REGEXES "system32"
      DESTINATION bin
)

if (WIN32)
   find_program(PROGRAM_CLANG_CL "clang-cl.exe" REQUIRED)
   add_custom_command(
      TARGET Iris_builder_executable
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PROGRAM_CLANG_CL} ${CMAKE_CURRENT_BINARY_DIR}/clang-cl.exe
   )
else()
   find_program(PROGRAM_CLANG "clang++.exe" REQUIRED)
   add_custom_command(
      TARGET Iris_builder_executable
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PROGRAM_CLANG} ${CMAKE_CURRENT_BINARY_DIR}/clang++
   )
endif ()
