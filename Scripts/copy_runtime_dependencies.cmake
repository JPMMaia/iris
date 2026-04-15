if(NOT DEFINED PROGRAM)
    message(FATAL_ERROR "PROGRAM not set")
endif()

if(NOT DEFINED DESTINATION)
    message(FATAL_ERROR "DESTINATION not set")
endif()

# Resolve dependencies
file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${PROGRAM}"
    RESOLVED_DEPENDENCIES_VAR deps
    UNRESOLVED_DEPENDENCIES_VAR unresolved
    PRE_EXCLUDE_REGEXES "api-ms-" "ext-ms-"
    POST_EXCLUDE_REGEXES "system32"
)

if(unresolved)
    message(WARNING "Unresolved dependencies: ${unresolved}")
endif()

# Copy each dependency
foreach(dll IN LISTS deps)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${dll}"
            "${DESTINATION}"
        COMMAND_ECHO STDOUT
    )
endforeach()
