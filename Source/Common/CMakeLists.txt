add_library(Iris_common SHARED)
add_library(Iris::Common ALIAS Iris_common)

target_compile_features(Iris_common PRIVATE cxx_std_23)

target_sources(Iris_common
   PUBLIC FILE_SET modules TYPE CXX_MODULES
      FILES
         "Archive.cppm"
         "Common.cppm"
         "Filesystem_common.cppm"
         "Filesystem.cppm"
   PRIVATE
      "Archive.cpp"
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

find_package(7zip CONFIG REQUIRED)
target_link_libraries(Iris_common PRIVATE 7zip::7zip)
install(IMPORTED_RUNTIME_ARTIFACTS 7zip::7zip DESTINATION "bin")
if(WIN32)
   # SysAllocStringLen / SysFreeString, used to exchange BSTRs with 7-Zip.
   target_link_libraries(Iris_common PRIVATE OleAut32)
endif()

# Archive.cpp resolves CreateObject with LoadLibrary/dlopen rather than through the
# import library, so nothing references 7zip at link time and the usual runtime-dependency
# copy does not pick it up. Place it next to the binaries explicitly.
add_custom_command(TARGET Iris_common POST_BUILD
   COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "$<TARGET_FILE:7zip::7zip>" "$<TARGET_FILE_DIR:Iris_common>"
   VERBATIM
)

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

if(BUILD_TESTING)
   add_executable(Iris_common_tests)
   target_link_libraries(Iris_common_tests PRIVATE Iris::Common)

   find_package(Catch2 CONFIG REQUIRED)
   target_link_libraries(Iris_common_tests PRIVATE Catch2::Catch2 Catch2::Catch2WithMain)

   target_sources(Iris_common_tests PRIVATE
      "Archive.tests.cpp"
   )

   include(Catch)
   catch_discover_tests(Iris_common_tests)
endif()
