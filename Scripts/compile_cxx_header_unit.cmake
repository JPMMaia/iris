macro(compile_cxx_header_unit target input header_type header_path header_unit_name additional_compile_options)
    set(custom_target_name "header_unit_${header_unit_name}_custom_target")

    if (MSVC)
        set(output_module_path "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${header_unit_name}.ifc")
        set(include_compiler_string)

        get_target_property(target_includes ${target} INCLUDE_DIRECTORIES)
        foreach (dir ${target_includes})
            list(APPEND include_compiler_string /I${dir})
        endforeach ()

        set(header_compile_options "/std:c++latest" "/EHsc" "/nologo" "/D_SCL_SECURE_NO_WARNINGS")

        get_property(is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
        if (${is_multi_config})
            list(APPEND header_compile_options "$<IF:$<CONFIG:Debug>,/MDd,/MD>")
        else ()
            if (CMAKE_BUILD_TYPE STREQUAL "Debug")
                list(APPEND header_compile_options "/MDd")
            else ()
                list(APPEND header_compile_options "/MD")
            endif ()
        endif ()

        list(APPEND header_compile_options ${additional_compile_options})

        add_custom_command(
            OUTPUT ${output_module_path}
            COMMAND ${CMAKE_CXX_COMPILER}
                ${header_compile_options}
                ${include_compiler_string}
                /c
                /exportHeader
                /headerName:${header_type}
                "${header_path}"
                /ifcOutput ${output_module_path}
            DEPENDS
                "${input}"
            COMMENT "Building ${header_unit_name} header unit"
        )

        add_custom_target(${custom_target_name}
            DEPENDS ${output_module_path}
        )

        add_dependencies(${target} ${custom_target_name})
        get_target_property(target_source_list ${target} SOURCES)
        if (target_source_list)
            set_source_files_properties(${target_source_list} APPEND PROPERTY OBJECT_DEPENDS ${output_module_path})
        endif ()

        target_compile_options(${target} PUBLIC
            "SHELL:/headerUnit:${header_type} ${header_path}=${output_module_path}"
        )
    elseif (UNIX AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set(output_module_path "${CMAKE_CURRENT_BINARY_DIR}/${header_unit_name}.pcm")
        set(include_compiler_string)

        get_target_property(target_includes ${target} INCLUDE_DIRECTORIES)

        set(resolved_header_path "${header_path}")
        if (NOT IS_ABSOLUTE "${resolved_header_path}")
            foreach (dir ${target_includes})
                if (EXISTS "${dir}/${header_path}")
                    set(resolved_header_path "${dir}/${header_path}")
                    break()
                endif ()
            endforeach ()
        endif ()

        if (NOT EXISTS "${resolved_header_path}")
            message(FATAL_ERROR
                "Failed to resolve header unit path for ${header_path}.\n"
                "Target: ${target}\n"
                "Include directories: ${target_includes}"
            )
        endif ()

        foreach (dir ${target_includes})
            list(APPEND include_compiler_string -I${dir})
        endforeach ()

        set(header_compile_options
            -std=c++23
            -stdlib=libstdc++
        )

        if (CMAKE_BUILD_TYPE STREQUAL "Debug")
            list(APPEND header_compile_options -g)
        endif ()

        list(APPEND header_compile_options ${additional_compile_options})

        set(clang_header_unit_mode "user")
        if ("${header_type}" STREQUAL "angle")
            set(clang_header_unit_mode "system")
        endif ()

        add_custom_command(
            OUTPUT ${output_module_path}
            COMMAND ${CMAKE_CXX_COMPILER}
                ${header_compile_options}
                ${include_compiler_string}
                --precompile
                -x c++-header
                -fmodule-header=${clang_header_unit_mode}
                "${header_path}"
                -o ${output_module_path}
            DEPENDS
                "${input}"
            COMMENT "Building ${header_unit_name} header unit"
        )

        add_custom_target(${custom_target_name}
            DEPENDS ${output_module_path}
        )

        add_dependencies(${target} ${custom_target_name})
        get_target_property(target_source_list ${target} SOURCES)
        if (target_source_list)
            set_source_files_properties(${target_source_list} APPEND PROPERTY OBJECT_DEPENDS ${output_module_path})
        endif ()

        target_compile_options(${target} PUBLIC
            "SHELL:-fmodule-file=${output_module_path}"
        )
    endif ()

endmacro()
