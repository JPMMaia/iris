module;

#include <stdio.h>

module iris.compiler.parallel;

import std;

import iris.common;
import iris.compiler.diagnostic;
import iris.compiler.profiler;

namespace iris::compiler
{
    std::uint32_t resolve_job_count(
        std::uint32_t const requested_job_count
    )
    {
        if (requested_job_count != 0)
            return requested_job_count;

        unsigned int const hardware_thread_count = std::thread::hardware_concurrency();
        return hardware_thread_count == 0 ? 1 : static_cast<std::uint32_t>(hardware_thread_count);
    }

    void write_task_output(
        Task_context& context,
        std::string_view const text
    )
    {
        if (context.immediate_output != nullptr)
        {
            std::fwrite(text.data(), 1, text.size(), context.immediate_output);
            return;
        }

        context.output.append(text);
    }

    static void merge_task_profiler(
        Profiler* const profiler,
        Profiler const& task_profiler
    )
    {
        for (auto const& [key, duration] : task_profiler.durations)
            add_duration(profiler, key, std::chrono::duration_cast<std::chrono::nanoseconds>(duration));
    }

    static std::FILE* get_output(
        Parallel_options const& options
    )
    {
        return options.output != nullptr ? options.output : stdout;
    }

    static void run_serially(
        Parallel_options const& options,
        std::size_t const count,
        std::function<void(Task_context&)> const& task
    )
    {
        for (std::size_t index = 0; index < count; ++index)
        {
            Profiler task_profiler;

            Task_context context
            {
                .index = index,
                .profiler = options.profiler != nullptr ? &task_profiler : nullptr,
                .output = {},
                .immediate_output = get_output(options),
            };

            {
                iris::common::Exit_as_build_error_scope const exit_scope;
                task(context);
            }

            merge_task_profiler(options.profiler, task_profiler);
        }
    }

    namespace
    {
        struct Task_state
        {
            Profiler profiler;
            std::pmr::string output;
            std::exception_ptr exception;
            std::vector<Compilation_breadcrumb> breadcrumbs;
            bool is_finished = false;
        };

        struct Batch_state
        {
            std::span<Task_state> tasks;
            std::FILE* output = nullptr;
            std::atomic<std::size_t> next_index = 0;
            std::atomic<std::size_t> first_failed_index = std::numeric_limits<std::size_t>::max();

            // Guards first_failed_index updates, is_finished, next_index_to_flush and writes to output.
            std::mutex mutex;
            std::size_t next_index_to_flush = 0;
        };

        // Writes every finished task's output that is next in line. Must hold batch.mutex.
        void flush_finished_outputs(Batch_state& batch)
        {
            std::size_t const last_index_to_flush = batch.first_failed_index.load();

            while (batch.next_index_to_flush < batch.tasks.size() && batch.next_index_to_flush <= last_index_to_flush)
            {
                Task_state& task_state = batch.tasks[batch.next_index_to_flush];
                if (!task_state.is_finished)
                    break;

                if (!task_state.output.empty())
                {
                    std::fwrite(task_state.output.data(), 1, task_state.output.size(), batch.output);
                    std::fflush(batch.output);
                }

                task_state.output.clear();
                ++batch.next_index_to_flush;
            }
        }

        void run_worker(
            Batch_state& batch,
            bool const use_profiler,
            std::function<void(Task_context&)> const& task
        )
        {
            iris::common::Exit_as_build_error_scope const exit_scope;

            while (true)
            {
                std::size_t const index = batch.next_index.fetch_add(1);
                if (index >= batch.tasks.size())
                    return;

                // A serial run would have stopped at the failure, so do not start anything past it.
                if (index > batch.first_failed_index.load())
                    return;

                Task_state& task_state = batch.tasks[index];

                Task_context context
                {
                    .index = index,
                    .profiler = use_profiler ? &task_state.profiler : nullptr,
                    .output = {},
                    .immediate_output = nullptr,
                };

                std::size_t const breadcrumb_count = get_compilation_breadcrumb_count();

                try
                {
                    task(context);
                }
                catch (...)
                {
                    task_state.exception = std::current_exception();
                    task_state.breadcrumbs = take_compilation_breadcrumbs(breadcrumb_count);
                }

                std::lock_guard const lock{ batch.mutex };

                task_state.output = std::move(context.output);
                task_state.is_finished = true;

                if (task_state.exception != nullptr && index < batch.first_failed_index.load())
                    batch.first_failed_index.store(index);

                flush_finished_outputs(batch);
            }
        }
    }

    void parallel_for(
        Parallel_options const& options,
        std::size_t const count,
        std::function<void(Task_context&)> const& task
    )
    {
        std::uint32_t const job_count = static_cast<std::uint32_t>(
            std::min<std::size_t>(std::max<std::uint32_t>(options.job_count, 1), count)
        );

        if (job_count <= 1)
        {
            run_serially(options, count, task);
            return;
        }

        std::vector<Task_state> tasks(count);

        Batch_state batch
        {
            .tasks = tasks,
            .output = get_output(options),
        };

        bool const use_profiler = options.profiler != nullptr;

        {
            std::vector<std::jthread> workers;
            workers.reserve(job_count - 1);
            for (std::uint32_t worker_index = 1; worker_index < job_count; ++worker_index)
                workers.emplace_back([&]() -> void { run_worker(batch, use_profiler, task); });

            run_worker(batch, use_profiler, task);
        }

        std::size_t const first_failed_index = batch.first_failed_index.load();

        for (std::size_t index = 0; index < count && index <= first_failed_index; ++index)
            merge_task_profiler(options.profiler, tasks[index].profiler);

        if (first_failed_index < count)
        {
            Task_state& failed_task = tasks[first_failed_index];
            restore_compilation_breadcrumbs(failed_task.breadcrumbs);
            std::rethrow_exception(failed_task.exception);
        }
    }
}
