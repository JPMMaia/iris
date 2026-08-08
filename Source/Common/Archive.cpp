module;

// MyInitGuid.h must be included exactly once per project, before any header
// that declares COM interfaces, so that the 7-Zip IIDs/CLSIDs are defined
// rather than merely declared. Archive.cpp is that place for Iris_common.
#include <Common/MyWindows.h>
#include <Common/MyInitGuid.h>

#include <Common/MyCom.h>

#include <7zip/Archive/IArchive.h>
#include <7zip/IStream.h>
#include <7zip/PropID.h>

#ifndef _WIN32
#include <dlfcn.h>
#endif

// 7-Zip format handler CLSIDs: {23170F69-40C1-278A-1000-000110<id>0000}
#define IRIS_DEFINE_ARCHIVE_GUID(name, id) Z7_DEFINE_GUID(name, \
    0x23170F69, 0x40C1, 0x278A, 0x10, 0x00, 0x00, 0x01, 0x10, id, 0x00, 0x00);

module iris.common.archive;

import std;

// These have C language linkage, so they are attached to the global module even
// though they appear in the module purview.
IRIS_DEFINE_ARCHIVE_GUID(CLSID_Format_Zip, 0x01)
IRIS_DEFINE_ARCHIVE_GUID(CLSID_Format_7z, 0x07)

// CreateObject is exported by 7zip.dll (CPP/7zip/Archive/ArchiveExports.cpp) but
// is absent from the vcpkg port's import library, whose .def only covers the
// LzmaLib API. So it is resolved at runtime, the way 7-Zip's own Client7z sample
// loads the format plugin.
using Create_object_function = HRESULT(STDAPICALLTYPE*)(GUID const* clsid, GUID const* iid, void** out_object);

namespace iris::common
{
    namespace
    {
        std::pmr::string to_error(std::string_view const message)
        {
            return std::pmr::string{message};
        }

        Create_object_function load_create_object()
        {
            static Create_object_function const function = []() -> Create_object_function
            {
#ifdef _WIN32
                HMODULE const library = ::LoadLibraryW(L"7zip.dll");
                if (library == nullptr)
                    return nullptr;

                return reinterpret_cast<Create_object_function>(
                    reinterpret_cast<void*>(::GetProcAddress(library, "CreateObject"))
                );
#else
                void* const library = ::dlopen("lib7zip.so", RTLD_LAZY | RTLD_LOCAL);
                if (library == nullptr)
                    return nullptr;

                return reinterpret_cast<Create_object_function>(::dlsym(library, "CreateObject"));
#endif
            }();

            return function;
        }

        // Used for diagnostics. u8string() rather than string(), which throws for
        // entry names the native narrow encoding cannot represent.
        std::string to_utf8(std::wstring_view const value)
        {
            std::u8string const utf8 = std::filesystem::path{value}.u8string();
            return std::string{reinterpret_cast<char const*>(utf8.data()), utf8.size()};
        }

        // Frees the parts of a PROPVARIANT that own memory. Only the variant
        // types this file reads are handled; 7-Zip's CPropVariant lives in
        // CPP/Windows/PropVariant.h, which the vcpkg port does not install.
        void clear_prop_variant(PROPVARIANT& value)
        {
            if (value.vt == VT_BSTR && value.bstrVal != nullptr)
                ::SysFreeString(value.bstrVal);

            std::memset(&value, 0, sizeof(value));
            value.vt = VT_EMPTY;
        }

        std::optional<std::wstring> get_string_property(IInArchive& archive, UInt32 const index, PROPID const property_id)
        {
            PROPVARIANT value;
            std::memset(&value, 0, sizeof(value));
            value.vt = VT_EMPTY;

            if (archive.GetProperty(index, property_id, &value) != S_OK)
            {
                clear_prop_variant(value);
                return std::nullopt;
            }

            std::optional<std::wstring> result;
            if (value.vt == VT_BSTR && value.bstrVal != nullptr)
                result = std::wstring{value.bstrVal, ::SysStringLen(value.bstrVal)};

            clear_prop_variant(value);
            return result;
        }

        bool get_bool_property(IInArchive& archive, UInt32 const index, PROPID const property_id)
        {
            PROPVARIANT value;
            std::memset(&value, 0, sizeof(value));
            value.vt = VT_EMPTY;

            if (archive.GetProperty(index, property_id, &value) != S_OK)
            {
                clear_prop_variant(value);
                return false;
            }

            bool const result = (value.vt == VT_BOOL) && (value.boolVal != VARIANT_FALSE);
            clear_prop_variant(value);
            return result;
        }

