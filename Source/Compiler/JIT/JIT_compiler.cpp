module;

#include <llvm/ExecutionEngine/Orc/LazyReexports.h>

module h.compiler.jit_compiler;

import std;
import llvm;

import h.common;
import h.common.filesystem;
import h.compiler;
import h.compiler.common;
import h.compiler.core_module_layer;
import h.compiler.recompile_module_layer;

namespace h::compiler
{
    JIT_data::~JIT_data()
    {
        recompile_module_layer.reset();
        core_module_layer.reset();
        compile_on_demand_layer.reset();
        mangle.reset();
        lazy_call_through_manager.reset();
        indirect_stubs_manager.reset();

        {
            llvm::Error error = epc_indirection_utils->cleanup();
            if (error)
                std::puts(std::format("Error while cleaning up EPC indirection utils: {}", llvm::toString(std::move(error))).c_str());
        }
        epc_indirection_utils.reset();

        llvm_jit.reset();
    }

    std::unique_ptr<JIT_data> create_jit_data(
        llvm::DataLayout& llvm_data_layout,
        std::pmr::vector<std::filesystem::path> search_library_paths,
        bool const debug
    )
    {
        llvm::orc::LLJITBuilder builder;
        builder.setDataLayout(llvm_data_layout);

        if (debug)
        {
              builder.setPrePlatformSetup(
                [](llvm::orc::LLJIT& llvm_jit) -> llvm::Error
                {
                    llvm::Error const error = llvm::orc::enableDebuggerSupport(llvm_jit);
                    return llvm::Error::success();
                }
            );
        }

        llvm::Expected<std::unique_ptr<llvm::orc::LLJIT>> llvm_jit_result = builder.create();

        if (llvm::Error error = llvm_jit_result.takeError())
            h::common::print_message_and_exit(std::format("Error while creating LLJIT: {}", llvm::toString(std::move(error))));

        std::unique_ptr<llvm::orc::LLJIT> llvm_jit = std::move(*llvm_jit_result);

        llvm::Expected<std::unique_ptr<llvm::orc::EPCIndirectionUtils>> epc_indirection_utils = llvm::orc::EPCIndirectionUtils::Create(llvm_jit->getExecutionSession());
        if (!epc_indirection_utils)
            h::common::print_message_and_exit(std::format("Could not create EPC indirection utils: {}", llvm::toString(epc_indirection_utils.takeError())));

        std::unique_ptr<llvm::orc::IndirectStubsManager> indirect_stubs_manager = epc_indirection_utils.get()->createIndirectStubsManager();

        llvm::Expected<std::unique_ptr<llvm::orc::LazyCallThroughManager>> local_lazy_call_through_manager = llvm::orc::createLocalLazyCallThroughManager(
            llvm_jit->getTargetTriple(),
            llvm_jit->getExecutionSession(),
            llvm::orc::ExecutorAddr()
        );
        if (!local_lazy_call_through_manager)
            h::common::print_message_and_exit(std::format("Could not create local lazy call through manager: {}", llvm::toString(local_lazy_call_through_manager.takeError())));

        std::unique_ptr<llvm::orc::MangleAndInterner> mangle = std::make_unique<llvm::orc::MangleAndInterner>(llvm_jit->getExecutionSession(), llvm_jit->getDataLayout());

        std::unique_ptr<llvm::orc::CompileOnDemandLayer> compile_on_demand_layer = std::make_unique<llvm::orc::CompileOnDemandLayer>(
            llvm_jit->getExecutionSession(),
            llvm_jit->getIRCompileLayer(),
            *local_lazy_call_through_manager.get(),
            [&epc = *epc_indirection_utils.get()]() { return epc.createIndirectStubsManager(); }
        );

        std::unique_ptr<Core_module_layer> core_module_layer = std::make_unique<Core_module_layer>(
            //*compile_on_demand_layer,
            llvm_jit->getIRCompileLayer(),
            *mangle
        );

        std::unique_ptr<Recompile_module_layer> recompile_module_layer = std::make_unique<Recompile_module_layer>(
            llvm_jit->getExecutionSession(),
            *core_module_layer,
            *local_lazy_call_through_manager->get(),
            *indirect_stubs_manager,
            *mangle
        );

        std::unique_ptr<JIT_data> jit_data = std::make_unique<JIT_data>();
        jit_data->llvm_jit = std::move(llvm_jit);
        jit_data->epc_indirection_utils = std::move(*epc_indirection_utils);
        jit_data->indirect_stubs_manager = std::move(indirect_stubs_manager);
        jit_data->lazy_call_through_manager = std::move(*local_lazy_call_through_manager);
        jit_data->mangle = std::move(mangle);
        jit_data->compile_on_demand_layer = std::move(compile_on_demand_layer);
        jit_data->core_module_layer = std::move(core_module_layer);
        jit_data->recompile_module_layer = std::move(recompile_module_layer);
        jit_data->search_library_paths = std::move(search_library_paths);

        return jit_data;
    }

