cmake_minimum_required(VERSION 3.28)

cmake_policy(SET CMP0207 NEW)

project(Iris LANGUAGES C CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_MODULE_STD OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(InstallRequiredSystemLibraries)

include("Scripts/compile_cxx_header_unit.cmake")
include("Scripts/compile_cpp_std_module.cmake")
include("Scripts/module_exports.cmake")

include(CTest)

if (MSVC)
    add_compile_options("/EHsc")
else ()
    if (CMAKE_DEBUG_TYPE STREQUAL "Debug")
        add_compile_options("-gdwarf-4")
        add_compile_options("-fPIC")
    endif ()
endif ()

add_subdirectory("External")
add_subdirectory("Tools/code_generator")
add_subdirectory("Tools/tests_result_replacer")
add_subdirectory("Source")

add_custom_target(
    Generate_code_after_core_update DEPENDS
        Generate_json_serializer_code
        Generate_h_core_typescript_interface
        Generate_h_core_typescript_intermediate_representation
        Generate_h_core_expressions_visitor
        Generate_h_natvis_soa_array
    Generate_h_natvis_soa_array_view
)

install(DIRECTORY "share/iris" TYPE DATA CONFIGURATIONS Debug Release)
