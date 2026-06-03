include(FetchContent)

FetchContent_Declare(
  extern_watcher
  GIT_REPOSITORY https://github.com/e-dant/watcher.git
  GIT_TAG        release/0.10.0
  EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(extern_watcher)

FetchContent_Declare(
  extern_lsp_framework
  GIT_REPOSITORY https://github.com/leon-bckl/lsp-framework.git
  GIT_TAG        f2c374d138edcc69de56073068705e79ad32d8be
  EXCLUDE_FROM_ALL
)
set(LSP_USE_SANITIZERS OFF)
FetchContent_MakeAvailable(extern_lsp_framework)

add_subdirectory(llvm)
add_subdirectory(clang)
