include(FetchContent)

FetchContent_Declare(
  extern_watcher
  GIT_REPOSITORY https://github.com/e-dant/watcher.git
  GIT_TAG        release/0.10.0
)
FetchContent_MakeAvailable(extern_watcher)

FetchContent_Declare(
  extern_lsp_framework
  GIT_REPOSITORY https://github.com/leon-bckl/lsp-framework.git
  GIT_TAG        fa2a8f7de4fe612e10badc15201c2176de850c14
)
set(LSP_USE_SANITIZERS OFF)
FetchContent_MakeAvailable(extern_lsp_framework)

add_subdirectory(llvm)
add_subdirectory(clang)
