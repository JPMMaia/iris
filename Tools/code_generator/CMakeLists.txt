add_library(Iris_Tools_Code_generator)
add_library(Iris::Tools::Code_generator ALIAS Iris_Tools_Code_generator)

target_link_libraries(Iris_Tools_Code_generator PUBLIC std_module)

find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(Iris_Tools_Code_generator PRIVATE nlohmann_json::nlohmann_json)

target_compile_features(Iris_Tools_Code_generator PUBLIC cxx_std_23)

target_sources(Iris_Tools_Code_generator 
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            "Code_generator.cppm"
    PRIVATE
        "Code_generator.cpp"
)


if(BUILD_TESTING)
   add_executable(Iris_Tools_Code_generator_tests)
   target_link_libraries(Iris_Tools_Code_generator_tests PRIVATE Iris_Tools_Code_generator)

   find_package(Catch2 CONFIG REQUIRED)
   target_link_libraries(Iris_Tools_Code_generator_tests PRIVATE Catch2::Catch2 Catch2::Catch2WithMain)

   target_sources(Iris_Tools_Code_generator_tests PRIVATE "Code_generator.tests.cpp")

   include(Catch)
   catch_discover_tests(Iris_Tools_Code_generator_tests)
endif()


add_executable(Iris_Tools_Generate_JSON)
add_executable(Iris::Tools::Generate_JSON ALIAS Iris_Tools_Generate_JSON)

target_compile_features(Iris_Tools_Generate_JSON PUBLIC cxx_std_23)

target_link_libraries(Iris_Tools_Generate_JSON PRIVATE Iris_Tools_Code_generator)

target_sources(Iris_Tools_Generate_JSON PRIVATE
    "main.cpp"
)


set(Iris_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../../")

add_custom_command(
    OUTPUT "${Iris_ROOT_DIR}/Tools/vscode/iris-extension/packages/core/src/Core_interface.ts"
    COMMAND Iris_Tools_Generate_JSON
                ARGS
                "typescript_interface"
                "${Iris_ROOT_DIR}/Source/Core/Core.cppm"
                "${Iris_ROOT_DIR}/Tools/vscode/iris-extension/packages/core/src/Core_interface.ts"
    DEPENDS Iris_Tools_Generate_JSON "${Iris_ROOT_DIR}/Source/Core/Core.cppm"
    VERBATIM
)
add_custom_target(Generate_h_core_typescript_interface DEPENDS "${Iris_ROOT_DIR}/Tools/vscode/iris-extension/packages/core/src/Core_interface.ts")

add_custom_command(
    OUTPUT "${Iris_ROOT_DIR}/Tools/vscode/iris-extension/packages/core/src/Core_intermediate_representation.ts"
    COMMAND Iris_Tools_Generate_JSON
                ARGS
                "typescript_intermediate_representation"
                "${Iris_ROOT_DIR}/Source/Core/Core.cppm"
                "${Iris_ROOT_DIR}/Tools/vscode/iris-extension/packages/core/src/Core_intermediate_representation.ts"
    DEPENDS Iris_Tools_Generate_JSON "${Iris_ROOT_DIR}/Source/Core/Core.cppm"
    VERBATIM
)
add_custom_target(Generate_h_core_typescript_intermediate_representation DEPENDS "${Iris_ROOT_DIR}/Tools/vscode/iris-extension/packages/core/src/Core_intermediate_representation.ts")

add_custom_command(
    OUTPUT "${Iris_ROOT_DIR}/Source/Binary_serializer/Generated.cppm"
    COMMAND Iris_Tools_Generate_JSON
                ARGS
                "serialize_binary_code"
                "${Iris_ROOT_DIR}/Source/Core/Core.cppm"
                "${Iris_ROOT_DIR}/Source/Binary_serializer/Generated.cppm"
    DEPENDS Iris_Tools_Generate_JSON "${Iris_ROOT_DIR}/Source/Core/Core.cppm"
    VERBATIM
)
add_custom_target(Iris_Binary_serializer_generate DEPENDS "${Iris_ROOT_DIR}/Source/Binary_serializer/Generated.cppm")

add_custom_command(
    OUTPUT "${Iris_ROOT_DIR}/Source/Core/Expressions_visitor.cppm"
    COMMAND Iris_Tools_Generate_JSON
                ARGS
                "expressions_visitor"
                "${Iris_ROOT_DIR}/Source/Core/Core.cppm"
                "${Iris_ROOT_DIR}/Source/Core/Expressions_visitor.cppm"
    DEPENDS Iris_Tools_Generate_JSON "${Iris_ROOT_DIR}/Source/Core/Core.cppm"
    VERBATIM
)
add_custom_target(Generate_h_core_expressions_visitor DEPENDS "${Iris_ROOT_DIR}/Source/Core/Expressions_visitor.cppm")

add_custom_command(
    OUTPUT "${Iris_ROOT_DIR}/share/iris/visualizers/iris_soa_array.natvis"
    COMMAND Iris_Tools_Generate_JSON
                ARGS
                "natvis_soa_array"
                "${Iris_ROOT_DIR}/Source/Core/Core.cppm"
                "${Iris_ROOT_DIR}/share/iris/visualizers/iris_soa_array.natvis"
    DEPENDS Iris_Tools_Generate_JSON "${Iris_ROOT_DIR}/Source/Core/Core.cppm"
    VERBATIM
)
add_custom_target(Generate_h_natvis_soa_array DEPENDS "${Iris_ROOT_DIR}/share/iris/visualizers/iris_soa_array.natvis")

add_custom_command(
    OUTPUT "${Iris_ROOT_DIR}/share/iris/visualizers/iris_soa_array_view.natvis"
    COMMAND Iris_Tools_Generate_JSON
                ARGS
                "natvis_soa_array_view"
                "${Iris_ROOT_DIR}/Source/Core/Core.cppm"
                "${Iris_ROOT_DIR}/share/iris/visualizers/iris_soa_array_view.natvis"
    DEPENDS Iris_Tools_Generate_JSON "${Iris_ROOT_DIR}/Source/Core/Core.cppm"
    VERBATIM
)
add_custom_target(Generate_h_natvis_soa_array_view DEPENDS "${Iris_ROOT_DIR}/share/iris/visualizers/iris_soa_array_view.natvis")
