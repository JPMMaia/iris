add_library(clang_target STATIC)
target_include_directories(clang_target PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(clang_target PRIVATE llvm_target)
target_sources(clang_target PUBLIC FILE_SET modules TYPE CXX_MODULES
   FILES "clang.cppm"
)


find_package(Clang CONFIG REQUIRED)
target_include_directories(clang_target PRIVATE ${CLANG_INCLUDE_DIRS})

if (MSVC)
   target_compile_options(clang_target PUBLIC "/D_SCL_SECURE_NO_WARNINGS")
endif ()

if(WIN32)
   target_link_libraries(
      clang_target
      PRIVATE
      clangBasic
      clangCodeGen
      libclang
   )
else()

   find_library(clang_library clang REQUIRED)
   target_link_libraries(clang_target PRIVATE ${clang_library})

   find_library(clang_cpp_library clang-cpp REQUIRED)
   target_link_libraries(clang_target PRIVATE ${clang_cpp_library})

   find_library(llvm_library LLVM REQUIRED)
   target_link_libraries(clang_target PRIVATE ${llvm_library})

endif()

if (WIN32)
   find_program(PROGRAM_CLANG_CL "clang-cl.exe" REQUIRED)
   install(FILES ${PROGRAM_CLANG_CL} TYPE BIN CONFIGURATIONS Debug Release)

   add_custom_command(
      TARGET clang_target
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PROGRAM_CLANG_CL} ${CMAKE_BINARY_DIR}/Source/Compiler/clang-cl.exe
      COMMAND ${CMAKE_COMMAND} -DPROGRAM="${PROGRAM_CLANG_CL}" -DDESTINATION="${CMAKE_BINARY_DIR}/Source/Compiler" -P "${CMAKE_SOURCE_DIR}/Scripts/copy_runtime_dependencies.cmake"
   )
else()
   find_program(PROGRAM_CLANG "clang++" REQUIRED)
   install(FILES ${PROGRAM_CLANG} TYPE BIN CONFIGURATIONS Debug Release)

   add_custom_command(
      TARGET clang_target
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PROGRAM_CLANG} ${CMAKE_BINARY_DIR}/Source/Compiler/clang++
   )
endif ()

compile_cxx_header_unit(clang_target "clang.h" "angle" "clang/clang.h" "clang.unit" "")
