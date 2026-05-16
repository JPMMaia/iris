if (TARGET std_module)
    return()
endif ()

if (MSVC)
    get_property(is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
    
    if (is_multi_config)
        set(STD_MODULE_OUT ${CMAKE_BINARY_DIR}/std_modules/$<CONFIG>)
        file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/std_modules/Debug)
        file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/std_modules/Release)
    else ()
        set(STD_MODULE_OUT ${CMAKE_BINARY_DIR}/std_modules)
        file(MAKE_DIRECTORY ${STD_MODULE_OUT})
    endif ()

    set(STD_IFC ${STD_MODULE_OUT}/std.ifc)
    set(STD_COMPAT_IFC ${STD_MODULE_OUT}/std_compat.ifc)


    set(STD_COMPILE_OPTIONS "/std:c++latest" "/EHsc" "/nologo" "/D_SCL_SECURE_NO_WARNINGS")

    if (${is_multi_config})
        list(APPEND STD_COMPILE_OPTIONS "$<IF:$<CONFIG:Debug>,/MDd,/MD>")
    else ()
        if (CMAKE_BUILD_TYPE STREQUAL "Debug")
            list(APPEND STD_COMPILE_OPTIONS "/MDd")
        else ()
            list(APPEND STD_COMPILE_OPTIONS "/MD")
        endif ()
    endif ()

    add_custom_command(
        OUTPUT ${STD_IFC}
        COMMAND ${CMAKE_CXX_COMPILER}
            ${STD_COMPILE_OPTIONS}
            /c
            "$ENV{VCToolsInstallDir}/modules/std.ixx"
            /ifcOutput ${STD_IFC}
        DEPENDS "$ENV{VCToolsInstallDir}/modules/std.ixx"
        COMMENT "Building std module interface"
    )

    add_custom_command(
        OUTPUT ${STD_COMPAT_IFC}
        COMMAND ${CMAKE_CXX_COMPILER}
            ${STD_COMPILE_OPTIONS}
            "/reference \"std=${STD_IFC}\""
            /c
            "$ENV{VCToolsInstallDir}/modules/std.compat.ixx"
            /ifcOutput ${STD_COMPAT_IFC}
        DEPENDS
            "$ENV{VCToolsInstallDir}/modules/std.compat.ixx"
            "${STD_IFC}"
        COMMENT "Building std.compat module interface"
    )

    add_custom_target(std_module_custom_target
        DEPENDS ${STD_IFC} ${STD_COMPAT_IFC}
    )

    add_library(std_module INTERFACE)
    target_compile_options(std_module INTERFACE ${STD_COMPILE_OPTIONS})
    target_compile_options(std_module INTERFACE "SHELL:/reference std=${STD_IFC}")
    target_compile_options(std_module INTERFACE "SHELL:/reference std.compat=${STD_COMPAT_IFC}")

    add_dependencies(std_module std_module_custom_target)
elseif (UNIX AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    
    set(STD_MODULE_OUT ${CMAKE_BINARY_DIR}/std_modules_clang)
    set(STD_PCM ${STD_MODULE_OUT}/std.pcm)
    set(STD_COMPAT_PCM ${STD_MODULE_OUT}/std.compat.pcm)

    file(MAKE_DIRECTORY ${STD_MODULE_OUT})

    execute_process(
        COMMAND ${CMAKE_CXX_COMPILER} --print-resource-dir
        OUTPUT_VARIABLE _iris_clang_resource_dir
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    set(_iris_std_source_hints
        /usr/include/c++
        /usr/local/include/c++
    )

    if (_iris_clang_resource_dir)
        list(APPEND _iris_std_source_hints
            ${_iris_clang_resource_dir}/include/c++
            ${_iris_clang_resource_dir}/include/c++/v1
        )
    endif ()

    file(GLOB _iris_std_include_version_dirs LIST_DIRECTORIES true
        "/usr/include/c++/*"
        "/usr/local/include/c++/*"
    )
    list(APPEND _iris_std_source_hints ${_iris_std_include_version_dirs})

    set(_iris_std_source)
    find_file(_iris_std_source
        NAMES std.cc
        HINTS ${_iris_std_source_hints}
        PATH_SUFFIXES bits
    )
    if (NOT _iris_std_source)
        find_file(_iris_std_source
            NAMES std.cppm
            HINTS ${_iris_std_source_hints}
            PATH_SUFFIXES bits modules
        )
    endif ()

    set(_iris_std_compat_source)
    find_file(_iris_std_compat_source
        NAMES std.compat.cc
        HINTS ${_iris_std_source_hints}
        PATH_SUFFIXES bits
    )
    if (NOT _iris_std_compat_source)
        find_file(_iris_std_compat_source
            NAMES std.compat.cppm
            HINTS ${_iris_std_source_hints}
            PATH_SUFFIXES bits modules
        )
    endif ()

    if (NOT _iris_std_source OR NOT _iris_std_compat_source)
        string(JOIN "\n  " _iris_hints_message ${_iris_std_source_hints})
        message(FATAL_ERROR
            "clang + libstdc++ std module sources were not found.\n"
            "Expected std.cc/std.compat.cc (or fallback std.cppm/std.compat.cppm).\n"
            "Compiler resource dir: ${_iris_clang_resource_dir}\n"
            "Searched hints:\n  ${_iris_hints_message}"
        )
    endif ()

    set(_iris_std_module_compile_options
        -std=c++23
        -stdlib=libstdc++
    )

    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        list(APPEND _iris_std_module_compile_options -g)
    endif ()

    add_custom_command(
        OUTPUT ${STD_PCM}
        COMMAND ${CMAKE_CXX_COMPILER}
            ${_iris_std_module_compile_options}
            -x c++-module
            --precompile
            ${_iris_std_source}
            -o ${STD_PCM}
        DEPENDS ${_iris_std_source}
        COMMENT "Building clang std.pcm from ${_iris_std_source}"
    )

    add_custom_command(
        OUTPUT ${STD_COMPAT_PCM}
        COMMAND ${CMAKE_CXX_COMPILER}
            ${_iris_std_module_compile_options}
            -x c++-module
            -fmodule-file=std=${STD_PCM}
            --precompile
            ${_iris_std_compat_source}
            -o ${STD_COMPAT_PCM}
        DEPENDS ${_iris_std_compat_source} ${STD_PCM}
        COMMENT "Building clang std.compat.pcm from ${_iris_std_compat_source}"
    )

    add_custom_target(std_module_custom_target
        DEPENDS ${STD_PCM} ${STD_COMPAT_PCM}
    )

    add_library(std_module INTERFACE)
    target_compile_options(std_module INTERFACE
        "SHELL:-stdlib=libstdc++"
        "SHELL:-fprebuilt-module-path=${STD_MODULE_OUT}"
        "SHELL:-fmodule-file=std=${STD_PCM}"
        "SHELL:-fmodule-file=std.compat=${STD_COMPAT_PCM}"
    )

    add_dependencies(std_module std_module_custom_target)

    message(STATUS "Building clang std modules from ${_iris_std_source} and ${_iris_std_compat_source}")
else ()
    add_library(std_module INTERFACE)
    message(STATUS "std_module is a no-op for ${CMAKE_SYSTEM_NAME} with ${CMAKE_CXX_COMPILER_ID}")
endif ()
