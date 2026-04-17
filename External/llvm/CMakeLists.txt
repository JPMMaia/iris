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

set(additional_compile_options)
if (MSVC)
   list(APPEND additional_compile_options "/wd4244")
endif ()
compile_cxx_header_unit(llvm_target "llvm.h" "angle" "llvm/llvm.h" "llvm_unit" "${additional_compile_options}")
