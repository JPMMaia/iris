add_library(Iris_binary_serializer)
add_library(Iris::Binary_serializer ALIAS Iris_binary_serializer)

target_compile_features(Iris_binary_serializer PUBLIC cxx_std_23)

target_link_libraries(Iris_binary_serializer PUBLIC Iris::Common Iris::Core)

target_sources(Iris_binary_serializer
    PUBLIC FILE_SET modules TYPE CXX_MODULES
        FILES
            "Binary_serializer.cppm"
            "Generated.cppm"
            "Generics.cppm"
)

add_dependencies(Iris_binary_serializer Iris_Binary_serializer_generate)

if(BUILD_TESTING)
   add_executable(Iris_binary_serializer_tests)
   target_link_libraries(Iris_binary_serializer_tests PRIVATE Iris_binary_serializer)

   find_package(Catch2 CONFIG REQUIRED)
   target_link_libraries(Iris_binary_serializer_tests PRIVATE Catch2::Catch2WithMain)

   target_sources(Iris_binary_serializer_tests PRIVATE "Binary_serializer.tests.cpp")

   include(Catch)
   catch_discover_tests(Iris_binary_serializer_tests)
endif()
