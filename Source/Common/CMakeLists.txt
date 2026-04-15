add_library(Iris_common)
add_library(Iris::Common ALIAS Iris_common)

target_compile_features(Iris_common PRIVATE cxx_std_23)

target_sources(Iris_common
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
   target_sources(Iris_common PRIVATE
      "Filesystem_windows.cpp"
   )
else()
   target_sources(Iris_common PRIVATE
      "Filesystem_linux.cpp"
   )
endif()

if (MSVC)
   target_link_libraries(Iris_common PUBLIC std_module)
endif ()
