add_library(Iris_graph SHARED)
add_library(Iris::Graph ALIAS Iris_graph)

target_link_libraries(Iris_graph PUBLIC std_module)
target_compile_features(Iris_graph PUBLIC cxx_std_23)

target_link_libraries(Iris_graph PUBLIC Iris::Core)

target_sources(Iris_graph
   PUBLIC FILE_SET modules TYPE CXX_MODULES
      FILES
         "Graph.cppm"
         "Module_dependency_graph.cppm"
   PRIVATE
      "Graph.cpp"
      "Module_dependency_graph.cpp"
)

target_generate_module_exports(Iris_graph)
install(TARGETS Iris_graph)

if(BUILD_TESTING)
   add_executable(Iris_graph_tests)
   target_link_libraries(Iris_graph_tests PRIVATE Iris::Graph Iris::Core)

   find_package(Catch2 CONFIG REQUIRED)
   target_link_libraries(Iris_graph_tests PRIVATE Catch2::Catch2 Catch2::Catch2WithMain)

   target_sources(Iris_graph_tests PRIVATE
      "Module_dependency_graph.tests.cpp"
   )

   include(Catch)
   catch_discover_tests(Iris_graph_tests)
endif()
