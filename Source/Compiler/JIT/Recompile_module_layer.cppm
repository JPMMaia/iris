export module iris.compiler.recompile_module_layer;

import std;
import llvm;

import iris.core;
import iris.compiler;
import iris.compiler.core_module_layer;

namespace iris::compiler
{
    export class Recompile_module_layer
    {
    public:

        Recompile_module_layer(
            llvm::orc::ExecutionSession& execution_session,
            iris::compiler::Core_module_layer& base_layer,
            llvm::orc::LazyCallThroughManager& lazy_call_through_manager,
            llvm::orc::IndirectStubsManager& indirect_stubs_manager,
            llvm::orc::MangleAndInterner& mangle
        );
        virtual ~Recompile_module_layer() = default;

        virtual llvm::Error add(
            llvm::orc::ResourceTrackerSP resource_tracker,
            Core_module_compilation_data core_module_compilation_data
        ) final;

        virtual void emit(
            std::unique_ptr<llvm::orc::MaterializationResponsibility> materialization_responsibility,
            Core_module_compilation_data core_module_compilation_data
        ) final;

    private:
        iris::compiler::Core_module_layer& m_base_layer;
        llvm::orc::ExecutionSession& m_execution_session;
        llvm::orc::LazyCallThroughManager& m_lazy_call_through_manager;
        llvm::orc::IndirectStubsManager& m_indirect_stubs_manager;
        llvm::orc::MangleAndInterner& m_mangle;
    };
}
