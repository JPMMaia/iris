export module iris.common.filesystem;

import std;

export import iris.common.filesystem_common;

namespace iris::common
{
    export std::filesystem::path get_executable_directory();
    
    export std::pmr::vector<std::filesystem::path> get_default_header_search_directories();
    
    export std::pmr::vector<std::filesystem::path> get_default_library_directories();
}
