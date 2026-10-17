# compiler_rt_builtins.cmake
#
# Provides iris_add_compiler_rt_builtins_library(), which builds
# share/iris/lib/iris_builtins.lib -- a small archive of compiler-rt's 128-bit integer helpers.
#
# Problem: arithmetic on Decimal7-Decimal18 is Int64-backed and widens its intermediates to i128,
# so multiply, divide and narrowing casts emit `sdiv i128`. x86-64 has no 128-bit divide, so the
# backend lowers that to a call to compiler-rt's __divti3 -- and MSVC's CRT does not provide it.
# Nothing on the link line did either, so any artifact touching those types failed to link with
# "undefined symbol: __divti3".
#
# Solution: fetch the handful of relevant compiler-rt sources at configure time, compile them into
# an archive, and have iris::compiler::link pass it on every link (see Source/Compiler/Linker_coff.cpp).
#
# Two constraints that are easy to get wrong:
#
#   * They MUST be compiled with clang-cl, not cl.exe. compiler-rt guards every 128-bit routine
#     behind `#ifdef CRT_HAS_128BIT`, which requires __int128. MSVC has no __int128, so cl.exe
#     compiles these files to empty objects and yields an archive defining none of the symbols.
#     (This is also why the vcpkg `compiler-rt` feature is no help: vcpkg builds LLVM with cl.exe.)
#
#   * They MUST be compiled with /Zl. Otherwise clang-cl embeds /DEFAULTLIB:libcmt.lib in each
#     object, dragging the static CRT into every iris link and colliding with the dynamic CRT as
#     duplicate _invalid_parameter_noinfo, _wctype and friends.
#
# Only has effect on Windows; elsewhere the function is a no-op, as glibc/libgcc already supply
# __divti3 and the ELF driver links them via -lc.

