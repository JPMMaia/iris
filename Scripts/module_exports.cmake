# module_exports.cmake
#
# Provides target_generate_module_exports(<target>) for SHARED libraries that
# contain C++ module interface units (.cppm).
#
# Problem: CMake's CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS uses `cmake -E __create_def`
# which drops module-linkage symbols of the form ?foo@...::<!my.module>.
# Consumers that `import` the module reference exactly those symbols, so they
# fail to link with LNK2019.
#
# Solution: disable per-target WINDOWS_EXPORT_ALL_SYMBOLS, then add a PRE_LINK
# command that runs Scripts/generate_module_def.py to produce a .def file that
# includes all External/defined symbols -- including the ::<!...> ones -- and
# pass that file to the linker via /DEF:.
#
# Only has effect on MSVC; on other platforms the function is a no-op.

if (MSVC)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    # dumpbin.exe lives in the same directory as cl.exe
    get_filename_component(_iris_cl_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    find_program(IRIS_DUMPBIN dumpbin
        HINTS "${_iris_cl_dir}"
        REQUIRED
    )
    unset(_iris_cl_dir)
endif ()

# target_generate_module_exports(<target>)
#
# Call this AFTER all target_sources() calls for <target> so that the SOURCES
# property is fully populated when this function reads it to scan .cppm files.
function(target_generate_module_exports target)
    if (NOT MSVC)
        return()
    endif ()

    # Disable CMake's auto-export; it does not handle C++ module linkage symbols.
    set_target_properties(${target} PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS OFF)

    set(_obj_dir  "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${target}.dir/$<CONFIG>")
    set(_def_file "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${target}.dir/$<CONFIG>/${target}_exports.def")

    # Gather .cppm sources.  In CMake 3.28, files added via
    # FILE_SET TYPE CXX_MODULES are stored in CXX_MODULE_SET_<name> properties,
    # NOT in the plain SOURCES property.
    set(_cppm_candidates "")

    # (1) Collect files from every named CXX_MODULE file set.
    get_target_property(_cxx_sets ${target} CXX_MODULE_SETS)
    if (_cxx_sets)
        foreach(_set_name IN LISTS _cxx_sets)
            get_target_property(_set_files ${target} CXX_MODULE_SET_${_set_name})
            if (_set_files)
                list(APPEND _cppm_candidates ${_set_files})
            endif ()
        endforeach()
    endif ()

    # (2) Also check the default (unnamed) CXX_MODULE file set.
    get_target_property(_default_set_files ${target} CXX_MODULE_SET)
    if (_default_set_files)
        list(APPEND _cppm_candidates ${_default_set_files})
    endif ()

    # (3) Fall back: scan plain SOURCES for any .cppm entries.
    get_target_property(_plain_srcs ${target} SOURCES)
    if (_plain_srcs)
        foreach(_s IN LISTS _plain_srcs)
            if ("${_s}" MATCHES "\\.cppm$")
                list(APPEND _cppm_candidates "${_s}")
            endif ()
        endforeach()
    endif ()

    list(REMOVE_DUPLICATES _cppm_candidates)

    # Scan each .cppm file for "export module <name>".
    set(_module_names "")
    foreach(_src IN LISTS _cppm_candidates)
        if (NOT IS_ABSOLUTE "${_src}")
            set(_src "${CMAKE_CURRENT_SOURCE_DIR}/${_src}")
        endif ()
        if (NOT EXISTS "${_src}")
            # Generated file not yet present at configure time; will be
            # picked up on the next cmake run after first generation.
            continue()
        endif ()
        file(STRINGS "${_src}" _lines REGEX "^export module [A-Za-z0-9_.]+")
        foreach(_line IN LISTS _lines)
            if ("${_line}" MATCHES "^export module ([A-Za-z0-9_.]+)")
                list(APPEND _module_names "${CMAKE_MATCH_1}")
            endif ()
        endforeach()
    endforeach()
    list(REMOVE_DUPLICATES _module_names)
    list(JOIN _module_names ";" _module_names_str)

    # PRE_LINK runs after all .obj files are compiled, before the linker.
    add_custom_command(TARGET ${target} PRE_LINK
        COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_SOURCE_DIR}/Scripts/generate_module_def.py"
                "${_def_file}"
                "${_obj_dir}"
                "${IRIS_DUMPBIN}"
                "${_module_names_str}"
        COMMENT "Generating module-aware exports.def for ${target}"
        VERBATIM
    )

    target_link_options(${target} PRIVATE "/DEF:${_def_file}")
endfunction()