        std::optional<GUID> get_format_clsid(std::filesystem::path const& archive_path)
        {
            std::string extension = archive_path.extension().string();
            for (char& character : extension)
                character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));

            if (extension == ".7z")
                return CLSID_Format_7z;

            if (extension == ".zip")
                return CLSID_Format_Zip;

            return std::nullopt;
        }

        // Returns the first path component of an archive entry name, or an
        // empty view when the name has no separator.
        std::wstring_view first_component(std::wstring_view const name)
        {
            std::size_t const separator = name.find_first_of(L"/\\");
            if (separator == std::wstring_view::npos)
                return {};

            return name.substr(0, separator);
        }

        std::wstring_view drop_first_component(std::wstring_view const name)
        {
            std::size_t const separator = name.find_first_of(L"/\\");
            if (separator == std::wstring_view::npos)
                return {};

            return name.substr(separator + 1);
        }

        // Archives produced by create_archive_from_directory wrap everything in
        // a single root directory. Strip that root, but only when every entry
        // agrees on it, so archives whose files sit at the top level are left
        // alone.
        bool has_common_root_directory(std::span<std::wstring const> const names)
        {
            // Take the first nested entry's leading component as the candidate root.
            std::wstring_view root;
            for (std::wstring const& name : names)
            {
                std::wstring_view const component = first_component(name);
                if (!component.empty())
                {
                    root = component;
                    break;
                }
            }

            if (root.empty())
                return false;

            // Every entry must either be that root directory itself or live inside it.
            for (std::wstring const& name : names)
            {
                std::wstring_view const view{name};

                if (view == root)
                    continue;

                bool const is_inside = view.size() > root.size()
                    && view.starts_with(root)
                    && (view[root.size()] == L'/' || view[root.size()] == L'\\');

                if (!is_inside)
                    return false;
            }

            return true;
        }

        // Rejects entries that would escape the destination directory (zip slip).
        std::optional<std::filesystem::path> resolve_output_path(
            std::filesystem::path const& destination_directory,
            std::wstring_view const relative_name
        )
        {
            std::filesystem::path relative_path{relative_name};
            if (relative_path.is_absolute() || relative_path.has_root_name())
                return std::nullopt;

            std::filesystem::path const candidate = (destination_directory / relative_path).lexically_normal();
            std::filesystem::path const root = destination_directory.lexically_normal();

            auto const [root_end, _] = std::mismatch(root.begin(), root.end(), candidate.begin(), candidate.end());
            if (root_end != root.end())
                return std::nullopt;

            return candidate;
        }

        class File_in_stream Z7_final :
            public IInStream,
            public CMyUnknownImp
        {
            Z7_IFACES_IMP_UNK_1(IInStream)
            Z7_IFACE_COM7_IMP(ISequentialInStream)

            std::ifstream m_stream;

        public:
            bool open(std::filesystem::path const& path)
            {
                m_stream.open(path, std::ios::binary);
                return m_stream.is_open();
            }
        };

        Z7_COM7F_IMF(File_in_stream::Read(void* data, UInt32 size, UInt32* processed_size))
        {
            if (processed_size != nullptr)
                *processed_size = 0;

            if (size == 0)
                return S_OK;

            m_stream.read(static_cast<char*>(data), static_cast<std::streamsize>(size));
            std::streamsize const read_count = m_stream.gcount();

            // eof is expected on the final read; only a hard failure is an error.
            if (m_stream.bad())
                return E_FAIL;

            if (m_stream.eof())
                m_stream.clear();

            if (processed_size != nullptr)
                *processed_size = static_cast<UInt32>(read_count);

            return S_OK;
        }

        Z7_COM7F_IMF(File_in_stream::Seek(Int64 offset, UInt32 seek_origin, UInt64* new_position))
        {
            std::ios::seekdir direction;
            switch (seek_origin)
            {
            case STREAM_SEEK_SET: direction = std::ios::beg; break;
            case STREAM_SEEK_CUR: direction = std::ios::cur; break;
            case STREAM_SEEK_END: direction = std::ios::end; break;
            default: return STG_E_INVALIDFUNCTION;
            }

            m_stream.clear();
            m_stream.seekg(static_cast<std::streamoff>(offset), direction);
            if (m_stream.fail())
                return E_FAIL;

            if (new_position != nullptr)
                *new_position = static_cast<UInt64>(m_stream.tellg());

            return S_OK;
        }

        class File_out_stream Z7_final :
            public IOutStream,
            public CMyUnknownImp
        {
            Z7_IFACES_IMP_UNK_1(IOutStream)
            Z7_IFACE_COM7_IMP(ISequentialOutStream)

            std::fstream m_stream;

        public:
            bool create(std::filesystem::path const& path)
            {
                m_stream.open(path, std::ios::binary | std::ios::out | std::ios::trunc);
                return m_stream.is_open();
            }

            void close()
            {
                if (m_stream.is_open())
                    m_stream.close();
            }

            bool failed() const
            {
                return !m_stream.good() && !m_stream.eof();
            }
        };

        Z7_COM7F_IMF(File_out_stream::Write(void const* data, UInt32 size, UInt32* processed_size))
        {
            if (processed_size != nullptr)
                *processed_size = 0;

            if (size == 0)
                return S_OK;

            m_stream.write(static_cast<char const*>(data), static_cast<std::streamsize>(size));
            if (!m_stream.good())
                return E_FAIL;

            if (processed_size != nullptr)
                *processed_size = size;

            return S_OK;
        }

        Z7_COM7F_IMF(File_out_stream::Seek(Int64 offset, UInt32 seek_origin, UInt64* new_position))
        {
            std::ios::seekdir direction;
            switch (seek_origin)
            {
            case STREAM_SEEK_SET: direction = std::ios::beg; break;
            case STREAM_SEEK_CUR: direction = std::ios::cur; break;
            case STREAM_SEEK_END: direction = std::ios::end; break;
            default: return STG_E_INVALIDFUNCTION;
            }

            m_stream.clear();
            m_stream.seekp(static_cast<std::streamoff>(offset), direction);
            if (m_stream.fail())
                return E_FAIL;

            if (new_position != nullptr)
                *new_position = static_cast<UInt64>(m_stream.tellp());

            return S_OK;
        }

        Z7_COM7F_IMF(File_out_stream::SetSize(UInt64 /* new_size */))
        {
            // std::fstream cannot truncate in place. The archive writers only
            // use SetSize to shrink a finished archive, and every stream here
            // is created with std::ios::trunc, so reporting success is safe.
            return S_OK;
        }

        class Extract_callback Z7_final :
            public IArchiveExtractCallback,
            public CMyUnknownImp
        {
            Z7_IFACES_IMP_UNK_1(IArchiveExtractCallback)
            Z7_IFACE_COM7_IMP(IProgress)

            IInArchive& m_archive;
            std::filesystem::path m_destination_directory;
            std::span<std::wstring const> m_item_names;
            bool m_strip_root_directory;

            std::optional<std::pmr::string> m_error;

            File_out_stream* m_current_stream = nullptr;
            CMyComPtr<ISequentialOutStream> m_current_stream_holder;
            std::wstring m_current_name;

            void record_error(std::string_view const message)
            {
                if (!m_error.has_value())
                    m_error = to_error(message);
            }

        public:
            Extract_callback(
                IInArchive& archive,
                std::filesystem::path destination_directory,
                std::span<std::wstring const> const item_names,
                bool const strip_root_directory
            ) :
                m_archive{archive},
                m_destination_directory{std::move(destination_directory)},
                m_item_names{item_names},
                m_strip_root_directory{strip_root_directory}
            {
            }

            std::optional<std::pmr::string> const& error() const
            {
                return m_error;
            }
        };

        Z7_COM7F_IMF(Extract_callback::SetTotal(UInt64 /* total */))
        {
            return S_OK;
        }

        Z7_COM7F_IMF(Extract_callback::SetCompleted(UInt64 const* /* completed */))
        {
            return S_OK;
        }

        Z7_COM7F_IMF(Extract_callback::GetStream(UInt32 index, ISequentialOutStream** out_stream, Int32 ask_extract_mode))
        {
            *out_stream = nullptr;
            m_current_stream = nullptr;
            m_current_stream_holder.Release();

            if (index >= m_item_names.size())
            {
                record_error(std::format("Archive reported an out-of-range item index {}", index));
                return E_FAIL;
            }

            std::wstring const& name = m_item_names[index];
            m_current_name = name;

            if (ask_extract_mode != NArchive::NExtract::NAskMode::kExtract)
                return S_OK;

            std::wstring_view const relative_name = m_strip_root_directory
                ? drop_first_component(name)
                : std::wstring_view{name};

            // The archive's own root directory entry maps onto the destination itself.
            if (relative_name.empty())
            {
                std::error_code error_code;
                std::filesystem::create_directories(m_destination_directory, error_code);
                return S_OK;
            }

            std::optional<std::filesystem::path> const output_path = resolve_output_path(m_destination_directory, relative_name);
            if (!output_path.has_value())
            {
                record_error(std::format("Entry '{}' resolves outside the destination directory", to_utf8(name)));
                return E_FAIL;
            }

            bool const is_directory = get_bool_property(m_archive, index, kpidIsDir);

            std::error_code error_code;
            std::filesystem::create_directories(is_directory ? *output_path : output_path->parent_path(), error_code);
            if (error_code)
            {
                record_error(std::format("Failed to create directory for '{}': {}", to_utf8(name), error_code.message()));
                return E_FAIL;
            }

            if (is_directory)
                return S_OK;

            auto stream = std::make_unique<File_out_stream>();
            if (!stream->create(*output_path))
            {
                record_error(std::format("Failed to create file '{}'", output_path->generic_string()));
                return E_FAIL;
            }

            m_current_stream = stream.get();
            m_current_stream_holder = stream.release();
            *out_stream = m_current_stream_holder;
            m_current_stream_holder->AddRef();

            return S_OK;
        }

        Z7_COM7F_IMF(Extract_callback::PrepareOperation(Int32 /* ask_extract_mode */))
        {
            return S_OK;
        }

        Z7_COM7F_IMF(Extract_callback::SetOperationResult(Int32 operation_result))
        {
            bool const write_failed = (m_current_stream != nullptr) && m_current_stream->failed();

            if (m_current_stream != nullptr)
                m_current_stream->close();

            m_current_stream = nullptr;
            m_current_stream_holder.Release();

            if (operation_result != NArchive::NExtract::NOperationResult::kOK)
            {
                record_error(
                    std::format(
                        "Entry '{}' failed to extract (7-Zip operation result {})",
                        to_utf8(m_current_name),
                        operation_result
                    )
                );
                return S_OK;
            }

            if (write_failed)
                record_error(std::format("Failed to write entry '{}' to disk", to_utf8(m_current_name)));

            return S_OK;
        }

        struct Update_item
        {
            std::filesystem::path source_path;
            std::wstring archive_name;
            bool is_directory = false;
            std::uint64_t size = 0;
        };

        class Update_callback Z7_final :
            public IArchiveUpdateCallback,
            public CMyUnknownImp
        {
            Z7_IFACES_IMP_UNK_1(IArchiveUpdateCallback)
            Z7_IFACE_COM7_IMP(IProgress)

            std::span<Update_item const> m_items;
            std::optional<std::pmr::string> m_error;

            void record_error(std::string_view const message)
            {
                if (!m_error.has_value())
                    m_error = to_error(message);
            }

        public:
            explicit Update_callback(std::span<Update_item const> const items) :
                m_items{items}
            {
            }

            std::optional<std::pmr::string> const& error() const
            {
                return m_error;
            }
        };

        Z7_COM7F_IMF(Update_callback::SetTotal(UInt64 /* total */))
        {
            return S_OK;
        }

        Z7_COM7F_IMF(Update_callback::SetCompleted(UInt64 const* /* completed */))
        {
            return S_OK;
        }

        Z7_COM7F_IMF(Update_callback::GetUpdateItemInfo(UInt32 /* index */, Int32* new_data, Int32* new_properties, UInt32* index_in_archive))
        {
            // Always creating a fresh archive, so every item is new.
            if (new_data != nullptr)
                *new_data = 1;
            if (new_properties != nullptr)
                *new_properties = 1;
            if (index_in_archive != nullptr)
                *index_in_archive = static_cast<UInt32>(-1);

            return S_OK;
        }

        Z7_COM7F_IMF(Update_callback::GetProperty(UInt32 index, PROPID property_id, PROPVARIANT* value))
        {
            std::memset(value, 0, sizeof(*value));
            value->vt = VT_EMPTY;

            if (index >= m_items.size())
                return E_INVALIDARG;

            Update_item const& item = m_items[index];

            switch (property_id)
            {
            case kpidPath:
            {
                BSTR const path = ::SysAllocStringLen(item.archive_name.c_str(), static_cast<UINT>(item.archive_name.size()));
                if (path == nullptr)
                    return E_OUTOFMEMORY;

                value->vt = VT_BSTR;
                value->bstrVal = path;
                break;
            }
            case kpidIsDir:
                value->vt = VT_BOOL;
                value->boolVal = item.is_directory ? VARIANT_TRUE : VARIANT_FALSE;
                break;
            case kpidSize:
                value->vt = VT_UI8;
                value->uhVal.QuadPart = item.size;
                break;
            case kpidAttrib:
                value->vt = VT_UI4;
                value->ulVal = item.is_directory ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_ARCHIVE;
                break;
            case kpidIsAnti:
                value->vt = VT_BOOL;
                value->boolVal = VARIANT_FALSE;
                break;
            default:
                break;
            }

            return S_OK;
        }

        Z7_COM7F_IMF(Update_callback::GetStream(UInt32 index, ISequentialInStream** in_stream))
        {
            *in_stream = nullptr;

            if (index >= m_items.size())
                return E_INVALIDARG;

            Update_item const& item = m_items[index];
            if (item.is_directory)
                return S_OK;

            auto stream = std::make_unique<File_in_stream>();
            if (!stream->open(item.source_path))
            {
                record_error(std::format("Failed to open '{}' for reading", item.source_path.generic_string()));
                return E_FAIL;
            }

            CMyComPtr<ISequentialInStream> holder = stream.release();
            *in_stream = holder.Detach();

            return S_OK;
        }

        Z7_COM7F_IMF(Update_callback::SetOperationResult(Int32 operation_result))
        {
            if (operation_result != NArchive::NUpdate::NOperationResult::kOK)
                record_error(std::format("7-Zip reported update operation result {}", operation_result));

            return S_OK;
        }
    }

    std::optional<std::pmr::string> extract_archive(
        std::filesystem::path const& archive_path,
        std::filesystem::path const& destination_directory
    )
    {
        if (std::filesystem::exists(destination_directory))
        {
            std::printf("Skipped extracting '%s' since '%s' already exists.\n",
                archive_path.generic_string().c_str(),
                destination_directory.generic_string().c_str());
            return std::nullopt;
        }

        if (!std::filesystem::exists(archive_path))
            return to_error(std::format("Archive does not exist: {}", archive_path.generic_string()));

        std::optional<GUID> const clsid = get_format_clsid(archive_path);
        if (!clsid.has_value())
        {
            return to_error(
                std::format("Unsupported archive extension '{}' (expected .7z or .zip)", archive_path.extension().string())
            );
        }

        std::printf("Extracting '%s' to '%s'.\n",
            archive_path.generic_string().c_str(),
            destination_directory.generic_string().c_str());

        Create_object_function const create_object = load_create_object();
        if (create_object == nullptr)
            return to_error("Failed to load the 7-Zip library (7zip.dll)");

        CMyComPtr<IInArchive> archive;
        if (create_object(&clsid.value(), &IID_IInArchive, reinterpret_cast<void**>(&archive)) != S_OK)
            return to_error(std::format("Failed to create a 7-Zip handler for '{}'", archive_path.generic_string()));

        auto* const in_stream_implementation = new File_in_stream{};
        CMyComPtr<IInStream> in_stream{in_stream_implementation};
        if (!in_stream_implementation->open(archive_path))
            return to_error(std::format("Failed to open archive: {}", archive_path.generic_string()));

        UInt64 const max_check_start_position = 1 << 23;
        if (archive->Open(in_stream, &max_check_start_position, nullptr) != S_OK)
            return to_error(std::format("Failed to open archive (unrecognized or corrupt): {}", archive_path.generic_string()));

        UInt32 number_of_items = 0;
        if (archive->GetNumberOfItems(&number_of_items) != S_OK)
        {
            archive->Close();
            return to_error(std::format("Failed to read the item count of '{}'", archive_path.generic_string()));
        }

        std::vector<std::wstring> item_names;
        item_names.reserve(number_of_items);
        for (UInt32 index = 0; index < number_of_items; ++index)
        {
            std::optional<std::wstring> name = get_string_property(*archive, index, kpidPath);
            if (!name.has_value())
            {
                archive->Close();
                return to_error(std::format("Failed to read the name of item {} in '{}'", index, archive_path.generic_string()));
            }

            item_names.push_back(std::move(*name));
        }

        bool const strip_root_directory = has_common_root_directory(item_names);

        auto* const callback_implementation = new Extract_callback{
            *archive,
            destination_directory,
            item_names,
            strip_root_directory
        };
        CMyComPtr<IArchiveExtractCallback> callback{callback_implementation};

        std::error_code error_code;
        std::filesystem::create_directories(destination_directory, error_code);
        if (error_code)
        {
            archive->Close();
            return to_error(std::format("Failed to create '{}': {}", destination_directory.generic_string(), error_code.message()));
        }

        // Extract everything in one call: solid blocks are decoded once this way.
        HRESULT const extract_result = archive->Extract(nullptr, static_cast<UInt32>(-1), 0, callback);

        std::optional<std::pmr::string> error = callback_implementation->error();

        archive->Close();

        if (error.has_value())
            return error;

        if (extract_result != S_OK)
            return to_error(std::format("Failed to extract '{}' (HRESULT 0x{:08X})", archive_path.generic_string(), static_cast<std::uint32_t>(extract_result)));

        return std::nullopt;
    }

    std::optional<std::pmr::string> create_archive_from_directory(
        std::filesystem::path const& source_directory,
        std::filesystem::path const& output_archive_path
    )
    {
        std::printf("Creating archive: %s\n", output_archive_path.generic_string().c_str());

        if (!std::filesystem::is_directory(source_directory))
            return to_error(std::format("Source directory does not exist: {}", source_directory.generic_string()));

        std::optional<GUID> const clsid = get_format_clsid(output_archive_path);
        if (!clsid.has_value())
        {
            return to_error(
                std::format("Unsupported archive extension '{}' (expected .7z or .zip)", output_archive_path.extension().string())
            );
        }

        std::error_code error_code;
        std::filesystem::create_directories(output_archive_path.parent_path(), error_code);
        if (error_code)
            return to_error(std::format("Failed to create output directory: {}", error_code.message()));

        // Everything is placed under a single root directory named after the
        // archive, which extract_archive strips again on the way out.
        std::wstring const root_directory_name = output_archive_path.stem().wstring();

        std::vector<Update_item> items;

        // The root directory itself, so the archive carries a single top-level
        // folder the way 7-Zip's own archives do.
        {
            Update_item root_item;
            root_item.source_path = source_directory;
            root_item.archive_name = root_directory_name;
            root_item.is_directory = true;
            items.push_back(std::move(root_item));
        }

        for (auto const& entry : std::filesystem::recursive_directory_iterator(source_directory, error_code))
        {
            bool const is_directory = entry.is_directory(error_code);
            if (!is_directory && !entry.is_regular_file(error_code))
                continue;

            std::filesystem::path const relative_path = std::filesystem::relative(entry.path(), source_directory, error_code);
            if (error_code)
                return to_error(std::format("Failed to resolve '{}': {}", entry.path().generic_string(), error_code.message()));

            Update_item item;
            item.source_path = entry.path();
            item.archive_name = (std::filesystem::path{root_directory_name} / relative_path).wstring();
            item.is_directory = is_directory;
            item.size = is_directory ? 0 : static_cast<std::uint64_t>(entry.file_size(error_code));

            items.push_back(std::move(item));
        }

        if (error_code)
            return to_error(std::format("Failed to walk '{}': {}", source_directory.generic_string(), error_code.message()));

        Create_object_function const create_object = load_create_object();
        if (create_object == nullptr)
            return to_error("Failed to load the 7-Zip library (7zip.dll)");

        CMyComPtr<IOutArchive> archive;
        if (create_object(&clsid.value(), &IID_IOutArchive, reinterpret_cast<void**>(&archive)) != S_OK)
            return to_error(std::format("Failed to create a 7-Zip writer for '{}'", output_archive_path.generic_string()));

        auto* const out_stream_implementation = new File_out_stream{};
        CMyComPtr<IOutStream> out_stream{out_stream_implementation};
        if (!out_stream_implementation->create(output_archive_path))
            return to_error(std::format("Failed to create '{}'", output_archive_path.generic_string()));

        auto* const callback_implementation = new Update_callback{items};
        CMyComPtr<IArchiveUpdateCallback> callback{callback_implementation};

        HRESULT const update_result = archive->UpdateItems(out_stream, static_cast<UInt32>(items.size()), callback);

        std::optional<std::pmr::string> error = callback_implementation->error();

        out_stream_implementation->close();

        if (!error.has_value() && update_result != S_OK)
        {
            error = to_error(
                std::format(
                    "Failed to create '{}' (HRESULT 0x{:08X})",
                    output_archive_path.generic_string(),
                    static_cast<std::uint32_t>(update_result)
                )
            );
        }

        if (error.has_value())
        {
            std::error_code remove_error;
            std::filesystem::remove(output_archive_path, remove_error);
            return error;
        }

        return std::nullopt;
    }
}
