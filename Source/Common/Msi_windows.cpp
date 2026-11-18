module;

#define NOMINMAX
#include <windows.h>
#include <objbase.h>
#include <bcrypt.h>
#include <msi.h>
#include <msiquery.h>
#include <fci.h>

#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>

module iris.common.msi;

import std;

// Writes a Windows Installer database directly with the Windows Installer API, the way WiX does
// under the hood: tables are created with SQL, the files are packed into a CAB with the File
// Compression Interface and the CAB is stored in the database's _Streams table.
namespace iris::common
{
    namespace
    {
        struct Msi_failure
        {
            std::string message;
        };

        [[noreturn]] void fail(std::string message)
        {
            throw Msi_failure{std::move(message)};
        }

        std::wstring to_wide(std::string_view const value)
        {
            if (value.empty())
                return {};

            int const size = ::MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
            std::wstring result(static_cast<std::size_t>(size), L'\0');
            ::MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size);
            return result;
        }

        std::string to_utf8(std::wstring_view const value)
        {
            if (value.empty())
                return {};

            int const size = ::WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
            std::string result(static_cast<std::size_t>(size), '\0');
            ::WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
            return result;
        }

        // Exact matches for std::wstring and paths, which otherwise convert to both overloads.
        std::string to_utf8(std::wstring const& value)
        {
            return to_utf8(std::wstring_view{value});
        }

        std::string to_utf8(std::filesystem::path const& path)
        {
            return to_utf8(std::wstring_view{path.native()});
        }

        // The database is forced to this codepage, so every string stored in it must fit.
        constexpr UINT g_database_codepage = 1252;

        bool fits_database_codepage(std::wstring_view const value)
        {
            if (value.empty())
                return true;

            BOOL used_default_character = FALSE;
            int const size = ::WideCharToMultiByte(
                g_database_codepage,
                WC_NO_BEST_FIT_CHARS,
                value.data(),
                static_cast<int>(value.size()),
                nullptr,
                0,
                nullptr,
                &used_default_character
            );
            return size > 0 && used_default_character == FALSE;
        }

        // ---------------------------------------------------------------------------------
        // GUIDs
        // ---------------------------------------------------------------------------------

        std::wstring create_random_guid()
        {
            GUID guid;
            if (::CoCreateGuid(&guid) != S_OK)
                fail("Failed to create a GUID");

            wchar_t buffer[39];
            ::StringFromGUID2(guid, buffer, 39);
            return std::wstring{buffer};
        }

        // Parses a braced GUID into its 16 bytes in textual (network) order.
        std::optional<std::array<std::uint8_t, 16>> parse_guid_bytes(std::string_view const value)
        {
            if (value.size() != 38 || value.front() != '{' || value.back() != '}')
                return std::nullopt;

            std::array<std::uint8_t, 16> bytes{};
            std::size_t byte_index = 0;
            for (std::size_t index = 1; index + 1 < value.size();)
            {
                if (value[index] == '-')
                {
                    ++index;
                    continue;
                }

                if (byte_index == bytes.size() || index + 2 >= value.size())
                    return std::nullopt;

                std::uint8_t byte = 0;
                std::from_chars_result const result = std::from_chars(value.data() + index, value.data() + index + 2, byte, 16);
                if (result.ec != std::errc{} || result.ptr != value.data() + index + 2)
                    return std::nullopt;

                bytes[byte_index++] = byte;
                index += 2;
            }

            if (byte_index != bytes.size())
                return std::nullopt;

            return bytes;
        }

        std::array<std::uint8_t, 20> sha1(std::span<std::uint8_t const> const data)
        {
            std::array<std::uint8_t, 20> digest{};
            NTSTATUS const status = ::BCryptHash(
                BCRYPT_SHA1_ALG_HANDLE,
                nullptr,
                0,
                const_cast<PUCHAR>(data.data()),
                static_cast<ULONG>(data.size()),
                digest.data(),
                static_cast<ULONG>(digest.size())
            );
            if (!BCRYPT_SUCCESS(status))
                fail(std::format("Failed to hash with SHA-1 (NTSTATUS 0x{:08X})", static_cast<std::uint32_t>(status)));

            return digest;
        }

        // ---------------------------------------------------------------------------------
        // Windows Installer handles and queries
        // ---------------------------------------------------------------------------------

        class Msi_handle
        {
        public:
            Msi_handle() = default;
            explicit Msi_handle(MSIHANDLE const value) : m_value{value} {}
            Msi_handle(Msi_handle const&) = delete;
            Msi_handle& operator=(Msi_handle const&) = delete;
            Msi_handle(Msi_handle&& other) noexcept : m_value{std::exchange(other.m_value, 0)} {}
            Msi_handle& operator=(Msi_handle&& other) noexcept
            {
                if (this != &other)
                {
                    reset();
                    m_value = std::exchange(other.m_value, 0);
                }
                return *this;
            }
            ~Msi_handle() { reset(); }

            void reset()
            {
                if (m_value != 0)
                    ::MsiCloseHandle(m_value);
                m_value = 0;
            }

            MSIHANDLE get() const { return m_value; }
            MSIHANDLE* put() { reset(); return &m_value; }

        private:
            MSIHANDLE m_value = 0;
        };

        std::string get_last_msi_error()
        {
            Msi_handle const record{::MsiGetLastErrorRecord()};
            if (record.get() == 0)
                return {};

            DWORD size = 0;
            wchar_t empty[1] = {L'\0'};
            ::MsiFormatRecordW(0, record.get(), empty, &size);
            std::wstring text(static_cast<std::size_t>(size) + 1, L'\0');
            size += 1;
            if (::MsiFormatRecordW(0, record.get(), text.data(), &size) != ERROR_SUCCESS)
                return {};
            text.resize(size);
            return std::format(" ({})", to_utf8(text));
        }

