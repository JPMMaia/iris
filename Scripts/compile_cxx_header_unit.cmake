macro(compile_cxx_header_unit target input header_type header_path header_unit_name additional_compile_options)
    
    set(ifc_path "${CMAKE_CURRENT_BINARY_DIR}/${header_unit_name}.ifc")

    get_target_property(target_includes ${target} INCLUDE_DIRECTORIES)
    foreach (dir ${target_includes})
        list(APPEND INCLUDE_COMPILER_STRING /I${dir})
    endforeach ()

    set(header_compile_options "/std:c++latest" "/EHsc" "/nologo" "/D_DLL" "/D_SCL_SECURE_NO_WARNINGS")

    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        list(APPEND header_compile_options "/D_DEBUG")
    endif ()

    list(APPEND header_compile_options ${additional_compile_options})

    add_custom_command(
        OUTPUT ${ifc_path}
        COMMAND ${CMAKE_CXX_COMPILER}
            ${header_compile_options}
            ${INCLUDE_COMPILER_STRING}
            /c
            /exportHeader
            /headerName:${header_type}
            "${header_path}"
            /ifcOutput ${ifc_path}
        DEPENDS 
            "${input}"
        COMMENT "Building ${header_unit_name} header unit"
    )

    set(custom_target_name "header_unit_${header_unit_name}_custom_target")
    add_custom_target(${custom_target_name}
        DEPENDS ${ifc_path}
    )

    add_dependencies(${target} ${custom_target_name})
    target_compile_options(${target} PUBLIC
        "SHELL:/headerUnit:${header_type} ${header_path}=${ifc_path}"
    )

endmacro()
