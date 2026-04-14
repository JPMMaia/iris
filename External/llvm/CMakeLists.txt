add_library(llvm_target STATIC)
target_include_directories(llvm_target PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_sources(llvm_target PUBLIC FILE_SET modules TYPE CXX_MODULES FILES "llvm.cppm")


find_package(LLVM CONFIG REQUIRED)

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(HandleLLVMOptions)
add_definitions(${LLVM_DEFINITIONS})
target_compile_definitions(llvm_target PRIVATE ${LLVM_DEFINITIONS} LLVM_DISABLE_ABI_BREAKING_CHECKS_ENFORCING)

target_include_directories(llvm_target PUBLIC ${LLVM_INCLUDE_DIRS})

if (WIN32)
   llvm_map_components_to_libnames(
      llvm_libs
      BinaryFormat
      Core
      LibDriver
      LTO
      MC
      nativecodegen
      OrcDebugging
      OrcJIT
      Object
      Option
      Passes
      ProfileData
      Support
      TargetParser
      WindowsDriver
      WindowsManifest
      X86
      X86asmparser
   )
   
   target_link_libraries(llvm_target PRIVATE ${llvm_libs})

   find_package(LLD REQUIRED CONFIG)
   target_link_libraries(llvm_target PRIVATE lldCOFF)
endif()

find_package(LLD REQUIRED CONFIG)
target_link_libraries(llvm_target PRIVATE lldCommon)

if(WIN32)
   find_package(ZLIB REQUIRED)
   get_target_property(ZLIB_IMPORTED_LOCATION_DEBUG ZLIB::ZLIB IMPORTED_LOCATION_DEBUG)
   
   cmake_path(GET ZLIB_IMPORTED_LOCATION_DEBUG PARENT_PATH ZLIB_IMPORTED_LOCATION_DEBUG_DIRECTORY)
   cmake_path(GET ZLIB_IMPORTED_LOCATION_DEBUG_DIRECTORY PARENT_PATH ZLIB_IMPORTED_LOCATION_DEBUG_PARENT_DIRECTORY)
   find_program(ZLIB_DLL_DEBUG "zlibd1.dll" PATHS "${ZLIB_IMPORTED_LOCATION_DEBUG_PARENT_DIRECTORY}")
   install(FILES ${ZLIB_DLL_DEBUG} TYPE BIN CONFIGURATIONS Debug)
   
   cmake_path(GET ZLIB_IMPORTED_LOCATION_DEBUG_PARENT_DIRECTORY PARENT_PATH ZLIB_IMPORTED_LOCATION_RELEASE_PARENT_DIRECTORY)
   find_program(ZLIB_DLL_RELEASE "zlib1.dll" PATHS "${ZLIB_IMPORTED_LOCATION_RELEASE_PARENT_DIRECTORY}" REQUIRED)
   install(FILES ${ZLIB_DLL_RELEASE} TYPE BIN CONFIGURATIONS Debug Release)
endif()

compile_cxx_header_unit(llvm_target "llvm.h" "angle" "llvm/llvm.h" "llvm_unit" "/wd4244")
