set(STD_MODULE_OUT ${CMAKE_BINARY_DIR}/std_modules)
set(STD_IFC ${STD_MODULE_OUT}/std.ifc)
set(STD_COMPAT_IFC ${STD_MODULE_OUT}/std_compat.ifc)

file(MAKE_DIRECTORY ${STD_MODULE_OUT})

add_custom_command(
    OUTPUT ${STD_IFC}
    COMMAND ${CMAKE_CXX_COMPILER}
        /std:c++latest
        /EHsc
        /nologo
        /W4
        /D_SCL_SECURE_NO_WARNINGS
        /c
        "$ENV{VCToolsInstallDir}/modules/std.ixx"
        /ifcOutput ${STD_IFC}
    DEPENDS "$ENV{VCToolsInstallDir}/modules/std.ixx"
    COMMENT "Building std module interface"
)

add_custom_command(
    OUTPUT ${STD_COMPAT_IFC}
    COMMAND ${CMAKE_CXX_COMPILER}
        /std:c++latest
        /EHsc
        /nologo
        /W4
        /D_SCL_SECURE_NO_WARNINGS
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
add_dependencies(std_module std_module_custom_target)
