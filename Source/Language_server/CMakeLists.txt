add_library(Iris_language_server SHARED)
add_library(Iris::Language_server ALIAS Iris_language_server)

set_target_properties(Iris_language_server PROPERTIES OUTPUT_NAME "iris_language_server_lib")

target_compile_features(Iris_language_server PUBLIC cxx_std_23)

target_link_libraries(Iris_language_server PUBLIC Iris::Common Iris::Core Iris::Compiler Iris::Parser)

target_link_libraries(Iris_language_server PRIVATE lsp)

find_package(argparse CONFIG REQUIRED)
target_link_libraries(Iris_language_server PRIVATE argparse::argparse)

find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(Iris_language_server PRIVATE nlohmann_json::nlohmann_json)

target_sources(Iris_language_server
   PUBLIC FILE_SET modules TYPE CXX_MODULES
        FILES
            "Breakpoint.cppm"
            "Code_action.cppm"
            "Completion.cppm"
            "Core.cppm"
            "Debug.cppm"
            "Diagnostics.cppm"
            "Go_to_location.cppm"
            "Inlay_hints.cppm"
            "Location.cppm"
            "Message_handler.cppm"
            "Server.cppm"
            "Signature_help.cppm"
   PRIVATE
      "Code_action.cpp"
      "Completion.cpp"
      "Core.cpp"
      "Diagnostics.cpp"
      "Go_to_location.cpp"
      "Inlay_hints.cpp"
      "Location.cpp"
      "Message_handler.cpp"
      "Server.cpp"
      "Signature_help.cpp"
)

target_generate_module_exports(Iris_language_server)
install(TARGETS Iris_language_server)


add_executable(Iris_language_server_app)
add_executable(Iris::Language_server_app ALIAS Iris_language_server_app)

target_link_libraries(Iris_language_server_app PRIVATE Iris_language_server)

set_target_properties(Iris_language_server_app PROPERTIES OUTPUT_NAME "iris_language_server")

target_sources(Iris_language_server_app
   PRIVATE
      "main.cpp"
)

install(TARGETS Iris_language_server_app)


if(BUILD_TESTING)
   add_executable(Iris_language_server_tests)
   target_link_libraries(Iris_language_server_tests PRIVATE Iris_language_server lsp)

   find_package(Catch2 CONFIG REQUIRED)
   target_link_libraries(Iris_language_server_tests PRIVATE Catch2::Catch2 Catch2::Catch2WithMain)

   target_sources(Iris_language_server_tests PRIVATE
      "Server.tests.cpp"
      "Signature_help.tests.cpp"
   )

   include(Catch)
   catch_discover_tests(Iris_language_server_tests)
endif()
