add_library(H_common)
add_library(H::Common ALIAS H_common)

target_compile_features(H_common PRIVATE cxx_std_23)
target_compile_definitions(H_common PUBLIC "_SCL_SECURE_NO_WARNINGS")

target_sources(H_common
   PUBLIC FILE_SET modules TYPE CXX_MODULES
      FILES
         "Common.cppm"
         "Filesystem_common.cppm"
         "Filesystem.cppm"
   PRIVATE
      "Common.cpp"
      "Filesystem_common.cpp"
)

if(WIN32)
   target_sources(H_common PRIVATE
      "Filesystem_windows.cpp"
   )
else()
   target_sources(H_common PRIVATE
      "Filesystem_linux.cpp"
   )
endif()

if (MSVC)
   target_link_libraries(H_common PUBLIC std_module)
endif ()