        void check(UINT const result, std::string_view const operation)
        {
            if (result != ERROR_SUCCESS)
                fail(std::format("{} failed with error {}{}", operation, result, get_last_msi_error()));
        }

        using Value = std::variant<std::nullptr_t, std::wstring, int>;

        class Database
        {
        public:
            explicit Database(MSIHANDLE const handle) : m_handle{handle} {}

            MSIHANDLE get() const { return m_handle.get(); }

            void execute(std::wstring const& sql)
            {
                Msi_handle view;
                check(::MsiDatabaseOpenViewW(m_handle.get(), sql.c_str(), view.put()), std::format("Preparing '{}'", to_utf8(sql)));
                check(::MsiViewExecute(view.get(), 0), std::format("Executing '{}'", to_utf8(sql)));
                check(::MsiViewClose(view.get()), "Closing a view");
            }

            void insert(std::wstring_view const table, std::initializer_list<std::wstring_view> const columns, std::initializer_list<Value> const values)
            {
                if (columns.size() != values.size())
                    fail("Internal error: column and value counts differ");

                std::wstring sql = std::format(L"INSERT INTO `{}` (", table);
                std::wstring placeholders;
                bool first = true;
                for (std::wstring_view const column : columns)
                {
                    if (!first)
                    {
                        sql += L", ";
                        placeholders += L", ";
                    }
                    sql += std::format(L"`{}`", column);
                    placeholders += L"?";
                    first = false;
                }
                sql += std::format(L") VALUES ({})", placeholders);

                Msi_handle const record{::MsiCreateRecord(static_cast<UINT>(values.size()))};
                if (record.get() == 0)
                    fail("Failed to create a Windows Installer record");

                UINT field = 1;
                for (Value const& value : values)
                {
                    if (std::wstring const* const string = std::get_if<std::wstring>(&value))
                    {
                        if (!fits_database_codepage(*string))
                            fail(std::format("'{}' contains characters a Windows Installer package cannot store (codepage {})", to_utf8(*string), g_database_codepage));
                        check(::MsiRecordSetStringW(record.get(), field, string->c_str()), "Setting a record string");
                    }
                    else if (int const* const integer = std::get_if<int>(&value))
                    {
                        check(::MsiRecordSetInteger(record.get(), field, *integer), "Setting a record integer");
                    }
                    ++field;
                }

                Msi_handle view;
                check(::MsiDatabaseOpenViewW(m_handle.get(), sql.c_str(), view.put()), std::format("Preparing '{}'", to_utf8(sql)));
                check(::MsiViewExecute(view.get(), record.get()), std::format("Inserting into {}", to_utf8(table)));
                check(::MsiViewClose(view.get()), "Closing a view");
            }

        private:
            Msi_handle m_handle;
        };

        // ---------------------------------------------------------------------------------
        // Short (8.3) file names
        // ---------------------------------------------------------------------------------

        bool is_short_name_character(wchar_t const character)
        {
            return (character >= L'A' && character <= L'Z')
                || (character >= L'a' && character <= L'z')
                || (character >= L'0' && character <= L'9')
                || character == L'_'
                || character == L'-';
        }

        std::wstring to_upper(std::wstring_view const value)
        {
            std::wstring result{value};
            for (wchar_t& character : result)
            {
                if (character >= L'a' && character <= L'z')
                    character = static_cast<wchar_t>(character - L'a' + L'A');
            }
            return result;
        }

        bool is_valid_short_name(std::wstring_view const name)
        {
            std::size_t const dot = name.find(L'.');
            std::wstring_view const base = name.substr(0, dot);
            std::wstring_view const extension = dot == std::wstring_view::npos ? std::wstring_view{} : name.substr(dot + 1);

            if (base.empty() || base.size() > 8 || extension.size() > 3)
                return false;
            if (dot != std::wstring_view::npos && extension.empty())
                return false;

            return std::ranges::all_of(base, is_short_name_character)
                && std::ranges::all_of(extension, is_short_name_character);
        }

        // Hands out names unique within one directory, where files and subdirectories share a
        // namespace. A long name that is already a valid 8.3 name is used as is.
        class Short_name_generator
        {
        public:
            // Returns the Windows Installer Filename value: "SHORT~1.EXT|long name", or just the
            // name when it is already a valid short name.
            std::wstring get_filename(std::wstring_view const long_name)
            {
                if (long_name.find(L'|') != std::wstring_view::npos)
                    fail(std::format("'{}' contains '|', which Windows Installer file names cannot", to_utf8(long_name)));

                if (is_valid_short_name(long_name) && m_used.insert(to_upper(long_name)).second)
                    return std::wstring{long_name};

                std::size_t const dot = long_name.rfind(L'.');
                std::wstring_view const stem = dot == std::wstring_view::npos ? long_name : long_name.substr(0, dot);
                std::wstring_view const extension_source = dot == std::wstring_view::npos ? std::wstring_view{} : long_name.substr(dot + 1);

                std::wstring base;
                for (wchar_t const character : stem)
                {
                    if (is_short_name_character(character))
                        base.push_back(character);
                }
                if (base.empty())
                    base = L"FILE";

                std::wstring extension;
                for (wchar_t const character : extension_source)
                {
                    if (is_short_name_character(character) && extension.size() < 3)
                        extension.push_back(character);
                }

                for (std::uint32_t number = 1; number < 10'000'000; ++number)
                {
                    std::wstring const suffix = std::format(L"~{}", number);
                    std::wstring candidate = base.substr(0, 8 - suffix.size()) + suffix;
                    if (!extension.empty())
                        candidate += L"." + extension;
                    candidate = to_upper(candidate);

                    if (m_used.insert(candidate).second)
                        return std::format(L"{}|{}", candidate, long_name);
                }

                fail(std::format("Ran out of short names for '{}'", to_utf8(long_name)));
            }

