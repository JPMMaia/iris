module;

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>

#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang-c/Index.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Support/VirtualFileSystem.h>
#include <xxhash.h>

module h.c_header_hash;

import h.common;

namespace h::c
{
    std::unique_ptr<clang::CompilerInstance> create_clang_instance(
        std::filesystem::path const& input_file_path,
        std::filesystem::path const& output_file_path,
        std::optional<std::string_view> const target_triple,
        std::span<std::filesystem::path const> const include_directories
    )
    {
        std::unique_ptr<clang::CompilerInstance> compiler_instance = std::make_unique<clang::CompilerInstance>();

        compiler_instance->createDiagnostics();

        compiler_instance->createFileManager();
        compiler_instance->createSourceManager(compiler_instance->getFileManager());

        std::pmr::vector<std::pmr::string> arguments_storage;
        arguments_storage.reserve(9+include_directories.size());
        arguments_storage.push_back("clang");
        arguments_storage.push_back(std::pmr::string{input_file_path.generic_string()});
        arguments_storage.push_back("-o");
        arguments_storage.push_back(std::pmr::string{output_file_path.generic_string()});
        arguments_storage.push_back("-E");
        arguments_storage.push_back("-P");
        arguments_storage.push_back("-std=c23");

        if (target_triple.has_value())
        {
            arguments_storage.push_back("-target");
            arguments_storage.push_back(target_triple->data());
        }

        for (std::filesystem::path const& include_directory : include_directories)
        {
            std::string argument = std::format("-I{}", include_directory.generic_string());
            arguments_storage.push_back(std::pmr::string{argument});
        }

        std::pmr::vector<char const*> arguments;
        arguments.reserve(arguments_storage.size());
        for (std::pmr::string const& argument : arguments_storage)
            arguments.push_back(argument.data());

        std::unique_ptr<clang::CompilerInvocation> compiler_invocation = clang::createInvocation(arguments);
        compiler_instance->setInvocation(std::move(compiler_invocation));

        return compiler_instance;
    }

    std::filesystem::path get_temporary_file_path()
    {
        llvm::SmallString<256> temporary_file_path;
        llvm::sys::fs::createTemporaryFile("hlang-cheader-", "h", temporary_file_path);
        return std::filesystem::path{temporary_file_path.data(), temporary_file_path.data() + temporary_file_path.size()};
    }

    std::optional<std::pmr::string> get_content_after_preprocess(
        std::filesystem::path const& file_path,
        std::optional<std::string_view> const target_triple,
        std::span<std::filesystem::path const> const include_directories
    )
    {
        std::filesystem::path const output_file_path = get_temporary_file_path();

        std::unique_ptr<clang::CompilerInstance> const compiler_instance = create_clang_instance(file_path, output_file_path, target_triple, include_directories);

        auto const printPreprocessedAction = std::make_unique<clang::PrintPreprocessedAction>();
        compiler_instance->ExecuteAction(*printPreprocessedAction);

        std::optional<std::pmr::string> file_contents = h::common::get_file_contents(output_file_path);
        return file_contents;
    }

    std::optional<std::uint64_t> calculate_header_file_hash(
        std::filesystem::path const& header_path,
        std::optional<std::string_view> target_triple,
        std::span<std::filesystem::path const> include_directories
    )
    {
        std::optional<std::pmr::string> const contents_after_preprocess = get_content_after_preprocess(header_path, target_triple, include_directories);
        if (!contents_after_preprocess.has_value())
            return std::nullopt;

        XXH64_hash_t const hash = XXH64(contents_after_preprocess->data(), contents_after_preprocess->size(), 0);
        return hash;
    }
}
