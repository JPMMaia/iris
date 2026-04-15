export module iris.compiler.core_module_layer;

import std;
import llvm;

import iris.core;
import iris.compiler;

namespace iris::compiler
{
    export struct Core_module_compilation_data
    {
        LLVM_data& llvm_data;
        iris::Module core_module;
        std::pmr::unordered_map<std::pmr::string, iris::Module> core_module_dependencies;
        Compilation_options compilation_options;
    };

    export class Core_module_materialization_unit : public llvm::orc::MaterializationUnit
    {
    public:

        Core_module_materialization_unit(
            Core_module_compilation_data core_module_compilation_data,
            llvm::orc::MangleAndInterner& mangle,
            llvm::orc::IRLayer& base_layer
        );

        llvm::StringRef getName() const final
        {
            return "Core_module_materialization_unit";
        }

        void materialize(
            std::unique_ptr<llvm::orc::MaterializationResponsibility> materialization_responsibility
        ) final;

        void discard(const llvm::orc::JITDylib& library, const llvm::orc::SymbolStringPtr& symbol_name) final;

    private:
        Core_module_compilation_data m_core_module_compilation_data;
        llvm::orc::MangleAndInterner& m_mangle;
        llvm::orc::IRLayer& m_base_layer;
    };

    export class Core_module_layer
    {
    public:

        Core_module_layer(
            llvm::orc::IRLayer& base_layer,
            llvm::orc::MangleAndInterner& mangle
        );

        llvm::Error add(
            llvm::orc::ResourceTrackerSP resource_tracker,
            Core_module_compilation_data core_module_compilation_data
        );

        void emit(
            std::unique_ptr<llvm::orc::MaterializationResponsibility> materialization_responsibility,
            Core_module_compilation_data core_module_compilation_data
        );

    private:
        llvm::orc::IRLayer& m_base_layer;
        llvm::orc::MangleAndInterner& m_mangle;
    };
}
