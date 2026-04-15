module;

#include <stdio.h>

module iris.compiler.core_module_layer;

import std;
import llvm;

import iris.core;
import iris.common;
import iris.compiler;
import iris.compiler.common;

namespace iris::compiler
{
    static llvm::orc::MaterializationUnit::Interface get_interface(
        iris::Module const& core_module,
        llvm::orc::MangleAndInterner& mangle
    )
    {
        llvm::orc::SymbolFlagsMap symbols;

        for (iris::Function_definition const& function_definition : core_module.definitions.function_definitions)
        {
            if (function_definition.name.ends_with("$body"))
            {
                std::string const mangled_name = mangle_name(core_module, function_definition.name, std::nullopt);

                symbols.insert(
                    { mangle(mangled_name.c_str()), llvm::JITSymbolFlags::Exported | llvm::JITSymbolFlags::Callable }
                );
            }
        }

        return llvm::orc::MaterializationUnit::Interface{ std::move(symbols), nullptr };
    }

    Core_module_materialization_unit::Core_module_materialization_unit(
        Core_module_compilation_data core_module_compilation_data,
        llvm::orc::MangleAndInterner& mangle,
        llvm::orc::IRLayer& base_layer
    ) :
        llvm::orc::MaterializationUnit(get_interface(core_module_compilation_data.core_module, mangle)),
        m_core_module_compilation_data{ std::move(core_module_compilation_data) },
        m_mangle{ mangle },
        m_base_layer{ base_layer }
    {
    }

    void Core_module_materialization_unit::materialize(
        std::unique_ptr<llvm::orc::MaterializationResponsibility> materialization_responsibility
    )
    {
        std::chrono::high_resolution_clock::time_point const begin_materializing = std::chrono::high_resolution_clock::now();

        llvm::orc::MangleAndInterner& mangle = m_mangle;

        llvm::orc::SymbolNameSet const requested_symbols = materialization_responsibility->getRequestedSymbols();

        auto const is_requested_symbol = [&](iris::Function_definition const& definition) -> bool
        {
            std::string const mangled_name = mangle_name(m_core_module_compilation_data.core_module, definition.name, std::nullopt);
            return requested_symbols.contains(mangle(mangled_name.c_str()));
        };

        std::pmr::vector<std::string_view> functions_to_compile;

        for (iris::Function_definition const& definition : m_core_module_compilation_data.core_module.definitions.function_definitions)
        {
            if (is_requested_symbol(definition))
                functions_to_compile.push_back(definition.name);
        }

        // TODO refactor code so that exceptions are not used
        try
        {
            std::unique_ptr<llvm::Module> llvm_module = iris::compiler::create_llvm_module(
                m_core_module_compilation_data.llvm_data,
                m_core_module_compilation_data.core_module,
                m_core_module_compilation_data.core_module_dependencies,
                functions_to_compile,
                m_core_module_compilation_data.compilation_options
            );

            {
                std::pmr::deque<iris::Function_definition>& function_definitions = m_core_module_compilation_data.core_module.definitions.function_definitions;
                function_definitions.erase(
                    std::remove_if(function_definitions.begin(), function_definitions.end(), is_requested_symbol),
                    function_definitions.end()
                );

                if (!function_definitions.empty())
                {
                    std::unique_ptr<Core_module_materialization_unit> new_materialization_unit = std::make_unique<Core_module_materialization_unit>(
                        std::move(m_core_module_compilation_data),
                        m_mangle,
                        m_base_layer
                    );

                    llvm::Error error = materialization_responsibility->replace(std::move(new_materialization_unit));
                    if (error)
                        iris::common::print_message_and_exit(std::format("Error while creating a new materialization unit to replace unrequested symbols to target library: {}", llvm::toString(std::move(error))));
                }
            }

            llvm::orc::ThreadSafeContext thread_safe_context{ std::make_unique<llvm::LLVMContext>() };
            llvm::orc::ThreadSafeModule thread_safe_module{ std::move(llvm_module), std::move(thread_safe_context) };
            m_base_layer.emit(std::move(materialization_responsibility), std::move(thread_safe_module));
        }
        catch (std::exception const& exception)
        {
            std::fprintf(stderr, "%s\n", exception.what());

            if (materialization_responsibility != nullptr)
                materialization_responsibility->failMaterialization();
        }
        catch (...)
        {
            if (materialization_responsibility != nullptr)
                materialization_responsibility->failMaterialization();
        }

        std::chrono::high_resolution_clock::time_point const end_materializing = std::chrono::high_resolution_clock::now();

        using namespace std::chrono_literals;

        auto const materialization_duration = (end_materializing - begin_materializing) / 1ms;
        std::puts(std::format("Materialization of {} took {} ms", m_core_module_compilation_data.core_module.name, materialization_duration).c_str());
    }

    void Core_module_materialization_unit::discard(const llvm::orc::JITDylib& library, const llvm::orc::SymbolStringPtr& symbol_name)
    {
        // TODO
    }


    Core_module_layer::Core_module_layer(
        llvm::orc::IRLayer& base_layer,
        llvm::orc::MangleAndInterner& mangle
    ) :
        m_base_layer{ base_layer },
        m_mangle{ mangle }
    {
    }

    llvm::Error Core_module_layer::add(
        llvm::orc::ResourceTrackerSP resource_tracker,
        Core_module_compilation_data core_module_compilation_data
    )
    {
        llvm::orc::JITDylib& library = resource_tracker->getJITDylib();

        std::unique_ptr<Core_module_materialization_unit> materialization_unit = std::make_unique<Core_module_materialization_unit>(
            std::move(core_module_compilation_data),
            m_mangle,
            m_base_layer
        );

        return library.define(std::move(materialization_unit), resource_tracker);
    }

    void Core_module_layer::emit(
        std::unique_ptr<llvm::orc::MaterializationResponsibility> materialization_responsibility,
        Core_module_compilation_data core_module_compilation_data
    )
    {
        std::unique_ptr<Core_module_materialization_unit> materialization_unit = std::make_unique<Core_module_materialization_unit>(
            std::move(core_module_compilation_data),
            m_mangle,
            m_base_layer
        );

        materialization_unit->materialize(std::move(materialization_responsibility));
    }
}