        private:
            std::set<std::wstring> m_used;
        };

        // ---------------------------------------------------------------------------------
        // Cabinet creation (FCI)
        // ---------------------------------------------------------------------------------

        // FCI passes paths through as narrow strings, so they are carried as UTF-8 and widened
        // in the callbacks below.
        struct Cabinet_context
        {
            std::filesystem::path working_directory;
            std::uint32_t temporary_file_count = 0;
        };

        FNFCIALLOC(cabinet_alloc)
        {
            return std::malloc(cb);
        }

        FNFCIFREE(cabinet_free)
        {
            std::free(memory);
        }

        FNFCIOPEN(cabinet_open)
        {
            int handle = -1;
            errno_t const error = ::_wsopen_s(&handle, to_wide(pszFile).c_str(), oflag | _O_BINARY, _SH_DENYNO, pmode);
            if (error != 0)
            {
                *err = error;
                return -1;
            }
            return handle;
        }

        FNFCIREAD(cabinet_read)
        {
            int const result = ::_read(static_cast<int>(hf), memory, cb);
            if (result < 0)
                *err = errno;
            return static_cast<UINT>(result);
        }

        FNFCIWRITE(cabinet_write)
        {
            int const result = ::_write(static_cast<int>(hf), memory, cb);
            if (result < 0)
                *err = errno;
            return static_cast<UINT>(result);
        }

        FNFCICLOSE(cabinet_close)
        {
            int const result = ::_close(static_cast<int>(hf));
            if (result != 0)
                *err = errno;
            return result;
        }

        FNFCISEEK(cabinet_seek)
        {
            long const result = ::_lseek(static_cast<int>(hf), dist, seektype);
            if (result < 0)
                *err = errno;
            return result;
        }

        FNFCIDELETE(cabinet_delete)
        {
            if (!::DeleteFileW(to_wide(pszFile).c_str()))
            {
                *err = static_cast<int>(::GetLastError());
                return -1;
            }
            return 0;
        }

        FNFCIGETTEMPFILE(cabinet_get_temporary_file)
        {
            Cabinet_context& context = *static_cast<Cabinet_context*>(pv);
            std::string const name = to_utf8(context.working_directory / std::format("fci{}.tmp", context.temporary_file_count++));
            if (name.size() + 1 > static_cast<std::size_t>(cbTempName))
                return FALSE;

            std::memcpy(pszTempName, name.c_str(), name.size() + 1);
            return TRUE;
        }

        FNFCIFILEPLACED(cabinet_file_placed)
        {
            return 0;
        }

        FNFCIGETNEXTCABINET(cabinet_get_next_cabinet)
        {
            // Everything goes into one cabinet; asking for another means it overflowed.
            return FALSE;
        }

        FNFCISTATUS(cabinet_status)
        {
            return 0;
        }

        FNFCIGETOPENINFO(cabinet_get_open_info)
        {
            std::wstring const path = to_wide(pszName);

            WIN32_FILE_ATTRIBUTE_DATA data;
            if (!::GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data))
            {
                *err = static_cast<int>(::GetLastError());
                return -1;
            }

            FILETIME local_time;
            ::FileTimeToLocalFileTime(&data.ftLastWriteTime, &local_time);
            ::FileTimeToDosDateTime(&local_time, pdate, ptime);
            *pattribs = static_cast<USHORT>(data.dwFileAttributes & (_A_RDONLY | _A_HIDDEN | _A_SYSTEM | _A_ARCH));

