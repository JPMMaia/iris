#include <catch2/catch_test_macros.hpp>
#include <stdio.h>

import std;
import iris.common;
import iris.compiler.diagnostic;
import iris.compiler.parallel;
import iris.compiler.profiler;

namespace iris::compiler
{
    struct Captured_output
    {
        std::FILE* file = nullptr;

        Captured_output() :
            file{ std::tmpfile() }
        {
            REQUIRE(file != nullptr);
        }

        ~Captured_output()
        {
            std::fclose(file);
        }

        std::string read()
        {
            std::fflush(file);
            std::rewind(file);

            std::string content;
            char buffer[256];
            while (std::size_t const read_count = std::fread(buffer, 1, sizeof(buffer), file))
                content.append(buffer, read_count);

            return content;
        }
    };

    static std::string create_expected_output(std::size_t const count)
    {
        std::string output;
        for (std::size_t index = 0; index < count; ++index)
            output += std::format("start {}\nend {}\n", index, index);
        return output;
    }

    // Makes later tasks finish first, so that ordered output and lowest-index errors are actually exercised.
    static void sleep_inversely(std::size_t const index, std::size_t const count)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{ 2 * (count - index) });
    }

    TEST_CASE("resolve_job_count")
    {
        CHECK(resolve_job_count(3) == 3);
        CHECK(resolve_job_count(0) >= 1);
    }

    TEST_CASE("parallel_for runs every task exactly once")
    {
        for (std::uint32_t const job_count : { 1u, 2u, 8u, 64u })
        {
            std::size_t const count = 100;
            std::vector<std::atomic<int>> run_counts(count);

            parallel_for(
                { .job_count = job_count },
                count,
                [&](Task_context& context) -> void { run_counts[context.index].fetch_add(1); }
            );

            for (std::size_t index = 0; index < count; ++index)
                CHECK(run_counts[index].load() == 1);
        }
    }

    TEST_CASE("parallel_for with no tasks does nothing")
    {
        bool was_task_run = false;
        parallel_for({ .job_count = 4 }, 0, [&](Task_context&) -> void { was_task_run = true; });
        CHECK(!was_task_run);
    }

    TEST_CASE("parallel_for writes task output in task order")
    {
        for (std::uint32_t const job_count : { 1u, 4u })
        {
            std::size_t const count = 16;
            Captured_output output;

            parallel_for(
                { .job_count = job_count, .output = output.file },
                count,
                [&](Task_context& context) -> void
                {
                    write_task_output(context, "start {}\n", context.index);
                    sleep_inversely(context.index, count);
                    write_task_output(context, "end {}\n", context.index);
                }
            );

            CHECK(output.read() == create_expected_output(count));
        }
    }

    TEST_CASE("parallel_for rethrows the lowest failing index and drops later output")
    {
        for (std::uint32_t const job_count : { 1u, 4u })
        {
            std::size_t const count = 16;
            Captured_output output;

            auto const run = [&]() -> void
            {
                parallel_for(
                    { .job_count = job_count, .output = output.file },
                    count,
                    [&](Task_context& context) -> void
                    {
                        write_task_output(context, "start {}\n", context.index);
                        sleep_inversely(context.index, count);
                        if (context.index == 5 || context.index == 9)
                            throw std::runtime_error{ std::format("failed {}", context.index) };
                        write_task_output(context, "end {}\n", context.index);
                    }
                );
            };

            try
            {
                run();
                FAIL("Expected an exception.");
            }
            catch (std::runtime_error const& error)
            {
                CHECK(std::string_view{ error.what() } == "failed 5");
            }

            CHECK(output.read() == create_expected_output(5) + "start 5\n");
        }
    }

    TEST_CASE("parallel_for turns print_message_and_exit into Build_error")
    {
        for (std::uint32_t const job_count : { 1u, 4u })
        {
            try
            {
                parallel_for(
                    { .job_count = job_count },
                    4,
                    [](Task_context& context) -> void
                    {
                        if (context.index == 2)
                            iris::common::print_message_and_exit(std::format("fatal {}", context.index));
                    }
                );
                FAIL("Expected a Build_error.");
            }
            catch (iris::common::Build_error const& error)
            {
                CHECK(std::string_view{ error.what() } == "fatal 2");
            }
        }
    }

    TEST_CASE("parallel_for carries the failing task's breadcrumbs to the calling thread")
    {
        for (std::uint32_t const job_count : { 1u, 4u })
        {
            try
            {
                parallel_for(
                    { .job_count = job_count },
                    4,
                    [](Task_context& context) -> void
                    {
                        Compilation_scope const scope{ "compiling module", std::format("module_{}", context.index) };
                        if (context.index == 3)
                            throw std::runtime_error{ "failed" };
                    }
                );
                FAIL("Expected an exception.");
            }
            catch (std::runtime_error const&)
            {
                std::pmr::string const context = format_compilation_context({});
                CHECK(context == "  while compiling module 'module_3'\n");
            }

            // Entering a scope with no exception in flight discards the stale trail.
            {
                Compilation_scope const scope{ "checking", "reset" };
            }
            CHECK(format_compilation_context({}).empty());
        }
    }

    TEST_CASE("parallel_for merges task profilers into the caller's profiler")
    {
        for (std::uint32_t const job_count : { 1u, 4u })
        {
            Profiler profiler;

            parallel_for(
                { .job_count = job_count, .profiler = &profiler },
                10,
                [](Task_context& context) -> void { add_duration(context.profiler, "task", std::chrono::milliseconds{ 1 }); }
            );

            REQUIRE(profiler.durations.size() == 1);
            CHECK(profiler.durations[0].first == "task");
            CHECK(std::chrono::duration_cast<std::chrono::milliseconds>(profiler.durations[0].second) == std::chrono::milliseconds{ 10 });
        }
    }

    TEST_CASE("parallel_for gives tasks no profiler when the caller has none")
    {
        // Catch2 assertions are not thread-safe, so the tasks only record what they saw.
        std::atomic<int> tasks_with_profiler = 0;

        parallel_for(
            { .job_count = 4 },
            4,
            [&](Task_context& context) -> void { if (context.profiler != nullptr) tasks_with_profiler.fetch_add(1); }
        );

        CHECK(tasks_with_profiler.load() == 0);
    }
}
