add_library(Iris_Tools_Tests_results_replacer)
add_library(Iris::Tools::Tests_results_replacer ALIAS Iris_Tools_Tests_results_replacer)

target_compile_features(Iris_Tools_Tests_results_replacer PUBLIC cxx_std_23)

target_sources(Iris_Tools_Tests_results_replacer 
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            "Tests_results_replacer.cppm"
    PRIVATE
        "Tests_results_replacer.cpp"
)


if(BUILD_TESTING)
   add_executable(Iris_Tools_Tests_results_replacer_tests)
   target_link_libraries(Iris_Tools_Tests_results_replacer_tests PRIVATE Iris_Tools_Tests_results_replacer)

   find_package(Catch2 CONFIG REQUIRED)
   target_link_libraries(Iris_Tools_Tests_results_replacer_tests PRIVATE Catch2::Catch2 Catch2::Catch2WithMain)

   target_sources(Iris_Tools_Tests_results_replacer_tests PRIVATE "Tests_results_replacer.tests.cpp")

   include(Catch)
   catch_discover_tests(Iris_Tools_Tests_results_replacer_tests)
endif()


add_executable(Iris_Tools_Replace_tests_results)
add_executable(Iris::Tools::Replace_tests_results ALIAS Iris_Tools_Replace_tests_results)

target_compile_features(Iris_Tools_Replace_tests_results PUBLIC cxx_std_23)

target_link_libraries(Iris_Tools_Replace_tests_results PRIVATE Iris_Tools_Tests_results_replacer)

target_sources(Iris_Tools_Replace_tests_results PRIVATE
    "main.cpp"
)