            int handle = -1;
            errno_t const error = ::_wsopen_s(&handle, path.c_str(), _O_RDONLY | _O_BINARY, _SH_DENYNO, 0);
            if (error != 0)
            {
                *err = error;
                return -1;
            }
            return handle;
        }

        struct Cabinet_file
        {
            std::filesystem::path source_path;
            std::string name_in_cabinet;
        };

        void create_cabinet(
            std::filesystem::path const& cabinet_path,
            std::span<Cabinet_file const> const files
        )
        {
            Cabinet_context context
            {
                .working_directory = cabinet_path.parent_path(),
            };

            CCAB ccab{};
            ccab.cb = 0x7FFFFFFF;
            ccab.cbFolderThresh = 0x7FFFFFFF;
            ccab.iCab = 1;
            ccab.iDisk = 1;

            std::string const cabinet_directory = to_utf8(cabinet_path.parent_path()) + "\\";
            std::string const cabinet_name = to_utf8(cabinet_path.filename());
            if (cabinet_directory.size() >= sizeof(ccab.szCabPath) || cabinet_name.size() >= sizeof(ccab.szCab))
                fail(std::format("Cabinet path '{}' is too long", to_utf8(cabinet_path)));
            std::memcpy(ccab.szCabPath, cabinet_directory.c_str(), cabinet_directory.size() + 1);
            std::memcpy(ccab.szCab, cabinet_name.c_str(), cabinet_name.size() + 1);

            ERF error{};
            HFCI const fci = ::FCICreate(
                &error,
                cabinet_file_placed,
                cabinet_alloc,
                cabinet_free,
                cabinet_open,
                cabinet_read,
                cabinet_write,
                cabinet_close,
                cabinet_seek,
                cabinet_delete,
                cabinet_get_temporary_file,
                &ccab,
                &context
            );
            if (fci == nullptr)
                fail(std::format("Failed to start creating the cabinet (FCI error {}, {})", error.erfOper, error.erfType));

            struct Fci_destroyer
            {
                HFCI fci;
                ~Fci_destroyer() { ::FCIDestroy(fci); }
            } const destroyer{fci};

            for (Cabinet_file const& file : files)
            {
                std::string source = to_utf8(file.source_path);
                std::string name = file.name_in_cabinet;
                if (!::FCIAddFile(
                    fci,
                    source.data(),
                    name.data(),
                    FALSE,
                    cabinet_get_next_cabinet,
                    cabinet_status,
                    cabinet_get_open_info,
                    TCOMPfromLZXWindow(21)
                ))
                {
                    fail(std::format("Failed to add '{}' to the cabinet (FCI error {}, {})", source, error.erfOper, error.erfType));
                }
            }

            if (!::FCIFlushCabinet(fci, FALSE, cabinet_get_next_cabinet, cabinet_status))
                fail(std::format("Failed to write the cabinet (FCI error {}, {})", error.erfOper, error.erfType));
        }

        // ---------------------------------------------------------------------------------
        // Package layout
        // ---------------------------------------------------------------------------------

        struct Package_directory
        {
            std::wstring key;
            Short_name_generator names;
        };

        struct Package_file
        {
            std::filesystem::path source_path;
            std::filesystem::path relative_path;
            std::wstring key;
            std::wstring component_key;
            std::wstring directory_key;
            std::wstring filename;
            std::uint64_t size = 0;
        };

        constexpr std::wstring_view g_install_directory_key = L"INSTALLDIR";
        constexpr std::wstring_view g_program_menu_directory_key = L"PROGRAMMENUDIR";
        constexpr std::wstring_view g_feature_key = L"Complete";
        constexpr std::wstring_view g_cabinet_stream_name = L"product.cab";
        constexpr std::wstring_view g_older_version_property = L"OLDERVERSIONDETECTED";
        constexpr std::wstring_view g_newer_version_property = L"NEWERVERSIONDETECTED";

        constexpr int g_component_attributes_64bit = 256;
        constexpr int g_feature_attributes_disallow_advertise = 8;
        constexpr int g_upgrade_attributes_migrate_features = 1;
        constexpr int g_upgrade_attributes_only_detect = 2;
        constexpr int g_upgrade_attributes_version_max_inclusive = 0x200;
        constexpr int g_remove_file_on_uninstall = 2;

        std::wstring to_key_relative(std::filesystem::path const& relative_path)
        {
            std::wstring value = relative_path.lexically_normal().generic_wstring();
            while (!value.empty() && value.back() == L'/')
                value.pop_back();
            if (value == L".")
                value.clear();
            return value;
        }

        void create_tables(Database& database)
        {
            database.execute(L"CREATE TABLE `Property` (`Property` CHAR(72) NOT NULL, `Value` CHAR(0) NOT NULL LOCALIZABLE PRIMARY KEY `Property`)");
            database.execute(L"CREATE TABLE `Directory` (`Directory` CHAR(72) NOT NULL, `Directory_Parent` CHAR(72), `DefaultDir` CHAR(255) NOT NULL LOCALIZABLE PRIMARY KEY `Directory`)");
            database.execute(L"CREATE TABLE `Component` (`Component` CHAR(72) NOT NULL, `ComponentId` CHAR(38), `Directory_` CHAR(72) NOT NULL, `Attributes` SHORT NOT NULL, `Condition` CHAR(255), `KeyPath` CHAR(72) PRIMARY KEY `Component`)");
            database.execute(L"CREATE TABLE `Feature` (`Feature` CHAR(38) NOT NULL, `Feature_Parent` CHAR(38), `Title` CHAR(64) LOCALIZABLE, `Description` CHAR(255) LOCALIZABLE, `Display` SHORT, `Level` SHORT NOT NULL, `Directory_` CHAR(72), `Attributes` SHORT NOT NULL PRIMARY KEY `Feature`)");
            database.execute(L"CREATE TABLE `FeatureComponents` (`Feature_` CHAR(38) NOT NULL, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Feature_`, `Component_`)");
            database.execute(L"CREATE TABLE `File` (`File` CHAR(72) NOT NULL, `Component_` CHAR(72) NOT NULL, `FileName` CHAR(255) NOT NULL LOCALIZABLE, `FileSize` LONG NOT NULL, `Version` CHAR(72), `Language` CHAR(20), `Attributes` SHORT, `Sequence` LONG NOT NULL PRIMARY KEY `File`)");
            database.execute(L"CREATE TABLE `Media` (`DiskId` SHORT NOT NULL, `LastSequence` LONG NOT NULL, `DiskPrompt` CHAR(64) LOCALIZABLE, `Cabinet` CHAR(255), `VolumeLabel` CHAR(32), `Source` CHAR(72) PRIMARY KEY `DiskId`)");
            database.execute(L"CREATE TABLE `Upgrade` (`UpgradeCode` CHAR(38) NOT NULL, `VersionMin` CHAR(20), `VersionMax` CHAR(20), `Language` CHAR(255), `Attributes` LONG NOT NULL, `Remove` CHAR(255), `ActionProperty` CHAR(72) NOT NULL PRIMARY KEY `UpgradeCode`, `VersionMin`, `VersionMax`, `Language`, `Attributes`)");
            database.execute(L"CREATE TABLE `LaunchCondition` (`Condition` CHAR(255) NOT NULL, `Description` CHAR(255) NOT NULL LOCALIZABLE PRIMARY KEY `Condition`)");
            database.execute(L"CREATE TABLE `Environment` (`Environment` CHAR(72) NOT NULL, `Name` CHAR(255) NOT NULL LOCALIZABLE, `Value` CHAR(255) LOCALIZABLE, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Environment`)");
            database.execute(L"CREATE TABLE `Shortcut` (`Shortcut` CHAR(72) NOT NULL, `Directory_` CHAR(72) NOT NULL, `Name` CHAR(128) NOT NULL LOCALIZABLE, `Component_` CHAR(72) NOT NULL, `Target` CHAR(72) NOT NULL, `Arguments` CHAR(255), `Description` CHAR(255) LOCALIZABLE, `Hotkey` SHORT, `Icon_` CHAR(72), `IconIndex` SHORT, `ShowCmd` SHORT, `WkDir` CHAR(72) PRIMARY KEY `Shortcut`)");
            database.execute(L"CREATE TABLE `RemoveFile` (`FileKey` CHAR(72) NOT NULL, `Component_` CHAR(72) NOT NULL, `FileName` CHAR(255) LOCALIZABLE, `DirProperty` CHAR(72) NOT NULL, `InstallMode` SHORT NOT NULL PRIMARY KEY `FileKey`)");

            for (std::wstring_view const table : {L"InstallExecuteSequence", L"InstallUISequence", L"AdminExecuteSequence", L"AdminUISequence"})
                database.execute(std::format(L"CREATE TABLE `{}` (`Action` CHAR(72) NOT NULL, `Condition` CHAR(255), `Sequence` SHORT PRIMARY KEY `Action`)", table));
        }

        void insert_sequence(Database& database, std::wstring_view const table, std::initializer_list<std::pair<std::wstring_view, int>> const actions)
        {
            for (auto const& [action, sequence] : actions)
                database.insert(table, {L"Action", L"Sequence"}, {std::wstring{action}, sequence});
        }

        // Standard actions at their documented sequence numbers. RemoveExistingProducts right
        // after InstallInitialize removes the old version completely before the new one goes in.
        void insert_sequences(Database& database)
        {
            insert_sequence(database, L"InstallExecuteSequence", {
                {L"LaunchConditions", 100},
                {L"FindRelatedProducts", 200},
                {L"ValidateProductID", 700},
                {L"CostInitialize", 800},
                {L"FileCost", 900},
                {L"CostFinalize", 1000},
                {L"MigrateFeatureStates", 1200},
                {L"InstallValidate", 1400},
                {L"InstallInitialize", 1500},
                {L"RemoveExistingProducts", 1510},
                {L"ProcessComponents", 1600},
                {L"UnpublishFeatures", 1800},
                {L"RemoveShortcuts", 3200},
                {L"RemoveEnvironmentStrings", 3300},
                {L"RemoveFiles", 3500},
                {L"RemoveFolders", 3600},
                {L"CreateFolders", 3700},
                {L"InstallFiles", 4000},
                {L"CreateShortcuts", 4500},
                {L"WriteEnvironmentStrings", 5200},
                {L"RegisterUser", 6000},
                {L"RegisterProduct", 6100},
                {L"PublishFeatures", 6300},
                {L"PublishProduct", 6400},
                {L"InstallFinalize", 6600},
            });

            insert_sequence(database, L"InstallUISequence", {
                {L"LaunchConditions", 100},
                {L"FindRelatedProducts", 200},
                {L"ValidateProductID", 700},
                {L"CostInitialize", 800},
                {L"FileCost", 900},
                {L"CostFinalize", 1000},
                {L"MigrateFeatureStates", 1200},
                {L"ExecuteAction", 1300},
            });

            insert_sequence(database, L"AdminExecuteSequence", {
                {L"CostInitialize", 800},
                {L"FileCost", 900},
                {L"CostFinalize", 1000},
                {L"InstallValidate", 1400},
                {L"InstallInitialize", 1500},
                {L"InstallAdminPackage", 3900},
                {L"InstallFiles", 4000},
                {L"InstallFinalize", 6600},
            });

            insert_sequence(database, L"AdminUISequence", {
                {L"CostInitialize", 800},
                {L"FileCost", 900},
                {L"CostFinalize", 1000},
                {L"ExecuteAction", 1300},
            });
        }

        void force_codepage(Database& database, std::filesystem::path const& working_directory)
        {
            std::filesystem::path const idt_path = working_directory / "codepage.idt";
            {
                std::ofstream stream{idt_path, std::ios::binary};
                stream << std::format("\r\n\r\n{}\t_ForceCodepage\r\n", g_database_codepage);
                if (!stream)
                    fail(std::format("Failed to write '{}'", to_utf8(idt_path)));
            }

            check(::MsiDatabaseImportW(database.get(), working_directory.wstring().c_str(), L"codepage.idt"), "Setting the database codepage");
        }

        void set_summary_string(MSIHANDLE const summary, UINT const property, std::wstring const& value)
        {
            check(::MsiSummaryInfoSetPropertyW(summary, property, VT_LPSTR, 0, nullptr, value.c_str()), std::format("Setting summary property {}", property));
        }

        void set_summary_integer(MSIHANDLE const summary, UINT const property, UINT const type, int const value)
        {
            check(::MsiSummaryInfoSetPropertyW(summary, property, type, value, nullptr, nullptr), std::format("Setting summary property {}", property));
        }

        void write_summary_information(Database& database, Msi_package_options const& options)
        {
            // Summary property IDs, from the Windows Installer documentation.
            constexpr UINT pid_codepage = 1;
            constexpr UINT pid_title = 2;
            constexpr UINT pid_subject = 3;
            constexpr UINT pid_author = 4;
            constexpr UINT pid_keywords = 5;
            constexpr UINT pid_comments = 6;
            constexpr UINT pid_template = 7;
            constexpr UINT pid_revision_number = 9;
            constexpr UINT pid_create_time = 12;
            constexpr UINT pid_last_save_time = 13;
            constexpr UINT pid_page_count = 14;
            constexpr UINT pid_word_count = 15;
            constexpr UINT pid_application_name = 18;
            constexpr UINT pid_security = 19;

            Msi_handle summary;
            check(::MsiGetSummaryInformationW(database.get(), nullptr, 20, summary.put()), "Opening the summary information");

            std::wstring const product_name = to_wide(options.product_name);

            set_summary_integer(summary.get(), pid_codepage, VT_I2, static_cast<int>(g_database_codepage));
            set_summary_string(summary.get(), pid_title, L"Installation Database");
            set_summary_string(summary.get(), pid_subject, product_name);
            set_summary_string(summary.get(), pid_author, to_wide(options.manufacturer));
            set_summary_string(summary.get(), pid_keywords, L"Installer");
            set_summary_string(summary.get(), pid_comments, std::format(L"This installer database contains the logic and data required to install {}.", product_name));
            set_summary_string(summary.get(), pid_template, std::format(L"{};1033", to_wide(options.architecture)));
            // The package code: unique for every package built.
            set_summary_string(summary.get(), pid_revision_number, create_random_guid());

            FILETIME now;
            ::GetSystemTimeAsFileTime(&now);
            check(::MsiSummaryInfoSetPropertyW(summary.get(), pid_create_time, VT_FILETIME, 0, &now, nullptr), "Setting the creation time");
            check(::MsiSummaryInfoSetPropertyW(summary.get(), pid_last_save_time, VT_FILETIME, 0, &now, nullptr), "Setting the save time");

            // Schema 500 (Windows Installer 5.0); word count 2: compressed files, long names.
            set_summary_integer(summary.get(), pid_page_count, VT_I4, 500);
            set_summary_integer(summary.get(), pid_word_count, VT_I4, 2);
            set_summary_string(summary.get(), pid_application_name, L"Iris");
            // Read-only recommended.
            set_summary_integer(summary.get(), pid_security, VT_I4, 2);

            check(::MsiSummaryInfoPersist(summary.get()), "Saving the summary information");
        }

        void validate_options(Msi_package_options const& options)
        {
            if (!std::filesystem::is_directory(options.source_directory))
                fail(std::format("Source directory does not exist: {}", to_utf8(options.source_directory)));
            if (options.product_name.empty())
                fail("The product name is empty");
            if (options.manufacturer.empty())
                fail("The manufacturer is empty");
            if (!parse_guid_bytes(options.upgrade_code).has_value())
                fail(std::format("Upgrade code '{}' is not a braced GUID", options.upgrade_code));
            if (options.architecture != "x64" && options.architecture != "Arm64")
                fail(std::format("Unsupported Windows Installer architecture '{}' (expected x64 or Arm64)", options.architecture));

            std::array<std::uint32_t, 3> parts{};
            std::array<std::uint32_t, 3> constexpr limits{255, 255, 65535};
            std::string_view remaining = options.product_version;
            for (std::size_t index = 0; index < parts.size(); ++index)
            {
                std::size_t const separator = remaining.find('.');
                std::string_view const part = remaining.substr(0, separator);
                std::from_chars_result const result = std::from_chars(part.data(), part.data() + part.size(), parts[index]);
                bool const is_last = index + 1 == parts.size();
                if (part.empty() || result.ec != std::errc{} || result.ptr != part.data() + part.size() || parts[index] > limits[index]
                    || (is_last != (separator == std::string_view::npos)))
                {
                    fail(std::format("Product version '{}' must be major.minor.build with major and minor at most 255 and build at most 65535", options.product_version));
                }
                remaining = is_last ? std::string_view{} : remaining.substr(separator + 1);
            }
        }

        void write_package(Msi_package_options const& options, std::filesystem::path const& working_directory)
        {
            validate_options(options);

            // Collect the files and directories, sorted so parents come before their children
            // and the output does not depend on directory enumeration order.
            std::vector<std::filesystem::path> relative_files;
            std::vector<std::filesystem::path> relative_directories;
            for (std::filesystem::directory_entry const& entry : std::filesystem::recursive_directory_iterator(options.source_directory))
            {
                std::filesystem::path const relative = std::filesystem::relative(entry.path(), options.source_directory);
                if (entry.is_directory())
                    relative_directories.push_back(relative);
                else if (entry.is_regular_file())
                    relative_files.push_back(relative);
            }
            std::ranges::sort(relative_directories);
            std::ranges::sort(relative_files);

            if (relative_files.empty())
                fail(std::format("There are no files to package in '{}'", to_utf8(options.source_directory)));

            MSIHANDLE database_handle = 0;
            check(::MsiOpenDatabaseW(options.output_path.wstring().c_str(), reinterpret_cast<LPCWSTR>(MSIDBOPEN_CREATE), &database_handle), std::format("Creating '{}'", to_utf8(options.output_path)));
            Database database{database_handle};

            force_codepage(database, working_directory);
            create_tables(database);
            insert_sequences(database);

            std::wstring const product_name = to_wide(options.product_name);
            std::wstring const manufacturer = to_wide(options.manufacturer);
            std::wstring const upgrade_code = to_wide(options.upgrade_code);
            std::wstring const product_version = to_wide(options.product_version);

            // Directories. The well-known folders' DefaultDir only names them in administrative
            // installs; at install time Windows Installer resolves them to the real folders.
            Short_name_generator program_files_names;
            Short_name_generator manufacturer_names;
            Short_name_generator program_menu_names;
            Short_name_generator shortcut_names;

            database.insert(L"Directory", {L"Directory", L"Directory_Parent", L"DefaultDir"}, {std::wstring{L"TARGETDIR"}, nullptr, std::wstring{L"SourceDir"}});
            database.insert(L"Directory", {L"Directory", L"Directory_Parent", L"DefaultDir"}, {std::wstring{L"ProgramFiles64Folder"}, std::wstring{L"TARGETDIR"}, std::wstring{L"PFiles64"}});
            database.insert(L"Directory", {L"Directory", L"Directory_Parent", L"DefaultDir"}, {std::wstring{L"MANUFACTURERDIR"}, std::wstring{L"ProgramFiles64Folder"}, program_files_names.get_filename(manufacturer)});
            database.insert(L"Directory", {L"Directory", L"Directory_Parent", L"DefaultDir"}, {std::wstring{g_install_directory_key}, std::wstring{L"MANUFACTURERDIR"}, manufacturer_names.get_filename(product_name)});

            std::map<std::wstring, Package_directory> directories;
            directories.emplace(std::wstring{}, Package_directory{.key = std::wstring{g_install_directory_key}});

            for (std::filesystem::path const& relative_directory : relative_directories)
            {
                std::wstring const relative_key = to_key_relative(relative_directory);
                Package_directory& parent = directories.at(to_key_relative(relative_directory.parent_path()));

                std::wstring const key = std::format(L"d{}", directories.size());
                std::wstring const default_directory = parent.names.get_filename(relative_directory.filename().wstring());
                database.insert(L"Directory", {L"Directory", L"Directory_Parent", L"DefaultDir"}, {key, parent.key, default_directory});

                directories.emplace(relative_key, Package_directory{.key = key});
            }

            if (!options.shortcuts.empty())
            {
                database.insert(L"Directory", {L"Directory", L"Directory_Parent", L"DefaultDir"}, {std::wstring{L"ProgramMenuFolder"}, std::wstring{L"TARGETDIR"}, std::wstring{L"PMenu"}});
                database.insert(L"Directory", {L"Directory", L"Directory_Parent", L"DefaultDir"}, {std::wstring{g_program_menu_directory_key}, std::wstring{L"ProgramMenuFolder"}, program_menu_names.get_filename(product_name)});
            }

            // Files, one component each with the file as its key path. Component GUIDs derive
            // from the upgrade code and the file's path, so a file keeps its component across
            // versions, as the component rules require.
            std::vector<Package_file> files;
            files.reserve(relative_files.size());
            for (std::filesystem::path const& relative_file : relative_files)
            {
                std::size_t const index = files.size() + 1;
                Package_directory& directory = directories.at(to_key_relative(relative_file.parent_path()));

                std::error_code error_code;
                std::uint64_t const size = std::filesystem::file_size(options.source_directory / relative_file, error_code);
                if (error_code)
                    fail(std::format("Failed to read the size of '{}': {}", to_utf8(relative_file), error_code.message()));
                if (size > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()))
                    fail(std::format("'{}' is too large for a Windows Installer package", to_utf8(relative_file)));

                files.push_back(
                    Package_file
                    {
                        .source_path = options.source_directory / relative_file,
                        .relative_path = relative_file,
                        .key = std::format(L"f{}", index),
                        .component_key = std::format(L"c{}", index),
                        .directory_key = directory.key,
                        .filename = directory.names.get_filename(relative_file.filename().wstring()),
                        .size = size,
                    }
                );
            }

            database.insert(L"Feature",
                {L"Feature", L"Title", L"Display", L"Level", L"Directory_", L"Attributes"},
                {std::wstring{g_feature_key}, product_name, 1, 1, std::wstring{g_install_directory_key}, g_feature_attributes_disallow_advertise});

            int sequence = 0;
            for (Package_file const& file : files)
            {
                std::optional<std::pmr::string> const component_id = create_name_based_guid(options.upgrade_code, to_utf8(to_upper(file.relative_path.generic_wstring())));
                if (!component_id.has_value())
                    fail("Internal error: failed to derive a component GUID");

                database.insert(L"Component",
                    {L"Component", L"ComponentId", L"Directory_", L"Attributes", L"KeyPath"},
                    {file.component_key, to_wide(component_id.value()), file.directory_key, g_component_attributes_64bit, file.key});
                database.insert(L"FeatureComponents", {L"Feature_", L"Component_"}, {std::wstring{g_feature_key}, file.component_key});
                database.insert(L"File",
                    {L"File", L"Component_", L"FileName", L"FileSize", L"Attributes", L"Sequence"},
                    {file.key, file.component_key, file.filename, static_cast<int>(file.size), 0, ++sequence});
            }

            database.insert(L"Media",
                {L"DiskId", L"LastSequence", L"Cabinet"},
                {1, sequence, std::format(L"#{}", g_cabinet_stream_name)});

            auto const find_file = [&](std::filesystem::path const& relative_path) -> Package_file const*
            {
                std::wstring const wanted = to_upper(to_key_relative(relative_path));
                for (Package_file const& file : files)
                {
                    if (to_upper(to_key_relative(file.relative_path)) == wanted)
                        return &file;
                }
                return nullptr;
            };

            // Shortcuts. Each lives in the component of the file it points at, which also
            // removes the Start menu folder on uninstall.
            for (std::size_t index = 0; index < options.shortcuts.size(); ++index)
            {
                Msi_shortcut const& shortcut = options.shortcuts[index];
                Package_file const* const target = find_file(shortcut.target);
                if (target == nullptr)
                    fail(std::format("Shortcut target '{}' is not in the package", to_utf8(shortcut.target)));

                database.insert(L"Shortcut",
                    {L"Shortcut", L"Directory_", L"Name", L"Component_", L"Target", L"WkDir"},
                    {
                        std::format(L"s{}", index + 1),
                        std::wstring{g_program_menu_directory_key},
                        shortcut_names.get_filename(to_wide(shortcut.name)),
                        target->component_key,
                        std::format(L"[#{}]", target->key),
                        target->directory_key,
                    });
                database.insert(L"RemoveFile",
                    {L"FileKey", L"Component_", L"DirProperty", L"InstallMode"},
                    {std::format(L"r{}", index + 1), target->component_key, std::wstring{g_program_menu_directory_key}, g_remove_file_on_uninstall});
            }

            // PATH. "=-*" sets a system variable on install and takes the value back out on
            // uninstall; "[~];" appends to whatever PATH already holds.
            if (options.path_directory.has_value())
            {
                std::wstring const relative_key = to_key_relative(options.path_directory.value());
                Package_file const* owner = nullptr;
                for (Package_file const& file : files)
                {
                    if (to_upper(to_key_relative(file.relative_path.parent_path())) == to_upper(relative_key))
                    {
                        owner = &file;
                        break;
                    }
                }
                if (owner == nullptr)
                    fail(std::format("PATH directory '{}' contains no files in the package", to_utf8(options.path_directory.value())));

                std::wstring directory_value = L"[INSTALLDIR]" + relative_key;
                std::ranges::replace(directory_value, L'/', L'\\');
                if (directory_value.ends_with(L'\\'))
                    directory_value.pop_back();

                database.insert(L"Environment",
                    {L"Environment", L"Name", L"Value", L"Component_"},
                    {std::wstring{L"PATH"}, std::wstring{L"=-*PATH"}, L"[~];" + directory_value, owner->component_key});
            }

            // Upgrades: replace any version up to and including this one, and refuse to install
            // over a newer one.
            database.insert(L"Upgrade",
                {L"UpgradeCode", L"VersionMax", L"Attributes", L"ActionProperty"},
                {upgrade_code, product_version, g_upgrade_attributes_migrate_features | g_upgrade_attributes_version_max_inclusive, std::wstring{g_older_version_property}});
            database.insert(L"Upgrade",
                {L"UpgradeCode", L"VersionMin", L"Attributes", L"ActionProperty"},
                {upgrade_code, product_version, g_upgrade_attributes_only_detect, std::wstring{g_newer_version_property}});
            database.insert(L"LaunchCondition",
                {L"Condition", L"Description"},
                {std::format(L"NOT {}", g_newer_version_property), std::wstring{L"A newer version of [ProductName] is already installed."}});

            for (auto const& [property, value] : std::initializer_list<std::pair<std::wstring_view, std::wstring>>{
                {L"ProductCode", create_random_guid()},
                {L"ProductName", product_name},
                {L"ProductVersion", product_version},
                {L"ProductLanguage", L"1033"},
                {L"Manufacturer", manufacturer},
                {L"UpgradeCode", upgrade_code},
                {L"ALLUSERS", L"1"},
                {L"ARPNOMODIFY", L"1"},
                {L"SecureCustomProperties", std::format(L"{};{}", g_older_version_property, g_newer_version_property)},
            })
            {
                database.insert(L"Property", {L"Property", L"Value"}, {std::wstring{property}, value});
            }

            // Pack the files in File-table sequence order and embed the cabinet.
            std::vector<Cabinet_file> cabinet_files;
            cabinet_files.reserve(files.size());
            for (Package_file const& file : files)
                cabinet_files.push_back(Cabinet_file{.source_path = file.source_path, .name_in_cabinet = to_utf8(file.key)});

            std::filesystem::path const cabinet_path = working_directory / "product.cab";
            create_cabinet(cabinet_path, cabinet_files);

            {
                Msi_handle const record{::MsiCreateRecord(2)};
                check(::MsiRecordSetStringW(record.get(), 1, std::wstring{g_cabinet_stream_name}.c_str()), "Setting the cabinet stream name");
                check(::MsiRecordSetStreamW(record.get(), 2, cabinet_path.wstring().c_str()), "Reading the cabinet");

                Msi_handle view;
                check(::MsiDatabaseOpenViewW(database.get(), L"INSERT INTO `_Streams` (`Name`, `Data`) VALUES (?, ?)", view.put()), "Preparing the cabinet insert");
                check(::MsiViewExecute(view.get(), record.get()), "Embedding the cabinet");
                check(::MsiViewClose(view.get()), "Closing a view");
            }

            write_summary_information(database, options);

            check(::MsiDatabaseCommit(database.get()), "Committing the database");
        }
    }

    std::optional<std::pmr::string> create_name_based_guid(
        std::string_view const namespace_guid,
        std::string_view const name
    )
    {
        std::optional<std::array<std::uint8_t, 16>> const namespace_bytes = parse_guid_bytes(namespace_guid);
        if (!namespace_bytes.has_value())
            return std::nullopt;

        std::vector<std::uint8_t> input{namespace_bytes->begin(), namespace_bytes->end()};
        input.insert(input.end(), name.begin(), name.end());

        std::array<std::uint8_t, 20> const digest = sha1(input);

        std::array<std::uint8_t, 16> bytes{};
        std::copy_n(digest.begin(), bytes.size(), bytes.begin());
        bytes[6] = static_cast<std::uint8_t>((bytes[6] & 0x0F) | 0x50);
        bytes[8] = static_cast<std::uint8_t>((bytes[8] & 0x3F) | 0x80);

        std::pmr::string result{"{"};
        for (std::size_t index = 0; index < bytes.size(); ++index)
        {
            if (index == 4 || index == 6 || index == 8 || index == 10)
                result.push_back('-');
            result += std::format("{:02X}", bytes[index]);
        }
        result.push_back('}');
        return result;
    }

    std::optional<std::pmr::string> create_msi_from_directory(Msi_package_options const& options)
    {
        std::printf("Creating Windows Installer package: %s\n", options.output_path.generic_string().c_str());

        std::error_code error_code;
        std::filesystem::path const working_directory = std::filesystem::temp_directory_path(error_code) / std::format("iris_msi_{}_{}", ::GetCurrentProcessId(), std::chrono::steady_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(working_directory, error_code);
        if (error_code)
            return std::pmr::string{std::format("Failed to create a working directory: {}", error_code.message())};

        if (!options.output_path.parent_path().empty())
            std::filesystem::create_directories(options.output_path.parent_path(), error_code);
        std::filesystem::remove(options.output_path, error_code);

        std::optional<std::pmr::string> error;
        try
        {
            write_package(options, working_directory);
        }
        catch (Msi_failure const& failure)
        {
            error = std::pmr::string{failure.message};
        }
        catch (std::filesystem::filesystem_error const& exception)
        {
            error = std::pmr::string{exception.what()};
        }

        std::filesystem::remove_all(working_directory, error_code);

        if (error.has_value())
            std::filesystem::remove(options.output_path, error_code);

        return error;
    }
}