    bool add_core_module(
        JIT_data& jit_data,
        llvm::orc::JITDylib& library,
        Core_module_compilation_data core_compilation_data
    )
    {
        llvm::Error error = jit_data.recompile_module_layer->add(
            library.createResourceTracker(),
            std::move(core_compilation_data)
        );
        if (error)
        {
            std::puts(std::format("Error while adding core module to JIT: {}", llvm::toString(std::move(error))).c_str());
            return false;
        }

        // Print execution session state. Useful for debugging.
        // jit_data.llvm_jit->getExecutionSession().dump(llvm::dbgs());

        return true;
    }

    bool add_core_module(
        JIT_data& jit_data,
        Core_module_compilation_data core_compilation_data
    )
    {
        llvm::orc::JITDylib& library = jit_data.llvm_jit->getMainJITDylib();
        return add_core_module(jit_data, library, core_compilation_data);
    }

    llvm::orc::JITDylib& get_main_library(
        JIT_data& jit_data
    )
    {
        return jit_data.llvm_jit->getMainJITDylib();
    }

    void link_static_library(
        JIT_data& jit_data,
        char const* const static_library_path
    )
    {
        std::optional<std::filesystem::path> const path = h::common::search_file(static_library_path, jit_data.search_library_paths);
        if (!path)
            h::common::print_message_and_exit(std::format("Could not find '{}' in search library paths", static_library_path));

        std::string const path_string = path->generic_string();

        llvm::Error error = jit_data.llvm_jit->linkStaticLibraryInto(jit_data.llvm_jit->getMainJITDylib(), path_string.c_str());
        if (error)
            h::common::print_message_and_exit(std::format("Error while linking static library: {}", llvm::toString(std::move(error))));
    }

    void load_platform_dynamic_library(
        JIT_data& jit_data,
        char const* const dynamic_library_path
    )
    {
        llvm::Expected<llvm::orc::JITDylib&> jit_dynamic_library = jit_data.llvm_jit->loadPlatformDynamicLibrary(dynamic_library_path);
        if (!jit_dynamic_library)
            h::common::print_message_and_exit(std::format("Link error while loading dynamic library: {}", llvm::toString(jit_dynamic_library.takeError())));
    }

    std::optional<llvm::orc::ExecutorSymbolDef> get_function(
        JIT_data& jit_data,
        std::string_view const mangled_function_name
    )
    {
        llvm::orc::MangleAndInterner& mangle = *jit_data.mangle;

        llvm::orc::SymbolStringPtr symbol = mangle(mangled_function_name.data());

        llvm::orc::ExecutionSession& execution_session = jit_data.llvm_jit->getExecutionSession();
        llvm::orc::JITDylib& main_library = jit_data.llvm_jit->getMainJITDylib();

        llvm::Expected<llvm::orc::ExecutorSymbolDef> function_address = execution_session.lookup(
            llvm::orc::makeJITDylibSearchOrder({ &main_library }),
            symbol,
            llvm::orc::SymbolState::Resolved
        );
        if (!function_address)
        {
            std::string const error_message = llvm::toString(function_address.takeError());
            std::puts(error_message.c_str());
            return std::nullopt;
        }

        return *function_address;
    }

    std::future<int> call_as_main_without_arguments(
        JIT_data& jit_data,
        std::string_view const function_name
    )
    {
        llvm::Expected<llvm::orc::ExecutorAddr> function_address = jit_data.llvm_jit->lookup({ function_name.data(), function_name.size() });
        if (!function_address)
            h::common::print_message_and_exit(std::format("Error while looking up function '{}': {}", function_name, llvm::toString(function_address.takeError())));

        int(*function_pointer)() = function_address->toPtr<int(*)()>();

        std::future<int> future = std::async(std::launch::async, function_pointer);
        return future;
    }

    void run_jit(
        JIT_data& jit_data,
        LLVM_data& llvm_data,
        std::span<h::Module> const core_modules,
        std::span<h::Module const> const core_module_dependencies,
        std::span<std::pmr::string const> const dynamic_libraries,
        std::span<std::pmr::string const> const static_libraries,
        std::string_view const entry_point
    )
    {
        // TODO remove this function

        // Print internal LLVM messages:
        //llvm::DebugFlag = true;

        /*for (std::size_t index = 0; index < core_modules.size(); ++index)
        {
            if (index == 1)
                continue;

            add_core_module(jit_data, { llvm_data, std::move(core_modules[index]), module_name_to_file_path_map });
        }

        for (std::pmr::string const& dynamic_library : dynamic_libraries)
        {
            load_platform_dynamic_library(jit_data, dynamic_library.c_str());
        }

        for (std::pmr::string const& static_library : static_libraries)
        {
            link_static_library(jit_data, static_library.c_str());
        }

        std::future<int> result = call_as_main_without_arguments(jit_data, entry_point);

        {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(5s);
        }

        add_core_module(jit_data, { llvm_data, std::move(core_modules[1]), module_name_to_file_path_map });

        {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(500s);
        }*/
    }
}
