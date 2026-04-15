add_library(Iris_C_header_importer)
add_library(Iris::C_header_importer ALIAS Iris_C_header_importer)

target_compile_features(Iris_C_header_importer PUBLIC cxx_std_23)

target_link_libraries(Iris_C_header_importer PUBLIC Iris::Binary_serializer Iris::Common Iris::Core)

## LLVM ##

find_package(Clang CONFIG REQUIRED)
target_include_directories(Iris_C_header_importer PRIVATE ${CLANG_INCLUDE_DIRS})

if(WIN32)
   target_link_libraries(Iris_C_header_importer PRIVATE clangBasic clangTooling libclang)
else()
   find_library(clang_cpp_library clang-cpp REQUIRED)
   target_link_libraries(Iris_C_header_importer PRIVATE ${clang_cpp_library})
endif()

## End LLVM ##

if(WIN32)
   find_package(xxHash CONFIG REQUIRED)
   target_link_libraries(Iris_C_header_importer PRIVATE xxHash::xxhash)
   install(IMPORTED_RUNTIME_ARTIFACTS xxHash::xxhash)
else()
   find_package(PkgConfig REQUIRED)
   pkg_check_modules(XXHASH REQUIRED IMPORTED_TARGET libxxhash)

   target_include_directories(Iris_C_header_importer PRIVATE ${XXHASIris_INCLUDE_DIRS})
   target_link_libraries(Iris_C_header_importer PRIVATE PkgConfig::XXHASH)
endif()

target_sources(Iris_C_header_importer
   PUBLIC FILE_SET modules TYPE CXX_MODULES
      FILES
         "C_header_hash.cppm"
         "C_header_importer.cppm"
   PRIVATE
      "C_header_hash.cpp"
      "C_header_importer.cpp"
)

if(BUILD_TESTING)
   find_package(Vulkan REQUIRED)
   set(vulkan_headers_location "${Vulkan_INCLUDE_DIRS}")
   message(STATUS "vulkan_headers_location is ${vulkan_headers_location}")

   add_executable(Iris_C_header_importer_tests)
   target_link_libraries(Iris_C_header_importer_tests PRIVATE Iris::JSON_serializer Iris_C_header_importer)

   find_package(Catch2 CONFIG REQUIRED)
   target_link_libraries(Iris_C_header_importer_tests PRIVATE Catch2::Catch2 Catch2::Catch2WithMain)

   target_link_libraries(Iris_C_header_importer_tests PRIVATE clangTooling)

   target_compile_definitions(
      Iris_C_header_importer_tests
      PRIVATE
         VULKAN_HEADERS_LOCATION="${vulkan_headers_location}"
   )

   target_sources(Iris_C_header_importer_tests PRIVATE "C_header_hash.tests.cpp" "C_header_importer.tests.cpp")

   include(Catch)
   catch_discover_tests(Iris_C_header_importer_tests)
endif()

add_executable(C_header_importer_tool)
target_link_libraries(C_header_importer_tool PRIVATE Iris::Core Iris::C_header_importer Iris::JSON_serializer)
target_sources(C_header_importer_tool PRIVATE "main.cpp")

find_package(argparse CONFIG REQUIRED)
target_link_libraries(C_header_importer_tool PRIVATE argparse::argparse)

macro(Import_c_header)

   set(options)
   set(oneValueArgs TARGET OUTPUT_DIRECTORY)
   set(multiValueArgs HEADERS)
   cmake_parse_arguments(ARGUMENT "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN} )

   set(additional_include_directories "")
   if(UNIX)
      list(APPEND additional_include_directories "--include-directory")
      list(APPEND additional_include_directories "/usr/include")
      list(APPEND additional_include_directories "--include-directory")
      list(APPEND additional_include_directories "/usr/include/linux")

      file(GLOB_RECURSE IOSTREAM_PATH "/usr/include/c++/*/iostream")
      get_filename_component(CPP_LIBRARY_INCLUDE_DIRECTORY "${IOSTREAM_PATH}" DIRECTORY)
      list(APPEND additional_include_directories "--include-directory")
      list(APPEND additional_include_directories "${CPP_LIBRARY_INCLUDE_DIRECTORY}")
      list(APPEND additional_include_directories "--include-directory")
      list(APPEND additional_include_directories "${CPP_LIBRARY_INCLUDE_DIRECTORY}/tr1")
      list(APPEND additional_include_directories "--include-directory")
      list(APPEND additional_include_directories "${CPP_LIBRARY_INCLUDE_DIRECTORY}/x86_64-pc-linux-gnu")
   endif()

   foreach(header IN LISTS ARGUMENT_HEADERS)
      cmake_path(GET header STEM header_without_extension)
      set(output_path "${ARGUMENT_OUTPUT_DIRECTORY}/C_${header_without_extension}.irisb")

      list(APPEND dependencies "${output_path}")

      add_custom_command(
         OUTPUT "${output_path}"
         COMMAND C_header_importer_tool
            ARGS "C.${header_without_extension}" "${header}" "${output_path}" ${additional_include_directories}
         DEPENDS "${header}"
         COMMENT "Generating ${output_path}"
      )
   endforeach()

   add_custom_target(
      C_standard_library ALL
         DEPENDS ${dependencies}
         SOURCES ${dependencies} ${ARGUMENT_HEADERS}
   )

endmacro()

add_library(Iris_C_header_exporter)
add_library(Iris::C_header_exporter ALIAS Iris_C_header_exporter)

target_compile_features(Iris_C_header_exporter PUBLIC cxx_std_23)

target_link_libraries(Iris_C_header_exporter PUBLIC Iris::Common Iris::Core)

target_sources(Iris_C_header_exporter
   PUBLIC FILE_SET modules TYPE CXX_MODULES
      FILES
         "C_header_exporter.cppm"
   PRIVATE
      "C_header_exporter.cpp"
)

if(BUILD_TESTING)
   add_executable(Iris_C_header_exporter_tests)
   target_link_libraries(Iris_C_header_exporter_tests PRIVATE Iris::JSON_serializer Iris::Parser Iris_C_header_exporter)

   find_package(Catch2 CONFIG REQUIRED)
   target_link_libraries(Iris_C_header_exporter_tests PRIVATE Catch2::Catch2 Catch2::Catch2WithMain)

   target_sources(Iris_C_header_exporter_tests PRIVATE "C_header_exporter.tests.cpp")

   include(Catch)
   catch_discover_tests(Iris_C_header_exporter_tests)
endif()
