module;

#include <memory_resource>
#include <optional>
#include <string>
#include <string_view>

module iris.common.msi;

namespace iris::common
{
    std::optional<std::pmr::string> create_msi_from_directory(Msi_package_options const& /* options */)
    {
        return std::pmr::string{"Windows Installer packages can only be created on Windows."};
    }

    std::optional<std::pmr::string> create_name_based_guid(
        std::string_view const /* namespace_guid */,
        std::string_view const /* name */
    )
    {
        return std::nullopt;
    }
}
