export module iris.compiler.parallel;

import std;

import iris.compiler.profiler;

namespace iris::compiler
{
    // Resolves a requested job count: 0 means one job per hardware thread. Never returns 0.
    export std::uint32_t resolve_job_count(
        std::uint32_t const requested_job_count
    );

    export struct Task_context
    {
        std::size_t index = 0;

        // Null when the caller did not pass a profiler. Otherwise a profiler owned by this task,
        // merged into the caller's profiler after the batch, so tasks never share one.
        Profiler* profiler = nullptr;

        // Filled by write_task_output when the batch runs on more than one thread.
        std::pmr::string output;
        std::FILE* immediate_output = nullptr;
    };

    // Use this instead of printf inside a task. On one thread the text is written immediately, as
    // before. On several threads each task's text is held back and written in task order, so the
    // log reads the same for any job count.
    export void write_task_output(
        Task_context& context,
        std::string_view const text
    );

    export template<typename... Arguments>
    void write_task_output(
        Task_context& context,
        std::format_string<Arguments...> const format,
        Arguments&&... arguments
    )
    {
        write_task_output(context, std::string_view{ std::format(format, std::forward<Arguments>(arguments)...) });
    }

    export struct Parallel_options
    {
        std::uint32_t job_count = 1;
        Profiler* profiler = nullptr;
        // Where task output goes. Null means stdout.
        std::FILE* output = nullptr;
    };

    // Runs task(context) for every index in [0, count).
    //
    // With one job (or one task) the tasks run in order on the calling thread. With more, the calling
    // thread and job_count - 1 workers take indices in increasing order.
    //
    // If tasks throw, the exception of the lowest failing index is rethrown on the calling thread,
    // together with the compilation breadcrumbs that task left behind. Tasks past that index are not
    // started and their output is dropped, so errors and logs match a serial run.
    //
    // Inside a task, print_message_and_exit throws Build_error instead of exiting.
    export void parallel_for(
        Parallel_options const& options,
        std::size_t const count,
        std::function<void(Task_context&)> const& task
    );

    export template<typename Element, typename Function>
    void parallel_for_each(
        Parallel_options const& options,
        std::span<Element> const elements,
        Function&& function
    )
    {
        parallel_for(
            options,
            elements.size(),
            [&](Task_context& context) -> void { function(elements[context.index], context); }
        );
    }
}