# iris_add_compiler_rt_builtins_library()
#
# Defines the Iris_builtins_library target (built as part of ALL) and installs the resulting
# archive into share/iris/lib.
function(iris_add_compiler_rt_builtins_library)
   if (NOT WIN32)
      return()
   endif ()

   find_program(IRIS_BUILTINS_CLANG_CL "clang-cl.exe" REQUIRED)
   find_program(IRIS_BUILTINS_LLVM_LIB "llvm-lib.exe" REQUIRED)

   include(FetchContent)

   # Pinned to a release tag. LLVM publishes no compiler-rt-only archive, and the monorepo tarball
   # is 159 MB for the ~35 KB needed here, so each file is fetched individually with
   # DOWNLOAD_NO_EXTRACT and hash-pinned. They share one SOURCE_DIR because the .c files include
   # the headers by plain name.
   #
   # To move to a newer LLVM: bump the tag and update the hashes below. A stale hash fails the
   # configure with an expected-vs-actual report, so the new values are just read off that error.
   set(_iris_builtins_llvm_tag "llvmorg-22.1.6")
   set(_iris_builtins_url_base
      "https://raw.githubusercontent.com/llvm/llvm-project/${_iris_builtins_llvm_tag}/compiler-rt/lib/builtins"
   )

   set(_iris_builtins_source_directory "${CMAKE_BINARY_DIR}/compiler-rt-builtins-src")
   set(_iris_builtins_object_directory "${CMAKE_BINARY_DIR}/compiler-rt-builtins")

   # Built into ${CMAKE_BINARY_DIR}/bin/share/iris/lib so a non-installed iris resolves it too.
   # Executables land in ${CMAKE_BINARY_DIR}/bin/<config>, and get_share_path() looks in
   # <executable directory>/../share/iris -- mirroring the installed <prefix>/bin layout.
   set(_iris_builtins_library "${CMAKE_BINARY_DIR}/bin/share/iris/lib/iris_builtins.lib")

   # divmodti4/udivmodti4 are the shared cores; the shifts and multiply are here because the
   # backend can emit them for i128 as well.
   set(_iris_builtins_names
      ashlti3 ashrti3 divmodti4 divti3 lshrti3 modti3 multi3 udivmodti4 udivti3 umodti3
   )

   set(_iris_builtins_header_names
      int_lib.h int_types.h int_util.h int_endianness.h int_div_impl.inc
   )

   set(_iris_builtins_file_hashes
      "ashlti3.c=39afba4710d9b24c9c27cccdc8e18a18db10ce3616dc60cdcfa494f0a49a3a60"
      "ashrti3.c=a6da2dc32167fbf369bbb0d185daeaf1b23aa13fda72f832e2097926c9fb2d74"
      "divmodti4.c=bcf5210b5534ab118d836fc8dd015519f5622c3238e0d526706a7848c5a4a144"
      "divti3.c=b8392be2b070cda2a01bc33c7912e06ab83ce9a1ec7995fe0acbe44540cf92ae"
      "lshrti3.c=89da91e31e33c3c62128c32ed1c740856a413c17ac1e7cd75702da37d1cf9fa4"
      "modti3.c=23b5f5af57a8a3ccdab785eec6e486bffef932cee87ff1152b3beedaa6296414"
      "multi3.c=b2e133764668e78119f7c76facbb11f0b4b7c0c6b279fb0b97a589da11d1d060"
      "udivmodti4.c=2fb3eb297e006000935452cb34622b389c5ce39af6e1416b4a11398b78142251"
      "udivti3.c=c0b20f06f6e0e8a6b5423ca6674cf0c57219d07554f6f35c9ecc36bf28915355"
      "umodti3.c=3d88088419d87b486858a4bf54ff08209944ae526c77b8dc5e0c0b6b8001c1e9"
      "int_div_impl.inc=833906405d418f0f421d6155acc5abf79d9902ed37a44903d01c95ff55d3a010"
      "int_endianness.h=ec5175561075845b7b16a92ad5ecdfd42a284de2736dd07e4ca7011b75c37709"
      "int_lib.h=d79f2a9f4756e89fdd7a786fea5361720f40563a6d7c34d078505a236892d28f"
      "int_types.h=bc956eca1626fee5b6242053bafd92bf0b1def230140ee36a9725034aeb02fa8"
      "int_util.h=63d06198ab720a75c20bfd84be895c2746581b129168bae6201e7384bbb0473b"
   )

   set(_iris_builtins_file_names)
   foreach (_iris_builtin_name IN LISTS _iris_builtins_names)
      list(APPEND _iris_builtins_file_names "${_iris_builtin_name}.c")
   endforeach ()
   list(APPEND _iris_builtins_file_names ${_iris_builtins_header_names})

   foreach (_iris_builtins_file_name IN LISTS _iris_builtins_file_names)
      set(_iris_builtins_file_hash "")
      foreach (_iris_builtins_entry IN LISTS _iris_builtins_file_hashes)
         if (_iris_builtins_entry MATCHES "^${_iris_builtins_file_name}=(.+)$")
            set(_iris_builtins_file_hash "${CMAKE_MATCH_1}")
         endif ()
      endforeach ()

      if (_iris_builtins_file_hash STREQUAL "")
         message(FATAL_ERROR "No SHA256 pinned for compiler-rt builtin '${_iris_builtins_file_name}'.")
      endif ()

      string(REPLACE "." "_" _iris_builtins_content_name "iris_builtin_${_iris_builtins_file_name}")
      FetchContent_Declare(${_iris_builtins_content_name}
         URL "${_iris_builtins_url_base}/${_iris_builtins_file_name}"
         URL_HASH "SHA256=${_iris_builtins_file_hash}"
         DOWNLOAD_NO_EXTRACT TRUE
         SOURCE_DIR "${_iris_builtins_source_directory}"
      )
      FetchContent_MakeAvailable(${_iris_builtins_content_name})
   endforeach ()

   set(_iris_builtins_headers)
   foreach (_iris_builtins_header_name IN LISTS _iris_builtins_header_names)
      list(APPEND _iris_builtins_headers "${_iris_builtins_source_directory}/${_iris_builtins_header_name}")
   endforeach ()

   set(_iris_builtins_objects)
   foreach (_iris_builtin_name IN LISTS _iris_builtins_names)
      set(_iris_builtin_object "${_iris_builtins_object_directory}/${_iris_builtin_name}.obj")

      add_custom_command(
         OUTPUT "${_iris_builtin_object}"
         COMMAND ${CMAKE_COMMAND} -E make_directory "${_iris_builtins_object_directory}"
         COMMAND "${IRIS_BUILTINS_CLANG_CL}"
            -c -O2 /Zl /GS-
            -o "${_iris_builtin_object}"
            "${_iris_builtins_source_directory}/${_iris_builtin_name}.c"
         DEPENDS "${_iris_builtins_source_directory}/${_iris_builtin_name}.c" ${_iris_builtins_headers}
         COMMENT "Building 128-bit builtin ${_iris_builtin_name}.c with clang-cl"
         VERBATIM
      )

      list(APPEND _iris_builtins_objects "${_iris_builtin_object}")
   endforeach ()

   add_custom_command(
      OUTPUT "${_iris_builtins_library}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin/share/iris/lib"
      COMMAND "${IRIS_BUILTINS_LLVM_LIB}" "/OUT:${_iris_builtins_library}" ${_iris_builtins_objects}
      DEPENDS ${_iris_builtins_objects}
      COMMENT "Archiving iris_builtins.lib"
      VERBATIM
   )

   add_custom_target(Iris_builtins_library ALL DEPENDS "${_iris_builtins_library}")

   install(FILES "${_iris_builtins_library}" DESTINATION "share/iris/lib" CONFIGURATIONS Debug Release)
endfunction()
