add_library(Iris_JSON_serializer SHARED)
add_library(Iris::JSON_serializer ALIAS Iris_JSON_serializer)

target_compile_features(Iris_JSON_serializer PUBLIC cxx_std_23)

target_link_libraries(Iris_JSON_serializer PUBLIC Iris::Common Iris::Core)

find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(Iris_JSON_serializer PRIVATE nlohmann_json::nlohmann_json)

target_sources(Iris_JSON_serializer
    PUBLIC FILE_SET modules TYPE CXX_MODULES
        FILES
            "Generated.cppm"
            #"Generics.cppm"
            "JSON_serializer.cppm"
            "Operators.cppm"
)

target_generate_module_exports(Iris_JSON_serializer)
install(TARGETS Iris_JSON_serializer)


if(BUILD_TESTING)
   add_executable(Iris_JSON_serializer_tests)
   target_link_libraries(Iris_JSON_serializer_tests PRIVATE Iris_JSON_serializer)

   find_package(Catch2 CONFIG REQUIRED)
   target_link_libraries(Iris_JSON_serializer_tests PRIVATE Catch2::Catch2WithMain)

   find_package(nlohmann_json CONFIG REQUIRED)
   target_link_libraries(Iris_JSON_serializer_tests PRIVATE nlohmann_json::nlohmann_json)

   target_sources(Iris_JSON_serializer_tests PRIVATE "JSON_serializer.tests.cpp")

   include(Catch)
   catch_discover_tests(Iris_JSON_serializer_tests)
endif()


set(Iris_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../../")

add_custom_command(
    OUTPUT "${Iris_ROOT_DIR}/Source/JSON_serializer/Generated.cppm"
    COMMAND Iris_Tools_Generate_JSON
                ARGS
                "serialize_json_code"
                "${Iris_ROOT_DIR}/Source/Core/Core.cppm"
                "${Iris_ROOT_DIR}/Source/JSON_serializer/Generated.cppm"
    DEPENDS Iris_Tools_Generate_JSON "${Iris_ROOT_DIR}/Source/Core/Core.cppm"
    VERBATIM
)
add_custom_target(Generate_json_serializer_generated DEPENDS "${Iris_ROOT_DIR}/Source/JSON_serializer/Generated.cppm")

add_custom_command(
    OUTPUT "${Iris_ROOT_DIR}/Source/JSON_serializer/Operators.cppm"
    COMMAND Iris_Tools_Generate_JSON
                ARGS
                "operators"
                "${Iris_ROOT_DIR}/Source/Core/Core.cppm"
                "${Iris_ROOT_DIR}/Source/JSON_serializer/Operators.cppm"
    DEPENDS Iris_Tools_Generate_JSON "${Iris_ROOT_DIR}/Source/Core/Core.cppm"
    VERBATIM
)
add_custom_target(Generate_json_serializer_operators_code DEPENDS "${Iris_ROOT_DIR}/Source/JSON_serializer/Operators.cppm")

add_custom_target(Generate_json_serializer_code DEPENDS Generate_json_serializer_generated Generate_json_serializer_operators_code)
