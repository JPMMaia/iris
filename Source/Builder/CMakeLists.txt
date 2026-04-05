add_executable(H_builder_executable)
add_executable(H::Builder_executable ALIAS H_builder_executable)

set_target_properties(H_builder_executable PROPERTIES OUTPUT_NAME "hlang")

target_compile_definitions(H_builder_executable PUBLIC "_SCL_SECURE_NO_WARNINGS")
target_compile_features(H_builder_executable PRIVATE cxx_std_23)

target_link_libraries(H_builder_executable PRIVATE H::Compiler)


find_package(argparse CONFIG REQUIRED)
target_link_libraries(H_builder_executable PRIVATE argparse::argparse)

target_sources(H_builder_executable PRIVATE
   "main.cpp"
)

install(TARGETS H_builder_executable)


if (WIN32)
   find_program(LIBCLANG_DLL_LOCATION "libclang.dll" REQUIRED)
   install(FILES ${LIBCLANG_DLL_LOCATION} TYPE BIN CONFIGURATIONS Debug Release)
endif ()

if (WIN32)
   find_program(PROGRAM_CLANG_CL "clang-cl.exe" REQUIRED)
   add_custom_command(
      TARGET H_builder_executable
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PROGRAM_CLANG_CL} ${CMAKE_CURRENT_BINARY_DIR}/clang-cl.exe
   )
else()
   find_program(PROGRAM_CLANG "clang++.exe" REQUIRED)
   add_custom_command(
      TARGET H_builder_executable
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PROGRAM_CLANG} ${CMAKE_CURRENT_BINARY_DIR}/clang++
   )
endif ()
