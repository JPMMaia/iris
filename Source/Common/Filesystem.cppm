module;

#include <filesystem>
#include <vector>

export module h.common.filesystem;

export import h.common.filesystem_common;

namespace h::common
{
    export std::filesystem::path get_executable_directory();
    
    export std::pmr::vector<std::filesystem::path> get_default_header_search_directories();
    
    export std::pmr::vector<std::filesystem::path> get_default_library_directories();
}
