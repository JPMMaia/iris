add_library(Iris_common SHARED)
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

target_link_libraries(Iris_common PUBLIC std_module)

find_package(curl CONFIG REQUIRED)
target_link_libraries(Iris_common PRIVATE CURL::libcurl_shared)
install(IMPORTED_RUNTIME_ARTIFACTS CURL::libcurl_shared DESTINATION "bin")

find_package(miniz CONFIG REQUIRED)
target_link_libraries(Iris_common PRIVATE miniz::miniz)
install(IMPORTED_RUNTIME_ARTIFACTS miniz::miniz DESTINATION "bin")

find_package(reproc CONFIG REQUIRED)
target_link_libraries(Iris_common PRIVATE reproc)
install(IMPORTED_RUNTIME_ARTIFACTS reproc DESTINATION "bin")

# zlib has no imported target; install the DLL manually
if(WIN32)
    set(_zlib_lib_dir "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
    if(EXISTS "${_zlib_lib_dir}/debug/bin/zlibd1.dll")
        install(FILES "${_zlib_lib_dir}/debug/bin/zlibd1.dll" DESTINATION "bin")
    endif()
    if(EXISTS "${_zlib_lib_dir}/bin/zlib1.dll")
        install(FILES "${_zlib_lib_dir}/bin/zlib1.dll" DESTINATION "bin")
    endif()
endif()

target_generate_module_exports(Iris_common)
install(TARGETS Iris_common)
