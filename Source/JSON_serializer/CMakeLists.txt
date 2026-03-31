add_library(H_JSON_serializer)
add_library(H::JSON_serializer ALIAS H_JSON_serializer)

target_compile_features(H_JSON_serializer PUBLIC cxx_std_23)

target_link_libraries(H_JSON_serializer PUBLIC H::Core)

find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(H_JSON_serializer PRIVATE nlohmann_json::nlohmann_json)

target_sources(H_JSON_serializer
    PUBLIC FILE_SET modules TYPE CXX_MODULES
        FILES
            "Generated.cppm"
            #"Generics.cppm"
            "JSON_serializer.cppm"
            "Operators.cppm"
)


if(BUILD_TESTING)
   add_executable(H_JSON_serializer_tests)
   target_link_libraries(H_JSON_serializer_tests PRIVATE H_JSON_serializer)

   find_package(Catch2 CONFIG REQUIRED)
   target_link_libraries(H_JSON_serializer_tests PRIVATE Catch2::Catch2WithMain)

   target_sources(H_JSON_serializer_tests PRIVATE "JSON_serializer.tests.cpp")

   include(Catch)
   catch_discover_tests(H_JSON_serializer_tests)
endif()


set(H_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../../")

add_custom_command(
    OUTPUT "${H_ROOT_DIR}/Source/JSON_serializer/Generated.cppm"
    COMMAND H_Tools_Generate_JSON
                ARGS
                "serialize_json_code"
                "${H_ROOT_DIR}/Source/Core/Core.cppm"
                "${H_ROOT_DIR}/Source/JSON_serializer/Generated.cppm"
    DEPENDS H_Tools_Generate_JSON "${H_ROOT_DIR}/Source/Core/Core.cppm"
    VERBATIM
)
add_custom_target(Generate_json_serializer_generated DEPENDS "${H_ROOT_DIR}/Source/JSON_serializer/Generated.cppm")

add_custom_command(
    OUTPUT "${H_ROOT_DIR}/Source/JSON_serializer/Operators.cppm"
    COMMAND H_Tools_Generate_JSON
                ARGS
                "operators"
                "${H_ROOT_DIR}/Source/Core/Core.cppm"
                "${H_ROOT_DIR}/Source/JSON_serializer/Operators.cppm"
    DEPENDS H_Tools_Generate_JSON "${H_ROOT_DIR}/Source/Core/Core.cppm"
    VERBATIM
)
add_custom_target(Generate_json_serializer_operators_code DEPENDS "${H_ROOT_DIR}/Source/JSON_serializer/Operators.cppm")

add_custom_target(Generate_json_serializer_code DEPENDS Generate_json_serializer_generated Generate_json_serializer_operators_code)
